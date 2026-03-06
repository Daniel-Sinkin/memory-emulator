#include "cache_walkthrough_experiment.hpp"

#include "memory_vm.hpp"

#include <print>

namespace ds_mem
{

auto run_cache_walkthrough_experiment() -> void
{
    std::println("=== Small cache walkthrough (byte-addressed VM) ===");

    const VMConfig config{
        .word_size = 1u,
        .cacheline_words = 4u,
        .memory_size = 64u,
        .l1_size = 16u,
        .l2_size = 32u,
        .l1_latency = 1u,
        .l2_latency = 5u,
        .ram_latency = 100u,
    };

    MemoryVM vm{config};
    auto& cpu = vm.cpu();

    for (auto addr = 0u; addr < 32u; ++addr)
    {
        vm.write(addr, static_cast<std::byte>(addr + 1u));
    }

    std::println("Initial cache state (all invalid slots):");
    cpu.print_detailed();

    std::println();
    std::println("Load sequence: 0, 1, 4, 16, 0");
    (void) cpu.read(0u);
    (void) cpu.read(1u);
    (void) cpu.read(4u);
    (void) cpu.read(16u);
    (void) cpu.read(0u);
    std::println("After loads:");
    cpu.print_detailed();

    std::println();
    std::println("Store sequence: write 0xAA to 0, write 0xBB to 16");
    cpu.write(0u, static_cast<std::byte>(0xAAu));
    cpu.write(16u, static_cast<std::byte>(0xBBu));
    std::println("After stores (expect dirty L1 lines):");
    cpu.print_detailed();

    std::println();
    std::println("Flush L1 -> L2 -> RAM");
    cpu.flush();
    std::println("After flush (dirty bits cleared):");
    cpu.print_detailed();

    std::println();
    cpu.print_stats();
}

}  // namespace ds_mem
