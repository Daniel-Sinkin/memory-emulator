// app/main.cpp
#include <array>
#include <cstddef>
#include <format>
#include <print>

#include "constants.hpp" // IWYU pragma: keep

namespace ds_mem {
constexpr u32 k_word_size = 1;
constexpr u32 k_cacheline_size = 4 * k_word_size;

constexpr u32 k_address_bits = 8u;
constexpr u32 k_memory_size = 1u << k_address_bits;
constexpr u32 k_l1_size = 16u;
constexpr u32 k_l2_size = 64u;
constexpr u32 k_l3_size = 128u;

using Cacheline = std::array<std::byte, k_cacheline_size>;

using RAM = std::array<std::byte, k_memory_size>;

template <typename T>
concept Printable = std::formattable<T, char> || std::is_same_v<T, std::byte>;

template <typename T>
concept ByteConvertible = requires(T v) {
    { static_cast<std::byte>(v) };
};

constexpr auto k_orange = "\033[38;5;208m";
constexpr auto k_reset = "\033[0m";

auto cacheline_has_nonzero(const auto &xs, usize cl_start, usize cl_size) -> bool {
    for (auto i = cl_start; i < cl_start + cl_size && i < xs.size(); ++i) {
        if (std::to_integer<u32>(xs[i]) != 0) {
            return true;
        }
    }
    return false;
}

template <Printable T, usize N>
auto print_memory(std::array<T, N> xs, usize blocks_per_row = 4, bool pprint = false) -> void {
    const auto k_target_cols = blocks_per_row * k_cacheline_size;
    const auto cols = N < k_target_cols ? N : k_target_cols;
    const auto rows = N / cols;
    for (auto row = 0zu; row < rows; ++row) {
        if (row > 0) {
            std::println();
        }
        std::print("{:02x}: ", row * cols);
        for (auto col = 0zu; col < cols; ++col) {
            if (col > 0 && col % k_cacheline_size == 0) {
                std::print(" ");
            }
            const auto idx = row * cols + col;
            const auto cl_start = (idx / k_cacheline_size) * k_cacheline_size;
            if constexpr (std::is_same_v<T, std::byte>) {
                const auto val = std::to_integer<u32>(xs[idx]);
                if (pprint && cacheline_has_nonzero(xs, cl_start, k_cacheline_size)) {
                    std::print("{}{:02X}{}", k_orange, val, k_reset);
                } else {
                    std::print("{:02X}", val);
                }
            } else {
                std::print("{}", xs[idx]);
            }
        }
        // ASCII column
        if constexpr (std::is_same_v<T, std::byte>) {
            std::print("  |");
            for (auto col = 0zu; col < cols; ++col) {
                const auto idx = row * cols + col;
                const auto ch = std::to_integer<unsigned char>(xs[idx]);
                std::print("{}", (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.');
            }
            std::print("|");
        }
    }
    std::println();
}

template <usize N>
    requires(N % k_cacheline_size == 0)
class Cache {
private:
    static constexpr u32 k_num_slots_{N / k_cacheline_size};

public:
    using MemoryT = std::array<std::byte, N>;
    using MetaT = std::array<u32, k_num_slots_>;
    struct CacheStats {
        int hit_count{0};
        int miss_count{0};
    };

    auto print() const -> void { print_memory(memory_, 4, true); }

    // clang-format off
    [[nodiscard]] auto num_slots() const noexcept -> u32 { return k_num_slots_; }

    auto stats()        noexcept -> CacheStats &       { return stats_;  }
    auto stats()  const noexcept -> const CacheStats & { return stats_;  }

    auto memory()       noexcept -> MetaT &       { return memory_; }
    auto memory() const noexcept -> const MetaT & { return memory_; }

    auto tags()         noexcept -> MetaT &       { return tags_;   }
    auto tags()   const noexcept -> const MetaT & { return tags_;   }

    auto valids()       noexcept -> MetaT &       { return valids_; }
    auto valids() const noexcept -> const MetaT & { return valids_; }
    // clang-format on

private:
    MemoryT memory_{};
    MetaT tags_{};
    MetaT valids_{};
    MetaT dirty_bits_{};
    CacheStats stats_{};
};

class CPU {
public:
    explicit CPU(RAM &ram) : ram_{ram} {}
    ~CPU() = default;
    CPU(const CPU &) = delete;
    CPU &operator=(const CPU &) = delete;
    CPU(CPU &&) = delete;
    CPU &operator=(CPU &&) = delete;

    auto get_cacheline_addr(u32 addr) const noexcept -> u32 {
        static_assert(k_cacheline_size > 0);
        auto cl_size = static_cast<u32>(k_cacheline_size);
        return (addr / cl_size) * cl_size;
    }

    [[nodiscard]] auto get_cacheline(u32 addr) const noexcept -> Cacheline {
        Cacheline out;
        const auto cl_addr = get_cacheline_addr(addr);
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            out[i] = ram_[cl_addr + i];
        }
        return out;
    }

    [[nodiscard]] auto read(u32 addr) const -> std::byte {
        return ram_[addr];
    }

    auto write(u32 addr, ByteConvertible auto value) -> void {
        ram_[addr] = static_cast<std::byte>(value);
    }

    auto write4(u32 addr, u32 value) -> void {
        write(addr, value);
        write(addr + 1, value >> 8u);
        write(addr + 2, value >> 16u);
        write(addr + 3, value >> 24u);
    }

    auto print() const -> void {
        std::println("---\nL3:\n---");
        l3_.print();
        std::println("---\nL2:\n---");
        l2_.print();
        std::println("---\nL1:\n---");
        l1_.print();
    }

    // clang-format off
    auto l1()       noexcept -> Cache<k_l1_size> &       { return l1_; }
    auto l1() const noexcept -> const Cache<k_l1_size> & { return l1_; }

    auto l2()       noexcept -> Cache<k_l2_size> &       { return l2_; }
    auto l2() const noexcept -> const Cache<k_l2_size> & { return l2_; }

    auto l3()       noexcept -> Cache<k_l3_size> &       { return l3_; }
    auto l3() const noexcept -> const Cache<k_l3_size> & { return l3_; }
    // clang-format on

private:
    RAM &ram_;
    Cache<k_l1_size> l1_{};
    Cache<k_l2_size> l2_{};
    Cache<k_l3_size> l3_{};
};

class MemoryVM {
public:
    MemoryVM() : cpu_{ram_} {}
    ~MemoryVM() = default;
    MemoryVM(const MemoryVM &) = delete;
    MemoryVM &operator=(const MemoryVM &) = delete;
    MemoryVM(MemoryVM &&) = delete;
    MemoryVM &operator=(MemoryVM &&) = delete;

    // Direct RAM access (bypasses caches — DMA, other bus masters, initial setup)
    [[nodiscard]] auto read(u32 addr) const -> std::byte {
        return ram_[addr];
    }

    auto write(u32 addr, ByteConvertible auto value) -> void {
        ram_[addr] = static_cast<std::byte>(value);
    }

    auto write4(u32 addr, u32 value) -> void {
        write(addr, value);
        write(addr + 1, value >> 8u);
        write(addr + 2, value >> 16u);
        write(addr + 3, value >> 24u);
    }

    auto print() const -> void {
        std::println("----\nRAM:\n----");
        print_memory(ram_, 4, true);
        cpu_.print();
    }

    // clang-format off
    auto cpu()       noexcept -> CPU &       { return cpu_; }
    auto cpu() const noexcept -> const CPU & { return cpu_; }
    // clang-format on

private:
    RAM ram_{};
    CPU cpu_;
};

} // namespace ds_mem

int main() {
    using namespace ds_mem;

    MemoryVM vm{};

    vm.write(110, 137);
    vm.write4(56, 0x21796548u); // "Hey!" in little-endian

    auto &cpu = vm.cpu();
    auto cl_addr = cpu.get_cacheline_addr(108);
    std::println("cl_addr = {}", cl_addr);

    vm.print();
}
