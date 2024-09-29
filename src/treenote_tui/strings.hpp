// strings.hpp

#pragma once

#include <cstdint>
#include <format>
#include <numeric>
#include <string>
#include <string_view>

#include "keymap.h"

#include "../treenote/utf8.h"

namespace treenote_tui::strings
{
    /* Stores a constant (translated) C style string and its utf-8 length */
    class text_string
    {
    public:
        explicit text_string(const char* untranslated_c_str) noexcept;
        [[nodiscard]] const char* c_str() const noexcept;
        [[nodiscard]] std::string_view str_view() const noexcept;
        [[nodiscard]] int length() const noexcept;
    
    private:
        const char*         text_;
        int                 size_;
    };
    
    class text_fstring_result;
    
    template<std::size_t I>
    class text_fstring
    {
    public:
        explicit text_fstring(const char* untranslated_c_str) noexcept;
        
        template<typename... Ts>
        requires (sizeof...(Ts) == I)
        [[nodiscard]] inline text_fstring_result operator()(const Ts&... args) const;
    
    private:
        const char*         text_;
        int                 size_;
    };
    
    class text_fstring_result
    {
    public:
        [[nodiscard]] const char* c_str() const noexcept;
        [[nodiscard]] std::string_view str_view() const noexcept;
        [[nodiscard]] int length() const noexcept;
        
        template<std::size_t>
        friend class text_fstring;
    private:
        text_fstring_result() = default;
        
        std::string         text_;
        int                 size_{ 0 };
    };
    
    class help_text_entry
    {
    public:
        constexpr help_text_entry() noexcept;
        help_text_entry(actions action, const char* untranslated_c_str) noexcept;
        
        [[nodiscard]] std::optional<const char*> c_str() const noexcept;
        [[nodiscard]] std::string_view str_view() const noexcept;
        [[nodiscard]] int length() const noexcept;
        [[nodiscard]] actions action() const noexcept;
        [[nodiscard]] bool has_value() const noexcept;
        [[nodiscard]] operator bool() const noexcept { return has_value(); };
        
    private:
        const char*         text_;
        int                 size_;
        actions             action_;
    };
    
    /* Strings used in window (known at compile time) */
    
    inline const text_string help_titl              { "treenote help text" };
    inline const text_string help_title             { "treenote help text" };
    inline const text_string close_prompt           { "Save modified buffer?" };
    inline const text_string file_prompt            { "File Name to Write"};
    inline const text_string modified               { "Modified" };
    inline const text_string empty_file             { "New Tree" };
    inline const text_string unbound_key            { "Unbound key" };
    inline const text_string nothing_undo           { "Nothing to undo" };
    inline const text_string nothing_redo           { "Nothing to redo" };
    inline const text_string nothing_delete         { "Nothing to delete" };
    inline const text_fstring<1> delete_prevent     { "To recursively delete, type {}" };
    inline const text_string undo_move_node         { "Undid move node" };
    inline const text_string redo_move_node         { "Redid move node" };
    inline const text_string undo_ins_node          { "Undid insert node" };
    inline const text_string redo_ins_node          { "Redid insert node" };
    inline const text_string undo_del_node          { "Undid delete node" };
    inline const text_string redo_del_node          { "Redid delete node" };
    inline const text_string undo_cut_node          { "Undid cut node" };
    inline const text_string redo_cut_node          { "Redid cut node" };
    inline const text_string undo_paste_node        { "Undid paste node" };
    inline const text_string redo_paste_node        { "Redid paste node" };
    inline const text_string undo_ins_text          { "Undid addition" };
    inline const text_string redo_ins_text          { "Redid addition" };
    inline const text_string undo_del_text          { "Undid deletion" };
    inline const text_string redo_del_text          { "Redid deletion" };
    inline const text_string undo_line_br           { "Undid line break" };
    inline const text_string redo_line_br           { "Redid line break" };
    inline const text_string undo_line_jn           { "Undid line join" };
    inline const text_string redo_line_jn           { "Redid line join" };
    inline const text_string cut_error              { "Nothing was cut" };
    inline const text_string copy_error             { "Nothing was copied" };
    inline const text_string paste_error            { "Node cut buffer is empty" };
    inline const text_string new_file_msg           { "New file" };
    inline const text_string cancelled              { "Cancelled" };
    inline const text_fstring<2> read_success       { "Loaded {} nodes from {} lines" };
    inline const text_fstring<2> write_success      { "Wrote {} nodes to {} lines" };
    inline const text_fstring<1> file_is_unwrit     { "File {} is unwritable" };
    inline const text_fstring<2> error_reading      { "Error reading {}: {}" };
    inline const text_fstring<2> error_writing      { "Error writing {}: {}" };
    inline const text_string is_directory           { "Is a directory" };
    inline const text_string is_device_file         { "Is a device file" };
    inline const text_string invalid_file           { "Invalid file" };
    inline const text_string permission_denied      { "Permission denied" };
    inline const text_string unknown_error          { "Unknown error" };
    inline const text_fstring<5> cursor_pos_msg     { "node: {} line_no: {}/{} col: {}/{}" };
    inline const text_fstring<2> dbg_unimp_act      { "Unimplemented action: {} ({})" };
    inline const text_fstring<2> dbg_unknwn_act     { "{}: '{}'" };
    inline const text_fstring<1> dbg_pressed        { "pressed: '{}'" };
    inline const text_string action_yes             { "Yes" };
    inline const text_string action_no              { "No" };
    inline const text_string action_cancel          { "Cancel" };
    inline const text_string action_close           { "Close" };
    inline const text_string action_help            { "Help" };
    inline const text_string action_exit            { "Exit" };
    inline const text_string action_write           { "Write Out" };
    inline const text_string action_save            { "Save" };
    inline const text_string action_cut             { "Cut" };
    inline const text_string action_paste           { "Paste" };
    inline const text_string action_undo            { "Undo" };
    inline const text_string action_redo            { "Redo" };
    inline const text_string action_copy            { "Copy" };
    inline const text_string action_refresh         { "Refresh" };
    inline const text_string action_location        { "Location" };
    inline const text_string action_go_to           { "Go To" };
    inline const text_string action_insert_node     { "New Node" };
    inline const text_string action_delete_node     { "Del Node" };
    inline const text_string action_previous_line   { "Prev Line" };
    inline const text_string action_next_line       { "Next Line" };
    inline const text_string action_previous_page   { "Prev Page" };
    inline const text_string action_next_page       { "Next Page" };
    inline const text_string action_first_line      { "First Line" };
    inline const text_string action_last_line       { "Last Line" };
    
    inline static const std::array help_strings{ std::to_array<help_text_entry>({
            { actions::show_help,       "Show this help text" },
            { actions::close_tree,      "Close the current tree" },
            { actions::write_tree,      "Write the tree to disk" },
            {},
            { actions::cut_node,        "Cut current node in tree and store it in cutbuffer" },
            { actions::copy_node,       "Copy current node in tree to cutbuffer" },
            { actions::paste_node,      "Paste contents of cutbuffer below current line" },
            { actions::paste_node_abv,  "Paste contents of cutbuffer above current line"},
            {},
            { actions::cursor_pos,      "Display the position of the cursor" },
            { actions::go_to,           "Go to position in tree" },
            {},
            { actions::undo,            "Undo the last operation " },
            { actions::redo,            "Redo the last done operation" },
            {},
            { actions::cursor_left,     "Go back one character" },
            { actions::cursor_right,    "Go forward one character" },
            { actions::cursor_prev_w,   "Go back one word" },
            { actions::cursor_next_w,   "Go forward one word" },
            { actions::cursor_sol,      "Go to beginning of current line" },
            { actions::cursor_eol,      "Go to end of current line" },
            {},
            { actions::cursor_up,       "Go to previous line" },
            { actions::cursor_down,     "Go to next line" },
            { actions::cursor_sof,      "Go to first line of file" },
            { actions::cursor_eof,      "Go to last line of file" },
            {},
            { actions::node_parent,     "Go to parent tree node" },
            { actions::node_child,      "Go to first child tree node" },
            { actions::node_prev,       "Go to next tree node" },
            { actions::node_next,       "Go to previous tree node" },
            {},
            { actions::scroll_up,       "Scroll up one line without moving the cursor" },
            { actions::scroll_down,     "Scroll down one line without moving the cursor" },
            { actions::page_up,         "Scroll up one page" },
            { actions::page_down,       "Scroll down one page" },
            { actions::center_view,     "Move cursor to the center line" },
            {},
            { actions::insert_node_def, "Insert node in tree" },
            { actions::insert_node_chi, "Insert tree node as child of current" },
            { actions::insert_node_bel, "Insert tree node below current at same depth" },
            { actions::insert_node_abv, "Insert tree node above current at same depth" },
            {},
            { actions::delete_node_chk, "Delete current tree node" },
            { actions::delete_node_rec, "Recursively delete current tree node and all children" },
            { actions::delete_node_spc, "Delete current tree node without deleting children" },
            {},
            { actions::raise_node,          "Raise current node in tree" },
            { actions::lower_node,          "Lower current node in tree" },
            { actions::reorder_backwards,   "Move current node backwards in tree" },
            { actions::reorder_forwards,    "Move current node forwards in tree" },
            {},
            { actions::newline,         "Insert a newline at the cursor position" },
            { actions::backspace,       "Delete the character to the left of the cursor" },
            { actions::delete_char,     "Delete the character under the cursor" },
            {},
            { actions::save_file,       "Save file without prompting" },
            { actions::suspend,         "Suspend treenote" },
    })};
    
    /* Inline function implementations for text_string */
    
    inline text_string::text_string(const char* untranslated_c_str) noexcept :
            text_{ untranslated_c_str /* gettext(untranslated_c_str) */ },
            size_{ std::saturate_cast<int>(treenote::utf8::length(text_)) }
    {
    }
    
    inline const char* text_string::c_str() const noexcept
    {
        return text_;
    }
    
    inline std::string_view text_string::str_view() const noexcept
    {
        return text_;
    }
    
    inline int text_string::length() const noexcept
    {
        return size_;
    }
    
    
    /* Inline function implementations for text_fstring and text_fstring_result */
    
    template<std::size_t I>
    inline text_fstring<I>::text_fstring(const char* untranslated_c_str) noexcept :
            text_{ untranslated_c_str /* gettext(untranslated_c_str) */ },
            size_{ std::saturate_cast<int>(treenote::utf8::length(text_)) }
    {
    }
    
    template<std::size_t I>
    template<typename... Ts>
    requires (sizeof...(Ts) == I)
    inline text_fstring_result text_fstring<I>::operator()(const Ts&... args) const
    {
        text_fstring_result result{};
        result.text_ = std::vformat(text_, std::make_format_args(args...));
        result.size_ = std::saturate_cast<int>(treenote::utf8::length(result.text_).value_or(0));
        return result;
    }
    
    inline const char* text_fstring_result::c_str() const noexcept
    {
        return text_.c_str();
    }
    
    inline std::string_view text_fstring_result::str_view() const noexcept
    {
        return text_;
    }
    
    inline int text_fstring_result::length() const noexcept
    {
        return size_;
    }
    
    
    /* Inline function implementations for help_text_entry */
    
    constexpr help_text_entry::help_text_entry() noexcept :
        text_{ nullptr }, size_{ 0 }, action_{ actions::unknown }
    {
    }
    
    inline help_text_entry::help_text_entry(actions action, const char* untranslated_c_str) noexcept :
            text_{ untranslated_c_str /* gettext(untranslated_c_str) */ },
            size_{ std::saturate_cast<int>(treenote::utf8::length(text_)) },
            action_{ action }
    {
    }
    
    inline std::optional<const char*> help_text_entry::c_str() const noexcept
    {
        if (has_value())
            return text_;
        else
            return {};
    }
    
    inline std::string_view help_text_entry::str_view() const noexcept
    {
        if (has_value())
            return text_;
        else
            return {};
    }
    
    inline int help_text_entry::length() const noexcept
    {
        return size_;
    }

    inline actions help_text_entry::action() const noexcept
    {
        return action_;
    }
        
    inline bool help_text_entry::has_value() const noexcept
    {
        return text_ != nullptr;
    }
}