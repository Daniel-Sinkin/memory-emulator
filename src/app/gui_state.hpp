#pragma once

#include "memory_vm.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ds_mem
{

class GuiState
{
  public:
    GuiState();

    auto reset_demo() -> void;
    auto run_load_sequence() -> void;
    auto run_store_sequence() -> void;
    auto flush() -> void;

    auto reset_asm_machine_state() -> void;
    auto invalidate_assembly_program() -> void;
    auto load_default_assembly_program() -> void;
    auto reset_default_assembly_program() -> void;
    auto format_assembly_program() -> void;
    auto save_assembly_program() -> bool;
    auto reload_assembly_program() -> bool;
    auto step_assembly() -> bool;
    auto run_assembly(usize max_steps = 4096u) -> void;
    auto clear_assembly_log() -> void;

    [[nodiscard]] auto assembly_buffer() -> char*;
    [[nodiscard]] auto assembly_buffer_size() const -> usize;
    [[nodiscard]] auto assembly_active_program_text() const -> const std::string&;
    [[nodiscard]] auto assembly_log() const -> const std::string&;
    [[nodiscard]] auto assembly_error() const -> const std::string&;
    [[nodiscard]] auto assembly_pc() const -> usize;
    [[nodiscard]] auto assembly_program_size() const -> usize;
    [[nodiscard]] auto assembly_current_source_line() const -> std::optional<usize>;
    [[nodiscard]] auto gpr() const -> const std::array<u32, 8>&;
    [[nodiscard]] auto fpr() const -> const std::array<f32, 4>&;

    [[nodiscard]] auto vm() -> MemoryVM&;
    [[nodiscard]] auto vm() const -> const MemoryVM&;

  private:
    auto reload_assembly_program_from_text(std::string_view source) -> bool;
    auto execute_assembly_line(std::string_view line, usize source_line) -> bool;
    auto append_assembly_log(const std::string& line) -> void;
    auto set_assembly_error(usize source_line, const std::string& message) -> void;
    auto seed_initial_memory() -> void;

    VMConfig config_{};
    std::unique_ptr<MemoryVM> vm_{};

    std::array<char, 8192> asm_buffer_{};
    std::array<u32, 8> gpr_{};
    std::array<f32, 4> fpr_{};
    usize asm_pc_{0u};
    std::vector<std::string> asm_program_lines_{};
    std::vector<usize> asm_source_lines_{};
    bool asm_program_loaded_{false};
    std::string asm_active_program_text_{};
    std::string asm_log_{};
    std::string asm_error_{};
};

}  // namespace ds_mem
