// app/main.cpp
#include <array>
#include <cstddef>
#include <format>
#include <gsl/gsl>
#include <optional>
#include <print>

#include "constants.hpp" // IWYU pragma: keep

namespace ds_mem {
constexpr u32 k_word_size = 1;
constexpr u32 k_cacheline_size = 4 * k_word_size;

constexpr u32 k_address_bits = 8u;
constexpr u32 k_memory_size = 1u << k_address_bits;
constexpr u32 k_l1_size = 16u;
constexpr u32 k_l2_size = 64u;

constexpr u32 k_l1_latency = 1;
constexpr u32 k_l2_latency = 5;
constexpr u32 k_ram_latency = 100;

using Cacheline = std::array<std::byte, k_cacheline_size>;

using RAM = std::array<std::byte, k_memory_size>;

template <typename T>
concept ByteConvertible = requires(T v) {
    { static_cast<std::byte>(v) };
};

constexpr auto k_cyan = "\033[38;5;44m";
constexpr auto k_orange = "\033[38;5;208m";
constexpr auto k_green = "\033[38;5;34m";
constexpr auto k_red = "\033[38;5;196m";
constexpr auto k_reset = "\033[0m";

auto print_ram(const RAM &ram, usize blocks_per_row = 4) -> void {
    constexpr auto N = k_memory_size;
    const auto cols = blocks_per_row * k_cacheline_size;
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
            const auto val = std::to_integer<u32>(ram[idx]);
            // Cyan: entire cacheline if any byte is non-zero
            auto has_nonzero = false;
            for (auto i = cl_start; i < cl_start + k_cacheline_size; ++i) {
                if (std::to_integer<u32>(ram[i]) != 0) {
                    has_nonzero = true;
                    break;
                }
            }
            if (has_nonzero) {
                std::print("{}{:02X}{}", k_cyan, val, k_reset);
            } else {
                std::print("{:02X}", val);
            }
        }
        // ASCII column
        std::print("  |");
        for (auto col = 0zu; col < cols; ++col) {
            const auto idx = row * cols + col;
            const auto ch = std::to_integer<unsigned char>(ram[idx]);
            std::print("{}", (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.');
        }
        std::print("|");
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
        u32 hit_count{0};
        u32 miss_count{0};
    };
    // clang-format off
    [[nodiscard]] auto num_slots() const noexcept -> u32 { return k_num_slots_; }

    auto stats()        noexcept -> CacheStats &       { return stats_;  }
    auto stats()  const noexcept -> const CacheStats & { return stats_;  }

    auto memory()       noexcept -> MemoryT &       { return memory_; }
    auto memory() const noexcept -> const MemoryT & { return memory_; }

    auto tags()         noexcept -> MetaT &       { return tags_;   }
    auto tags()   const noexcept -> const MetaT & { return tags_;   }

    auto valids()       noexcept -> MetaT &       { return valids_; }
    auto valids() const noexcept -> const MetaT & { return valids_; }

    auto dirty_bits()       noexcept -> MetaT &       { return dirty_bits_; }
    auto dirty_bits() const noexcept -> const MetaT & { return dirty_bits_; }

    auto hit()  noexcept { ++stats_.hit_count;  }
    auto miss() noexcept { ++stats_.miss_count; }
    // clang-format on
    struct Evicted {
        u32 cl_idx;
        Cacheline data;
    };

    [[nodiscard]] auto lookup(u32 cl_idx, u32 offset) -> std::optional<std::byte> {
        const auto slot = cl_idx % k_num_slots_;
        const auto tag = cl_idx / k_num_slots_;
        if (valids_[slot] && tags_[slot] == tag) {
            ++stats_.hit_count;
            return memory_[slot * k_cacheline_size + offset];
        }
        ++stats_.miss_count;
        return std::nullopt;
    }

    [[nodiscard]] auto get_cacheline_data(u32 cl_idx) const -> Cacheline {
        const auto slot = cl_idx % k_num_slots_;
        Cacheline out{};
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            out[i] = memory_[slot * k_cacheline_size + i];
        }
        return out;
    }

    [[nodiscard]] auto has(u32 cl_idx) const noexcept -> bool {
        const auto slot = cl_idx % k_num_slots_;
        const auto tag = cl_idx / k_num_slots_;
        return valids_[slot] && tags_[slot] == tag;
    }

    // Reconstruct cacheline address from slot index (only valid if slot is valid)
    [[nodiscard]] auto cl_idx_of_slot(u32 slot) const noexcept -> u32 {
        return tags_[slot] * k_num_slots_ + slot;
    }

    auto write_byte(u32 cl_idx, u32 offset, std::byte value) -> void {
        const auto slot = cl_idx % k_num_slots_;
        memory_[slot * k_cacheline_size + offset] = value;
    }

    auto mark_dirty(u32 cl_idx) -> void {
        const auto slot = cl_idx % k_num_slots_;
        dirty_bits_[slot] = 1;
    }

    auto clear_dirty(u32 cl_idx) -> void {
        const auto slot = cl_idx % k_num_slots_;
        dirty_bits_[slot] = 0;
    }

    struct DirtyEntry {
        u32 cl_idx;
        Cacheline data;
    };

    [[nodiscard]] auto flush_all() -> std::vector<DirtyEntry> {
        std::vector<DirtyEntry> dirty;
        for (auto slot = 0u; slot < k_num_slots_; ++slot) {
            if (valids_[slot] && dirty_bits_[slot]) {
                const auto cl_idx = tags_[slot] * k_num_slots_ + slot;
                dirty.push_back({cl_idx, get_cacheline_data(cl_idx)});
                dirty_bits_[slot] = 0;
            }
        }
        return dirty;
    }

    auto fill(u32 cl_idx, const Cacheline &data) -> std::optional<Evicted> {
        const auto slot = cl_idx % k_num_slots_;
        const auto tag = cl_idx / k_num_slots_;

        std::optional<Evicted> evicted;

        // If slot is valid and dirty, capture evicted cacheline for writeback
        if (valids_[slot] && dirty_bits_[slot]) {
            const auto old_cl_idx = tags_[slot] * k_num_slots_ + slot;
            evicted = Evicted{old_cl_idx, get_cacheline_data(old_cl_idx)};
        }

        // Fill the slot
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            memory_[slot * k_cacheline_size + i] = data[i];
        }
        tags_[slot] = tag;
        valids_[slot] = 1;
        dirty_bits_[slot] = 0;

        return evicted;
    }

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

    [[nodiscard]] auto fetch_cacheline_from_ram(u32 cl_idx) const noexcept -> Cacheline {
        Cacheline out{};
        const auto base = cl_idx * k_cacheline_size;
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            out[i] = ram_[base + i];
        }
        return out;
    }

    auto writeback_to_ram(u32 cl_idx, const Cacheline &data) -> void {
        const auto base = cl_idx * k_cacheline_size;
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            ram_[base + i] = data[i];
        }
    }

    auto read(u32 addr) -> std::byte {
        Expects(addr < k_memory_size);
        const auto cl_idx = addr / k_cacheline_size;
        const auto cl_offset = addr % k_cacheline_size;

        if (auto val = l1_.lookup(cl_idx, cl_offset)) {
            return *val;
        }

        if (auto val = l2_.lookup(cl_idx, cl_offset)) {
            fill_l1(cl_idx, l2_.get_cacheline_data(cl_idx));
            return *val;
        }

        auto data = fetch_cacheline_from_ram(cl_idx);
        fill_l2(cl_idx, data);
        fill_l1(cl_idx, data);
        return data[cl_offset];
    }

    auto write(u32 addr, ByteConvertible auto value) -> void {
        Expects(addr < k_memory_size);
        const auto cl_idx = addr / k_cacheline_size;
        const auto cl_offset = addr % k_cacheline_size;

        // Write-allocate: ensure cacheline is in L1 (counts hits/misses)
        if (!l1_.lookup(cl_idx, cl_offset)) {
            if (auto val = l2_.lookup(cl_idx, cl_offset)) {
                fill_l1(cl_idx, l2_.get_cacheline_data(cl_idx));
            } else {
                auto data = fetch_cacheline_from_ram(cl_idx);
                fill_l2(cl_idx, data);
                fill_l1(cl_idx, data);
            }
        }

        // Modify byte in L1 and mark dirty
        l1_.write_byte(cl_idx, cl_offset, static_cast<std::byte>(value));
        l1_.mark_dirty(cl_idx);
    }

    auto write_through(u32 addr, ByteConvertible auto value) -> void {
        write(addr, value);

        const auto cl_idx = addr / k_cacheline_size;

        auto data = l1_.get_cacheline_data(cl_idx);
        l2_.fill(cl_idx, data);
        l2_.mark_dirty(cl_idx);
        writeback_to_ram(cl_idx, data);
        l1_.clear_dirty(cl_idx);
        l2_.clear_dirty(cl_idx);
    }

    auto write4(u32 addr, u32 value) -> void {
        write(addr, value);
        write(addr + 1, value >> 8u);
        write(addr + 2, value >> 16u);
        write(addr + 3, value >> 24u);
    }

    auto flush() -> void {
        // Flush L1 dirty lines into L2, marking them dirty in L2
        for (const auto &[cl_idx, data] : l1_.flush_all()) {
            fill_l2(cl_idx, data);
            l2_.mark_dirty(cl_idx);
        }
        // Flush L2 dirty lines to RAM
        for (const auto &[cl_idx, data] : l2_.flush_all()) {
            writeback_to_ram(cl_idx, data);
        }
    }

    auto print() const -> void {
        std::println("---\nL2:\n---");
        print_l2();
        std::println("---\nL1:\n---");
        print_l1();
    }

    auto print_detailed() const -> void {
        std::println("=== L1 Cache ({} bytes, {} slots) ===", k_l1_size, k_l1_size / k_cacheline_size);
        std::println("{:>4}  {:>1}  {:>1}  {:>3}  {:>6}  {:<{}}  {}",
                     "slot", "V", "D", "tag", "addr", "data", k_cacheline_size * 2, "ascii");
        std::println("{:-<4}  {:-<1}  {:-<1}  {:-<3}  {:-<6}  {:-<{}}  {:-<{}}",
                     "", "", "", "", "", "", k_cacheline_size * 2, "", k_cacheline_size);
        for (auto slot = 0u; slot < k_l1_size / k_cacheline_size; ++slot) {
            print_detail_row(l1_, slot, false);
        }
        std::println();
        std::println("=== L2 Cache ({} bytes, {} slots) ===", k_l2_size, k_l2_size / k_cacheline_size);
        std::println("{:>4}  {:>1}  {:>1}  {:>3}  {:>6}  {:<{}}  {}",
                     "slot", "V", "D", "tag", "addr", "data", k_cacheline_size * 2, "ascii");
        std::println("{:-<4}  {:-<1}  {:-<1}  {:-<3}  {:-<6}  {:-<{}}  {:-<{}}",
                     "", "", "", "", "", "", k_cacheline_size * 2, "", k_cacheline_size);
        for (auto slot = 0u; slot < k_l2_size / k_cacheline_size; ++slot) {
            print_detail_row(l2_, slot, true);
        }
    }

    auto print_stats() const -> void {
        const auto &[l1_hit, l1_miss] = l1_.stats();
        const auto &[l2_hit, l2_miss] = l2_.stats();
        const auto ram_accesses = l2_miss;
        const auto total_cycles = l1_hit * k_l1_latency + l2_hit * k_l2_latency + ram_accesses * k_ram_latency;
        const auto total_accesses = l1_hit + l1_miss;
        std::println("L1  : {:4} hits, {:4} misses", l1_hit, l1_miss);
        std::println("L2  : {:4} hits, {:4} misses", l2_hit, l2_miss);
        std::println("RAM : {:4} accesses", ram_accesses);
        std::println("Cycles: {} ({:.1f} avg per access)",
                     total_cycles,
                     total_accesses > 0 ? static_cast<double>(total_cycles) / static_cast<double>(total_accesses) : 0.0);
    }

    // clang-format off
    auto l1()       noexcept -> Cache<k_l1_size> &       { return l1_; }
    auto l1() const noexcept -> const Cache<k_l1_size> & { return l1_; }

    auto l2()       noexcept -> Cache<k_l2_size> &       { return l2_; }
    auto l2() const noexcept -> const Cache<k_l2_size> & { return l2_; }
    // clang-format on

private:
    template <usize M>
    auto print_detail_row(const Cache<M> &cache, u32 slot, bool check_stale) const -> void {
        const auto &valids = cache.valids();
        const auto &dirty = cache.dirty_bits();
        const auto &tags = cache.tags();
        const auto &mem = cache.memory();
        const auto valid = valids[slot] != 0;
        const auto is_dirty = dirty[slot] != 0;
        const auto tag = tags[slot];

        std::print("{:>4}  {:>1}  {:>1}  ", slot, valid ? 1 : 0, is_dirty ? 1 : 0);

        if (!valid) {
            std::println("{:>3}  {:>6}  {:<{}}  {:<{}}",
                         "-", "-", "-", k_cacheline_size * 2, "-", k_cacheline_size);
            return;
        }

        const auto cl_idx = cache.cl_idx_of_slot(slot);
        const auto ram_addr = cl_idx * k_cacheline_size;

        std::print("{:>3}  0x{:04X}  ", tag, ram_addr);

        // Pick color
        const auto *color = [&]() {
            if (is_dirty)
                return k_orange;
            if (check_stale) {
                if (l1_.has(cl_idx)) {
                    const auto l1_slot = cl_idx % (k_l1_size / k_cacheline_size);
                    if (l1_.dirty_bits()[l1_slot])
                        return k_orange;
                }
            }
            return k_green;
        }();

        // Hex data
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            const auto val = std::to_integer<u32>(mem[slot * k_cacheline_size + i]);
            std::print("{}{:02X}{}", color, val, k_reset);
        }

        // ASCII
        std::print("  ");
        for (auto i = 0zu; i < k_cacheline_size; ++i) {
            const auto ch = std::to_integer<unsigned char>(mem[slot * k_cacheline_size + i]);
            std::print("{}{}{}", color,
                       (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.',
                       k_reset);
        }
        std::println();
    }

    // L1: orange = dirty, green = valid+clean
    auto print_l1(usize blocks_per_row = 4) const -> void {
        const auto cols = k_l1_size < blocks_per_row * k_cacheline_size ? k_l1_size : blocks_per_row * k_cacheline_size;
        const auto rows = k_l1_size / cols;
        const auto &mem = l1_.memory();
        const auto &valids = l1_.valids();
        const auto &dirty = l1_.dirty_bits();
        for (auto row = 0zu; row < rows; ++row) {
            if (row > 0)
                std::println();
            std::print("{:02x}: ", row * cols);
            for (auto col = 0zu; col < cols; ++col) {
                if (col > 0 && col % k_cacheline_size == 0)
                    std::print(" ");
                const auto idx = row * cols + col;
                const auto slot = idx / k_cacheline_size;
                const auto val = std::to_integer<u32>(mem[idx]);
                if (valids[slot]) {
                    const auto *color = dirty[slot] ? k_orange : k_green;
                    std::print("{}{:02X}{}", color, val, k_reset);
                } else {
                    std::print("{:02X}", val);
                }
            }
            std::print("  |");
            for (auto col = 0zu; col < cols; ++col) {
                const auto idx = row * cols + col;
                const auto ch = std::to_integer<unsigned char>(mem[idx]);
                std::print("{}", (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.');
            }
            std::print("|");
        }
        std::println();
    }

    // L2: red = dirty, orange = valid+clean but stale (L1 has dirty copy), green = valid+clean+fresh
    auto print_l2(usize blocks_per_row = 4) const -> void {
        const auto cols = k_l2_size < blocks_per_row * k_cacheline_size ? k_l2_size : blocks_per_row * k_cacheline_size;
        const auto rows = k_l2_size / cols;
        const auto &mem = l2_.memory();
        const auto &valids = l2_.valids();
        const auto &dirty = l2_.dirty_bits();
        for (auto row = 0zu; row < rows; ++row) {
            if (row > 0)
                std::println();
            std::print("{:02x}: ", row * cols);
            for (auto col = 0zu; col < cols; ++col) {
                if (col > 0 && col % k_cacheline_size == 0)
                    std::print(" ");
                const auto idx = row * cols + col;
                const auto slot = idx / k_cacheline_size;
                const auto val = std::to_integer<u32>(mem[idx]);
                if (valids[slot]) {
                    const auto *color = [&]() {
                        if (dirty[slot])
                            return k_red;
                        // Check if L1 has a dirty copy of the same cacheline
                        const auto cl_idx = l2_.cl_idx_of_slot(static_cast<u32>(slot));
                        if (l1_.has(cl_idx)) {
                            const auto l1_slot = cl_idx % (k_l1_size / k_cacheline_size);
                            if (l1_.dirty_bits()[l1_slot])
                                return k_orange;
                        }
                        return k_green;
                    }();
                    std::print("{}{:02X}{}", color, val, k_reset);
                } else {
                    std::print("{:02X}", val);
                }
            }
            std::print("  |");
            for (auto col = 0zu; col < cols; ++col) {
                const auto idx = row * cols + col;
                const auto ch = std::to_integer<unsigned char>(mem[idx]);
                std::print("{}", (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.');
            }
            std::print("|");
        }
        std::println();
    }

    auto fill_l2(u32 fill_idx, const Cacheline &fill_data) -> void {
        if (auto evicted = l2_.fill(fill_idx, fill_data)) {
            writeback_to_ram(evicted->cl_idx, evicted->data);
        }
    }
    auto fill_l1(u32 fill_idx, const Cacheline &fill_data) -> void {
        if (auto evicted = l1_.fill(fill_idx, fill_data)) {
            fill_l2(evicted->cl_idx, evicted->data);
            l2_.mark_dirty(evicted->cl_idx);
        }
    }

    RAM &ram_;
    Cache<k_l1_size> l1_{};
    Cache<k_l2_size> l2_{};
};

class MemoryVM {
public:
    MemoryVM() : cpu_{ram_} {}
    ~MemoryVM() = default;
    MemoryVM(const MemoryVM &) = delete;
    MemoryVM &operator=(const MemoryVM &) = delete;
    MemoryVM(MemoryVM &&) = delete;
    MemoryVM &operator=(MemoryVM &&) = delete;

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
        print_ram(ram_);
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
    auto &cpu = vm.cpu();
    cpu.write(0x00, 'A');
    cpu.write(0x04, 'B');
    cpu.write(0x08, 'C');
    cpu.write(0x0C, 'D');
    cpu.flush();
    cpu.write(0x10, 'E');

    cpu.print_detailed();
}
