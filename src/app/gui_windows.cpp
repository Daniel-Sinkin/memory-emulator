#include "gui_windows.hpp"

#include "cache.hpp"
#include "constants.hpp"
#include "cpu.hpp"
#include "gui_state.hpp"
#include "memory_vm.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <imgui.h>
#include <sstream>
#include <string>
#include <utility>

namespace ds_mem
{
namespace
{

[[nodiscard]] auto printable_ascii(std::byte value) -> char
{
    const auto ch = std::to_integer<unsigned char>(value);
    return (ch >= 0x20U && ch <= 0x7EU) ? static_cast<char>(ch) : '.';
}

[[nodiscard]] auto trim_right_copy(std::string value) -> std::string
{
    while (!value.empty())
    {
        const auto ch = static_cast<unsigned char>(value.back());
        if (!std::isspace(ch))
        {
            break;
        }
        value.pop_back();
    }
    return value;
}

[[nodiscard]] auto trim_left_copy(std::string value) -> std::string
{
    while (!value.empty())
    {
        const auto ch = static_cast<unsigned char>(value.front());
        if (!std::isspace(ch))
        {
            break;
        }
        value.erase(value.begin());
    }
    return value;
}

[[nodiscard]] auto split_code_comment(const std::string& line)
    -> std::pair<std::string, std::string>
{
    const auto semicolon_pos = line.find(';');
    const auto hash_pos = line.find('#');
    const auto comment_pos = std::min(semicolon_pos, hash_pos);
    if (comment_pos == std::string::npos)
    {
        return {trim_right_copy(line), {}};
    }
    auto code = trim_right_copy(line.substr(0u, comment_pos));
    auto comment = trim_left_copy(line.substr(comment_pos));
    return {std::move(code), std::move(comment)};
}

auto render_cache_state_words(bool valid, bool dirty, bool stale) -> void
{
    constexpr auto k_invalid = ImVec4{0.40F, 0.43F, 0.55F, 1.00F};
    constexpr auto k_dirty = ImVec4{0.96F, 0.62F, 0.23F, 1.00F};
    constexpr auto k_stale = ImVec4{0.90F, 0.75F, 0.35F, 1.00F};
    constexpr auto k_clean = ImVec4{0.48F, 0.86F, 0.55F, 1.00F};

    if (!valid)
    {
        ImGui::TextColored(k_invalid, "INVALID");
        return;
    }

    if (dirty)
    {
        ImGui::TextColored(k_dirty, "DIRTY");
        return;
    }
    if (stale)
    {
        ImGui::TextColored(k_stale, "STALE");
        return;
    }
    ImGui::TextColored(k_clean, "CLEAN");
}

[[nodiscard]] auto cache_line_is_stale(const Cache& cache, const Cache& l1, u32 slot) -> bool
{
    const auto slot_idx = static_cast<usize>(slot);
    if (cache.valids()[slot_idx] == 0u || cache.dirty_bits()[slot_idx] != 0u)
    {
        return false;
    }

    const auto cl_idx = cache.cl_idx_of_slot(slot);
    if (!l1.has(cl_idx))
    {
        return false;
    }

    const auto l1_slot = cl_idx % l1.num_slots();
    return l1.dirty_bits()[static_cast<usize>(l1_slot)] != 0u;
}

[[nodiscard]] auto
cacheline_hex(const std::vector<std::byte>& memory, usize base, u32 cacheline_size) -> std::string
{
    std::string out;
    for (auto i = 0u; i < cacheline_size; ++i)
    {
        if (i > 0u)
        {
            out += ' ';
        }
        out += std::format("{:02X}", std::to_integer<u32>(memory[base + static_cast<usize>(i)]));
    }
    return out;
}

[[nodiscard]] auto
cacheline_ascii(const std::vector<std::byte>& memory, usize base, u32 cacheline_size) -> std::string
{
    std::string out;
    out.reserve(static_cast<usize>(cacheline_size));
    for (auto i = 0u; i < cacheline_size; ++i)
    {
        out.push_back(printable_ascii(memory[base + static_cast<usize>(i)]));
    }
    return out;
}

auto render_controls_window(GuiState& state) -> void
{
    if (!ImGui::Begin("Controls"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Reset Demo"))
    {
        state.reset_demo();
    }

    if (ImGui::Button("Run Load Sequence"))
    {
        state.run_load_sequence();
    }

    if (ImGui::Button("Run Store Sequence"))
    {
        state.run_store_sequence();
    }

    if (ImGui::Button("Flush"))
    {
        state.flush();
    }

    const auto& config = state.vm().config();
    ImGui::Separator();
    ImGui::Text("Word size: %u byte(s)", config.word_size);
    ImGui::Text("Cacheline size: %u byte(s)", config.cacheline_size());
    ImGui::Text("Memory: %u bytes", config.memory_size);
    ImGui::Text("L1: %u bytes", config.l1_size);
    ImGui::Text("L2: %u bytes", config.l2_size);
    ImGui::Separator();
    ImGui::TextUnformatted("Sequence:");
    ImGui::TextUnformatted("Loads : 0, 1, 4, 16, 0");
    ImGui::TextUnformatted("Stores: [0]=0xAA, [16]=0xBB");

    ImGui::End();
}

auto render_assembly_window(GuiState& state) -> void
{
    if (!ImGui::Begin("Assembly Program"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Step"))
    {
        (void) state.step_assembly();
    }
    ImGui::SameLine();
    if (ImGui::Button("Run"))
    {
        state.run_assembly();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset ASM"))
    {
        state.reset_asm_machine_state();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log"))
    {
        state.clear_assembly_log();
    }

    ImGui::Separator();
    ImGui::Text(
        "PC: %u / %u",
        static_cast<u32>(state.assembly_pc()),
        static_cast<u32>(state.assembly_program_size())
    );
    if (!state.assembly_error().empty())
    {
        ImGui::TextColored(
            ImVec4(1.00F, 0.40F, 0.40F, 1.00F), "Error: %s", state.assembly_error().c_str()
        );
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Program View (active, current PC line highlighted)");
    if (ImGui::BeginChild(
            "asm_program_view", ImVec2(0.0F, 320.0F), true, ImGuiWindowFlags_HorizontalScrollbar
        ))
    {
        const auto active_source_line = state.assembly_current_source_line();
        std::istringstream lines{state.assembly_active_program_text()};
        std::string source_line;
        auto line_number = 1u;

        while (std::getline(lines, source_line))
        {
            const auto is_pc_line =
                active_source_line && *active_source_line == static_cast<usize>(line_number);

            if (is_pc_line)
            {
                const auto min = ImGui::GetCursorScreenPos();
                const auto max = ImVec2(
                    min.x + ImGui::GetContentRegionAvail().x,
                    min.y + ImGui::GetTextLineHeightWithSpacing()
                );
                ImGui::GetWindowDrawList()->AddRectFilled(
                    min, max, ImGui::GetColorU32(ImVec4(0.20F, 0.29F, 0.49F, 0.35F)), 2.0F
                );
            }

            const auto [code_part, comment_part] = split_code_comment(source_line);
            if (is_pc_line)
            {
                ImGui::TextColored(ImVec4(0.48F, 0.64F, 0.97F, 1.00F), ">> %4u |", line_number);
            }
            else
            {
                ImGui::Text("   %4u |", line_number);
            }

            if (!code_part.empty())
            {
                ImGui::SameLine();
                ImGui::TextUnformatted(code_part.c_str());
            }
            if (!comment_part.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.57F, 0.73F, 0.58F, 1.00F), "%s", comment_part.c_str());
            }

            ++line_number;
        }
    }
    ImGui::EndChild();

    ImGui::TextUnformatted("General Registers (R0..R7, u32)");
    if (ImGui::BeginTable("asm_regs_u32", 8, ImGuiTableFlags_Borders))
    {
        for (auto i = 0u; i < 8u; ++i)
        {
            ImGui::TableSetupColumn(std::format("R{}", i).c_str());
        }
        ImGui::TableNextRow();
        const auto& gpr = state.gpr();
        for (auto i = 0u; i < 8u; ++i)
        {
            ImGui::TableSetColumnIndex(static_cast<int>(i));
            ImGui::Text("%u", gpr[static_cast<usize>(i)]);
        }
        ImGui::EndTable();
    }

    ImGui::TextUnformatted("Float Registers (F0..F3, f32)");
    if (ImGui::BeginTable("asm_regs_f32", 4, ImGuiTableFlags_Borders))
    {
        for (auto i = 0u; i < 4u; ++i)
        {
            ImGui::TableSetupColumn(std::format("F{}", i).c_str());
        }
        ImGui::TableNextRow();
        const auto& fpr = state.fpr();
        for (auto i = 0u; i < 4u; ++i)
        {
            ImGui::TableSetColumnIndex(static_cast<int>(i));
            ImGui::Text("%.6g", static_cast<double>(fpr[static_cast<usize>(i)]));
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Log");
    ImGui::BeginChild(
        "asm_log_scroll", ImVec2(0.0F, 0.0F), true, ImGuiWindowFlags_HorizontalScrollbar
    );
    ImGui::TextUnformatted(state.assembly_log().c_str());
    ImGui::EndChild();

    ImGui::End();
}

auto render_assembly_editor_window(GuiState& state) -> void
{
    if (!ImGui::Begin("Assembly Editor"))
    {
        ImGui::End();
        return;
    }

    auto* buffer = state.assembly_buffer();
    const auto buffer_size = state.assembly_buffer_size();
    (void) ImGui::InputTextMultiline(
        "Draft Program",
        buffer,
        buffer_size,
        ImVec2(-1.0F, 360.0F),
        ImGuiInputTextFlags_AllowTabInput
    );

    if (ImGui::Button("Save"))
    {
        (void) state.save_assembly_program();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Default"))
    {
        state.reset_default_assembly_program();
    }
    ImGui::SameLine();
    if (ImGui::Button("Format Draft"))
    {
        state.format_assembly_program();
    }

    ImGui::TextUnformatted("Save makes this draft active in Program View.");
    ImGui::End();
}

auto render_stats_window(const CPU& cpu) -> void
{
    if (!ImGui::Begin("Stats"))
    {
        ImGui::End();
        return;
    }

    const auto& config = cpu.config();
    const auto& l1_stats = cpu.l1().stats();
    const auto& l2_stats = cpu.l2().stats();

    const auto l1_hit = static_cast<u64>(l1_stats.hit_count);
    const auto l1_miss = static_cast<u64>(l1_stats.miss_count);
    const auto l2_hit = static_cast<u64>(l2_stats.hit_count);
    const auto l2_miss = static_cast<u64>(l2_stats.miss_count);
    const auto ram_accesses = l2_miss;
    const auto total_cycles = l1_hit * static_cast<u64>(config.l1_latency)
                              + l2_hit * static_cast<u64>(config.l2_latency)
                              + ram_accesses * static_cast<u64>(config.ram_latency);
    const auto total_accesses = l1_hit + l1_miss;
    const auto avg_cycles = total_accesses > 0u ? static_cast<double>(total_cycles)
                                                      / static_cast<double>(total_accesses)
                                                : 0.0;

    ImGui::Text(
        "L1  : %llu hits, %llu misses",
        static_cast<unsigned long long>(l1_hit),
        static_cast<unsigned long long>(l1_miss)
    );
    ImGui::Text(
        "L2  : %llu hits, %llu misses",
        static_cast<unsigned long long>(l2_hit),
        static_cast<unsigned long long>(l2_miss)
    );
    ImGui::Text("RAM : %llu accesses", static_cast<unsigned long long>(ram_accesses));
    ImGui::Text(
        "Cycles: %llu (%.2f avg/access)", static_cast<unsigned long long>(total_cycles), avg_cycles
    );

    ImGui::End();
}

auto render_ram_window(const MemoryVM& vm) -> void
{
    if (!ImGui::Begin("RAM"))
    {
        ImGui::End();
        return;
    }

    const auto& config = vm.config();
    const auto bytes_per_row = 16u;

    ImGui::Text("Bytes: %u", config.memory_size);
    ImGui::Text("Rows : %u", (config.memory_size + bytes_per_row - 1u) / bytes_per_row);
    ImGui::Separator();

    ImGui::BeginChild(
        "ram_scroll", ImVec2(0.0F, 0.0F), false, ImGuiWindowFlags_HorizontalScrollbar
    );
    for (auto row = 0u; row < config.memory_size; row += bytes_per_row)
    {
        std::string line = std::format("{:04X}: ", row);
        std::string ascii;
        const auto row_end = std::min(row + bytes_per_row, config.memory_size);
        for (auto addr = row; addr < row_end; ++addr)
        {
            const auto value = vm.read(addr);
            if (addr > row && ((addr - row) % config.cacheline_size()) == 0u)
            {
                line += " ";
            }
            line += std::format("{:02X}", std::to_integer<u32>(value));
            ascii.push_back(printable_ascii(value));
        }
        line += std::format("  |{}|", ascii);
        ImGui::TextUnformatted(line.c_str());
    }
    ImGui::EndChild();

    ImGui::End();
}

auto render_cache_window(
    const char* title, const Cache& cache, u32 cacheline_size, const Cache* l1_for_stale_check
) -> void
{
    if (!ImGui::Begin(title))
    {
        ImGui::End();
        return;
    }

    ImGui::Text(
        "Size: %u bytes | Slots: %u | Cacheline: %u bytes",
        cache.total_size(),
        cache.num_slots(),
        cacheline_size
    );
    ImGui::TextUnformatted("State legend:");
    render_cache_state_words(true, true, false);
    ImGui::SameLine(0.0F, 12.0F);
    render_cache_state_words(true, false, true);
    ImGui::SameLine(0.0F, 12.0F);
    render_cache_state_words(true, false, false);
    ImGui::SameLine(0.0F, 12.0F);
    render_cache_state_words(false, false, false);
    ImGui::Separator();

    constexpr auto k_table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                   | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("cache_table", 8, k_table_flags, ImVec2(0.0F, 0.0F)))
    {
        ImGui::TableSetupColumn("Slot");
        ImGui::TableSetupColumn("V");
        ImGui::TableSetupColumn("D");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Tag");
        ImGui::TableSetupColumn("Addr");
        ImGui::TableSetupColumn("Data");
        ImGui::TableSetupColumn("ASCII");
        ImGui::TableHeadersRow();

        const auto& valids = cache.valids();
        const auto& dirty_bits = cache.dirty_bits();
        const auto& tags = cache.tags();
        const auto& memory = cache.memory();

        for (auto slot = 0u; slot < cache.num_slots(); ++slot)
        {
            const auto slot_idx = static_cast<usize>(slot);
            const auto valid = valids[slot_idx] != 0u;
            const auto dirty = dirty_bits[slot_idx] != 0u;
            const auto stale = l1_for_stale_check != nullptr
                               && cache_line_is_stale(cache, *l1_for_stale_check, slot);

            ImGui::TableNextRow();
            if (valid && dirty)
            {
                const auto row_color = ImGui::GetColorU32(ImVec4(0.47F, 0.31F, 0.04F, 0.31F));
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_color);
            }
            else if (valid && stale)
            {
                const auto row_color = ImGui::GetColorU32(ImVec4(0.43F, 0.35F, 0.12F, 0.20F));
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_color);
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", slot);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", valid ? 1u : 0u);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", dirty ? 1u : 0u);
            ImGui::TableSetColumnIndex(3);
            render_cache_state_words(valid, dirty, stale);

            if (!valid)
            {
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted("-");
                ImGui::TableSetColumnIndex(5);
                ImGui::TextUnformatted("-");
                ImGui::TableSetColumnIndex(6);
                ImGui::TextUnformatted("-");
                ImGui::TableSetColumnIndex(7);
                ImGui::TextUnformatted("-");
                continue;
            }

            const auto tag = tags[slot_idx];
            const auto cl_idx = cache.cl_idx_of_slot(slot);
            const auto ram_addr = cl_idx * cacheline_size;
            const auto base = slot_idx * static_cast<usize>(cacheline_size);
            const auto data_hex = cacheline_hex(memory, base, cacheline_size);
            const auto data_ascii = cacheline_ascii(memory, base, cacheline_size);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", tag);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("0x%04X", ram_addr);
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(data_hex.c_str());
            ImGui::TableSetColumnIndex(7);
            ImGui::TextUnformatted(data_ascii.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

}  // namespace

auto render_gui_windows(GuiState& state) -> void
{
    render_controls_window(state);
    render_assembly_editor_window(state);
    render_assembly_window(state);

    auto& vm = state.vm();
    auto& cpu = vm.cpu();

    render_stats_window(cpu);
    render_ram_window(vm);
    render_cache_window("L1 Cache", cpu.l1(), vm.config().cacheline_size(), nullptr);
    render_cache_window("L2 Cache", cpu.l2(), vm.config().cacheline_size(), &cpu.l1());
}

}  // namespace ds_mem
