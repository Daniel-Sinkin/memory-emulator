#pragma once

#include "cpu.hpp"
#include "vm_config.hpp"

#include <cstddef>
#include <vector>

namespace ds_mem
{

class MemoryVM
{
  public:
    explicit MemoryVM(VMConfig config = {});
    ~MemoryVM() = default;
    MemoryVM(const MemoryVM&) = delete;
    MemoryVM& operator=(const MemoryVM&) = delete;
    MemoryVM(MemoryVM&&) = delete;
    MemoryVM& operator=(MemoryVM&&) = delete;

    [[nodiscard]] auto config() const noexcept -> const VMConfig&
    {
        return config_;
    }
    [[nodiscard]] auto read(u32 addr) const -> std::byte;

    template <ByteConvertible T>
    auto write(u32 addr, T value) -> void
    {
        write_byte(addr, static_cast<std::byte>(value));
    }

    auto write4(u32 addr, u32 value) -> void;
    auto print() const -> void;

    auto cpu() noexcept -> CPU&
    {
        return cpu_;
    }
    auto cpu() const noexcept -> const CPU&
    {
        return cpu_;
    }

  private:
    auto write_byte(u32 addr, std::byte value) -> void;

    VMConfig config_;
    std::vector<std::byte> ram_{};
    CPU cpu_;
};

}  // namespace ds_mem
