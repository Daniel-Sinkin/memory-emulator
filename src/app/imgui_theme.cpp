#include "imgui_theme.hpp"

#include "constants.hpp"

#include <array>
#include <filesystem>
#include <imgui.h>
#include <print>

namespace ds_mem
{
namespace
{

constexpr auto rgba_u32(u32 rgba) noexcept -> ImVec4
{
    constexpr auto k_inv = 1.0F / 255.0F;
    const auto r = static_cast<f32>((rgba >> 24u) & 0xFFu) * k_inv;
    const auto g = static_cast<f32>((rgba >> 16u) & 0xFFu) * k_inv;
    const auto b = static_cast<f32>((rgba >> 8u) & 0xFFu) * k_inv;
    const auto a = static_cast<f32>((rgba >> 0u) & 0xFFu) * k_inv;
    return ImVec4{r, g, b, a};
}

auto try_load_krypton_14_font(ImGuiIO& io) -> void
{
    constexpr auto k_font_size_px = 14.0F;
    constexpr auto k_font_file = "MonaspaceKrypton-Regular.otf";

    std::array<std::filesystem::path, 6> candidates = {
#if defined(DS_MEM_PROJECT_ROOT)
        std::filesystem::path{DS_MEM_PROJECT_ROOT} / "fonts" / k_font_file,
#else
        std::filesystem::path{},
#endif
        std::filesystem::path{"fonts"} / k_font_file,
        std::filesystem::path{"./fonts"} / k_font_file,
        std::filesystem::path{"../fonts"} / k_font_file,
        std::filesystem::path{"../../fonts"} / k_font_file,
        std::filesystem::path{"../../../fonts"} / k_font_file,
    };

    for (const auto& path : candidates)
    {
        if (path.empty() || !std::filesystem::exists(path))
        {
            continue;
        }

        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path.string().c_str(), k_font_size_px);
            font != nullptr)
        {
            io.FontDefault = font;
            std::println("[UI] Loaded font '{}' as krypton-14", path.string());
            return;
        }
    }

    std::println(stderr, "[UI] Failed to load krypton-14 font (MonaspaceKrypton-Regular.otf)");
}

}  // namespace

auto apply_kickstart_night_theme() -> void
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    struct ColorAssign
    {
        ImGuiCol slot;
        u32 rgba;
    };

    // Fixed theme: Kickstart Night
    constexpr ColorAssign palette[] = {
        {ImGuiCol_Text, 0xC0CAF5FF},
        {ImGuiCol_TextDisabled, 0x565F89FF},
        {ImGuiCol_WindowBg, 0x1A1B26FF},
        {ImGuiCol_ChildBg, 0x16161EFF},
        {ImGuiCol_PopupBg, 0x101014FF},
        {ImGuiCol_Border, 0x0F0F14FF},
        {ImGuiCol_BorderShadow, 0x00000000},
        {ImGuiCol_FrameBg, 0x24283BFF},
        {ImGuiCol_FrameBgHovered, 0x2E3350FF},
        {ImGuiCol_FrameBgActive, 0x343B5CFF},
        {ImGuiCol_TitleBg, 0x101014FF},
        {ImGuiCol_TitleBgActive, 0x1A1B26FF},
        {ImGuiCol_TitleBgCollapsed, 0x101014FF},
        {ImGuiCol_MenuBarBg, 0x16161EFF},
        {ImGuiCol_Button, 0x2E3350FF},
        {ImGuiCol_ButtonHovered, 0x3B4261FF},
        {ImGuiCol_ButtonActive, 0x24283BFF},
        {ImGuiCol_Header, 0x33467CFF},
        {ImGuiCol_HeaderHovered, 0x3D59A1FF},
        {ImGuiCol_HeaderActive, 0x2A3C6BFF},
        {ImGuiCol_Separator, 0x2A2E45FF},
        {ImGuiCol_SeparatorHovered, 0x3B4261FF},
        {ImGuiCol_SeparatorActive, 0x7AA2F7FF},
        {ImGuiCol_Tab, 0x16161EFF},
        {ImGuiCol_TabHovered, 0x24283BFF},
        {ImGuiCol_TabActive, 0x1F2335FF},
        {ImGuiCol_TabUnfocused, 0x16161EFF},
        {ImGuiCol_TabUnfocusedActive, 0x1F2335FF},
        {ImGuiCol_ScrollbarBg, 0x101014FF},
        {ImGuiCol_ScrollbarGrab, 0x2A2E45FF},
        {ImGuiCol_ScrollbarGrabHovered, 0x3B4261FF},
        {ImGuiCol_ScrollbarGrabActive, 0x7AA2F7FF},
        {ImGuiCol_CheckMark, 0x7AA2F7FF},
        {ImGuiCol_SliderGrab, 0x7AA2F7FF},
        {ImGuiCol_SliderGrabActive, 0x5B82D6FF},
        {ImGuiCol_TableRowBg, 0x16161EFF},
        {ImGuiCol_TableRowBgAlt, 0x1A1B26FF},
    };

    for (const auto& entry : palette)
    {
        colors[entry.slot] = rgba_u32(entry.rgba);
    }

    style.WindowBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FramePadding = ImVec2{6.0F, 4.0F};
    style.ItemSpacing = ImVec2{8.0F, 5.0F};
    style.ItemInnerSpacing = ImVec2{6.0F, 4.0F};
    style.FrameRounding = 4.0F;
    style.TabRounding = 4.0F;
    style.GrabRounding = 4.0F;
    style.ScrollbarRounding = 6.0F;

    auto& io = ImGui::GetIO();
    try_load_krypton_14_font(io);
}

}  // namespace ds_mem
