#include "experiments.hpp"

namespace ds_mem
{
namespace
{

constexpr auto k_fp32_bytes = static_cast<u32>(sizeof(f32));

[[nodiscard]] auto mat_addr(const GemmLayout& layout, u32 base, u32 row, u32 col) -> u32
{
    const auto idx = row * layout.n + col;
    return base + idx * k_fp32_bytes;
}

}  // namespace

auto default_gemm_layout(u32 n) -> GemmLayout
{
    const auto matrix_size_bytes = n * n * k_fp32_bytes;
    return GemmLayout{
        .n = n,
        .base_a = 0u,
        .base_b = matrix_size_bytes,
        .base_c = 2u * matrix_size_bytes,
    };
}

auto gemm_bytes_required(const GemmLayout& layout) -> u32
{
    const auto matrix_size_bytes = layout.n * layout.n * k_fp32_bytes;
    return layout.base_c + matrix_size_bytes;
}

auto init_matrices(CPU& cpu, const GemmLayout& layout) -> void
{
    for (auto i = 0u; i < layout.n; ++i)
    {
        for (auto j = 0u; j < layout.n; ++j)
        {
            cpu.write_f32(
                mat_addr(layout, layout.base_a, i, j), static_cast<f32>((i + j) % 3u + 1u)
            );
            cpu.write_f32(
                mat_addr(layout, layout.base_b, i, j), static_cast<f32>((i * 2u + j) % 3u + 1u)
            );
            cpu.write_f32(mat_addr(layout, layout.base_c, i, j), 0.0F);
        }
    }
    cpu.flush();
}

auto gemm_ijk(CPU& cpu, const GemmLayout& layout) -> void
{
    for (auto i = 0u; i < layout.n; ++i)
    {
        for (auto j = 0u; j < layout.n; ++j)
        {
            auto sum = cpu.read_f32(mat_addr(layout, layout.base_c, i, j));
            for (auto k = 0u; k < layout.n; ++k)
            {
                const auto a = cpu.read_f32(mat_addr(layout, layout.base_a, i, k));
                const auto b = cpu.read_f32(mat_addr(layout, layout.base_b, k, j));
                sum += a * b;
            }
            cpu.write_f32(mat_addr(layout, layout.base_c, i, j), sum);
        }
    }
}

auto gemm_ikj(CPU& cpu, const GemmLayout& layout) -> void
{
    for (auto i = 0u; i < layout.n; ++i)
    {
        for (auto k = 0u; k < layout.n; ++k)
        {
            const auto a = cpu.read_f32(mat_addr(layout, layout.base_a, i, k));
            for (auto j = 0u; j < layout.n; ++j)
            {
                auto c = cpu.read_f32(mat_addr(layout, layout.base_c, i, j));
                const auto b = cpu.read_f32(mat_addr(layout, layout.base_b, k, j));
                c += a * b;
                cpu.write_f32(mat_addr(layout, layout.base_c, i, j), c);
            }
        }
    }
}

}  // namespace ds_mem
