#include "cpu.hpp"

#include "printer.hpp"

#include <bit>
#include <gsl/gsl>
#include <print>
#include <utility>

namespace ds_mem
{
namespace
{

auto validated(VMConfig config) -> VMConfig
{
    validate_config_or_throw(config);
    return config;
}

}  // namespace

CPU::CPU(std::vector<std::byte>& ram, VMConfig config)
    : ram_{ram}, config_{validated(config)}, l1_{config_.l1_size, config_.cacheline_size()},
      l2_{config_.l2_size, config_.cacheline_size()}
{
}

auto CPU::fetch_cacheline_from_ram(u32 cl_idx) const -> std::vector<std::byte>
{
    const auto cacheline_size = config_.cacheline_size();
    const auto base = cl_idx * cacheline_size;

    std::vector<std::byte> out(static_cast<usize>(cacheline_size));
    for (auto i = 0u; i < cacheline_size; ++i)
    {
        const auto idx = static_cast<usize>(base + i);
        out[static_cast<usize>(i)] = ram_[idx];
    }
    return out;
}

auto CPU::writeback_to_ram(u32 cl_idx, std::span<const std::byte> data) -> void
{
    const auto cacheline_size = config_.cacheline_size();
    Expects(data.size() == static_cast<usize>(cacheline_size));

    const auto base = cl_idx * cacheline_size;
    for (auto i = 0u; i < cacheline_size; ++i)
    {
        const auto idx = static_cast<usize>(base + i);
        ram_[idx] = data[static_cast<usize>(i)];
    }
}

auto CPU::read(u32 addr) -> std::byte
{
    Expects(addr < config_.memory_size);

    const auto cacheline_size = config_.cacheline_size();
    const auto cl_idx = addr / cacheline_size;
    const auto cl_offset = addr % cacheline_size;

    if (auto value = l1_.lookup(cl_idx, cl_offset))
    {
        return *value;
    }

    if (auto value = l2_.lookup(cl_idx, cl_offset))
    {
        const auto l2_data = l2_.get_cacheline_data(cl_idx);
        fill_l1(cl_idx, l2_data);
        return *value;
    }

    const auto ram_data = fetch_cacheline_from_ram(cl_idx);
    fill_l2(cl_idx, ram_data);
    fill_l1(cl_idx, ram_data);
    return ram_data[static_cast<usize>(cl_offset)];
}

auto CPU::read_u32(u32 addr) -> u32
{
    Expects(static_cast<u64>(addr) + 4u <= static_cast<u64>(config_.memory_size));

    const auto b0 = std::to_integer<u32>(read(addr));
    const auto b1 = std::to_integer<u32>(read(addr + 1u));
    const auto b2 = std::to_integer<u32>(read(addr + 2u));
    const auto b3 = std::to_integer<u32>(read(addr + 3u));

    return b0 | (b1 << 8u) | (b2 << 16u) | (b3 << 24u);
}

auto CPU::read_f32(u32 addr) -> f32
{
    return std::bit_cast<f32>(read_u32(addr));
}

auto CPU::write_byte(u32 addr, std::byte value) -> void
{
    Expects(addr < config_.memory_size);

    const auto cacheline_size = config_.cacheline_size();
    const auto cl_idx = addr / cacheline_size;
    const auto cl_offset = addr % cacheline_size;

    if (!l1_.lookup(cl_idx, cl_offset))
    {
        if (l2_.lookup(cl_idx, cl_offset))
        {
            const auto l2_data = l2_.get_cacheline_data(cl_idx);
            fill_l1(cl_idx, l2_data);
        }
        else
        {
            const auto ram_data = fetch_cacheline_from_ram(cl_idx);
            fill_l2(cl_idx, ram_data);
            fill_l1(cl_idx, ram_data);
        }
    }

    l1_.write_byte(cl_idx, cl_offset, value);
    l1_.mark_dirty(cl_idx);
}

auto CPU::write_u32(u32 addr, u32 value) -> void
{
    Expects(static_cast<u64>(addr) + 4u <= static_cast<u64>(config_.memory_size));

    write(addr, static_cast<std::byte>(value & 0xFFu));
    write(addr + 1u, static_cast<std::byte>((value >> 8u) & 0xFFu));
    write(addr + 2u, static_cast<std::byte>((value >> 16u) & 0xFFu));
    write(addr + 3u, static_cast<std::byte>((value >> 24u) & 0xFFu));
}

auto CPU::write_f32(u32 addr, f32 value) -> void
{
    write_u32(addr, std::bit_cast<u32>(value));
}

auto CPU::write_through_byte(u32 addr, std::byte value) -> void
{
    write_byte(addr, value);

    const auto cl_idx = addr / config_.cacheline_size();
    const auto l1_data = l1_.get_cacheline_data(cl_idx);

    std::ignore = l2_.fill(cl_idx, l1_data);
    l2_.mark_dirty(cl_idx);
    writeback_to_ram(cl_idx, l1_data);
    l1_.clear_dirty(cl_idx);
    l2_.clear_dirty(cl_idx);
}

auto CPU::write4(u32 addr, u32 value) -> void
{
    write_u32(addr, value);
}

auto CPU::fill_l2(u32 fill_idx, std::span<const std::byte> fill_data) -> void
{
    if (auto evicted = l2_.fill(fill_idx, fill_data))
    {
        writeback_to_ram(evicted->cl_idx, evicted->data);
    }
}

auto CPU::fill_l1(u32 fill_idx, std::span<const std::byte> fill_data) -> void
{
    if (auto evicted = l1_.fill(fill_idx, fill_data))
    {
        fill_l2(evicted->cl_idx, evicted->data);
        l2_.mark_dirty(evicted->cl_idx);
    }
}

auto CPU::flush() -> void
{
    for (const auto& entry : l1_.flush_all())
    {
        fill_l2(entry.cl_idx, entry.data);
        l2_.mark_dirty(entry.cl_idx);
    }

    for (const auto& entry : l2_.flush_all())
    {
        writeback_to_ram(entry.cl_idx, entry.data);
    }
}

auto CPU::print() const -> void
{
    std::println("---\nL2:\n---");
    print_l2_overview(l2_, l1_, config_.cacheline_size());
    std::println("---\nL1:\n---");
    print_l1_overview(l1_, config_.cacheline_size());
}

auto CPU::print_detailed() const -> void
{
    print_cache_detailed(l1_, nullptr, config_.cacheline_size(), "L1");
    std::println();
    print_cache_detailed(l2_, &l1_, config_.cacheline_size(), "L2");
}

auto CPU::print_stats() const -> void
{
    ds_mem::print_stats(l1_.stats(), l2_.stats(), config_);
}

}  // namespace ds_mem
