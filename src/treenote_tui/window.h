// window.h

#pragma once

#include <chrono>
#include <deque>
#include <filesystem>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../treenote/note.h"
#include "window_detail.hpp"
#include "window_help.hpp"
#include "keymap.h"

namespace treenote_tui
{
    inline constexpr std::string_view treenote_version_string{ "0.1" };
    
    class window
    {
    public:
        static window create();
        
        window(const window&) = delete;
        window(window&&) = delete;
        window& operator=(const window&) = delete;
        window& operator=(window&&) = delete;
        
        ~window();
        
        int operator()(std::deque<std::string>& filenames);
    
    private:
        using tce = treenote::tree::cache_entry;
        
        window();
    
        void tree_open();
        bool tree_save(bool prompt);
        [[nodiscard]] bool tree_close();
    
        void display_tree_pos();
        void undo();
        void redo();
    
        void set_color(detail::color_type name, detail::sub_window& sw) const;
        void unset_color(detail::color_type name, detail::sub_window& sw) const;
        void set_default_color(detail::color_type name, detail::sub_window& sw) const;
    
        void draw_top();
        void draw_status();
        void draw_help();
        void draw_content_current_line_no_wrap(int display_line, const tce& entry, int& cursor_x);
        void draw_content_non_current_line_no_wrap(int display_line, const tce& entry);
        void draw_content(coord& default_cursor_pos);
        void draw_content_selective(coord& default_cursor_pos);
    
        void update_cursor_pos(coord& default_cursor_pos);
        void update_screen();
        void update_viewport_pos(std::size_t lines_below = 0);
        void update_viewport_cursor_pos();
        void update_viewport_clamp_lower();
        void update_window_sizes();
        
        
        std::locale                 new_locale_{ "" };
        std::filesystem::path       current_filename_{ "" };
        treenote::note              current_file_;
        coord                       screen_dimensions_{ .y = 0, .x = 0 };
        
        detail::sub_window          sub_win_top_;
        detail::sub_window          sub_win_status_;
        detail::sub_window          sub_win_help_;
        detail::sub_window          sub_win_content_;
        detail::sub_window          sub_win_lineno_;                // todo: implement this subwindow later
    
        detail::status_bar_mode     status_mode_{ detail::status_bar_mode::DEFAULT };
        detail::status_bar_message  status_msg_;
        detail::status_bar_prompt   prompt_info_;
        detail::display_strings     strings_;
        
        detail::redraw_mask         screen_redraw_;
        
        keymap                      keymap_;
        std::size_t                 line_start_y_{ 0 };
        int                         previous_cursor_y{ 0 };
        bool                        show_help_bar_: 1 { true };
        bool                        term_has_color: 1 { false };
        bool                        word_wrap_enabled: 1 { false };
    };
}

using treenote_tui::window;