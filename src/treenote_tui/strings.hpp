// strings.hpp

#pragma once

#include <cstdint>
#include <format>
#include <numeric>
#include <string>
#include <string_view>

#include <curses.h>

#include "../treenote/utf8.h"

namespace treenote_tui::detail
{
    /* Stores a constant (translated) C style string and its utf-8 length */
    class text_string
    {
    public:
        explicit text_string(const char* untranslated_c_str);
        [[nodiscard]] inline const char* c_str() const noexcept;
        [[nodiscard]] inline std::string_view str_view() const noexcept;
        [[nodiscard]] inline int length() const noexcept;
    
    private:
        const char*         text_;
        int                 size_;
    };
    
    class text_fstring_result;
    
    template<std::size_t I>
    class text_fstring
    {
    public:
        explicit text_fstring(const char* untranslated_c_str);
        
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
        [[nodiscard]] inline const char* c_str() const noexcept;
        [[nodiscard]] inline std::string_view str_view() const noexcept;
        [[nodiscard]] inline int length() const noexcept;
        
        template<std::size_t>
        friend class text_fstring;
    private:
        text_fstring_result() = default;
        
        std::string         text_;
        int                 size_{ 0 };
    };
    
    
    /* Strings used in window (known at compile time) */
    struct display_strings
    {
        text_string close_prompt        { "Save modified buffer?" };
        text_string file_prompt         { "File Name to Write"};
        text_string modified            { "Modified" };
        text_string empty_file          { "New Tree" };
        text_string unbound_key         { "Unbound key" };
        text_string nothing_undo        { "Nothing to undo" };
        text_string nothing_redo        { "Nothing to redo" };
        text_string nothing_delete      { "Nothing to delete" };
        text_string delete_prevent      { "To recursively delete, type ^Del" };
        text_string undo_move_node      { "Undid move node" };
        text_string redo_move_node      { "Redid move node" };
        text_string undo_ins_node       { "Undid insert node" };
        text_string redo_ins_node       { "Redid insert node" };
        text_string undo_del_node       { "Undid delete node" };
        text_string redo_del_node       { "Redid delete node" };
        text_string undo_cut_node       { "Undid cut node" };
        text_string redo_cut_node       { "Redid cut node" };
        text_string undo_paste_node     { "Undid paste node" };
        text_string redo_paste_node     { "Redid paste node" };
        text_string undo_ins_text       { "Undid addition" };
        text_string redo_ins_text       { "Redid addition" };
        text_string undo_del_text       { "Undid deletion" };
        text_string redo_del_text       { "Redid deletion" };
        text_string undo_line_br        { "Undid line break" };
        text_string redo_line_br        { "Redid line break" };
        text_string undo_line_jn        { "Undid line join" };
        text_string redo_line_jn        { "Redid line join" };
        text_string cut_error           { "Nothing was cut" };
        text_string copy_error          { "Nothing was copied" };
        text_string paste_error         { "Node cut buffer is empty" };
        text_string new_file_msg        { "New file" };
        text_string cancelled           { "Cancelled" };
        text_fstring<2> read_success    { "Loaded {} nodes from {} lines" };
        text_fstring<2> write_success   { "Wrote {} nodes to {} lines" };
        text_fstring<1> file_is_unwrit  { "File {} is unwritable" };
        text_fstring<2> error_reading   { "Error reading {}: {}" };
        text_fstring<2> error_writing   { "Error writing {}: {}" };
        text_string is_directory        { "Is a directory" };
        text_string is_device_file      { "Is a device file" };
        text_string invalid_file        { "Invalid file" };
        text_string permission_denied   { "Permission denied" };
        text_string unknown_error       { "Unknown error" };
        text_fstring<5> cursor_pos_msg  { "node: {} line_no: {}/{} col: {}/{}" };
        text_fstring<2> dbg_unimp_act   { "Unimplemented action: {} ({})" };
        text_fstring<2> dbg_unknwn_act  { "{}: '{}'" };
        text_fstring<1> dbg_pressed     { "pressed: '{}'" };
    };
    
    
    /* Inline function implementations for text_string */
    
    inline text_string::text_string(const char* untranslated_c_str) :
            text_{ gettext(untranslated_c_str) },
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
    inline text_fstring<I>::text_fstring(const char* untranslated_c_str) :
            text_{ gettext(untranslated_c_str) },
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
    
    
}