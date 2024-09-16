// window_detail.hpp

#pragma once

#include <cstdint>
#include <numeric>

#include <curses.h>

#include "../treenote/utf8.h"

struct coord
{
    int y;
    int x;
};

namespace treenote_tui::detail
{
    /* Non-reusable component classes and structs used in treenote_tui::window */
    
    /* Names for ncurses color pairs used in window */
    enum class color_type : std::int8_t
    {
        standard,
        inverse,
        warning
    };
    
    enum class status_bar_mode : std::int8_t
    {
        DEFAULT,
        PROMPT_CLOSE,
        PROMPT_FILENAME
    };
    
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
    
    /* Strings used in window (known at compile time )*/
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
    
    /* Provides a bitfield to control which parts are withdrawn by window */
    class redraw_mask
    {
    public:
        enum mode : std::int8_t
        {
            RD_NONE         = 0b0000,
            RD_TOP          = 0b0001,
            RD_CONTENT      = 0b0010,
            RD_STATUS       = 0b0100,
            RD_HELP         = 0b1000,
            RD_ALL          = 0b1111
        };
        
        template<typename... Masks>
        requires (std::same_as<Masks, mode> and ...)
        void add_mask(Masks... ms);
        
        void set_all();
        void clear();
        bool has_mask(mode m);
    
    private:
        mode                value_{ RD_NONE };
    };
    
    /* Wrapper class for ncurses sub-windows of stdscr */
    class sub_window
    {
    public:
        explicit sub_window(coord size, coord begin);
        
        sub_window() = default;
        sub_window(const sub_window&) = delete;
        sub_window(sub_window&& other) noexcept;
        sub_window& operator=(const sub_window&) = delete;
        sub_window& operator=(sub_window&& other) noexcept;
        ~sub_window();
        
        [[nodiscard]] WINDOW* operator*();
        [[nodiscard]] WINDOW* get();
        [[nodiscard]] const coord& size() const noexcept;
        [[nodiscard]] const coord& pos() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] bool is_enabled() const noexcept;
        
        void set_color(color_type name, bool term_has_color);
        void unset_color(color_type name, bool term_has_color);
        void set_default_color(color_type name, bool term_has_color);
    
    private:
        WINDOW*             ptr_{ nullptr };
        coord               size_{ .y = 0, .x = 0 };
        coord               pos_{ .y = -1, .x = -1 };
    };
    
    /* Class for managing status bar messages */
    class status_bar_message
    {
        using clock_t       = std::chrono::system_clock;
        using time_point_t  = std::chrono::time_point<clock_t>;
        using text_str_ref  = std::reference_wrapper<const text_string>;
        using message_t     = std::variant<std::monostate, text_str_ref, text_fstring_result>;
        static constexpr long timeout_length{ 2 };
    
    public:
        status_bar_message() = delete;
        explicit status_bar_message(redraw_mask& mask);
        
        [[nodiscard]] inline const char* c_str() const;
        [[nodiscard]] inline int length() const;
        [[nodiscard]] inline bool is_error() const noexcept;
        [[nodiscard]] inline bool has_message() const noexcept;
        inline void set_message(const text_string& msg);
        inline void set_message(text_fstring_result&& msg);
        inline void set_warning(const text_string& msg);
        inline void set_warning(text_fstring_result&& msg);
        inline void force_clear();
        inline void clear();
    
    private:
        bool                                error_{ false };                /* if true, message should be displayed as error */
        std::size_t                         draw_count_{ 0 };               /* number of times the message has been shown    */
        message_t                           message_{};                     /* string to display                             */
        time_point_t                        start_time_{};                  /* (of first display)                            */
        std::reference_wrapper<redraw_mask> mask_;                          /* reference to window's redraw mask             */
    };
    
    /* Struct for managing the status bar prompts */
    struct status_bar_prompt
    {
        std::string         text;                           /* editable string to display: should be ascii */
        std::size_t         cursor_pos{ 0 };                /* horizontal cursor position within line      */
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
    
    
    /* Inline function implementations for redraw_mask */
    
    template<typename... Masks>
    requires (std::same_as<Masks, redraw_mask::mode> and ...)
    inline void redraw_mask::add_mask(Masks... ms)
    {
        value_ = static_cast<mode>((value_ | ... | ms));
    }
    
    inline void redraw_mask::set_all()
    {
        value_ = RD_ALL;
    }
    
    inline void redraw_mask::clear()
    {
        value_ = RD_NONE;
    }
    
    inline bool redraw_mask::has_mask(mode m)
    {
        return ((value_ & m) == m);
    }
    
    
    /* Inline function implementations for sub_window */
    
    inline sub_window::sub_window(coord size, coord begin) :
            ptr_{ subwin(stdscr, size.y, size.x, begin.y, begin.x) }, size_{ size }, pos_{ begin }
    {
    }
    
    inline sub_window::~sub_window()
    {
        if (is_enabled())
            delwin(ptr_);
    }
    
    inline sub_window::sub_window(sub_window&& other) noexcept:
            ptr_{ other.ptr_ }, size_{ other.size_ }, pos_{ other.pos_ }
    {
        other.ptr_ = nullptr;
    }
    
    inline sub_window& sub_window::operator=(sub_window&& other) noexcept
    {
        if (is_enabled())
            delwin(ptr_);
        
        ptr_ = other.ptr_;
        size_ = other.size_;
        pos_ = other.pos_;
        other.ptr_ = nullptr;
        return *this;
    }
    
    inline WINDOW* sub_window::operator*()
    {
        return get();
    }
    
    inline WINDOW* sub_window::get()
    {
        if (is_enabled())
            return ptr_;
        else
            throw std::out_of_range("Cannot return nullptr from sub_window");
    }
    
    inline const coord& sub_window::size() const noexcept
    {
        return size_;
    }
    
    inline const coord& sub_window::pos() const noexcept
    {
        return pos_;
    }
    
    inline sub_window::operator bool() const noexcept
    {
        return is_enabled();
    }
    
    inline bool sub_window::is_enabled() const noexcept
    {
        return (ptr_ != nullptr);
    }
    
    
    /* Inline drawing help functions for sub_window */
    
    inline void sub_window::set_color(color_type name, bool term_has_color)
    {
        switch (name)
        {
            case color_type::standard:
                wattron(get(), A_NORMAL);
                return;
            
            case color_type::inverse:
                wattron(get(), A_REVERSE);
                return;
            
            case color_type::warning:
                if (term_has_color)
                    wattron(get(), A_BOLD | COLOR_PAIR(1));
                else
                    wattron(get(), A_BOLD | A_STANDOUT);
                return;
        }
    }
    
    inline void sub_window::unset_color(color_type name, bool term_has_color)
    {
        switch (name)
        {
            case color_type::standard:
                wattroff(get(), A_NORMAL);
                return;
            
            case color_type::inverse:
                wattroff(get(), A_REVERSE);
                return;
            
            case color_type::warning:
                if (term_has_color)
                    wattroff(get(), A_BOLD | COLOR_PAIR(1));
                else
                    wattroff(get(), A_BOLD | A_STANDOUT);
                return;
        }
    }
    
    inline void sub_window::set_default_color(color_type name, bool term_has_color)
    {
        switch (name)
        {
            case color_type::standard:
                wbkgd(get(), A_NORMAL);
                break;
            
            case color_type::inverse:
                wbkgd(get(), A_REVERSE);
                break;
            
            case color_type::warning:
                if (term_has_color)
                    wbkgd(get(), A_BOLD | COLOR_PAIR(1));
                else
                    wbkgd(get(), A_BOLD | A_STANDOUT);
                break;
        }
    }


    /* Inline functions for status_bar_message */
    
    inline status_bar_message::status_bar_message(redraw_mask& mask):
        mask_{ mask }
    {
    }
    
    inline const char* status_bar_message::c_str() const
    {
        if (std::holds_alternative<text_str_ref>(message_))
            return std::get<text_str_ref>(message_).get().c_str();
        else if (std::holds_alternative<text_fstring_result>(message_))
            return std::get<text_fstring_result>(message_).c_str();
        else
            return nullptr;
    }
    
    inline int status_bar_message::length() const
    {
        if (std::holds_alternative<text_str_ref>(message_))
            return std::get<text_str_ref>(message_).get().length();
        else if (std::holds_alternative<text_fstring_result>(message_))
            return std::get<text_fstring_result>(message_).length();
        else
            return 0;
    }
    
    inline bool status_bar_message::is_error() const noexcept
    {
        return error_;
    }
    
    inline bool status_bar_message::has_message() const noexcept
    {
        return not std::holds_alternative<std::monostate>(message_);
    }
    
    inline void status_bar_message::set_message(const text_string& msg)
    {
        message_ = std::cref(msg);
        error_ = false;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::set_message(text_fstring_result&& msg)
    {
        message_ = std::move(msg);
        error_ = false;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::set_warning(const text_string& msg)
    {
        message_ = std::cref(msg);
        error_ = true;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::set_warning(text_fstring_result&& msg)
    {
        message_ = std::move(msg);
        error_ = true;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::force_clear()
    {
        message_ = {};
        error_ = false;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::clear()
    {
        if (draw_count_ == 0)
        {
            start_time_ = clock_t::now();
            ++draw_count_;
        }
        else
        {
            if (clock_t::now() - start_time_ >= std::chrono::duration<long>(timeout_length))
                force_clear();
            else
                ++draw_count_;
        }
    }
}
