cmake_minimum_required(VERSION 3.28)
project(memory_emu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(COMMON_WARNINGS
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -Wshadow
  -Wconversion
  -Wfloat-conversion
  -Wsign-conversion
  -Wcast-qual
  -Wcast-align
  -Wformat=2
  -Wundef
  -Wnull-dereference
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wold-style-cast
  -Wzero-as-null-pointer-constant
  -Wdouble-promotion
  -Wimplicit-fallthrough
  -Wreturn-type
)

include(FetchContent)

FetchContent_Declare(
  GSL
  GIT_REPOSITORY https://github.com/microsoft/GSL.git
  GIT_TAG        v4.0.0
)
FetchContent_MakeAvailable(GSL)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
  "src/lib/*.cpp"
)

if(LIB_SOURCES)
  add_library(memory_emu_lib STATIC ${LIB_SOURCES})
  target_include_directories(memory_emu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
  target_link_libraries(memory_emu_lib PUBLIC Microsoft.GSL::GSL)
  target_compile_options(memory_emu_lib PRIVATE ${COMMON_WARNINGS})
  target_precompile_headers(memory_emu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/pch.hpp)
else()
  add_library(memory_emu_lib INTERFACE)
  target_include_directories(memory_emu_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
  target_link_libraries(memory_emu_lib INTERFACE Microsoft.GSL::GSL)
endif()

add_executable(memory_emu src/app/main.cpp)

target_link_libraries(memory_emu PRIVATE memory_emu_lib)
target_precompile_headers(memory_emu REUSE_FROM memory_emu_lib)
target_compile_options(memory_emu PRIVATE ${COMMON_WARNINGS})
