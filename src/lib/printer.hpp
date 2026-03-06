#pragma once

#include "cache.hpp"
#include "constants.hpp"
#include "vm_config.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace ds_mem
{

auto print_ram(std::span<const std::byte> ram, u32 cacheline_size, usize blocks_per_row = 4zu)
    -> void;
auto print_l1_overview(const Cache& l1, u32 cacheline_size, usize blocks_per_row = 4zu) -> void;
auto print_l2_overview(
    const Cache& l2, const Cache& l1, u32 cacheline_size, usize blocks_per_row = 4zu
) -> void;
auto print_cache_detailed(
    const Cache& cache, const Cache* l1_for_stale_check, u32 cacheline_size, std::string_view name
) -> void;
auto print_stats(
    const Cache::CacheStats& l1_stats, const Cache::CacheStats& l2_stats, const VMConfig& config
) -> void;

}  // namespace ds_mem
