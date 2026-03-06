#include "printer.hpp"

#include <algorithm>
#include <gsl/gsl>
#include <print>

namespace ds_mem
{
namespace
{

constexpr auto k_cyan = "\033[38;5;44m";
constexpr auto k_orange = "\033[38;5;208m";
constexpr auto k_green = "\033[38;5;34m";
constexpr auto k_red = "\033[38;5;196m";
constexpr auto k_reset = "\033[0m";

[[nodiscard]] auto row_width(u32 cacheline_size, usize blocks_per_row) -> usize
{
    const auto blocks = blocks_per_row == 0zu ? 1zu : blocks_per_row;
    return static_cast<usize>(cacheline_size) * blocks;
}

[[nodiscard]] auto is_printable_ascii(unsigned char ch) -> bool
{
    return ch >= 0x20U && ch <= 0x7EU;
}

auto print_ascii_column(std::span<const std::byte> mem, usize row_start, usize row_len) -> void
{
    std::print("  |");
    for (auto col = 0zu; col < row_len; ++col)
    {
        const auto ch = std::to_integer<unsigned char>(mem[row_start + col]);
        std::print("{}", is_printable_ascii(ch) ? static_cast<char>(ch) : '.');
    }
    std::print("|");
}

[[nodiscard]] auto
has_nonzero_cacheline(std::span<const std::byte> mem, usize line_start, usize cacheline_size)
    -> bool
{
    const auto line_end = std::min(line_start + cacheline_size, mem.size());
    for (auto idx = line_start; idx < line_end; ++idx)
    {
        if (std::to_integer<u32>(mem[idx]) != 0u)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto color_for_l1_slot(const Cache& l1, u32 slot) -> const char*
{
    const auto& valids = l1.valids();
    const auto& dirty = l1.dirty_bits();
    const auto idx = static_cast<usize>(slot);

    if (valids[idx] == 0u)
    {
        return nullptr;
    }
    return dirty[idx] != 0u ? k_orange : k_green;
}

[[nodiscard]] auto color_for_l2_slot(const Cache& l2, const Cache& l1, u32 slot) -> const char*
{
    const auto& valids = l2.valids();
    const auto& dirty = l2.dirty_bits();
    const auto idx = static_cast<usize>(slot);

    if (valids[idx] == 0u)
    {
        return nullptr;
    }

    if (dirty[idx] != 0u)
    {
        return k_red;
    }

    const auto cl_idx = l2.cl_idx_of_slot(slot);
    if (l1.has(cl_idx))
    {
        const auto l1_slot = cl_idx % l1.num_slots();
        if (l1.dirty_bits()[static_cast<usize>(l1_slot)] != 0u)
        {
            return k_orange;
        }
    }
    return k_green;
}

auto print_cache_bytes(
    const Cache& cache, u32 cacheline_size, usize blocks_per_row, const Cache* l1_for_stale_check
) -> void
{
    Expects(cacheline_size > 0u);

    const auto cols = row_width(cacheline_size, blocks_per_row);
    const auto& mem = cache.memory();
    const auto total_bytes = mem.size();

    if (total_bytes == 0zu)
    {
        std::println("(empty)");
        return;
    }

    for (auto row_start = 0zu; row_start < total_bytes; row_start += cols)
    {
        if (row_start > 0zu)
        {
            std::println();
        }

        const auto row_len = std::min(cols, total_bytes - row_start);
        std::print("{:02x}: ", row_start);

        for (auto col = 0zu; col < row_len; ++col)
        {
            if (col > 0zu && col % static_cast<usize>(cacheline_size) == 0zu)
            {
                std::print(" ");
            }

            const auto idx = row_start + col;
            const auto slot = static_cast<u32>(idx / static_cast<usize>(cacheline_size));
            const auto value = std::to_integer<u32>(mem[idx]);

            const char* color = nullptr;
            if (l1_for_stale_check == nullptr)
            {
                color = color_for_l1_slot(cache, slot);
            }
            else
            {
                color = color_for_l2_slot(cache, *l1_for_stale_check, slot);
            }

            if (color == nullptr)
            {
                std::print("{:02X}", value);
            }
            else
            {
                std::print("{}{:02X}{}", color, value, k_reset);
            }
        }

        print_ascii_column(mem, row_start, row_len);
    }
    std::println();
}

[[nodiscard]] auto detail_color(const Cache& cache, const Cache* l1_for_stale_check, u32 slot)
    -> const char*
{
    const auto& dirty = cache.dirty_bits();
    const auto idx = static_cast<usize>(slot);
    const auto is_dirty = dirty[idx] != 0u;
    if (is_dirty)
    {
        return k_orange;
    }

    if (l1_for_stale_check != nullptr)
    {
        const auto cl_idx = cache.cl_idx_of_slot(slot);
        if (l1_for_stale_check->has(cl_idx))
        {
            const auto l1_slot = cl_idx % l1_for_stale_check->num_slots();
            if (l1_for_stale_check->dirty_bits()[static_cast<usize>(l1_slot)] != 0u)
            {
                return k_orange;
            }
        }
    }

    return k_green;
}

}  // namespace

auto print_ram(std::span<const std::byte> ram, u32 cacheline_size, usize blocks_per_row) -> void
{
    Expects(cacheline_size > 0u);

    const auto cols = row_width(cacheline_size, blocks_per_row);
    if (ram.empty())
    {
        std::println("(empty)");
        return;
    }

    for (auto row_start = 0zu; row_start < ram.size(); row_start += cols)
    {
        if (row_start > 0zu)
        {
            std::println();
        }

        const auto row_len = std::min(cols, ram.size() - row_start);
        std::print("{:02x}: ", row_start);

        for (auto col = 0zu; col < row_len; ++col)
        {
            if (col > 0zu && col % static_cast<usize>(cacheline_size) == 0zu)
            {
                std::print(" ");
            }

            const auto idx = row_start + col;
            const auto cl_start = idx - (idx % static_cast<usize>(cacheline_size));
            const auto value = std::to_integer<u32>(ram[idx]);

            if (has_nonzero_cacheline(ram, cl_start, static_cast<usize>(cacheline_size)))
            {
                std::print("{}{:02X}{}", k_cyan, value, k_reset);
            }
            else
            {
                std::print("{:02X}", value);
            }
        }

        print_ascii_column(ram, row_start, row_len);
    }
    std::println();
}

auto print_l1_overview(const Cache& l1, u32 cacheline_size, usize blocks_per_row) -> void
{
    print_cache_bytes(l1, cacheline_size, blocks_per_row, nullptr);
}

auto print_l2_overview(const Cache& l2, const Cache& l1, u32 cacheline_size, usize blocks_per_row)
    -> void
{
    print_cache_bytes(l2, cacheline_size, blocks_per_row, &l1);
}

auto print_cache_detailed(
    const Cache& cache, const Cache* l1_for_stale_check, u32 cacheline_size, std::string_view name
) -> void
{
    Expects(cacheline_size > 0u);

    const auto slots = cache.num_slots();
    const auto byte_width = static_cast<usize>(cacheline_size) * 2zu;

    std::println("=== {} Cache ({} bytes, {} slots) ===", name, cache.total_size(), slots);
    std::println(
        "{:>4}  {:>1}  {:>1}  {:>3}  {:>6}  {:<{}}  {}",
        "slot",
        "V",
        "D",
        "tag",
        "addr",
        "data",
        byte_width,
        "ascii"
    );
    std::println(
        "{:-<4}  {:-<1}  {:-<1}  {:-<3}  {:-<6}  {:-<{}}  {:-<{}}",
        "",
        "",
        "",
        "",
        "",
        "",
        byte_width,
        "",
        static_cast<usize>(cacheline_size)
    );

    const auto& valids = cache.valids();
    const auto& dirty = cache.dirty_bits();
    const auto& tags = cache.tags();
    const auto& mem = cache.memory();

    for (auto slot = 0u; slot < slots; ++slot)
    {
        const auto slot_idx = static_cast<usize>(slot);
        const auto valid = valids[slot_idx] != 0u;
        const auto is_dirty = dirty[slot_idx] != 0u;
        const auto tag = tags[slot_idx];

        std::print("{:>4}  {:>1}  {:>1}  ", slot, valid ? 1u : 0u, is_dirty ? 1u : 0u);
        if (!valid)
        {
            std::println(
                "{:>3}  {:>6}  {:<{}}  {:<{}}",
                "-",
                "-",
                "-",
                byte_width,
                "-",
                static_cast<usize>(cacheline_size)
            );
            continue;
        }

        const auto cl_idx = cache.cl_idx_of_slot(slot);
        const auto ram_addr = cl_idx * cacheline_size;
        const auto* color = detail_color(cache, l1_for_stale_check, slot);

        std::print("{:>3}  0x{:04X}  ", tag, ram_addr);

        for (auto i = 0u; i < cacheline_size; ++i)
        {
            const auto mem_idx =
                slot_idx * static_cast<usize>(cacheline_size) + static_cast<usize>(i);
            const auto value = std::to_integer<u32>(mem[mem_idx]);
            std::print("{}{:02X}{}", color, value, k_reset);
        }

        std::print("  ");
        for (auto i = 0u; i < cacheline_size; ++i)
        {
            const auto mem_idx =
                slot_idx * static_cast<usize>(cacheline_size) + static_cast<usize>(i);
            const auto ch = std::to_integer<unsigned char>(mem[mem_idx]);
            std::print(
                "{}{}{}", color, is_printable_ascii(ch) ? static_cast<char>(ch) : '.', k_reset
            );
        }
        std::println();
    }
}

auto print_stats(
    const Cache::CacheStats& l1_stats, const Cache::CacheStats& l2_stats, const VMConfig& config
) -> void
{
    const auto l1_hit = static_cast<u64>(l1_stats.hit_count);
    const auto l1_miss = static_cast<u64>(l1_stats.miss_count);
    const auto l2_hit = static_cast<u64>(l2_stats.hit_count);
    const auto l2_miss = static_cast<u64>(l2_stats.miss_count);

    const auto ram_accesses = l2_miss;
    const auto total_cycles = l1_hit * static_cast<u64>(config.l1_latency)
                              + l2_hit * static_cast<u64>(config.l2_latency)
                              + ram_accesses * static_cast<u64>(config.ram_latency);
    const auto total_accesses = l1_hit + l1_miss;

    std::println("L1  : {:4} hits, {:4} misses", l1_hit, l1_miss);
    std::println("L2  : {:4} hits, {:4} misses", l2_hit, l2_miss);
    std::println("RAM : {:4} accesses", ram_accesses);
    std::println(
        "Cycles: {} ({:.1f} avg per access)",
        total_cycles,
        total_accesses > 0u
            ? static_cast<double>(total_cycles) / static_cast<double>(total_accesses)
            : 0.0
    );
}

}  // namespace ds_mem
