cmake_minimum_required(VERSION 3.28)
project(memory_emu LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Disable online FetchContent updates" FORCE)

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

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

if(APPLE)
  target_link_libraries(imgui_lib PUBLIC "-framework OpenGL")
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
endif()

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
  "src/lib/*.cpp"
)

add_library(memory_emu_lib STATIC ${LIB_SOURCES})
target_include_directories(memory_emu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
target_link_libraries(memory_emu_lib PUBLIC Microsoft.GSL::GSL)
target_compile_options(memory_emu_lib PRIVATE ${COMMON_WARNINGS})
target_precompile_headers(memory_emu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/pch.hpp)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
  "src/app/*.cpp"
)

add_executable(memory_emu ${APP_SOURCES})

target_link_libraries(memory_emu PRIVATE
  memory_emu_lib
  imgui_lib
  glfw
  glm::glm
)
target_compile_options(memory_emu PRIVATE ${COMMON_WARNINGS})
target_precompile_headers(memory_emu REUSE_FROM memory_emu_lib)
target_compile_definitions(memory_emu PRIVATE
  DS_MEM_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(APPLE)
  target_compile_definitions(imgui_lib PRIVATE GL_SILENCE_DEPRECATION)
  target_compile_definitions(memory_emu PRIVATE GL_SILENCE_DEPRECATION)
endif()
