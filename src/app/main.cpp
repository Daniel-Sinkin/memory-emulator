#include "experiments.hpp"
#include "memory_vm.hpp"

#include <print>

int main()
{
    using namespace ds_mem;

    const auto layout = default_gemm_layout(32u);

    const VMConfig config{
        .word_size = static_cast<u32>(sizeof(f32)),
        .cacheline_words = 4u,
        .memory_size = gemm_bytes_required(layout),
        .l1_size = 128u,
        .l2_size = 1024u,
        .l1_latency = 1u,
        .l2_latency = 5u,
        .ram_latency = 100u,
    };

    std::println("row stride A, column stride B");
    {
        MemoryVM vm{config};
        auto& cpu = vm.cpu();
        init_matrices(cpu, layout);
        cpu.flush();
        gemm_ijk(cpu, layout);
        cpu.flush();
        cpu.print_stats();
    }

    std::println();
    std::println("row stride A and B");
    {
        MemoryVM vm{config};
        auto& cpu = vm.cpu();
        init_matrices(cpu, layout);
        cpu.flush();
        gemm_ikj(cpu, layout);
        cpu.flush();
        cpu.print_stats();
    }
}
