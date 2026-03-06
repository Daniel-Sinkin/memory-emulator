#pragma once

#include "constants.hpp"

namespace ds_mem
{

struct VMConfig
{
    u32 word_size{1u};
    u32 cacheline_words{4u};
    u32 memory_size{1u << 12u};
    u32 l1_size{128u};
    u32 l2_size{1024u};
    u32 l1_latency{1u};
    u32 l2_latency{5u};
    u32 ram_latency{100u};

    [[nodiscard]] auto cacheline_size() const noexcept -> u32
    {
        return word_size * cacheline_words;
    }
};

auto validate_config_or_throw(const VMConfig& config) -> void;

}  // namespace ds_mem
