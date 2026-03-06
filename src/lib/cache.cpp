#include "cache.hpp"

#include <algorithm>
#include <gsl/gsl>

namespace ds_mem
{

Cache::Cache(u32 total_size, u32 cacheline_size)
    : total_size_{total_size}, cacheline_size_{cacheline_size},
      num_slots_{cacheline_size == 0u ? 0u : total_size / cacheline_size},
      memory_(static_cast<usize>(total_size)), tags_(static_cast<usize>(num_slots_)),
      valids_(static_cast<usize>(num_slots_)), dirty_bits_(static_cast<usize>(num_slots_))
{
    Expects(cacheline_size_ > 0u);
    Expects(total_size_ > 0u);
    Expects(total_size_ % cacheline_size_ == 0u);
}

auto Cache::slot_of(u32 cl_idx) const noexcept -> u32
{
    return cl_idx % num_slots_;
}

auto Cache::tag_of(u32 cl_idx) const noexcept -> u32
{
    return cl_idx / num_slots_;
}

auto Cache::lookup(u32 cl_idx, u32 offset) -> std::optional<std::byte>
{
    Expects(offset < cacheline_size_);

    const auto slot = slot_of(cl_idx);
    const auto tag = tag_of(cl_idx);

    if (valids_[static_cast<usize>(slot)] != 0u && tags_[static_cast<usize>(slot)] == tag)
    {
        ++stats_.hit_count;
        const auto idx = static_cast<usize>(slot) * static_cast<usize>(cacheline_size_)
                         + static_cast<usize>(offset);
        return memory_[idx];
    }

    ++stats_.miss_count;
    return std::nullopt;
}

auto Cache::get_cacheline_data(u32 cl_idx) const -> std::vector<std::byte>
{
    const auto slot = slot_of(cl_idx);
    const auto base = static_cast<usize>(slot) * static_cast<usize>(cacheline_size_);

    std::vector<std::byte> out(static_cast<usize>(cacheline_size_));
    std::copy_n(
        memory_.begin() + static_cast<std::ptrdiff_t>(base),
        static_cast<std::ptrdiff_t>(cacheline_size_),
        out.begin()
    );
    return out;
}

auto Cache::has(u32 cl_idx) const noexcept -> bool
{
    const auto slot = slot_of(cl_idx);
    const auto tag = tag_of(cl_idx);
    return valids_[static_cast<usize>(slot)] != 0u && tags_[static_cast<usize>(slot)] == tag;
}

auto Cache::cl_idx_of_slot(u32 slot) const noexcept -> u32
{
    return tags_[static_cast<usize>(slot)] * num_slots_ + slot;
}

auto Cache::write_byte(u32 cl_idx, u32 offset, std::byte value) -> void
{
    Expects(offset < cacheline_size_);
    const auto slot = slot_of(cl_idx);
    const auto idx =
        static_cast<usize>(slot) * static_cast<usize>(cacheline_size_) + static_cast<usize>(offset);
    memory_[idx] = value;
}

auto Cache::mark_dirty(u32 cl_idx) -> void
{
    const auto slot = slot_of(cl_idx);
    dirty_bits_[static_cast<usize>(slot)] = 1u;
}

auto Cache::clear_dirty(u32 cl_idx) -> void
{
    const auto slot = slot_of(cl_idx);
    dirty_bits_[static_cast<usize>(slot)] = 0u;
}

auto Cache::flush_all() -> std::vector<DirtyEntry>
{
    std::vector<DirtyEntry> dirty;
    for (auto slot = 0u; slot < num_slots_; ++slot)
    {
        const auto idx = static_cast<usize>(slot);
        if (valids_[idx] != 0u && dirty_bits_[idx] != 0u)
        {
            const auto cl_idx = tags_[idx] * num_slots_ + slot;
            dirty.push_back({cl_idx, get_cacheline_data(cl_idx)});
            dirty_bits_[idx] = 0u;
        }
    }
    return dirty;
}

auto Cache::fill(u32 cl_idx, std::span<const std::byte> data) -> std::optional<Evicted>
{
    Expects(data.size() == static_cast<usize>(cacheline_size_));

    const auto slot = slot_of(cl_idx);
    const auto tag = tag_of(cl_idx);
    const auto idx = static_cast<usize>(slot);
    const auto base = idx * static_cast<usize>(cacheline_size_);

    std::optional<Evicted> evicted;
    if (valids_[idx] != 0u && dirty_bits_[idx] != 0u)
    {
        const auto old_cl_idx = tags_[idx] * num_slots_ + slot;
        evicted = Evicted{old_cl_idx, get_cacheline_data(old_cl_idx)};
    }

    std::copy(data.begin(), data.end(), memory_.begin() + static_cast<std::ptrdiff_t>(base));
    tags_[idx] = tag;
    valids_[idx] = 1u;
    dirty_bits_[idx] = 0u;

    return evicted;
}

}  // namespace ds_mem
