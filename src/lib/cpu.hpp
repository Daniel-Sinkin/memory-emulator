#pragma once

#include "cache.hpp"
#include "constants.hpp"
#include "vm_config.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ds_mem
{

template <typename T>
concept ByteConvertible = requires(T value) {
    { static_cast<std::byte>(value) };
};

class CPU
{
  public:
    CPU(std::vector<std::byte>& ram, VMConfig config);
    ~CPU() = default;
    CPU(const CPU&) = delete;
    CPU& operator=(const CPU&) = delete;
    CPU(CPU&&) = delete;
    CPU& operator=(CPU&&) = delete;

    [[nodiscard]] auto config() const noexcept -> const VMConfig&
    {
        return config_;
    }

    [[nodiscard]] auto read(u32 addr) -> std::byte;
    [[nodiscard]] auto read_u32(u32 addr) -> u32;
    [[nodiscard]] auto read_f32(u32 addr) -> f32;

    template <ByteConvertible T>
    auto write(u32 addr, T value) -> void
    {
        write_byte(addr, static_cast<std::byte>(value));
    }

    template <ByteConvertible T>
    auto write_through(u32 addr, T value) -> void
    {
        write_through_byte(addr, static_cast<std::byte>(value));
    }

    auto write_u32(u32 addr, u32 value) -> void;
    auto write_f32(u32 addr, f32 value) -> void;
    auto write4(u32 addr, u32 value) -> void;
    auto flush() -> void;

    auto print() const -> void;
    auto print_detailed() const -> void;
    auto print_stats() const -> void;

    auto l1() noexcept -> Cache&
    {
        return l1_;
    }
    auto l1() const noexcept -> const Cache&
    {
        return l1_;
    }

    auto l2() noexcept -> Cache&
    {
        return l2_;
    }
    auto l2() const noexcept -> const Cache&
    {
        return l2_;
    }

  private:
    [[nodiscard]] auto fetch_cacheline_from_ram(u32 cl_idx) const -> std::vector<std::byte>;
    auto writeback_to_ram(u32 cl_idx, std::span<const std::byte> data) -> void;
    auto fill_l2(u32 fill_idx, std::span<const std::byte> fill_data) -> void;
    auto fill_l1(u32 fill_idx, std::span<const std::byte> fill_data) -> void;
    auto write_byte(u32 addr, std::byte value) -> void;
    auto write_through_byte(u32 addr, std::byte value) -> void;

    std::vector<std::byte>& ram_;
    VMConfig config_;
    Cache l1_;
    Cache l2_;
};

}  // namespace ds_mem
