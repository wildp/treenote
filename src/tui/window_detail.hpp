// tui/window_detail.hpp
//
// Copyright (C) 2025 Peter Wild
//
// This file is part of tred.
//
// tred is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// tred is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with tred.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include <chrono>
#include <cstdint>
#include <variant>

#include <curses.h>

#include "strings.hpp"

struct coord
{
    int y;
    int x;
};

namespace tred::tui::detail
{
    /* Non-reusable component classes and structs used in treenote_tui::window */
    
    class window_event_loop;
    
    /* Names for ncurses color pairs used in window */
    enum class color_type : std::int8_t
    {
        standard,
        inverse,
        warning,
        emphasis,
    };
    
    enum class status_bar_mode : std::int8_t
    {
        default_mode,
        prompt_close,
        prompt_filename,
        prompt_location
    };

    /* Provides a way of calling endwin() after sub_windows are destroyed  */

    struct defer_endwin
    {
        defer_endwin() = default;
        ~defer_endwin() { endwin(); }
        
        defer_endwin(const defer_endwin&) = delete;
        defer_endwin(defer_endwin&&) = delete;
        defer_endwin& operator=(const defer_endwin&) = delete;
        defer_endwin& operator=(defer_endwin&&) = delete;
    };
    
    
    /* Provides a bitfield to control which parts are redrawn by window */
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
        [[nodiscard]] bool has_mask(mode m) const;
    
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
        using text_str_ref  = std::reference_wrapper<const strings::text_string>;
        using message_t     = std::variant<std::monostate, text_str_ref, strings::text_fstring_result>;
        static constexpr long timeout_length{ 2 };
    
    public:
        status_bar_message() = delete;
        explicit status_bar_message(redraw_mask& mask);
        
        [[nodiscard]] inline const char* c_str() const;
        [[nodiscard]] inline int length() const;
        [[nodiscard]] inline bool is_error() const noexcept;
        [[nodiscard]] inline bool has_message() const noexcept;
        inline void set_message(const strings::text_string& msg);
        inline void set_message(strings::text_fstring_result&& msg);
        inline void set_warning(const strings::text_string& msg);
        inline void set_warning(strings::text_fstring_result&& msg);
        inline void force_clear();
        inline void clear();
    
    private:
        bool                                error_{ false };                /* if true, message should be displayed as error */
        std::size_t                         draw_count_{ 0 };               /* number of times the message has been shown    */
        message_t                           message_;                       /* string to display                             */
        time_point_t                        start_time_;                    /* (of first display)                            */
        std::reference_wrapper<redraw_mask> mask_;                          /* reference to window's redraw mask             */
    };
    
    /* Struct for managing the status bar prompts */
    struct status_bar_prompt
    {
        std::string         text;                           /* editable string to display: should be ascii */
        std::size_t         cursor_pos{ 0 };                /* horizontal cursor position within line      */
    };
    
    
    /* Struct for managing the help bar */
    struct help_bar_entry
    {
        using text_str_ref  = std::reference_wrapper<const strings::text_string>;
        
        actions         action;
        text_str_ref    desc;
    };
    
    /* Struct for managing the help bar */
    struct help_bar_content
    {
        std::vector<help_bar_entry>     entries;                    /* entries to display in help bar                               */
        short                           min_width{ default_width }; /* minimum width of each entry in help bar                      */
        short                           max_width{ 0 };             /* maximum width (uncapped if less than min_width)              */
        bool                            last_is_bottom{ false };    /* true if last col of entries should be inserted from bottom   */
        
    private:
        static constexpr short          default_width{ 16 };
    };
    
    
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
    
    inline bool redraw_mask::has_mask(const mode m) const
    {
        return (value_ & m) == m;
    }
    
    
    /* Inline function implementations for sub_window */
    
    inline sub_window::sub_window(const coord size, const coord begin) :
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
    
    inline void sub_window::set_color(const color_type name, const bool term_has_color)
    {
        switch (name)
        {
            case color_type::standard:
                wattron(get(), A_NORMAL);
                break;
            
            case color_type::inverse:
                wattron(get(), A_REVERSE);
                break;
            
            case color_type::warning:
                if (term_has_color)
                    wattron(get(), A_BOLD | COLOR_PAIR(1));
                else
                    wattron(get(), A_BOLD | A_STANDOUT);
                break;
                
            case color_type::emphasis:
                if (term_has_color)
                    wattron(get(), A_BOLD | COLOR_PAIR(2));
                else
                    wattron(get(), A_BOLD);
                break;
        }
    }
    
    inline void sub_window::unset_color(const color_type name, const bool term_has_color)
    {
        switch (name)
        {
            case color_type::standard:
                wattroff(get(), A_NORMAL);
                break;
            
            case color_type::inverse:
                wattroff(get(), A_REVERSE);
                break;
            
            case color_type::warning:
                if (term_has_color)
                    wattroff(get(), A_BOLD | COLOR_PAIR(1));
                else
                    wattroff(get(), A_BOLD | A_STANDOUT);
                break;
            
            case color_type::emphasis:
                if (term_has_color)
                    wattroff(get(), A_BOLD | COLOR_PAIR(2));
                else
                    wattroff(get(), A_BOLD);
                break;
        }
    }
    
    inline void sub_window::set_default_color(const color_type name, const bool term_has_color)
    {
        switch (name)
        {
            case color_type::standard:
            case color_type::emphasis:
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
        else if (std::holds_alternative<strings::text_fstring_result>(message_))
            return std::get<strings::text_fstring_result>(message_).c_str();
        else
            return nullptr;
    }
    
    inline int status_bar_message::length() const
    {
        if (std::holds_alternative<text_str_ref>(message_))
            return std::get<text_str_ref>(message_).get().length();
        else if (std::holds_alternative<strings::text_fstring_result>(message_))
            return std::get<strings::text_fstring_result>(message_).length();
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
    
    inline void status_bar_message::set_message(const strings::text_string& msg)
    {
        message_ = std::cref(msg);
        error_ = false;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::set_message(strings::text_fstring_result&& msg)
    {
        message_ = std::move(msg);
        error_ = false;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::set_warning(const strings::text_string& msg)
    {
        message_ = std::cref(msg);
        error_ = true;
        draw_count_ = 0;
        mask_.get().add_mask(redraw_mask::RD_STATUS);
    }
    
    inline void status_bar_message::set_warning(strings::text_fstring_result&& msg)
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
