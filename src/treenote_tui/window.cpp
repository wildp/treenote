// window.cpp

#include "window.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "read_helper.h"

#include "../treenote/legacy_tree_string.h"

// todo: add system to reject interacting with files with line length of over std::short_max
// todo: write help page introduction

namespace treenote_tui
{
    volatile std::sig_atomic_t global_signal_status;
    
    namespace
    {
        void signal_handler(int signal)
        {
            global_signal_status = signal;
        }
    }
}

namespace treenote_tui::detail
{
    class window_event_loop
    {
        window*             win_;
        char_read_helper    crh_;

    public:
        window_event_loop(window& window) :
                win_{ &window }
        {
        }
        
        char_read_helper& crh()
        {
            return crh_;
        }
        
        template<std::invocable<actions, bool&> F1, typename F2, std::invocable<MEVENT&> F3, std::invocable<> F4>
        requires (std::invocable<F2, std::string&> or std::invocable<F2>)
        void operator()(const keymap::map_t& local_keymap,
                        F1 action_handler,
                        F2 input_handler,
                        F3 mouse_handler,
                        F4 common)
        {
            for (bool exit{ false }; not exit;)
            {
                crh_.extract_char();
                
                if (global_signal_status)
                    return;

                /* act on input */
                if (crh_.is_resize())
                {
                    /* update overall window size information */

                    win_->update_window_sizes();
                    win_->status_msg_.force_clear();
                }
                else if (crh_.is_mouse())
                {
                    for (MEVENT mouse; getmouse(&mouse) == OK;)
                    {
                        if (coord pos{ .y = mouse.y, .x = mouse.x }; wmouse_trafo(*(win_->sub_win_help_), &(pos.y), &(pos.x), false))
                        {
                            if (mouse.bstate & BUTTON1_RELEASED)
                                std::invoke(action_handler, win_->get_help_action_from_mouse(pos), exit);
                        }
                        else
                        {
                            std::invoke(mouse_handler, mouse);
                        }
                    }
                }
                else if (std::invocable<F2> or crh_.is_command())
                {
                    /* command key sent: execute instruction */

                    crh_.extract_second_char();
                    std::invoke(action_handler, crh_.get_action(local_keymap), exit);
                }
                else if constexpr (std::invocable<F2, std::string&>)
                {
                    /* key is readable input: send onwards */

                    std::string inserted{ crh_.value_string() };
                    crh_.extract_more_readable_chars(inserted);
                    std::invoke(input_handler, inserted);
                }
                
                std::invoke(common);
            }
            
            crh_.clear();
            if (crh_.is_resize())
            {
                /* update overall window size information */
                win_->update_window_sizes();
            }
        }
    };
}

namespace treenote_tui
{
    static constexpr std::string_view program_name_text{ "treenote" };
    static constexpr int program_name_ver_text_len{ std::saturate_cast<int>(program_name_text.size() + 1 + treenote_version_string.size()) };
    static constexpr int pad_size{ 2 };


    /* Constructors and Destructors, and related funcs */
    
    window::window() :
            status_msg_{ screen_redraw_ }
    {
        std::locale::global(new_locale_);
        
        initscr();
        raw();          // disable keyboard interrupts
        nonl();         // disable conversion of enter to new line
        noecho();       // do not echo keyboard input
        curs_set(1);    // show cursor
        timeout(100);
        
        intrflush(stdscr, false);
        keypad(stdscr, true);
        meta(stdscr, true);
        use_extended_names(true);
        
        keymap_ = keymap::make_default();
        help_info_ = keymap::make_editor_help_bar();
        
        update_window_sizes();
        
        if (has_colors() != FALSE)
        {
            term_has_color_ = true;
            start_color();
            use_default_colors();
            init_pair(1, COLOR_WHITE, COLOR_RED);
            init_pair(2, COLOR_CYAN, -1);
            bkgd(COLOR_PAIR(0) | ' ');
        }
        
        mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED | REPORT_MOUSE_POSITION, nullptr);
        mouseinterval(0);
        
        std::signal(SIGHUP, signal_handler);
        std::signal(SIGTERM, signal_handler);
        std::signal(SIGQUIT, signal_handler);
    }
    
    window::~window()
    {
        sub_win_top_.~sub_window();
        sub_win_status_.~sub_window();
        sub_win_help_.~sub_window();
        sub_win_content_.~sub_window();
        sub_win_lineno_.~sub_window();
        endwin();
    }
    
    window window::create()
    {
        static bool window_exists{ false };
        
        if (not window_exists)
        {
            window_exists = true;
            return window{};
        }
        else
        {
            throw std::logic_error("Cannot create more than 1 main window");
        }
    }


    /* Editor related operations */

    void window::tree_open()
    {
        using file_msg = treenote::note::file_msg;
        
        if (not current_filename_.empty())
        {
            auto load_info{ current_file_.load_file(current_filename_) };
            
            switch (load_info.first)
            {
                case file_msg::none:
                    status_msg_.set_message(strings::read_success(load_info.second.node_count, load_info.second.line_count));
                    break;
                case file_msg::does_not_exist:
                    status_msg_.set_message(strings::new_file_msg);
                    break;
                case file_msg::is_unwritable:
                    status_msg_.set_warning(strings::file_is_unwrit(current_filename_));
                    break;
                case file_msg::is_directory:
                    status_msg_.set_warning(strings::error_reading(current_filename_, strings::is_directory.str_view()));
                    break;
                case file_msg::is_device_file:
                    status_msg_.set_warning(strings::error_reading(current_filename_, strings::is_device_file.str_view()));
                    break;
                case file_msg::is_invalid_file:
                    status_msg_.set_warning(strings::error_reading(current_filename_, strings::invalid_file.str_view()));
                    break;
                case file_msg::is_unreadable:
                    status_msg_.set_warning(strings::error_reading(current_filename_, strings::permission_denied.str_view()));
                    break;
                case file_msg::unknown_error:
                    status_msg_.set_warning(strings::error_reading(current_filename_.string(), strings::unknown_error.str_view()));
                    break;
            }
        }
        else
        {
            current_file_.make_empty();
            status_msg_.set_message(strings::new_file_msg);
        }
    }
    
    /* returns true if successful and false otherwise */
    bool window::tree_save(bool prompt)
    {
        using detail::redraw_mask;
        using detail::status_bar_mode;
        using file_msg = treenote::note::file_msg;
        
        /* get filename if necessary */
        
        if (prompt or current_filename_.empty())
        {
            auto saved_help_info{ std::move(help_info_) };
            
            status_mode_ = status_bar_mode::prompt_filename;
            help_info_ = keymap::make_filename_editor_help_bar();
            screen_redraw_.add_mask(redraw_mask::RD_STATUS, redraw_mask::RD_HELP);
            bool cancelled{ false };
            
            treenote::legacy_tree_string line_editor{ current_filename_.c_str() };
            
            prompt_info_.text = line_editor.to_str(0);
            prompt_info_.cursor_pos = line_editor.line_length(0);
            update_screen();
            
            detail::window_event_loop wel{ *this };
            wel(keymap_.make_filename_editor_keymap(),
                [&](actions action, bool& exit)
                {
                    switch (action)
                    {
                        case actions::newline:
                            /* save file to filename */
                            exit = true;
                            break;
                        
                        case actions::backspace:
                            if (prompt_info_.cursor_pos > 0)
                            {
                                std::size_t cursor_dec_amt{ 0 };
                                line_editor.delete_char_before(0, prompt_info_.cursor_pos, cursor_dec_amt);
                                
                                if (cursor_dec_amt > prompt_info_.cursor_pos)
                                    prompt_info_.cursor_pos = 0;
                                else
                                    prompt_info_.cursor_pos -= cursor_dec_amt;
                                
                                prompt_info_.text = line_editor.to_str(0);
                                screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                            }
                            break;
                        
                        case actions::delete_char:
                            if (prompt_info_.cursor_pos < line_editor.line_length(0))
                            {
                                line_editor.delete_char_current(0, prompt_info_.cursor_pos);
                                
                                prompt_info_.text = line_editor.to_str(0);
                                screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                            }
                            break;
                        
                        case actions::cursor_left:
                            if (prompt_info_.cursor_pos > 0)
                                --prompt_info_.cursor_pos;
                            
                            /* redraw only if possible for a horizontal scroll */
                            if (prompt_info_.text.size() > static_cast<std::size_t>(std::max(0, (sub_win_status_.size().x - 2 - strings::file_prompt.length()))))
                                screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                            break;
                        
                        case actions::cursor_right:
                            if (prompt_info_.cursor_pos < line_editor.line_length(0))
                                ++prompt_info_.cursor_pos;
                            
                            /* redraw only if possible for a horizontal scroll */
                            if (prompt_info_.text.size() > static_cast<std::size_t>(std::max(0, (sub_win_status_.size().x - 2 - strings::file_prompt.length()))))
                                screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                            break;
                        
                        case actions::prompt_cancel:
                            /* cancel save operation */
                            exit = true;
                            cancelled = true;
                            break;
                        
                        default:
                            break;
                    }
                },
                [&](std::string& input)
                {
                    std::size_t cursor_inc_amt{ 0 };
                    line_editor.insert_str(0, prompt_info_.cursor_pos, input, cursor_inc_amt);
                    
                    prompt_info_.cursor_pos += cursor_inc_amt;
                    prompt_info_.text = line_editor.to_str(0);
                    screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                },
                [&](MEVENT& mouse)
                {
                    coord mouse_pos{ .y = mouse.y, .x = mouse.x };
                    
                    if (wmouse_trafo(*sub_win_status_, &(mouse_pos.y), &(mouse_pos.x), false))
                    {
                        if (mouse.bstate & BUTTON1_RELEASED)
                        {
                            /* copied from draw_status */
                            
                            const auto& prompt{ (status_mode_ == status_bar_mode::prompt_filename) ? strings::file_prompt : strings::goto_prompt };
                            
                            int cursor_display_x{ std::saturate_cast<int>(prompt_info_.cursor_pos) };
                            int start_of_line_index{ 0 };
                            
                            const int line_start_pos{ std::max(std::min(prompt.length() + 2, sub_win_status_.size().x - 4), 2) };
                            const int space_available{ sub_win_status_.size().x - line_start_pos };
                            const int cursor_limit{ space_available - 2 };
                            const int page_offset{ space_available - 2 };
                            
                            while (cursor_display_x > cursor_limit)
                            {
                                start_of_line_index += page_offset;
                                cursor_display_x -= page_offset;
                            }
                            
                            /* end copying */
                            
                            prompt_info_.cursor_pos = std::saturate_cast<std::size_t>(mouse_pos.x + start_of_line_index - line_start_pos);
                            prompt_info_.cursor_pos = std::min(line_editor.line_length(0), prompt_info_.cursor_pos);
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                        }
                    }
                },
                [&]() { update_screen(); }
            );
            
            status_mode_ = status_bar_mode::default_mode;
            help_info_ = std::move(saved_help_info);
            screen_redraw_.add_mask(redraw_mask::RD_TOP, redraw_mask::RD_STATUS, redraw_mask::RD_HELP);
            
            if (cancelled)
            {
                status_msg_.set_message(strings::cancelled);
                return false;
            }
            
            current_filename_ = prompt_info_.text;
        }
        
        /* now we actually save the file */
        
        auto save_info{ current_file_.save_file(current_filename_) };
        bool success{ false };
        
        /* then we display stats / warnings */
        
        switch (save_info.first)
        {
            case file_msg::none:
            case file_msg::does_not_exist:
            case file_msg::is_unreadable:
                status_msg_.set_message(strings::write_success(save_info.second.node_count, save_info.second.line_count));
                success = true;
                break;
            case file_msg::is_directory:
                status_msg_.set_warning(strings::error_writing(current_filename_, strings::is_directory.str_view()));
                break;
            case file_msg::is_device_file:
            case file_msg::is_invalid_file:
                status_msg_.set_warning(strings::error_writing(current_filename_, strings::invalid_file.str_view()));
                break;
            case file_msg::is_unwritable:
                status_msg_.set_warning(strings::error_writing(current_filename_, strings::permission_denied.str_view()));
                break;
            case file_msg::unknown_error:
                status_msg_.set_warning(strings::error_writing(current_filename_, strings::unknown_error.str_view()));
                break;
        }
        
        return success;
    }
    
    /* returns true if successful (i.e. not cancelled) and false otherwise */
    bool window::tree_close()
    {
        using detail::redraw_mask;
        using detail::status_bar_mode;
        
        if (current_file_.modified())
        {
            auto saved_help_info{ std::move(help_info_) };
            
            status_mode_ = status_bar_mode::prompt_close;
            help_info_ = keymap::make_quit_prompt_help_bar();
            screen_redraw_.add_mask(redraw_mask::RD_STATUS, redraw_mask::RD_HELP);
            update_screen();
            
            std::optional<bool> save{};
            
            detail::window_event_loop wel{ *this };
            wel(keymap_.make_quit_prompt_keymap(),
                [&](actions action, bool& exit)
                {
                    switch (action)
                    {
                        case actions::prompt_cancel:
                            /* cancel closing file */
                            exit = true;
                            break;
                            
                        case actions::prompt_yes:
                            /* save and quit */
                            save = true;
                            exit = true;
                            break;
                            
                        case actions::prompt_no:
                            /* save without quitting */
                            save = false;
                            exit = true;
                            break;
                            
                        default:
                            break;
                    }
                },
                [&]() {}, /* always treat input as command */
                [&](MEVENT& /* mouse */) {},
                [&]() {}
            );
            
            status_mode_ = status_bar_mode::default_mode;
            help_info_ = std::move(saved_help_info);
            screen_redraw_.add_mask(redraw_mask::RD_STATUS, redraw_mask::RD_HELP);
            
            if (not save.has_value())
            {
                /* closing has been cancelled; don't exit */
                status_msg_.set_message(strings::cancelled);
                return false;
            }
            else if (*save)
            {
                /* save tree; an extra prompt will be given if necessary */
                return tree_save(false);
            }
        }
        
        current_file_.close_file();
        
        /* no need to save, we can exit */
        return true;
    }
    
    void window::help_screen()
    {
        using detail::redraw_mask;
        using detail::status_bar_mode;
        
        static constexpr std::size_t help_line_length{ strings::help_strings.size() + 1 };
        
        auto saved_help_info{ std::move(help_info_) };
        const auto saved_line_start{ line_start_y_ };
        help_info_ = keymap::make_help_screen_help_bar();
        const keymap::bindings_t bindings{ keymap_.make_key_bindings() };
        
        screen_redraw_.set_all();
        update_screen_help_mode(bindings);
        
        detail::window_event_loop wel{ *this };
        wel(keymap_.make_help_screen_keymap(),
            [&](actions action, bool& exit)
            {
                switch (action)
                {
                    case actions::close_tree:
                        /* close help screen */
                        exit = true;
                        break;
                    
                    case actions::cursor_up:
                    case actions::scroll_up:
                        /* scroll up one line */
                        line_start_y_ = std::sub_sat(line_start_y_, 1uz);
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        break;
                    
                    case actions::cursor_down:
                    case actions::scroll_down:
                        /* scroll down one line */
                        line_start_y_ = std::min(line_start_y_ + 1uz, std::sub_sat<std::size_t>(help_line_length, sub_win_content_.size().y));
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        break;
                    
                    case actions::page_up:
                        /* scroll up one page */
                        line_start_y_ = std::sub_sat<std::size_t>(line_start_y_, sub_win_content_.size().y);
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        break;
                    
                    case actions::page_down:
                        /* scroll down one page */
                        line_start_y_ = std::min(line_start_y_ + sub_win_content_.size().y, std::sub_sat<std::size_t>(help_line_length, sub_win_content_.size().y));
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        break;
                    
                    case actions::cursor_sof:
                        /* scroll up one page */
                        line_start_y_ = 0;
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        break;
                    
                    case actions::cursor_eof:
                        /* scroll down one page */
                        line_start_y_ = std::sub_sat<std::size_t>(help_line_length, sub_win_content_.size().y);
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        break;
                    
                    case actions::center_view:
                        /* redraw everything */
                        screen_redraw_.set_all();
                        break;
                    
                    default:
                        break;
                }    
            },
            [&](std::string& /* inserted */) {},
            [&](MEVENT& mouse)
            {
                coord mouse_pos{ .y = mouse.y, .x = mouse.x };
                
                if (wmouse_trafo(*sub_win_content_, &(mouse_pos.y), &(mouse_pos.x), false))
                {
                    if (mouse.bstate & BUTTON4_PRESSED and line_start_y_ > 0)
                    {
                        line_start_y_ = std::sub_sat(line_start_y_, 2uz);
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                    }
                    
                    if (mouse.bstate & BUTTON5_PRESSED and line_start_y_ + sub_win_content_.size().y < help_line_length)
                    {
                        line_start_y_ = std::min(line_start_y_ + 2uz, std::sub_sat<std::size_t>(help_line_length, sub_win_content_.size().y));
                        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                    }
                }
            },
            [&]() { update_screen_help_mode(bindings); }
        );
        
        status_mode_ = status_bar_mode::default_mode;
        help_info_ = std::move(saved_help_info);
        line_start_y_ = saved_line_start;
        screen_redraw_.set_all();
    }

    void window::display_tree_pos()
    {
        const auto& index{ current_file_.cursor_current_index() };
        const auto& line{ current_file_.cursor_current_line() };
        const auto max_x{ current_file_.cursor_max_x() };
        const auto max_lines{ current_file_.cursor_max_line() };

        std::stringstream node_idx;
        
        if (std::ranges::size(index) > 1)
            std::ranges::for_each(std::ranges::begin(index), std::ranges::end(index) - 1, [&](std::size_t i){ node_idx << i + 1 << '-'; });

        if (std::ranges::size(index) > 0)
            node_idx << treenote::last_index_of(index) + 1;
        
        status_msg_.set_message(strings::cursor_pos_msg(node_idx.str(), line + 1, max_lines, current_file_.cursor_x() + 1, max_x + 1));
    }
    
    void window::location_prompt()
    {
        /* mostly copied from tree_save */
        
        using detail::redraw_mask;
        using detail::status_bar_mode;
        
        auto saved_help_info{ std::move(help_info_) };
        
        status_mode_ = status_bar_mode::prompt_location;
        help_info_ = keymap::make_goto_editor_help_bar();
        screen_redraw_.add_mask(redraw_mask::RD_STATUS, redraw_mask::RD_HELP);
        bool cancelled{ false };
        
        treenote::legacy_tree_string line_editor{ "" };
        
        prompt_info_.text = line_editor.to_str(0);
        prompt_info_.cursor_pos = line_editor.line_length(0);
        update_screen();
        
        /* get location from prompt */
        
        detail::window_event_loop wel{ *this };
        wel(keymap_.make_goto_editor_keymap(),
            [&](actions action, bool& exit)
            {
                switch (action)
                {
                    case actions::newline:
                        /* use location */
                        exit = true;
                        break;
                    
                    case actions::backspace:
                        if (prompt_info_.cursor_pos > 0)
                        {
                            std::size_t cursor_dec_amt{ 0 };
                            line_editor.delete_char_before(0, prompt_info_.cursor_pos, cursor_dec_amt);
                            
                            if (cursor_dec_amt > prompt_info_.cursor_pos)
                                prompt_info_.cursor_pos = 0;
                            else
                                prompt_info_.cursor_pos -= cursor_dec_amt;
                            
                            prompt_info_.text = line_editor.to_str(0);
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                        }
                        break;
                    
                    case actions::delete_char:
                        if (prompt_info_.cursor_pos < line_editor.line_length(0))
                        {
                            line_editor.delete_char_current(0, prompt_info_.cursor_pos);
                            
                            prompt_info_.text = line_editor.to_str(0);
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                        }
                        break;
                    
                    case actions::cursor_left:
                        if (prompt_info_.cursor_pos > 0)
                            --prompt_info_.cursor_pos;
                        
                        /* redraw only if possible for a horizontal scroll */
                        if (prompt_info_.text.size() > static_cast<std::size_t>(std::max(0, (sub_win_status_.size().x - 2 - strings::file_prompt.length()))))
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                        break;
                    
                    case actions::cursor_right:
                        if (prompt_info_.cursor_pos < line_editor.line_length(0))
                            ++prompt_info_.cursor_pos;
                        
                        /* redraw only if possible for a horizontal scroll */
                        if (prompt_info_.text.size() > static_cast<std::size_t>(std::max(0, (sub_win_status_.size().x - 2 - strings::file_prompt.length()))))
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                        break;
                    
                    case actions::prompt_cancel:
                        /* cancel save operation */
                        exit = true;
                        cancelled = true;
                        break;
                    
                    default:
                        break;
                }
            },
            [&](std::string& input)
            {
                std::size_t cursor_inc_amt{ 0 };
                line_editor.insert_str(0, prompt_info_.cursor_pos, input, cursor_inc_amt);
                
                prompt_info_.cursor_pos += cursor_inc_amt;
                prompt_info_.text = line_editor.to_str(0);
                screen_redraw_.add_mask(redraw_mask::RD_STATUS);
            },
            [&](MEVENT& mouse) /* NOTE: copied from mouse handler for tree_save */
            {
                coord mouse_pos{ .y = mouse.y, .x = mouse.x };
                
                if (wmouse_trafo(*sub_win_status_, &(mouse_pos.y), &(mouse_pos.x), false))
                {
                    if (mouse.bstate & BUTTON1_RELEASED)
                    {
                        /* copied from draw_status */
                        
                        const auto& prompt{ (status_mode_ == status_bar_mode::prompt_filename) ? strings::file_prompt : strings::goto_prompt };
                        
                        int cursor_display_x{ std::saturate_cast<int>(prompt_info_.cursor_pos) };
                        int start_of_line_index{ 0 };
                        
                        const int line_start_pos{ std::max(std::min(prompt.length() + 2, sub_win_status_.size().x - 4), 2) };
                        const int space_available{ sub_win_status_.size().x - line_start_pos };
                        const int cursor_limit{ space_available - 2 };
                        const int page_offset{ space_available - 2 };
                        
                        while (cursor_display_x > cursor_limit)
                        {
                            start_of_line_index += page_offset;
                            cursor_display_x -= page_offset;
                        }
                        
                        /* end copying */
                        
                        prompt_info_.cursor_pos = std::saturate_cast<std::size_t>(mouse_pos.x + start_of_line_index - line_start_pos);
                        prompt_info_.cursor_pos = std::min(line_editor.line_length(0), prompt_info_.cursor_pos);
                        screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                    }
                }
            },
            [&]() { update_screen(); }
        );
        
        status_mode_ = status_bar_mode::default_mode;
        help_info_ = std::move(saved_help_info);
        screen_redraw_.add_mask(redraw_mask::RD_STATUS, redraw_mask::RD_HELP);
        
        if (cancelled)
        {
            status_msg_.set_message(strings::cancelled);
            return;
        }
        
        /* now we parse the input */
        
        const std::string input{ line_editor.to_str(0) };
        auto input_view{ input | std::views::split(',') };
        auto input_view_size{ std::ranges::distance(std::ranges::begin(input_view), std::ranges::end(input_view)) };
        
        if (input_view_size == 0 or input_view_size > 3)
        {
            /* invalid number of input entries */
            status_msg_.set_message(strings::invalid_location);
            return;
        }
        
        auto tree_index_it{ std::ranges::begin(input_view) };
        bool valid{ true };
        
        auto fn{ [&valid](const auto sv) -> std::size_t {
                auto outer_result{ sv | std::views::split(' ')
                        | std::views::filter(
                                [](const auto& r)
                                {
                                    return (std::ranges::begin(r) != std::ranges::end(r));
                                })
                        | std::views::transform(
                                [&valid](const auto sv2) -> std::size_t
                                {
                                    std::size_t result{ 0 };
                                    for (const char c : sv2)
                                    {
                                        if ('0' <= c and c <= '9')
                                            result = (result * 10) + (c - '0');
                                        else
                                            valid = false;
                                    }
                                    return result;
                                })
                         | std::ranges::to<std::vector>() };
                     
                     if (outer_result.size() == 1)
                         return std::sub_sat(outer_result[0], 1uz);
                     
                     valid = false;
                     return 0;
                 } };
        
        const auto index{ *(tree_index_it++) | std::views::split('-') | std::views::transform(fn) | std::ranges::to<std::vector>() };
        const auto line{ (input_view_size > 1) ? fn(*(tree_index_it++)) : 0 };
        const auto col{ (input_view_size > 2) ? fn(*(tree_index_it++)) : 0 };
        
        if (not valid)
        {
            /* invalid input entries */
            status_msg_.set_message(strings::invalid_location);
            return;
        }
        
        /* actually move the cursor */
        
        current_file_.cursor_go_to(index, line, col);
        update_viewport_center_line();
    }
    
    void window::undo()
    {
        using detail::redraw_mask;
        using treenote::cmd_names;
        
        switch (current_file_.undo())
        {
            case cmd_names::error:
                status_msg_.set_warning(strings::nothing_undo);
                break;
            case cmd_names::move_node:
                status_msg_.set_message(strings::undo_move_node);
                break;
            case cmd_names::insert_node:
                status_msg_.set_message(strings::undo_ins_node);
                break;
            case cmd_names::delete_node:
                status_msg_.set_message(strings::undo_del_node);
                break;
            case cmd_names::cut_node:
                status_msg_.set_message(strings::undo_cut_node);
                break;
            case cmd_names::paste_node:
                status_msg_.set_message(strings::undo_paste_node);
                break;
            case cmd_names::insert_text:
                status_msg_.set_message(strings::undo_ins_text);
                break;
            case cmd_names::delete_text:
                status_msg_.set_message(strings::undo_del_text);
                break;
            case cmd_names::line_break:
                status_msg_.set_message(strings::undo_line_br);
                break;
            case cmd_names::line_join:
                status_msg_.set_message(strings::undo_line_jn);
                break;
            case cmd_names::none:
                /* don't display a message */
                break;
        }
        
        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        update_viewport_clamp_lower();
    }
    
    void window::redo()
    {
        using detail::redraw_mask;
        using treenote::cmd_names;
        
        switch (current_file_.redo())
        {
            case cmd_names::error:
                status_msg_.set_warning(strings::nothing_redo);
                break;
            case cmd_names::move_node:
                status_msg_.set_message(strings::redo_move_node);
                break;
            case cmd_names::insert_node:
                status_msg_.set_message(strings::redo_ins_node);
                break;
            case cmd_names::delete_node:
                status_msg_.set_message(strings::redo_del_node);
                break;
            case cmd_names::cut_node:
                status_msg_.set_message(strings::redo_cut_node);
                break;
            case cmd_names::paste_node:
                status_msg_.set_message(strings::redo_paste_node);
                break;
            case cmd_names::insert_text:
                status_msg_.set_message(strings::redo_ins_text);
                break;
            case cmd_names::delete_text:
                status_msg_.set_message(strings::redo_del_text);
                break;
            case cmd_names::line_break:
                status_msg_.set_message(strings::redo_line_br);
                break;
            case cmd_names::line_join:
                status_msg_.set_message(strings::redo_line_jn);
                break;
            case cmd_names::none:
                /* don't display a message */
                break;
        }
        
        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        update_viewport_pos();
    }

    /* Input related functions */
    
    actions window::get_help_action_from_mouse(coord mouse_pos)
    {
        const int size{ std::saturate_cast<int>(help_info_.entries.size()) };
        const int width{ sub_win_help_.size().x };
        const int min{ help_info_.min_width };
        const int max{ help_info_.max_width };
        
        const int rows{ sub_win_help_.size().y };
        const int cols{ std::max(1, std::min(width / min, (size + rows - 1) / rows)) };
        
        const int spacing{ (min > max) ? std::max(min, width / cols) : std::clamp(width / cols, min, max) };
        const int slack{ (min > max) ? width % spacing : 0 };
        
        for (int i{ 0 }, c{ 0 }; c < cols; ++c)
        {
            for (int r{ 0 }; r < rows and i < size; ++r, ++i)
            {
                if (help_info_.last_is_bottom and r == 0 and i + 1 == size)
                {
                    /* draw last key entry on bottom row */
                    r = rows - 1;
                }
                
                if (r == mouse_pos.y)
                {
                    const auto& entry{ help_info_.entries.at(i) };
                    const int x_min{ (spacing * c) + ((slack * c) / cols) };
                    const int x_max{ std::min(width, (spacing * (c + 1)) + ((slack * (c + 1)) / cols)) };
                    
                    if (x_min <= mouse_pos.x and mouse_pos.x < x_max)
                    {
                        /* success: action found! */
                        return entry.action;
                    }
                }
            }
        }
        
        return actions::unknown;
    }

    /* Drawing functions */
    
    /* Updates the top bar. This function should be called everytime the file name changes,
     * when the window is resized horizontally, and when the modification status changes.
     * Called via window::update_window();
     * doupdate() must be called after calling this function.                               */
    void window::draw_top()
    {
        using detail::color_type;
        
        if (not sub_win_top_)
            return;
        
        wclear(*sub_win_top_);
        sub_win_top_.set_default_color(color_type::inverse, term_has_color_);
        
        std::string filename_str{ current_filename_.string() };
        
        if (filename_str.empty())
            filename_str = strings::empty_file.c_str();
        
        const bool show_modified{ current_file_.modified() };
        bool use_padding{ false };
        
        const int filename_len{ std::saturate_cast<int>(filename_str.length()) };
        const int line_length{ sub_win_top_.size().x };
        int filename_x_pos{ 0 };
        
        if (line_length >= filename_len + 2 * (pad_size + 1) + strings::modified.length() + program_name_ver_text_len)
        {
            /* line is wide enough to show everything */
            filename_x_pos = program_name_ver_text_len +
                             std::max(0, (line_length - filename_len - program_name_ver_text_len - strings::modified.length()) / 2);
            mvwprintw(*sub_win_top_, 0, pad_size, "%s %s", program_name_text.data(), treenote_version_string.data());
            use_padding = true;
        }
        else if (line_length >= filename_len + 2 * pad_size + strings::modified.length() + 1)
        {
            /* not enough space to show name of program, but enough to show entirety of filename */
            filename_x_pos = std::max(0, (line_length - filename_len - strings::modified.length()) / 2);
            mvwprintw(*sub_win_top_, 0, filename_x_pos, "%s", filename_str.c_str());
            use_padding = true;
        }
        else if (not show_modified)
        {
            /* maybe not enough space for filename, but modified does not need to be shown */
            if (line_length < filename_len)
            {
                /* remove characters from the start of filename_str to fit in line */
                const std::ptrdiff_t offset{ std::clamp(filename_len + 3 - line_length, 0, filename_len) };
                std::string tmp{ "..." };
                if (offset < filename_len)
                {
                    tmp.insert(std::ranges::cend(tmp), std::ranges::cbegin(filename_str) + offset, std::ranges::cend(filename_str));
                    filename_str = tmp;
                }
                else
                {
                    /* none of filename shown, hide dots */
                    filename_str = "";
                }
            }
            else
            {
                /* enough space for filename: center in absence of space for the modified text */
                filename_x_pos = std::max(0, (line_length - filename_len) / 2);
            }
        }
        else if (line_length < filename_len + (strings::modified.length() + 1))
        {
            /* remove characters from the start of filename_str to fit in line (with the modified text shown) */
            const std::ptrdiff_t offset{ std::clamp(filename_len + 3 - line_length + (strings::modified.length() + 1), 0, filename_len) };
            std::string tmp = std::string{ "..." };
            if (offset < filename_len)
            {
                tmp.insert(std::ranges::cend(tmp), std::ranges::cbegin(filename_str) + offset, std::ranges::cend(filename_str));
                filename_str = tmp;
            }
            else
            {
                /* none of filename shown, hide dots */
                filename_str = "";
            }
        }
        
        mvwprintw(*sub_win_top_, 0, filename_x_pos, "%s", filename_str.c_str());
        
        if (show_modified)
        {
            const int mod_text_x_pos{ std::max(0, sub_win_top_.size().x - strings::modified.length() - (pad_size * static_cast<int>(use_padding))) };
            mvwprintw(*sub_win_top_, 0, mod_text_x_pos, "%s", strings::modified.c_str());
        }
        
        touchwin(*sub_win_top_);
        wnoutrefresh(*sub_win_top_);
    }
    
    /* Variant of draw_top for non-filenames
     * Called via window::update_window();
     * doupdate() must be called after calling this function.                               */
    void window::draw_top_text_string(const strings::text_string& str)
    {
        using detail::color_type;
        
        if (not sub_win_top_)
            return;
        
        wclear(*sub_win_top_);
        sub_win_top_.set_default_color(color_type::inverse, term_has_color_);
 
        const int line_length{ sub_win_top_.size().x };
        int string_x_pos{ 0 };
        
        if (line_length >= str.length())
        {
            /* line is wide enough to show everything */
            string_x_pos = std::max(0, (line_length - str.length()) / 2);
            mvwprintw(*sub_win_top_, 0, string_x_pos, "%s", str.c_str());
        }
        else
        {
            /* remove characters from the start of filename_str to fit in line */
            const std::ptrdiff_t offset{ std::clamp(str.length() + 3 - line_length, 0, str.length()) };
            
            if (offset < str.length())
            {
                std::string tmp{ str.c_str() };
                treenote::utf8::drop_first_n_chars(tmp, offset);
                mvwprintw(*sub_win_top_, 0, 0, "...%s", tmp.c_str());
            }
        }
        
        touchwin(*sub_win_top_);
        wnoutrefresh(*sub_win_top_);
    }

    /* Called via window::update_window();
     * doupdate() must be called after calling this function. */
    void window::draw_status()
    {
        using detail::status_bar_mode;
        using detail::color_type;
        
        if (not sub_win_status_)
            return;
        
        wclear(*sub_win_status_);
        
        if (status_mode_ == status_bar_mode::default_mode)
        {
            /* status bar is in notification mode */
            sub_win_status_.set_default_color(color_type::standard, term_has_color_);
            
            if (status_msg_.has_message())
            {
                if (status_msg_.is_error())
                    sub_win_status_.set_color(color_type::warning, term_has_color_);
                else
                    sub_win_status_.set_color(color_type::inverse, term_has_color_);
                
                const int msg_len{ status_msg_.length() };
                
                if (msg_len + 4 <= sub_win_status_.size().x)
                {
                    /* enough space for message in window: display normally */
                    const int x_pos{ std::max(0, (sub_win_status_.size().x - msg_len - 4) / 2) };
                    mvwprintw(*sub_win_status_, 0, x_pos, "[ %s ]", status_msg_.c_str());
                }
                else
                {
                    /* message too large: don't show angle brackets */
                    const int x_pos{ std::max(0, (sub_win_status_.size().x - msg_len) / 2) };
                    mvwprintw(*sub_win_status_, 0, x_pos, "%s", status_msg_.c_str());
                }
                
                if (status_msg_.is_error())
                    sub_win_status_.unset_color(color_type::warning, term_has_color_);
                else
                    sub_win_status_.unset_color(color_type::inverse, term_has_color_);
            }
        }
        else
        {
            /* status bar is in prompt mode */
            sub_win_status_.set_default_color(color_type::inverse, term_has_color_);
            
            if (status_mode_ == status_bar_mode::prompt_close)
            {
                wprintw(*sub_win_status_, "%s ", strings::close_prompt.c_str());
            }
            else if (status_mode_ == status_bar_mode::prompt_filename or status_mode_ == status_bar_mode::prompt_location)
            {
                /* handle long inputs with a scrolling system */
                
                const auto& prompt{ (status_mode_ == status_bar_mode::prompt_filename) ? strings::file_prompt : strings::goto_prompt };
                
                int cursor_x{ std::saturate_cast<int>(prompt_info_.cursor_pos) };
                int start_of_line_index{ 0 };
                
                const int line_start_pos{ std::max(std::min(prompt.length() + 2, sub_win_status_.size().x - 4), 2) };
                const int space_available{ sub_win_status_.size().x - line_start_pos };
                const int cursor_limit{ space_available - 2 };
                const int page_offset{ space_available - 2};
                
                while (cursor_x > cursor_limit)
                {
                    start_of_line_index += page_offset;
                    cursor_x -= page_offset;
                }
                
                wprintw(*sub_win_status_, "%s ", prompt.c_str());
                mvwprintw(*sub_win_status_, 0, line_start_pos - 2, ": %s", prompt_info_.text.substr(start_of_line_index, space_available).c_str());
                
                if (start_of_line_index != 0)
                {
                    /* line has been scrolled: replace first character with continuation */
                    sub_win_status_.set_color(color_type::inverse, term_has_color_);
                    mvwprintw(*sub_win_status_, 0, line_start_pos - 1, "<");
                    sub_win_status_.unset_color(color_type::inverse, term_has_color_);
                }
                
                if (std::saturate_cast<int>(prompt_info_.text.size()) > start_of_line_index + space_available)
                {
                    /* length of input exceeds window size, replace final character with continuation */
                    sub_win_status_.set_color(color_type::inverse, term_has_color_);
                    mvwprintw(*sub_win_status_, 0, sub_win_status_.size().x - 1, ">");
                    sub_win_status_.unset_color(color_type::inverse, term_has_color_);
                }
            }
            else
            {
                std::unreachable();
            }
            
        }
        
        touchwin(*sub_win_status_);
        wnoutrefresh(*sub_win_status_);
        
    }
    
    /* Called via window::update_window();
     * doupdate() must be called after calling this function. */
    void window::draw_help()
    {
        using detail::color_type;
        
        if (not sub_win_help_)
            return;
        
        wclear(*sub_win_help_);
        sub_win_help_.set_default_color(color_type::standard, term_has_color_);
        
        const int size{ std::saturate_cast<int>(help_info_.entries.size()) };
        const int width{ sub_win_help_.size().x };
        const int min{ help_info_.min_width };
        const int max{ help_info_.max_width };
        
        const int rows{ sub_win_help_.size().y };
        const int cols{ std::max(1, std::min(width / min, (size + rows - 1) / rows)) };
        
        const int spacing{ (min > max) ? std::max(min, width / cols) : std::clamp(width / cols, min, max) };
        const int slack{ (min > max) ? width % spacing : 0 };
        
        std::vector<std::string> entry_key_names{ help_info_.entries
                                                  | std::views::transform([&](const detail::help_bar_entry& he) {
                                                      return keymap_.key_for(he.action);
                                                  }) | std::ranges::to<std::vector>() };
        
        for (int i{ 0 }, c{ 0 }; c < cols; ++c)
        {
            /* determine the maximum width of a key in this column */
            auto lengths{ entry_key_names | std::views::drop(i) | std::views::take(rows) |
                          std::views::transform([](const std::string& s){ return treenote::utf8::length(s).value_or(s.length()); }) };
            
            const std::size_t max_length{ std::max(2uz, std::ranges::max(lengths)) };
            
            auto beg{ std::ranges::begin(lengths) };
            const auto end{ std::ranges::end(lengths) };
            
            for (int r{ 0 }; r < rows and i < size; ++r, ++i, ++beg)
            {
                if (help_info_.last_is_bottom and r == 0 and i + 1 == size)
                {
                    /* draw last key entry on bottom row */
                    r = rows - 1;
                }
                
                const auto& entry{ help_info_.entries.at(i) };
                const auto& entry_key{ entry_key_names.at(i) };
                const int pos{ (spacing * c) + ((slack * c) / cols) };
                
                sub_win_help_.set_color(color_type::inverse, term_has_color_);
                
                if (beg != end and *beg < max_length)
                {
                    /* lengthen key string to match the largest in column  */
                    std::string content(max_length, ' ');
                    content.replace((max_length - *beg + 1) / 2, entry_key.size(), entry_key);
                    mvwprintw(*sub_win_help_, r, pos, "%s", content.c_str());
                }
                else
                {   
                    mvwprintw(*sub_win_help_, r, pos, "%s", entry_key.c_str());
                }
                
                sub_win_help_.unset_color(color_type::inverse, term_has_color_);
                wprintw(*sub_win_help_, " %s ", entry.desc.get().c_str());
            }
        }
        
        touchwin(*sub_win_help_);
        wnoutrefresh(*sub_win_help_);
    }
    
    /* Called via window::draw_content() or window::draw_content_selective();
     * touchline(), wnoutrefresh(), and doupdate() must be called after calling this function. */
    void window::draw_content_current_line_no_wrap(const int display_line, const tce& entry, int& cursor_x)
    {
        using detail::color_type;
        
        const std::size_t prefix_length{ treenote::note::get_entry_prefix_length(entry) * 4 };
        const std::size_t line_length{ treenote::note::get_entry_line_length(entry) };
        
        int start_of_line_index{ 0 };
        
        const int cursor_limit{ sub_win_content_.size().x - ((sub_win_content_.size().x > 8) ? 3 : 2) };
        const int page_offset{ std::max(9, sub_win_content_.size().x) - 8 };
        
        /* move the cursor position and text so both are on screen */
        while (cursor_x > cursor_limit)
        {
            start_of_line_index += page_offset;
            cursor_x -= page_offset;
        }
        
        if (start_of_line_index >= std::saturate_cast<int>(prefix_length))
        {
            /* no need to draw line prefix */
            const std::size_t start{ start_of_line_index - prefix_length };
            const std::string line_content{ treenote::note::get_entry_content(entry, start, sub_win_content_.size().x) };
            
            mvwprintw(*sub_win_content_, display_line, 0, "%s", line_content.c_str());
        }
        else
        {
            /* disregard first n characters of prefix */
            std::string line_prefix{ treenote::make_line_string_default(current_file_.get_entry_prefix(entry)) };
            treenote::utf8::drop_first_n_chars(line_prefix, start_of_line_index);
            const std::size_t content_length{ sub_win_content_.size().x + start_of_line_index - prefix_length };
            const std::string line_content{ treenote::note::get_entry_content(entry, 0, content_length) };
            
            mvwprintw(*sub_win_content_, display_line, 0, "%s%s", line_prefix.c_str(), line_content.c_str());
        }
        
        if (start_of_line_index != 0)
        {
            /* line has been scrolled: replace first character with continuation */
            sub_win_content_.set_color(color_type::inverse, term_has_color_);
            mvwprintw(*sub_win_content_, display_line, 0, "<");
            sub_win_content_.unset_color(color_type::inverse, term_has_color_);
        }
        
        if (std::saturate_cast<int>(line_length + prefix_length) - start_of_line_index > sub_win_content_.size().x)
        {
            /* length of line content and prefix exceeds window size, replace final character with continuation */
            sub_win_content_.set_color(color_type::inverse, term_has_color_);
            mvwprintw(*sub_win_content_, display_line, sub_win_content_.size().x - 1, ">");
            sub_win_content_.unset_color(color_type::inverse, term_has_color_);
        }
    }
    
    /* Called via window::draw_content() or window::draw_content_selective();
     * touchline(), wnoutrefresh(), and doupdate() must be called after calling this function. */
    void window::draw_content_non_current_line_no_wrap(const int display_line, const treenote::tree::cache_entry& entry)
    {
        using detail::color_type;
        
        const std::size_t prefix_length{ treenote::note::get_entry_prefix_length(entry) * 4 };
        const std::string line_prefix{ treenote::make_line_string_default(current_file_.get_entry_prefix(entry)) };
        const std::size_t line_length{ treenote::note::get_entry_line_length(entry) };
        const std::string line_content{ treenote::note::get_entry_content(entry, 0, sub_win_content_.size().x - prefix_length) };
        
        mvwprintw(*sub_win_content_, display_line, 0, "%s%s", line_prefix.c_str(), line_content.c_str());
        
        if (std::saturate_cast<int>(line_length + prefix_length) > sub_win_content_.size().x)
        {
            /* length of line content and prefix exceeds window size, replace final character with continuation */
            sub_win_content_.set_color(color_type::inverse, term_has_color_);
            mvwprintw(*sub_win_content_, display_line, sub_win_content_.size().x - 1, ">");
            sub_win_content_.unset_color(color_type::inverse, term_has_color_);
        }
    }
    
    /* Called via window::update_window();
     * doupdate() must be called after calling this function. */
    void window::draw_content_no_wrap(coord& default_cursor_pos)
    {
        using detail::status_bar_mode;
        using detail::color_type;
        
        if (not sub_win_content_)
            return;
        
        wclear(*sub_win_content_);
        sub_win_content_.set_default_color(color_type::standard, term_has_color_);

        int display_line{ 0 };
        auto lc{ current_file_.get_lc_range(line_start_y_, sub_win_content_.size().y) };
        
        for (const auto& entry: lc)
        {
            if (display_line == default_cursor_pos.y and status_mode_ == status_bar_mode::default_mode)
                draw_content_current_line_no_wrap(display_line, entry, default_cursor_pos.x);
            else
                draw_content_non_current_line_no_wrap(display_line, entry);
            
            ++display_line;
        }
        
        touchline(*sub_win_content_, 0, sub_win_content_.size().y);
        wnoutrefresh(*sub_win_content_);
    }
    
    /* Called via window::update_window();
     * doupdate() must be called after calling this function.
     * Redraws at most 2 lines instead of the whole screen.   */
    void window::draw_content_selective_no_wrap(coord& default_cursor_pos)
    {
        using detail::status_bar_mode;
        using detail::color_type;
        
        if (not sub_win_content_)
            return;
        
        /* a possible optimisation: check if lines need to be redrawn in first place
         * e.g. when scrolling horizontally and not crossing a page boundary         */
        
        sub_win_content_.set_default_color(color_type::standard, term_has_color_);
        
        auto lc{ current_file_.get_lc_range(line_start_y_, sub_win_content_.size().y) };
        
        if (static_cast<std::size_t>(std::max(default_cursor_pos.y, previous_cursor_y)) >= std::ranges::size(lc))
        {
            /* error: abort attempt to selectively redraw and redraw entire screen instead */
            return window::draw_content_no_wrap(default_cursor_pos);
        }
        
        /* clear line and replace it with line */
        wmove(*sub_win_content_, default_cursor_pos.y, 0);
        wclrtoeol(*sub_win_content_);
        draw_content_current_line_no_wrap(default_cursor_pos.y, *(std::ranges::begin(lc) + default_cursor_pos.y), default_cursor_pos.x);
        touchline(*sub_win_content_, default_cursor_pos.y, 1);
        
        /* assume that the screen position has not moved, since if it has, redraw_mask::RD_CONTENT
         * will have been set, so this function should not have been called in the first place      */
        if (previous_cursor_y != default_cursor_pos.y)
        {
            wmove(*sub_win_content_, previous_cursor_y, 0);
            wclrtoeol(*sub_win_content_);
            draw_content_non_current_line_no_wrap(previous_cursor_y, *(std::ranges::begin(lc) + previous_cursor_y));
            touchline(*sub_win_content_, previous_cursor_y, 1);
        }
        
        wnoutrefresh(*sub_win_content_);
    }
    
    /* Called via window::update_window_help_mode();
     * doupdate() must be called after calling this function. */
    void window::draw_content_help_mode_no_wrap(const keymap::bindings_t& bindings)
    {
        using detail::status_bar_mode;
        using detail::color_type;
        
        if (not sub_win_content_)
            return;
        
        wclear(*sub_win_content_);
        sub_win_content_.set_default_color(color_type::standard, term_has_color_);
        
        constexpr int offset{ 12 };
        
        constexpr std::size_t bindings_start{ 1 };
        
        for (int display_line{ 0 }; display_line < sub_win_content_.size().y; ++display_line)
        {
            const std::size_t y_pos{ line_start_y_ + display_line };
            
            if (bindings_start <= y_pos and y_pos < bindings_start + strings::help_strings.size() )
            {
                const std::size_t idx{ y_pos - 1 };
                
                wmove(*sub_win_content_, display_line, 0);
                wclrtoeol(*sub_win_content_);
                
                if (not bindings.at(idx).empty())
                {
                    sub_win_content_.set_color(color_type::emphasis, term_has_color_);
                    wprintw(*sub_win_content_, "%s", bindings[idx][0].c_str());
                    sub_win_content_.unset_color(color_type::emphasis, term_has_color_);
                    
                    if (bindings.at(idx).size() > 1)
                    {
                        mvwprintw(*sub_win_content_, display_line, offset, "(");
                        sub_win_content_.set_color(color_type::emphasis, term_has_color_);
                        wprintw(*sub_win_content_, "%s", bindings[idx][1].c_str());
                        sub_win_content_.unset_color(color_type::emphasis, term_has_color_);
                        wprintw(*sub_win_content_, ")");
                    }
                }
                
                if (strings::help_strings.at(idx).has_value())
                {
                    mvwprintw(*sub_win_content_, display_line, offset * 2, "%s", *strings::help_strings.at(idx).c_str());
                }
                
            }
        }
        
        
        touchline(*sub_win_content_, 0, sub_win_content_.size().y);
        wnoutrefresh(*sub_win_content_);
    }

    
    /* Update functions */
    
    /* Called via window::update_window();
     * window::draw_content_current_line_no_wrap() must be called beforehand
     * if (status_mode_ == status_bar_mode::DEFAULT), so that the x component
     * of the cursor_pos is within bounds                                     */
    void window::update_cursor_pos(coord& default_cursor_pos)
    {
        using detail::status_bar_mode;
        
        if (status_mode_ == status_bar_mode::default_mode)
        {
            /* prevent cursor from being outside content area */
            default_cursor_pos = { .y = std::min(default_cursor_pos.y, sub_win_content_.size().y - 1),
                                   .x = std::min(default_cursor_pos.x, sub_win_content_.size().x - 1) };
            
            move(sub_win_content_.pos().y + default_cursor_pos.y, sub_win_content_.pos().x + default_cursor_pos.x);
        }
        else if (status_mode_ == status_bar_mode::prompt_close)
        {
            move(sub_win_status_.pos().y, std::min(sub_win_status_.pos().x + strings::close_prompt.length() + 1, sub_win_status_.size().x - 1));
        }
        else if (status_mode_ == status_bar_mode::prompt_filename or status_mode_ == status_bar_mode::prompt_location)
        {
            /* handle long filenames with a scrolling system */
            
            const auto& prompt{ (status_mode_ == status_bar_mode::prompt_filename) ? strings::file_prompt : strings::goto_prompt };
            int cursor_x{ std::saturate_cast<int>(prompt_info_.cursor_pos) };
            
            const int line_start_pos{ std::max(std::min(prompt.length() + 2, sub_win_status_.size().x - 4), 2) };
            const int space_available{ sub_win_status_.size().x - line_start_pos };
            const int cursor_limit{ space_available - 2 };
            const int page_offset{ space_available - 2};
            
            while (cursor_x > cursor_limit)
                cursor_x -= page_offset;
            
            move(sub_win_status_.pos().y, sub_win_status_.pos().x + line_start_pos + cursor_x);
        }
        else
        {
            std::unreachable();
        }
        
    }
    
    void window::update_screen()
    {
        using detail::redraw_mask;

        status_msg_.clear();
        curs_set(0);
        
        coord cursor_pos{ .y = std::saturate_cast<int>(std::sub_sat(current_file_.cursor_y(), line_start_y_)),
                          .x = std::saturate_cast<int>(current_file_.cursor_x() + current_file_.cursor_current_indent_lvl() * 4) };
        
        if (screen_redraw_.has_mask(redraw_mask::RD_ALL))
            clear();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_TOP))
            draw_top();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_CONTENT))
            draw_content_no_wrap(cursor_pos);
        else
            draw_content_selective_no_wrap(cursor_pos);
        
        if (screen_redraw_.has_mask(redraw_mask::RD_STATUS))
            draw_status();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_HELP))
            draw_help();
        
        doupdate();
        curs_set(1);
        update_cursor_pos(cursor_pos);
        previous_cursor_y = cursor_pos.y;
        
        screen_redraw_.clear();
    }
    
    void window::update_screen_help_mode(const keymap::bindings_t& bindings)
    {
        using detail::redraw_mask;
        
        status_msg_.clear();
        curs_set(0);
        
        if (screen_redraw_.has_mask(redraw_mask::RD_ALL))
            clear();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_TOP))
            draw_top_text_string(strings::help_title);
        
        if (screen_redraw_.has_mask(redraw_mask::RD_CONTENT))
            draw_content_help_mode_no_wrap(bindings);
        
        if (screen_redraw_.has_mask(redraw_mask::RD_STATUS))
            draw_status();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_HELP))
            draw_help();
        
        doupdate();
        
        screen_redraw_.clear();
    }
    
    void window::update_viewport_pos(std::size_t lines_below)
    {
        using detail::redraw_mask;
        
        /* Move the viewport so to show the cursor if it is offscreen */
        if (current_file_.cursor_y() < line_start_y_ and line_start_y_ != 0)
        {
            line_start_y_ = current_file_.cursor_y();
            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        }
        else if (current_file_.cursor_y() + lines_below >= line_start_y_ + sub_win_content_.size().y)
        {
            line_start_y_ = current_file_.cursor_y() - (sub_win_content_.size().y - 1);
            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        }
    
        update_viewport_clamp_lower();
    }
    
    void window::update_viewport_cursor_pos()
    {
        using detail::redraw_mask;
        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        
        /* Move the cursor to keep it in the viewport */
        if (current_file_.cursor_y() < line_start_y_)
        {
            current_file_.cursor_mv_down(line_start_y_ - current_file_.cursor_y());
        }
        else if (current_file_.cursor_y() >= line_start_y_ + (sub_win_content_.size().y - 1))
        {
            current_file_.cursor_mv_up(current_file_.cursor_y() - (line_start_y_ + (sub_win_content_.size().y - 1)));
        }
    }
    
    void window::update_viewport_clamp_lower()
    {
        using detail::redraw_mask;
        
        /* Prevent the viewport from extending beyond the lowest line if file is taller than viewport */
        if (line_start_y_ + sub_win_content_.size().y > current_file_.cursor_max_y())
        {
            line_start_y_ = current_file_.cursor_max_y() - std::min(current_file_.cursor_max_y(), static_cast<std::size_t>(sub_win_content_.size().y));
            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        }
    }
    
    void window::update_viewport_center_line()
    {
        using detail::redraw_mask;
        
        line_start_y_ = current_file_.cursor_y() -
                        std::min(current_file_.cursor_y(), static_cast<std::size_t>(sub_win_content_.size().y) / 2);
        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        update_viewport_clamp_lower();
    }
    
    void window::update_window_sizes()
    {
        using detail::sub_window;
        
        static constexpr int top_height{ 1 };
        static constexpr int status_height{ 1 };
        static constexpr int threshold1{ 5 };
        static constexpr int threshold2{ 2 };
        static constexpr int threshold3{ 1 };
        
        screen_dimensions_ = { .y = getmaxy(stdscr), .x = getmaxx(stdscr) };
        bool show_status{ true };
        bool show_top{ true };
        bool show_help{ help_height_ != 0 };
        
        if (screen_dimensions_.y <= threshold1)
        {
            show_help = false;
            if (screen_dimensions_.y <= threshold2)
            {
                show_top = false;
                if (screen_dimensions_.y <= threshold3)
                    show_status = false;
            }
        }
        
        int content_height{ screen_dimensions_.y };
        content_height -= static_cast<int>(show_top) * top_height;
        content_height -= static_cast<int>(show_status) * status_height;
        content_height -= static_cast<int>(show_help) * help_height_;
        
        if (show_top)
            sub_win_top_ = sub_window{ { .y = top_height, .x = screen_dimensions_.x },
                                       { .y = 0,          .x = 0 } };
        else if (sub_win_top_)
            sub_win_top_ = sub_window{};
        
        sub_win_content_ = sub_window{ { .y = content_height, .x = screen_dimensions_.x - 1 },
                                       { .y = static_cast<int>(show_top) * top_height, .x = 1 } };
        
        if (show_status)
            sub_win_status_ = sub_window{ { .y = status_height, .x = screen_dimensions_.x },
                                          { .y = screen_dimensions_.y - status_height - (static_cast<int>(show_help) * help_height_), .x = 0 } };
        else if (sub_win_status_)
            sub_win_status_ = sub_window{};
        
        if (show_help)
            sub_win_help_ = sub_window{ { .y = help_height_, .x = screen_dimensions_.x },
                                        { .y = screen_dimensions_.y - help_height_, .x = 0 } };
        else if (sub_win_help_)
            sub_win_help_ = sub_window{};
        
        update_viewport_pos();
        screen_redraw_.set_all();
    }


    /* Main function for main_window */
    
    int window::operator()(std::deque<std::string>& filenames)
    {
        using detail::redraw_mask;
        
        const auto editor_keymap{ keymap_.make_editor_keymap() };
        
        do
        {
            if (not filenames.empty())
            {
                current_filename_ = filenames.front();
                filenames.pop_front();
            }
            
            tree_open();
            update_screen();
            
            detail::window_event_loop wel{ *this };
            wel(editor_keymap,
                [&](actions action, bool& exit)
                {
                    switch (action)
                    {
                        case actions::show_help:
                            help_screen();
                            break;
                        case actions::close_tree:
                            exit = tree_close();
                            break;
                        case actions::write_tree:
                            tree_save(true);
                            break;
                        case actions::save_file:
                            tree_save(false);
                            break;
                        case actions::suspend:
                            endwin();
                            std::raise(SIGSTOP);
                            screen_redraw_.set_all();
                            break;
                            
                        case actions::cursor_pos:
                            display_tree_pos();
                            break;
                        case actions::go_to:
                            location_prompt();
                            break;
                            
                        case actions::cut_node:
                            if (current_file_.node_cut() != 0)
                                status_msg_.set_message(strings::cut_error);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::copy_node:
                            if (current_file_.node_copy() != 0)
                                status_msg_.set_message(strings::copy_error);
                            break;
                        case actions::paste_node:
                            if (current_file_.node_paste_default() != 0)
                                status_msg_.set_warning(strings::paste_error);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::paste_node_abv:
                            if (current_file_.node_paste_above() != 0)
                                status_msg_.set_warning(strings::paste_error);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                            
                        case actions::undo:
                            undo();
                            break;
                        case actions::redo:
                            redo();
                            break;
                            
                        case actions::raise_node:
                            current_file_.node_move_higher_rec();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        case actions::lower_node:
                            current_file_.node_move_lower_rec();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        case actions::reorder_backwards:
                            current_file_.node_move_back_rec();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        case actions::reorder_forwards:
                            current_file_.node_move_forward_rec();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
    
                        case actions::insert_node_def:
                            current_file_.node_insert_default();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        case actions::insert_node_abv:
                            current_file_.node_insert_above();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        case actions::insert_node_bel:
                            current_file_.node_insert_below();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        case actions::insert_node_chi:
                            current_file_.node_insert_child();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;

    
                        case actions::delete_node_chk:
                            {
                                auto result = current_file_.node_delete_check();
                                if (result == 1)
                                    status_msg_.set_message(strings::nothing_delete);
                                if (result == 2)
                                    status_msg_.set_warning(strings::delete_prevent(keymap_.key_for(actions::delete_node_rec)));
                                screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                                update_viewport_pos();
                            }
                            break;
                        case actions::delete_node_rec:
                            {
                                auto result = current_file_.node_delete_rec();
                                if (result == 1)
                                    status_msg_.set_message(strings::nothing_delete);
                                screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                                update_viewport_pos();
                            }
                            break;
                        case actions::delete_node_spc:
                            current_file_.node_delete_special();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_pos();
                            break;
                        
                        /* Direct Cursor Movement: */

                        case actions::cursor_left:
                            current_file_.cursor_mv_left();
                            break;
                        case actions::cursor_right:
                            current_file_.cursor_mv_right();
                            break;
                        case actions::cursor_up:
                            current_file_.cursor_mv_up(1 + wel.crh().extract_multiple_of_same_action(actions::cursor_up, editor_keymap));
                            update_viewport_pos();
                            break;
                        case actions::cursor_down:
                            current_file_.cursor_mv_down(1 + wel.crh().extract_multiple_of_same_action(actions::cursor_down, editor_keymap));
                            update_viewport_pos();
                            break;
                        case actions::cursor_prev_w:
                            current_file_.cursor_wd_backward();
                            update_viewport_pos();
                            break;
                        case actions::cursor_next_w:
                            current_file_.cursor_wd_forward();
                            update_viewport_pos();
                            break;
                        case actions::cursor_sol:
                            current_file_.cursor_to_SOL();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::cursor_eol:
                            current_file_.cursor_to_EOL();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::cursor_sof:
                            current_file_.cursor_to_SOF();
                            update_viewport_pos();
                            break;
                        case actions::cursor_eof:
                            current_file_.cursor_to_EOF();
                            update_viewport_pos();
                            break;

                        /* Page Position Movement: */

                        case actions::scroll_up:
                            line_start_y_ = std::sub_sat(line_start_y_, 1uz);
                            update_viewport_cursor_pos();
                            break;
                        case actions::scroll_down:
                            line_start_y_ = std::min(line_start_y_ + 1uz, std::sub_sat<std::size_t>(current_file_.cursor_max_y(), sub_win_content_.size().y));
                            update_viewport_cursor_pos();
                            break;
                        case actions::page_up:
                            current_file_.cursor_mv_up(sub_win_content_.size().y);
                            line_start_y_ -= std::min(line_start_y_, static_cast<std::size_t>(sub_win_content_.size().y));
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_clamp_lower();
                            break;
                        case actions::page_down:
                            current_file_.cursor_mv_down(sub_win_content_.size().y);
                            line_start_y_ += sub_win_content_.size().y;
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_clamp_lower();
                            break;
                        case actions::center_view:
                            update_viewport_center_line();
                            break;

                        /* Node-based Movement: */
                        
                        case actions::node_parent:
                            current_file_.cursor_nd_parent();
                            update_viewport_pos();
                            break;
                        case actions::node_child:
                            current_file_.cursor_nd_child();
                            update_viewport_pos(current_file_.cursor_max_line());
                            break;
                        case actions::node_prev:
                            current_file_.cursor_nd_prev();
                            update_viewport_pos();
                            break;
                        case actions::node_next:
                            current_file_.cursor_nd_next();
                            update_viewport_pos(current_file_.cursor_max_line());
                            break;
                            
                        /* Line input keys */
                        
                        case actions::newline:
                            current_file_.line_newline();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::backspace:
                            current_file_.line_backspace();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::delete_char:
                            current_file_.line_delete_char();
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                            
                        /* Unbound key entered */
                        
                        case actions::unknown:
                            status_msg_.set_warning(strings::unbound_key(wel.crh().key_name()));
                            break;
                            
                        default:
                            break;
                    }
                },
                [&](std::string& inserted)
                {
                    current_file_.line_insert_text(inserted);
                },
                [&](MEVENT& mouse)
                {
                    coord mouse_pos{ .y = mouse.y, .x = mouse.x };
                    
                    if (wmouse_trafo(*sub_win_content_, &(mouse_pos.y), &(mouse_pos.x), false))
                    {
                        if (mouse.bstate & BUTTON1_RELEASED)
                        {
                            const std::size_t cache_entry_pos{ line_start_y_ + mouse_pos.y };
                            
                            if (cache_entry_pos == current_file_.cursor_y())
                            {
                                /* moving cursor to current line; current line may be scrolled */
                                
                                /* constants copied from draw_content_current_line_no_wrap */
                                const std::size_t prefix_length{ current_file_.cursor_current_indent_lvl() * 4 };
                                const int cursor_limit{ sub_win_content_.size().x - ((sub_win_content_.size().x > 8) ? 3 : 2) };
                                const int page_offset{ std::max(9, sub_win_content_.size().x) - 8 };
                                
                                /* code to calculate cursor x_coord copied from update_screen */
                                int cursor_display_x{ std::saturate_cast<int>(current_file_.cursor_x() + prefix_length) };
                                
                                /* copied from draw_content_current_line_no_wrap */
                                int start_of_line_index{ 0 };
                                
                                while (cursor_display_x > cursor_limit)
                                {
                                    start_of_line_index += page_offset;
                                    cursor_display_x -= page_offset;
                                }
                                
                                /* end copying */
                                
                                current_file_.cursor_go_to(cache_entry_pos,
                                                           std::sub_sat(std::saturate_cast<std::size_t>(mouse_pos.x + start_of_line_index), prefix_length));
                            }
                            else if (auto lc{ current_file_.get_lc_range(cache_entry_pos, 1) }; not lc.empty())
                            {
                                /* moving cursor to another line */
                                const auto& entry{ *std::ranges::begin(lc) };
                                const std::size_t prefix_length{ treenote::note::get_entry_prefix_length(entry) * 4 };
                                current_file_.cursor_go_to(cache_entry_pos,
                                                           std::sub_sat(std::saturate_cast<std::size_t>(mouse_pos.x), prefix_length));
                            }
                            else
                            {
                                /* out of range */
                                current_file_.cursor_to_EOF();
                                current_file_.cursor_to_EOL();
                            }
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                        }
                        
                        if (mouse.bstate & BUTTON4_PRESSED and line_start_y_ > 0)
                        {
                            line_start_y_ = std::sub_sat(line_start_y_, 2uz);
                            update_viewport_cursor_pos();
                        }
                        
                        if (mouse.bstate & BUTTON5_PRESSED and line_start_y_ + sub_win_content_.size().y < current_file_.cursor_max_y())
                        {
                            line_start_y_ = std::min(line_start_y_ + 2uz, std::sub_sat<std::size_t>(current_file_.cursor_max_y(), sub_win_content_.size().y));
                            update_viewport_cursor_pos();
                        }
                    }
                },
                [&]() { update_screen(); }
            );
        }
        while (not global_signal_status and not filenames.empty());
        
        if (global_signal_status and current_file_.modified())
        {
            /* autosave file before closing */
            autosave_msg = current_file_.save_to_tmp(current_filename_);
            autosave_path = current_filename_;
            return 1;
        }
        
        return 0;
    }
}