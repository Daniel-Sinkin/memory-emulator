#include "vm_config.hpp"

#include <stdexcept>

namespace ds_mem
{
namespace
{
auto require(bool cond, const char* message) -> void
{
    if (!cond)
    {
        throw std::invalid_argument(message);
    }
}
}  // namespace

auto validate_config_or_throw(const VMConfig& config) -> void
{
    require(config.word_size > 0u, "word_size must be greater than 0");
    require(config.cacheline_words > 0u, "cacheline_words must be greater than 0");
    require(config.memory_size > 0u, "memory_size must be greater than 0");
    require(config.l1_size > 0u, "l1_size must be greater than 0");
    require(config.l2_size > 0u, "l2_size must be greater than 0");

    const auto cacheline_size = config.cacheline_size();
    require(cacheline_size > 0u, "cacheline_size must be greater than 0");
    require(
        config.memory_size % cacheline_size == 0u, "memory_size must be divisible by cacheline_size"
    );
    require(config.l1_size % cacheline_size == 0u, "l1_size must be divisible by cacheline_size");
    require(config.l2_size % cacheline_size == 0u, "l2_size must be divisible by cacheline_size");
}

}  // namespace ds_mem
