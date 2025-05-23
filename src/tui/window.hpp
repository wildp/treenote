// tui/window.hpp
//
// Copyright (C) 2025 Peter Wild
//
// This file is part of Treenote.
//
// Treenote is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Treenote is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Treenote.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include <csignal>
#include <deque>
#include <filesystem>
#include <locale>
#include <string>
#include <string_view>

#include "../core/editor.hpp"

#include "keymap.hpp"
#include "window_detail.hpp"

namespace treenote::tui
{
    extern volatile std::sig_atomic_t global_signal_status;
    inline constexpr std::string_view treenote_version_string{ "1.0" };
    
    class window
    {
        friend class detail::window_event_loop;
        
    public:
        static window create();
        
        window(const window&) = delete;
        window(window&&) = delete;
        window& operator=(const window&) = delete;
        window& operator=(window&&) = delete;
        ~window() = default;
        
        int operator()(std::deque<std::string>& filenames);
        
        inline static std::filesystem::path                     autosave_path{};
        inline static std::optional<core::editor::file_msg>       autosave_msg{};
    
    private:
        using tce = core::tree::cache_entry;
        
        window();
    
        void tree_open();
        bool tree_save(bool prompt);
        [[nodiscard]] bool tree_close();
        
        void help_screen();
        void display_tree_pos();
        void location_prompt();
        
        void undo();
        void redo();
        
        [[nodiscard]] actions get_help_action_from_mouse(coord mouse_pos) const;
    
        void draw_top();
        void draw_top_text_string(const strings::text_string& str);
        void draw_status();
        void draw_help();
        void draw_sidebar_line(int display_line, const tce& entry);
        void draw_content_current_line_no_wrap(int display_line, const tce& entry, int& cursor_x, bool draw_sidebar);
        void draw_content_non_current_line_no_wrap(int display_line, const tce& entry, bool draw_sidebar);
        void draw_content_no_wrap(coord& default_cursor_pos);
        void draw_content_selective_no_wrap(coord& default_cursor_pos);
        void draw_content_help_mode_no_wrap(const keymap::bindings_t& bindings);
    
        void update_cursor_pos(coord& default_cursor_pos);
        void update_screen();
        void update_screen_help_mode(const keymap::bindings_t& bindings);
        void update_viewport_pos(std::size_t lines_below = 0);
        void update_viewport_cursor_pos();
        void update_viewport_clamp_lower();
        void update_viewport_center_line();
        void update_window_sizes(bool clamp_line_start);

        [[no_unique_address]] detail::defer_endwin _defer_endwin;
        
        std::locale                 new_locale_{ "" };
        std::filesystem::path       current_filename_{ "" };
        core::editor                current_file_;
        coord                       screen_dimensions_{ .y = 0, .x = 0 };
        
        std::size_t                 line_start_y_{ 0 };
        int                         previous_cursor_y{ 0 };
        
        unsigned char               help_height_{ 2 };
        unsigned char               sidebar_width_{ 2 };
        bool                        term_has_color_ { false };
        
        detail::redraw_mask         screen_redraw_;
        
        detail::sub_window          sub_win_top_;
        detail::sub_window          sub_win_status_;
        detail::sub_window          sub_win_help_;
        detail::sub_window          sub_win_content_;
        detail::sub_window          sub_win_sidebar_;
        
        detail::status_bar_mode     status_mode_{ detail::status_bar_mode::default_mode };
        detail::status_bar_message  status_msg_;
        detail::status_bar_prompt   prompt_info_;
        detail::help_bar_content    help_info_;
        
        keymap                      keymap_;
    };
}