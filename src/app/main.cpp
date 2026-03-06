#include "gui_state.hpp"
#include "gui_windows.hpp"
#include "imgui_theme.hpp"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#if defined(__APPLE__)
#    include <OpenGL/gl.h>
#else
#    include <GL/gl.h>
#endif

namespace
{

auto init_glfw_window() -> GLFWwindow*
{
    if (glfwInit() == GLFW_FALSE)
    {
        return nullptr;
    }

#if defined(__APPLE__)
    const auto* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const auto* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    auto* window = glfwCreateWindow(1600, 1000, "Memory Emulator", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ds_mem::apply_kickstart_night_theme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return window;
}

auto shutdown_gui(GLFWwindow* window) -> void
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

}  // namespace

int main()
{
    auto* window = init_glfw_window();
    if (window == nullptr)
    {
        return 1;
    }

    ds_mem::GuiState state{};
    constexpr auto clear_r = 0.10F;
    constexpr auto clear_g = 0.12F;
    constexpr auto clear_b = 0.14F;
    constexpr auto clear_a = 1.00F;

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
        ds_mem::render_gui_windows(state);

        ImGui::Render();
        auto display_width = 0;
        auto display_height = 0;
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(clear_r, clear_g, clear_b, clear_a);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    shutdown_gui(window);
    return 0;
}
