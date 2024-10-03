// window.h

#pragma once

#include <csignal>
#include <deque>
#include <filesystem>
#include <locale>
#include <string>
#include <string_view>

#include "../treenote/note.h"
#include "window_detail.hpp"
#include "keymap.h"

namespace treenote_tui
{
    extern volatile std::sig_atomic_t global_signal_status;
    inline constexpr std::string_view treenote_version_string{ "0.1" };
    
    class window
    {
        friend class detail::window_event_loop;
        
    public:
        static window create();
        
        window(const window&) = delete;
        window(window&&) = delete;
        window& operator=(const window&) = delete;
        window& operator=(window&&) = delete;
        
        ~window();
        
        int operator()(std::deque<std::string>& filenames);
        
        inline static std::filesystem::path                     autosave_path{};
        inline static std::optional<treenote::note::file_msg>   autosave_msg{};
    
    private:
        using tce = treenote::tree::cache_entry;
        
        window();
    
        void tree_open();
        bool tree_save(bool prompt);
        [[nodiscard]] bool tree_close();
        
        void help_screen();
        void display_tree_pos();
        void location_prompt();
        
        void undo();
        void redo();
        
        actions get_help_action_from_mouse(coord mouse_pos);
    
        void draw_top();
        void draw_top_text_string(const strings::text_string& str);
        void draw_status();
        void draw_help();
        void draw_content_current_line_no_wrap(int display_line, const tce& entry, int& cursor_x);
        void draw_content_non_current_line_no_wrap(int display_line, const tce& entry);
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
        void update_window_sizes();
        
        
        std::locale                 new_locale_{ "" };
        std::filesystem::path       current_filename_{ "" };
        treenote::note              current_file_;
        coord                       screen_dimensions_{ .y = 0, .x = 0 };
        
        std::size_t                 line_start_y_{ 0 };
        int                         previous_cursor_y{ 0 };
        
        unsigned char               help_height_{ 2 };
        bool                        term_has_color_ { false };
        
        detail::redraw_mask         screen_redraw_;
        
        detail::sub_window          sub_win_top_;
        detail::sub_window          sub_win_status_;
        detail::sub_window          sub_win_help_;
        detail::sub_window          sub_win_content_;
        detail::sub_window          sub_win_lineno_;                // todo: implement this subwindow later
        
        detail::status_bar_mode     status_mode_{ detail::status_bar_mode::default_mode };
        detail::status_bar_message  status_msg_;
        detail::status_bar_prompt   prompt_info_;
        detail::help_bar_content    help_info_;
        
        keymap                      keymap_;
    };
}