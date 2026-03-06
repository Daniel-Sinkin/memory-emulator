#include "memory_vm.hpp"

#include "printer.hpp"

#include <gsl/gsl>
#include <print>

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

MemoryVM::MemoryVM(VMConfig config)
    : config_{validated(config)}, ram_(static_cast<usize>(config_.memory_size)), cpu_{ram_, config_}
{
}

auto MemoryVM::read(u32 addr) const -> std::byte
{
    Expects(addr < config_.memory_size);
    return ram_[static_cast<usize>(addr)];
}

auto MemoryVM::write_byte(u32 addr, std::byte value) -> void
{
    Expects(addr < config_.memory_size);
    ram_[static_cast<usize>(addr)] = value;
}

auto MemoryVM::write4(u32 addr, u32 value) -> void
{
    write(addr, value);
    write(addr + 1u, value >> 8u);
    write(addr + 2u, value >> 16u);
    write(addr + 3u, value >> 24u);
}

auto MemoryVM::print() const -> void
{
    std::println("----\nRAM:\n----");
    print_ram(ram_, config_.cacheline_size());
    cpu_.print();
}

}  // namespace ds_mem
