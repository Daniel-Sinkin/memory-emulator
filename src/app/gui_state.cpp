#include "gui_state.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <limits>
#include <sstream>
#include <string_view>

namespace ds_mem
{
namespace
{

[[nodiscard]] auto default_gui_config() -> VMConfig
{
    return VMConfig{
        .word_size = 1u,
        .cacheline_words = 4u,
        .memory_size = 64u,
        .l1_size = 16u,
        .l2_size = 32u,
        .l1_latency = 1u,
        .l2_latency = 5u,
        .ram_latency = 100u,
    };
}

[[nodiscard]] auto trim_left(std::string_view value) -> std::string_view
{
    while (!value.empty())
    {
        const auto ch = static_cast<unsigned char>(value.front());
        if (!std::isspace(ch))
        {
            break;
        }
        value.remove_prefix(1u);
    }
    return value;
}

[[nodiscard]] auto trim_right(std::string_view value) -> std::string_view
{
    while (!value.empty())
    {
        const auto ch = static_cast<unsigned char>(value.back());
        if (!std::isspace(ch))
        {
            break;
        }
        value.remove_suffix(1u);
    }
    return value;
}

[[nodiscard]] auto trim(std::string_view value) -> std::string_view
{
    return trim_right(trim_left(value));
}

[[nodiscard]] auto strip_comment(std::string_view line) -> std::string_view
{
    const auto hash_pos = line.find('#');
    const auto semicolon_pos = line.find(';');
    const auto first_comment_pos = std::min(hash_pos, semicolon_pos);
    if (first_comment_pos == std::string_view::npos)
    {
        return line;
    }
    return line.substr(0u, first_comment_pos);
}

[[nodiscard]] auto find_comment_pos(std::string_view line) -> usize
{
    const auto hash_pos = line.find('#');
    const auto semicolon_pos = line.find(';');
    const auto first_comment_pos = std::min(hash_pos, semicolon_pos);
    if (first_comment_pos == std::string_view::npos)
    {
        return line.size();
    }
    return first_comment_pos;
}

[[nodiscard]] auto to_upper(std::string value) -> std::string
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); }
    );
    return value;
}

[[nodiscard]] auto tokenize(std::string_view line) -> std::vector<std::string>
{
    std::string normalized{line};
    std::replace(normalized.begin(), normalized.end(), ',', ' ');

    std::vector<std::string> out;
    std::istringstream stream{normalized};
    std::string token;
    while (stream >> token)
    {
        out.push_back(token);
    }
    return out;
}

[[nodiscard]] auto parse_u32_literal(const std::string& token, u32& out) -> bool
{
    try
    {
        usize parsed_chars = 0u;
        const auto value = std::stoull(token, &parsed_chars, 0);
        if (parsed_chars != token.size())
        {
            return false;
        }
        if (value > static_cast<unsigned long long>(std::numeric_limits<u32>::max()))
        {
            return false;
        }
        out = static_cast<u32>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

[[nodiscard]] auto parse_f32_literal(const std::string& token, f32& out) -> bool
{
    try
    {
        usize parsed_chars = 0u;
        const auto value = std::stof(token, &parsed_chars);
        if (parsed_chars != token.size())
        {
            return false;
        }
        out = value;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

[[nodiscard]] auto
parse_register_index(const std::string& token, char prefix, usize max, usize& out) -> bool
{
    if (token.size() < 2u)
    {
        return false;
    }
    const auto upper = to_upper(token);
    if (upper.front() != prefix)
    {
        return false;
    }

    u32 index = 0u;
    if (!parse_u32_literal(upper.substr(1u), index))
    {
        return false;
    }
    if (index >= max)
    {
        return false;
    }
    out = static_cast<usize>(index);
    return true;
}

constexpr auto k_default_assembly_program = R"(; Simple direct CPU control demo
; Registers: R0..R7 (u32), F0..F3 (f32)
; Comments start with ';' and can also be full-line comments.

MOV     R0, 0x00        ; R0 = address 0
LOADB   R1, R0          ; R1 = byte at [R0]
MOV     R2, 0xAA        ; R2 = value 0xAA
STOREB  R0, R2          ; store R2 to [R0]
LOAD32  R3, 0x00        ; R3 = little-endian u32 from [0x00]
MOVF    F0, 1.5         ; F0 = 1.5
STOREF  0x20, F0        ; store F0 as fp32 at [0x20]
LOADF   F1, 0x20        ; F1 = fp32 from [0x20]
PRINT   R1              ; log R1
PRINT32 0x00            ; log u32 at [0x00]
PRINTF  0x20            ; log fp32 at [0x20]
FLUSH                   ; write dirty cache lines back
)";

}  // namespace

GuiState::GuiState() : config_{default_gui_config()}
{
    load_default_assembly_program();
    (void) save_assembly_program();
    reset_demo();
}

auto GuiState::reset_demo() -> void
{
    vm_ = std::make_unique<MemoryVM>(config_);
    seed_initial_memory();
    reset_asm_machine_state();
    if (!asm_program_loaded_)
    {
        (void) save_assembly_program();
    }
}

auto GuiState::seed_initial_memory() -> void
{
    for (auto addr = 0u; addr < 32u; ++addr)
    {
        vm_->write(addr, static_cast<std::byte>(addr + 1u));
    }
}

auto GuiState::run_load_sequence() -> void
{
    auto& cpu = vm_->cpu();
    (void) cpu.read(0u);
    (void) cpu.read(1u);
    (void) cpu.read(4u);
    (void) cpu.read(16u);
    (void) cpu.read(0u);
}

auto GuiState::run_store_sequence() -> void
{
    auto& cpu = vm_->cpu();
    cpu.write(0u, static_cast<std::byte>(0xAAu));
    cpu.write(16u, static_cast<std::byte>(0xBBu));
}

auto GuiState::flush() -> void
{
    vm_->cpu().flush();
}

auto GuiState::reset_asm_machine_state() -> void
{
    gpr_.fill(0u);
    fpr_.fill(0.0F);
    asm_pc_ = 0u;
    asm_error_.clear();
}

auto GuiState::invalidate_assembly_program() -> void
{
    asm_program_loaded_ = false;
    asm_program_lines_.clear();
    asm_source_lines_.clear();
    asm_pc_ = 0u;
    asm_error_.clear();
}

auto GuiState::load_default_assembly_program() -> void
{
    std::fill(asm_buffer_.begin(), asm_buffer_.end(), '\0');
    std::strncpy(
        asm_buffer_.data(), k_default_assembly_program, asm_buffer_.size() - static_cast<usize>(1u)
    );
    asm_error_.clear();
}

auto GuiState::reset_default_assembly_program() -> void
{
    load_default_assembly_program();
    (void) save_assembly_program();
}

auto GuiState::format_assembly_program() -> void
{
    std::istringstream input{std::string{asm_buffer_.data()}};
    std::vector<std::string> lines;
    std::string line;

    usize max_opcode_width = 0u;
    while (std::getline(input, line))
    {
        const auto comment_pos = find_comment_pos(line);
        const auto code_part = trim(std::string_view{line}.substr(0u, comment_pos));
        if (!code_part.empty())
        {
            const auto opcode_end = code_part.find_first_of(" \t");
            const auto opcode =
                opcode_end == std::string_view::npos ? code_part : code_part.substr(0u, opcode_end);
            max_opcode_width = std::max(max_opcode_width, opcode.size());
        }
        lines.push_back(line);
    }

    std::string formatted;
    for (const auto& original_line : lines)
    {
        const auto line_view = std::string_view{original_line};
        const auto comment_pos = find_comment_pos(line_view);
        const auto code_part = trim(line_view.substr(0u, comment_pos));
        const auto comment_part = comment_pos < line_view.size()
                                      ? trim(line_view.substr(comment_pos))
                                      : std::string_view{};

        std::string out_line;
        if (!code_part.empty())
        {
            const auto opcode_end = code_part.find_first_of(" \t");
            const auto opcode =
                opcode_end == std::string_view::npos ? code_part : code_part.substr(0u, opcode_end);
            const auto operand_part = opcode_end == std::string_view::npos
                                          ? std::string_view{}
                                          : trim(code_part.substr(opcode_end));

            out_line += std::string{opcode};
            if (!operand_part.empty())
            {
                const auto spacing = std::max<usize>(1u, max_opcode_width - opcode.size() + 1u);
                out_line.append(spacing, ' ');
                out_line += std::string{operand_part};
            }
        }

        if (!comment_part.empty())
        {
            if (!out_line.empty())
            {
                out_line += "  ";
            }
            out_line += std::string{comment_part};
        }

        formatted += out_line;
        formatted += '\n';
    }

    if (formatted.size() >= asm_buffer_.size())
    {
        asm_error_ = "Formatted assembly exceeds editor buffer.";
        append_assembly_log(std::format("ERROR {}", asm_error_));
        return;
    }

    std::fill(asm_buffer_.begin(), asm_buffer_.end(), '\0');
    std::copy(formatted.begin(), formatted.end(), asm_buffer_.begin());
    asm_error_.clear();
    append_assembly_log(
        std::format("Formatted assembly with opcode width {}.", static_cast<u32>(max_opcode_width))
    );
}

auto GuiState::reload_assembly_program_from_text(std::string_view source) -> bool
{
    asm_program_lines_.clear();
    asm_source_lines_.clear();
    asm_error_.clear();

    std::istringstream input{std::string{source}};
    std::string line;
    usize source_line = 1u;
    while (std::getline(input, line))
    {
        const auto no_comment = strip_comment(line);
        const auto content = trim(no_comment);
        if (!content.empty())
        {
            asm_program_lines_.emplace_back(content);
            asm_source_lines_.push_back(source_line);
        }
        ++source_line;
    }

    asm_pc_ = 0u;
    asm_program_loaded_ = true;
    append_assembly_log(
        std::format("Loaded {} instruction(s).", static_cast<u32>(asm_program_lines_.size()))
    );
    return true;
}

auto GuiState::reload_assembly_program() -> bool
{
    return save_assembly_program();
}

auto GuiState::save_assembly_program() -> bool
{
    const std::string candidate{asm_buffer_.data()};
    if (!reload_assembly_program_from_text(candidate))
    {
        return false;
    }
    asm_active_program_text_ = candidate;
    append_assembly_log("Saved assembly program.");
    return true;
}

auto GuiState::set_assembly_error(usize source_line, const std::string& message) -> void
{
    asm_error_ = std::format("Line {}: {}", source_line, message);
    append_assembly_log(std::format("ERROR {}", asm_error_));
}

auto GuiState::append_assembly_log(const std::string& line) -> void
{
    asm_log_ += line;
    asm_log_ += '\n';

    constexpr auto k_max_log_size = static_cast<usize>(64u * 1024u);
    if (asm_log_.size() > k_max_log_size)
    {
        const auto erase_size = asm_log_.size() - k_max_log_size;
        asm_log_.erase(0u, erase_size);
    }
}

auto GuiState::execute_assembly_line(std::string_view line, usize source_line) -> bool
{
    const auto tokens = tokenize(line);
    if (tokens.empty())
    {
        return true;
    }

    const auto op = to_upper(tokens[0]);
    const auto& cpu = vm_->cpu();
    const auto memory_size = cpu.config().memory_size;

    auto parse_r = [&](const std::string& token, usize& index) -> bool
    { return parse_register_index(token, 'R', gpr_.size(), index); };
    auto parse_f = [&](const std::string& token, usize& index) -> bool
    { return parse_register_index(token, 'F', fpr_.size(), index); };
    auto parse_u32_operand = [&](const std::string& token, u32& value) -> bool
    {
        usize reg = 0u;
        if (parse_r(token, reg))
        {
            value = gpr_[reg];
            return true;
        }
        return parse_u32_literal(token, value);
    };
    auto parse_f32_operand = [&](const std::string& token, f32& value) -> bool
    {
        usize reg = 0u;
        if (parse_f(token, reg))
        {
            value = fpr_[reg];
            return true;
        }
        return parse_f32_literal(token, value);
    };
    auto in_range = [&](u32 addr, u32 width) -> bool
    { return static_cast<u64>(addr) + static_cast<u64>(width) <= static_cast<u64>(memory_size); };
    auto require_arg_count = [&](usize expected) -> bool
    {
        if (tokens.size() != expected)
        {
            set_assembly_error(
                source_line,
                std::format(
                    "{} expects {} argument(s), got {}.", op, expected - 1u, tokens.size() - 1u
                )
            );
            return false;
        }
        return true;
    };

    if (op == "NOP")
    {
        if (!require_arg_count(1u))
        {
            return false;
        }
        return true;
    }

    if (op == "FLUSH")
    {
        if (!require_arg_count(1u))
        {
            return false;
        }
        vm_->cpu().flush();
        append_assembly_log("FLUSH");
        return true;
    }

    if (op == "MOV" || op == "ADD" || op == "SUB")
    {
        if (!require_arg_count(3u))
        {
            return false;
        }
        usize dst = 0u;
        if (!parse_r(tokens[1], dst))
        {
            set_assembly_error(
                source_line, std::format("Invalid destination register '{}'.", tokens[1])
            );
            return false;
        }
        u32 rhs = 0u;
        if (!parse_u32_operand(tokens[2], rhs))
        {
            set_assembly_error(source_line, std::format("Invalid u32 operand '{}'.", tokens[2]));
            return false;
        }

        if (op == "MOV")
        {
            gpr_[dst] = rhs;
        }
        else if (op == "ADD")
        {
            gpr_[dst] += rhs;
        }
        else
        {
            gpr_[dst] -= rhs;
        }
        append_assembly_log(std::format("{} R{} -> {}", op, dst, gpr_[dst]));
        return true;
    }

    if (op == "MOVF")
    {
        if (!require_arg_count(3u))
        {
            return false;
        }
        usize dst = 0u;
        if (!parse_f(tokens[1], dst))
        {
            set_assembly_error(
                source_line, std::format("Invalid destination float register '{}'.", tokens[1])
            );
            return false;
        }
        f32 value = 0.0F;
        if (!parse_f32_operand(tokens[2], value))
        {
            set_assembly_error(source_line, std::format("Invalid f32 operand '{}'.", tokens[2]));
            return false;
        }
        fpr_[dst] = value;
        append_assembly_log(std::format("MOVF F{} -> {}", dst, fpr_[dst]));
        return true;
    }

    if (op == "LOADB" || op == "LOAD32")
    {
        if (!require_arg_count(3u))
        {
            return false;
        }
        usize dst = 0u;
        if (!parse_r(tokens[1], dst))
        {
            set_assembly_error(
                source_line, std::format("Invalid destination register '{}'.", tokens[1])
            );
            return false;
        }
        u32 addr = 0u;
        if (!parse_u32_operand(tokens[2], addr))
        {
            set_assembly_error(
                source_line, std::format("Invalid address operand '{}'.", tokens[2])
            );
            return false;
        }
        const auto width = op == "LOADB" ? 1u : 4u;
        if (!in_range(addr, width))
        {
            set_assembly_error(source_line, std::format("Address out of bounds: 0x{:X}.", addr));
            return false;
        }
        if (op == "LOADB")
        {
            gpr_[dst] = std::to_integer<u32>(vm_->cpu().read(addr));
        }
        else
        {
            gpr_[dst] = vm_->cpu().read_u32(addr);
        }
        append_assembly_log(std::format("{} R{} <- [0x{:X}] = {}", op, dst, addr, gpr_[dst]));
        return true;
    }

    if (op == "STOREB" || op == "STORE32")
    {
        if (!require_arg_count(3u))
        {
            return false;
        }
        u32 addr = 0u;
        if (!parse_u32_operand(tokens[1], addr))
        {
            set_assembly_error(
                source_line, std::format("Invalid address operand '{}'.", tokens[1])
            );
            return false;
        }
        u32 value = 0u;
        if (!parse_u32_operand(tokens[2], value))
        {
            set_assembly_error(source_line, std::format("Invalid source operand '{}'.", tokens[2]));
            return false;
        }
        const auto width = op == "STOREB" ? 1u : 4u;
        if (!in_range(addr, width))
        {
            set_assembly_error(source_line, std::format("Address out of bounds: 0x{:X}.", addr));
            return false;
        }
        if (op == "STOREB")
        {
            vm_->cpu().write(addr, static_cast<std::byte>(value & 0xFFu));
        }
        else
        {
            vm_->cpu().write_u32(addr, value);
        }
        append_assembly_log(std::format("{} [0x{:X}] <- {}", op, addr, value));
        return true;
    }

    if (op == "LOADF")
    {
        if (!require_arg_count(3u))
        {
            return false;
        }
        usize dst = 0u;
        if (!parse_f(tokens[1], dst))
        {
            set_assembly_error(
                source_line, std::format("Invalid destination float register '{}'.", tokens[1])
            );
            return false;
        }
        u32 addr = 0u;
        if (!parse_u32_operand(tokens[2], addr))
        {
            set_assembly_error(
                source_line, std::format("Invalid address operand '{}'.", tokens[2])
            );
            return false;
        }
        if (!in_range(addr, 4u))
        {
            set_assembly_error(source_line, std::format("Address out of bounds: 0x{:X}.", addr));
            return false;
        }
        fpr_[dst] = vm_->cpu().read_f32(addr);
        append_assembly_log(std::format("LOADF F{} <- [0x{:X}] = {}", dst, addr, fpr_[dst]));
        return true;
    }

    if (op == "STOREF")
    {
        if (!require_arg_count(3u))
        {
            return false;
        }
        u32 addr = 0u;
        if (!parse_u32_operand(tokens[1], addr))
        {
            set_assembly_error(
                source_line, std::format("Invalid address operand '{}'.", tokens[1])
            );
            return false;
        }
        if (!in_range(addr, 4u))
        {
            set_assembly_error(source_line, std::format("Address out of bounds: 0x{:X}.", addr));
            return false;
        }
        f32 value = 0.0F;
        if (!parse_f32_operand(tokens[2], value))
        {
            set_assembly_error(source_line, std::format("Invalid source operand '{}'.", tokens[2]));
            return false;
        }
        vm_->cpu().write_f32(addr, value);
        append_assembly_log(std::format("STOREF [0x{:X}] <- {}", addr, value));
        return true;
    }

    if (op == "PRINT")
    {
        if (!require_arg_count(2u))
        {
            return false;
        }
        usize reg = 0u;
        if (parse_r(tokens[1], reg))
        {
            append_assembly_log(std::format("PRINT R{} = {}", reg, gpr_[reg]));
            return true;
        }
        if (parse_f(tokens[1], reg))
        {
            append_assembly_log(std::format("PRINT F{} = {}", reg, fpr_[reg]));
            return true;
        }
        u32 value = 0u;
        if (parse_u32_operand(tokens[1], value))
        {
            append_assembly_log(std::format("PRINT {}", value));
            return true;
        }
        f32 fvalue = 0.0F;
        if (parse_f32_operand(tokens[1], fvalue))
        {
            append_assembly_log(std::format("PRINT {}", fvalue));
            return true;
        }
        set_assembly_error(source_line, std::format("Invalid PRINT operand '{}'.", tokens[1]));
        return false;
    }

    if (op == "PRINTB" || op == "PRINT32" || op == "PRINTF")
    {
        if (!require_arg_count(2u))
        {
            return false;
        }
        u32 addr = 0u;
        if (!parse_u32_operand(tokens[1], addr))
        {
            set_assembly_error(
                source_line, std::format("Invalid address operand '{}'.", tokens[1])
            );
            return false;
        }
        const auto width = op == "PRINTB" ? 1u : 4u;
        if (!in_range(addr, width))
        {
            set_assembly_error(source_line, std::format("Address out of bounds: 0x{:X}.", addr));
            return false;
        }
        if (op == "PRINTB")
        {
            const auto value = std::to_integer<u32>(vm_->cpu().read(addr));
            append_assembly_log(std::format("PRINTB [0x{:X}] = {}", addr, value));
        }
        else if (op == "PRINT32")
        {
            const auto value = vm_->cpu().read_u32(addr);
            append_assembly_log(std::format("PRINT32 [0x{:X}] = {}", addr, value));
        }
        else
        {
            const auto value = vm_->cpu().read_f32(addr);
            append_assembly_log(std::format("PRINTF [0x{:X}] = {}", addr, value));
        }
        return true;
    }

    set_assembly_error(source_line, std::format("Unknown opcode '{}'.", tokens[0]));
    return false;
}

auto GuiState::step_assembly() -> bool
{
    if (!asm_program_loaded_)
    {
        if (!asm_active_program_text_.empty())
        {
            if (!reload_assembly_program_from_text(asm_active_program_text_))
            {
                return false;
            }
        }
        else if (!save_assembly_program())
        {
            return false;
        }
    }

    if (asm_pc_ >= asm_program_lines_.size())
    {
        return false;
    }

    asm_error_.clear();
    const auto source_line = asm_source_lines_[asm_pc_];
    const auto line = asm_program_lines_[asm_pc_];
    if (!execute_assembly_line(line, source_line))
    {
        return false;
    }

    ++asm_pc_;
    return true;
}

auto GuiState::run_assembly(usize max_steps) -> void
{
    if (!asm_program_loaded_)
    {
        if (!asm_active_program_text_.empty())
        {
            if (!reload_assembly_program_from_text(asm_active_program_text_))
            {
                return;
            }
        }
        else if (!save_assembly_program())
        {
            return;
        }
    }

    if (asm_pc_ >= asm_program_lines_.size())
    {
        append_assembly_log("Program already complete.");
        return;
    }

    auto steps = 0zu;
    while (asm_pc_ < asm_program_lines_.size() && steps < max_steps)
    {
        if (!step_assembly())
        {
            return;
        }
        ++steps;
    }

    if (asm_pc_ >= asm_program_lines_.size())
    {
        append_assembly_log(std::format("Program complete ({} step(s)).", steps));
    }
    else
    {
        append_assembly_log(std::format("Run paused after {} step(s).", steps));
    }
}

auto GuiState::clear_assembly_log() -> void
{
    asm_log_.clear();
}

auto GuiState::assembly_buffer() -> char*
{
    return asm_buffer_.data();
}

auto GuiState::assembly_buffer_size() const -> usize
{
    return asm_buffer_.size();
}

auto GuiState::assembly_log() const -> const std::string&
{
    return asm_log_;
}

auto GuiState::assembly_active_program_text() const -> const std::string&
{
    return asm_active_program_text_;
}

auto GuiState::assembly_error() const -> const std::string&
{
    return asm_error_;
}

auto GuiState::assembly_pc() const -> usize
{
    return asm_pc_;
}

auto GuiState::assembly_program_size() const -> usize
{
    return asm_program_lines_.size();
}

auto GuiState::assembly_current_source_line() const -> std::optional<usize>
{
    if (!asm_program_loaded_ || asm_pc_ >= asm_source_lines_.size())
    {
        return std::nullopt;
    }
    return asm_source_lines_[asm_pc_];
}

auto GuiState::gpr() const -> const std::array<u32, 8>&
{
    return gpr_;
}

auto GuiState::fpr() const -> const std::array<f32, 4>&
{
    return fpr_;
}

auto GuiState::vm() -> MemoryVM&
{
    return *vm_;
}

auto GuiState::vm() const -> const MemoryVM&
{
    return *vm_;
}

}  // namespace ds_mem
