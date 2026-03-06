#pragma once

#include "constants.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ds_mem
{

class Cache
{
  public:
    struct CacheStats
    {
        u32 hit_count{0u};
        u32 miss_count{0u};
    };

    struct Evicted
    {
        u32 cl_idx{0u};
        std::vector<std::byte> data{};
    };

    struct DirtyEntry
    {
        u32 cl_idx{0u};
        std::vector<std::byte> data{};
    };

    Cache(u32 total_size, u32 cacheline_size);

    [[nodiscard]] auto total_size() const noexcept -> u32
    {
        return total_size_;
    }
    [[nodiscard]] auto cacheline_size() const noexcept -> u32
    {
        return cacheline_size_;
    }
    [[nodiscard]] auto num_slots() const noexcept -> u32
    {
        return num_slots_;
    }

    auto stats() noexcept -> CacheStats&
    {
        return stats_;
    }
    auto stats() const noexcept -> const CacheStats&
    {
        return stats_;
    }

    auto memory() noexcept -> std::vector<std::byte>&
    {
        return memory_;
    }
    auto memory() const noexcept -> const std::vector<std::byte>&
    {
        return memory_;
    }

    auto tags() noexcept -> std::vector<u32>&
    {
        return tags_;
    }
    auto tags() const noexcept -> const std::vector<u32>&
    {
        return tags_;
    }

    auto valids() noexcept -> std::vector<u32>&
    {
        return valids_;
    }
    auto valids() const noexcept -> const std::vector<u32>&
    {
        return valids_;
    }

    auto dirty_bits() noexcept -> std::vector<u32>&
    {
        return dirty_bits_;
    }
    auto dirty_bits() const noexcept -> const std::vector<u32>&
    {
        return dirty_bits_;
    }

    [[nodiscard]] auto lookup(u32 cl_idx, u32 offset) -> std::optional<std::byte>;
    [[nodiscard]] auto get_cacheline_data(u32 cl_idx) const -> std::vector<std::byte>;
    [[nodiscard]] auto has(u32 cl_idx) const noexcept -> bool;
    [[nodiscard]] auto cl_idx_of_slot(u32 slot) const noexcept -> u32;

    auto write_byte(u32 cl_idx, u32 offset, std::byte value) -> void;
    auto mark_dirty(u32 cl_idx) -> void;
    auto clear_dirty(u32 cl_idx) -> void;
    [[nodiscard]] auto flush_all() -> std::vector<DirtyEntry>;
    [[nodiscard]] auto fill(u32 cl_idx, std::span<const std::byte> data) -> std::optional<Evicted>;

  private:
    [[nodiscard]] auto slot_of(u32 cl_idx) const noexcept -> u32;
    [[nodiscard]] auto tag_of(u32 cl_idx) const noexcept -> u32;

    u32 total_size_{0u};
    u32 cacheline_size_{0u};
    u32 num_slots_{0u};

    std::vector<std::byte> memory_{};
    std::vector<u32> tags_{};
    std::vector<u32> valids_{};
    std::vector<u32> dirty_bits_{};
    CacheStats stats_{};
};

}  // namespace ds_mem
