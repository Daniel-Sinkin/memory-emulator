#pragma once

#include "constants.hpp"
#include "cpu.hpp"

namespace ds_mem
{

struct GemmLayout
{
    u32 n{32u};
    u32 base_a{0u};  // Byte address
    u32 base_b{0u};  // Byte address
    u32 base_c{0u};  // Byte address
};

[[nodiscard]] auto default_gemm_layout(u32 n = 32u) -> GemmLayout;
[[nodiscard]] auto gemm_bytes_required(const GemmLayout& layout) -> u32;
auto init_matrices(CPU& cpu, const GemmLayout& layout) -> void;
auto gemm_ijk(CPU& cpu, const GemmLayout& layout) -> void;
auto gemm_ikj(CPU& cpu, const GemmLayout& layout) -> void;

}  // namespace ds_mem
