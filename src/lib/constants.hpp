#pragma once

#include <cstddef>
#include <cstdint>
#ifdef __cpp_lib_stdfloat
#include <stdfloat>
#endif

namespace ds_mem {

using usize = std::size_t;

#ifdef __cpp_lib_stdfloat
using f32 = std::float32_t;
using f64 = std::float64_t;
#else
static_assert(sizeof(float) == 4, "float is not 32-bit");
static_assert(sizeof(double) == 8, "double is not 64-bit");
using f32 = float;
using f64 = double;
#endif

using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

} // namespace ds_mem
