// window.cpp

#include "window.h"

#include <algorithm>
#include <csignal>
#include <cuchar>
#include <iostream>
#include <queue>
#include <string_view>
#include <utility>

#include "keys.hpp"

#include "../treenote/legacy_tree_string.h"

// todo: add signal handler for SIGINT, SIGQUIT, etc...
// todo: add system to reject interacting with files with line length of over std::short_max
// todo: implement help bars where necessary
// todo: write and implement help page
// todo: add help text (with list of controls)

namespace treenote_tui
{
    /* Miscellaneous helper functions */
    
    namespace detail
    {
        namespace
        {
            /* Create new string with a single (mb) character */
            inline std::string wint_to_string(const wint_t char_input)
            {
                std::mbstate_t mbstate{};
                std::string buf(MB_CUR_MAX, '\0');
                const std::size_t len{ std::c32rtomb(buf.data(), static_cast<char32_t>(char_input), &mbstate) };
                buf.resize(len);
                return buf;
            }
            
            /* Append a single (mb) character to a std::string */
            inline void append_wint_to_string(std::string& str, const wint_t char_input)
            {
                std::mbstate_t mbstate{};
                const std::size_t old_len{ str.length() };
                str.resize(old_len + MB_CUR_MAX);
                const std::size_t len{ std::c32rtomb(&(str[old_len]), static_cast<char32_t>(char_input), &mbstate) };
                str.resize(old_len + len);
            }
            
            /* Implementation for reading characters */
            
            /* Class to help with reading keys from ncurses */
            class char_read_helper
            {
            public:
                [[nodiscard]] key::input_t value() const noexcept;
                [[nodiscard]] actions get_action(const keymap_t& keymap) const noexcept;
                [[nodiscard]] std::string keyname() const;
                [[nodiscard]] bool is_resize() const noexcept;
                [[nodiscard]] bool is_command() const noexcept;
                void extract_char();
                void extract_second_char();
                void extract_more_readable_chars(std::string& inserted);
                std::size_t extract_multiple_of_same_action(actions target, const keymap_t& keymap);
                
                template<keymap::keycode_function Key>
                [[nodiscard]] bool is_key();
            
            private:
                void force_extract_char();
                
                wint_t input_{ 0 };
                wint_t second_input_{ 0 };
                int input_info_{ 0 };
                bool carry_over_{ false };
            };
            
            inline key::input_t char_read_helper::value() const noexcept
            {
                if (second_input_ == 0)
                    return input_;
                else
                    return make_alt_code(second_input_);
            }
            
            inline actions char_read_helper::get_action(const keymap_t& keymap) const noexcept
            {
                auto action{ actions::unknown };
                auto val{ value() };
                if (keymap.contains(val))
                    action = keymap.at(val);
                return action;
            }
            
            inline std::string char_read_helper::keyname() const
            {
                std::string result{ ::keyname(static_cast<int>(input_)) };
                if (second_input_ != 0)
                    result += ::keyname(static_cast<int>(second_input_));
                return result;
            }
            
            /* this must always be checked first before is_command is checked */
            inline bool char_read_helper::is_resize() const noexcept
            {
                return (input_info_ == KEY_CODE_YES and input_ == KEY_RESIZE);
            }
            
            inline bool char_read_helper::is_command() const noexcept
            {
                return (input_ < ' ' or input_info_ == KEY_CODE_YES);
            }
            
            /* Reads another char, blocking until a char is read */
            inline void char_read_helper::extract_char()
            {
                /* do not get new keycode if another key has been got but not acted on */
                if (carry_over_)
                    carry_over_ = false;
                else
                    force_extract_char();
            }
            
            inline void char_read_helper::extract_second_char()
            {
                /* extract second key if key press is alt or esc */
                if (input_ == key::code::esc)
                {
                    timeout(0);
                    if (get_wch(&second_input_) == ERR)
                        second_input_ = 0;
                    timeout(-1);
                }
            }
            
            /* Continues extracting readable chars, if any.
             * NOTE: This should only be called after extracting a readable char. */
            inline void char_read_helper::extract_more_readable_chars(std::string& inserted)
            {
                timeout(0);
                for (bool loop{ true }; loop;)
                {
                    force_extract_char();
                    if (input_info_ != ERR)
                    {
                        if (is_resize() or is_command())
                        {
                            loop = false;
                            carry_over_ = true;
                        }
                        else if (input_ == key::code::esc)
                        {
                            unget_wch('\x1b');
                            loop = false;
                        }
                        else
                        {
                            append_wint_to_string(inserted, input_);
                        }
                    }
                    else
                    {
                        loop = false;
                    }
                }
                timeout(-1);
            }
            
            inline std::size_t char_read_helper::extract_multiple_of_same_action(const actions target, const keymap_t& keymap)
            {
                std::size_t count{ 0 };
                timeout(0);
                for (bool loop{ true }; loop;)
                {
                    force_extract_char();
                    if (input_info_ != ERR)
                    {
                        if (input_ == key::code::esc)
                        {
                            unget_wch('\x1b');
                            loop = false;
                        }
                        else if (not is_resize() and is_command())
                        {
                            auto action{ actions::unknown };
                            if (keymap.contains(value()))
                                action = keymap.at(value());
                            
                            if (action == target)
                                ++count;
                            else
                            {
                                loop = false;
                                carry_over_ = true;
                            }
                        }
                        else
                        {
                            loop = false;
                            carry_over_ = true;
                        }
                    }
                    else
                    {
                        loop = false;
                    }
                }
                timeout(-1);
                return count;
            }
            
            inline void char_read_helper::force_extract_char()
            {
                input_info_ = get_wch(&input_);
                second_input_ = 0;
            }
            
            template<keymap::keycode_function Key>
            inline bool char_read_helper::is_key()
            {
                return value() == std::invoke(Key{});
            }
            
        }
    }
    
    inline constexpr std::string_view program_name_text{ "treenote " };
    inline constexpr int program_name_ver_text_len{ detail::bounded_cast<int>(program_name_text.size() + treenote_version_string.size()) };
    inline constexpr int pad_size{ 2 };
    

    /* Constructors and Destructors, and related funcs */
    
    window::window()
    {
        std::locale::global(new_locale_);
        
        initscr();
        raw();          // disable keyboard interrupts
        nonl();         // disable conversion of enter to new line
        noecho();       // do not echo keyboard input
        curs_set(1);    // show cursor
        
        intrflush(stdscr, FALSE);
        keypad(stdscr, TRUE);
        use_extended_names(true);
        
        keymap_ = std::invoke(keymap::defaults{});
        
        update_window_sizes();
        
        if (has_colors() != FALSE)
        {
            term_has_color = true;
            start_color();
            
            // todo: make this user definable later
            init_pair(detail::CONTENT_COLOR, COLOR_WHITE, COLOR_BLACK);
            init_pair(detail::BORDER_COLOR, COLOR_BLACK, COLOR_WHITE);
            init_pair(detail::STATUS_MSG_COLOR, COLOR_BLACK, COLOR_WHITE);
            init_pair(detail::WARNING_MSG_COLOR, COLOR_WHITE, COLOR_RED);
            init_pair(detail::CONT_ARROW_COLOR, COLOR_BLACK, COLOR_WHITE);
            
            bkgd(COLOR_PAIR(detail::CONTENT_COLOR) | ' ');
        }
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


    /* Tree related operations */

    void window::tree_open()
    {
        using file_msg = treenote::note::file_msg;
        
        if (current_filename_ != "")
        {
            auto load_info{ current_file_.load_file(current_filename_) };
            
            switch (load_info.first)
            {
                // TODO: replace "current_filename_.string()" with "current_filename" if/when P2845R0 is accepted
                case file_msg::none:
                    status_msg_.set_message(screen_redraw_, strings_.read_success(load_info.second.node_count, load_info.second.line_count));
                    break;
                case file_msg::does_not_exist:
                    status_msg_.set_message(screen_redraw_, strings_.new_file_msg);
                    break;
                case file_msg::is_unwritable:
                    status_msg_.set_warning(screen_redraw_, strings_.file_is_unwrit(current_filename_.string()));
                    break;
                case file_msg::is_directory:
                    status_msg_.set_warning(screen_redraw_, strings_.error_reading(current_filename_.string(), strings_.is_directory.str_view()));
                    break;
                case file_msg::is_device_file:
                    status_msg_.set_warning(screen_redraw_, strings_.error_reading(current_filename_.string(), strings_.is_device_file.str_view()));
                    break;
                case file_msg::is_invalid_file:
                    status_msg_.set_warning(screen_redraw_, strings_.error_reading(current_filename_.string(), strings_.invalid_file.str_view()));
                    break;
                case file_msg::is_unreadable:
                    status_msg_.set_warning(screen_redraw_, strings_.error_reading(current_filename_.string(), strings_.permission_denied.str_view()));
                    break;
                case file_msg::unknown_error:
                    status_msg_.set_warning(screen_redraw_, strings_.error_reading(current_filename_.string(), strings_.unknown_error.str_view()));
                    break;
            }
        }
        else
        {
            current_file_.make_empty();
            status_msg_.set_message(screen_redraw_, strings_.new_file_msg);
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
            status_mode_ = status_bar_mode::PROMPT_FILENAME;
            screen_redraw_.set_all();
            bool cancelled{ false };
            
            detail::char_read_helper crh{};
            treenote::legacy_tree_string line_editor{ current_filename_.c_str() };
            
            prompt_info_.text = line_editor.to_str(0);
            prompt_info_.cursor_pos = line_editor.line_length(0);
            update_screen();
            
            for (bool exit{ false }; not exit;)
            {
                crh.extract_char();
                
                if (crh.is_resize())
                {
                    /* update overall window size information */
                    update_window_sizes();
                    screen_redraw_.set_all();
                }
                else if (crh.is_command())
                {
                    crh.extract_second_char();
                    
                    /* I don't think it's possible to use a switch here since the input_t values of some
                     * keys are not known until runtime (due to needing to register our own keycodes)    */
                     
                    if (crh.is_key<key::ctrl<'c'>>())
                    {
                        /* cancel save operation */
                        exit = true;
                        cancelled = true;
                    }
                    else if (crh.is_key<key::enter>() or crh.is_key<key::ctrl<'m'>>())
                    {
                        /* save file to filename */
                        exit = true;
                    }
                    else if (crh.is_key<key::backspace>())
                    {
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
                    }
                    else if (crh.is_key<key::del>())
                    {
                        if (prompt_info_.cursor_pos < line_editor.line_length(0))
                        {
                            line_editor.delete_char_current(0, prompt_info_.cursor_pos);
                            
                            prompt_info_.text = line_editor.to_str(0);
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                        }
                    }
                    else if (crh.is_key<key::left>())
                    {
                        if (prompt_info_.cursor_pos > 0)
                            --prompt_info_.cursor_pos;
                        
                        /* redraw only if possible for a horizontal scroll */
                        if (prompt_info_.text.size() > static_cast<std::size_t>(std::min(0, (sub_win_content_.size().y - 2 - strings_.file_prompt.length()))))
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                    }
                    else if (crh.is_key<key::right>())
                    {
                        if (prompt_info_.cursor_pos < line_editor.line_length(0))
                            ++prompt_info_.cursor_pos;
                        
                        /* redraw only if possible for a horizontal scroll */
                        if (prompt_info_.text.size() > static_cast<std::size_t>(std::min(0, (sub_win_content_.size().y - 2 - strings_.file_prompt.length()))))
                            screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                    }
                }
                else
                {
                    std::string input{ detail::wint_to_string(crh.value()) };
                    crh.extract_more_readable_chars(input);
                    
                    std::size_t cursor_inc_amt{ 0 };
                    line_editor.insert_str(0, prompt_info_.cursor_pos, input, cursor_inc_amt);
                    
                    prompt_info_.cursor_pos += cursor_inc_amt;
                    prompt_info_.text = line_editor.to_str(0);
                    screen_redraw_.add_mask(redraw_mask::RD_STATUS);
                }
                
                update_screen();
            }
            
            status_mode_ = status_bar_mode::DEFAULT;
            screen_redraw_.set_all();
            
            if (cancelled)
            {
                status_msg_.set_message(screen_redraw_, strings_.cancelled);
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
            // TODO: replace "current_filename_.string()" with "current_filename" if/when P2845R0 is accepted
            case file_msg::none:
            case file_msg::does_not_exist:
            case file_msg::is_unreadable:
                status_msg_.set_message(screen_redraw_, strings_.write_success(save_info.second.node_count, save_info.second.line_count));
                success = true;
                break;
            case file_msg::is_directory:
                status_msg_.set_warning(screen_redraw_, strings_.error_writing(current_filename_.string(), strings_.is_directory.str_view()));
                break;
            case file_msg::is_device_file:
            case file_msg::is_invalid_file:
                status_msg_.set_warning(screen_redraw_, strings_.error_writing(current_filename_.string(), strings_.invalid_file.str_view()));
                break;
            case file_msg::is_unwritable:
                status_msg_.set_warning(screen_redraw_, strings_.error_writing(current_filename_.string(), strings_.permission_denied.str_view()));
                break;
            case file_msg::unknown_error:
                status_msg_.set_warning(screen_redraw_, strings_.error_writing(current_filename_.string(), strings_.unknown_error.str_view()));
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
            status_mode_ = status_bar_mode::PROMPT_CLOSE;
            screen_redraw_.set_all();
            update_screen();
            
            detail::char_read_helper crh{};
            std::optional<bool> save{};
            
            for (bool exit{ false }; not exit;)
            {
                crh.extract_char();
                
                if (crh.is_resize())
                {
                    /* update overall window size information */
                    update_window_sizes();
                    screen_redraw_.set_all();
                    update_screen();
                }
                else if (crh.is_command())
                {
                    crh.extract_second_char();
                    
                    if (crh.is_key<key::ctrl<'c'>>())
                    {
                        /* cancel closing file */
                        exit = true;
                    }
                    else if (crh.is_key<key::ctrl<'q'>>())
                    {
                        /* force quit */
                        save = false;
                        exit = true;
                    }
                }
                else
                {
                    std::string input{ detail::wint_to_string(crh.value()) };
                    crh.extract_more_readable_chars(input);
                    
                    if (input == "y" or input == "Y")
                    {
                        /* save and quit */
                        save = true;
                        exit = true;
                    }
                    else if (input == "n" or input == "N")
                    {
                        /* save without quitting */
                        save = false;
                        exit = true;
                    }
                }
            }
            
            status_mode_ = status_bar_mode::DEFAULT;
            screen_redraw_.set_all();
            
            if (not save.has_value())
            {
                /* closing has been cancelled; don't exit */
                status_msg_.set_message(screen_redraw_, strings_.cancelled);
                return false;
            }
            else if (*save)
            {
                /* save tree; an extra prompt will be given if necessary */
                return tree_save(false);
            }
        }

        /* no need to save, we can exit */
        return true;
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
//            node_idx << treenote::last_index_of(index) + 1;
        
        status_msg_.set_message(screen_redraw_, strings_.cursor_pos_msg(node_idx.str(), line + 1, max_lines, current_file_.cursor_x() + 1, max_x + 1));
    }
    
    void window::undo()
    {
        using detail::redraw_mask;
        using treenote::cmd_names;
        
        switch (current_file_.undo())
        {
            case cmd_names::error:
                status_msg_.set_warning(screen_redraw_, strings_.nothing_undo);
                break;
            case cmd_names::move_node:
                status_msg_.set_message(screen_redraw_, strings_.undo_move_node);
                break;
            case cmd_names::insert_node:
                status_msg_.set_message(screen_redraw_, strings_.undo_ins_node);
                break;
            case cmd_names::delete_node:
                status_msg_.set_message(screen_redraw_, strings_.undo_del_node);
                break;
            case cmd_names::cut_node:
                status_msg_.set_message(screen_redraw_, strings_.undo_cut_node);
                break;
            case cmd_names::paste_node:
                status_msg_.set_message(screen_redraw_, strings_.undo_paste_node);
                break;
            case cmd_names::insert_text:
                status_msg_.set_message(screen_redraw_, strings_.undo_ins_text);
                break;
            case cmd_names::delete_text:
                status_msg_.set_message(screen_redraw_, strings_.undo_del_text);
                break;
            case cmd_names::line_break:
                status_msg_.set_message(screen_redraw_, strings_.undo_line_br);
                break;
            case cmd_names::line_join:
                status_msg_.set_message(screen_redraw_, strings_.undo_line_jn);
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
                status_msg_.set_warning(screen_redraw_, strings_.nothing_redo);
                break;
            case cmd_names::move_node:
                status_msg_.set_message(screen_redraw_, strings_.redo_move_node);
                break;
            case cmd_names::insert_node:
                status_msg_.set_message(screen_redraw_, strings_.redo_ins_node);
                break;
            case cmd_names::delete_node:
                status_msg_.set_message(screen_redraw_, strings_.redo_del_node);
                break;
            case cmd_names::cut_node:
                status_msg_.set_message(screen_redraw_, strings_.redo_cut_node);
                break;
            case cmd_names::paste_node:
                status_msg_.set_message(screen_redraw_, strings_.redo_paste_node);
                break;
            case cmd_names::insert_text:
                status_msg_.set_message(screen_redraw_, strings_.redo_ins_text);
                break;
            case cmd_names::delete_text:
                status_msg_.set_message(screen_redraw_, strings_.redo_del_text);
                break;
            case cmd_names::line_break:
                status_msg_.set_message(screen_redraw_, strings_.redo_line_br);
                break;
            case cmd_names::line_join:
                status_msg_.set_message(screen_redraw_, strings_.redo_line_jn);
                break;
            case cmd_names::none:
                /* don't display a message */
                break;
        }
        
        screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
        update_viewport_pos();
    }
    
    
    /* Drawing help functions */
    
    void window::set_color(detail::color_type name, detail::sub_window& sw) const
    {
        if (term_has_color)
            wattron(sw.get(), COLOR_PAIR(name));
        else if (name != detail::CONTENT_COLOR)
            wattron(sw.get(), A_REVERSE);
    }
    
    void window::unset_color(detail::color_type name, detail::sub_window& sw) const
    {
        if (term_has_color)
            wattroff(sw.get(), COLOR_PAIR(name));
        else if (name != detail::CONTENT_COLOR)
            wattroff(sw.get(), A_REVERSE);
    }
    
    void window::set_default_color(detail::color_type name, detail::sub_window& sw) const
    {
        if (sw and term_has_color)
            wbkgd(sw.get(), COLOR_PAIR(name));
    }


    /* Drawing functions */
    
    /* Updates the top bar. This function should be called everytime the file name changes,
     * when the window is resized horizontally, and when the modification status changes.
     * Called via window::update_window();
     * doupdate() must be called after calling this function.                               */
    void window::draw_top()
    {
        if (sub_win_top_)
        {
            wclear(sub_win_top_.get());
            set_default_color(detail::BORDER_COLOR, sub_win_top_);
            
            std::string filename_str{ current_filename_.string() };
            
            if (filename_str.empty())
                filename_str = strings_.empty_file.c_str();
            
            const bool show_modified{ current_file_.modified() };
            bool use_padding{ false };
            
            const int filename_len{ detail::bounded_cast<int>(filename_str.length()) };
            const int line_length{ sub_win_top_.size().x };
            int filename_x_pos{ 0 };
            
            if (line_length >= filename_len + 2 * (pad_size + 1) + strings_.modified.length() + program_name_ver_text_len)
            {
                /* line is wide enough to show everything */
                filename_x_pos = program_name_ver_text_len +
                                 std::max(0, (line_length - filename_len - program_name_ver_text_len - strings_.modified.length()) / 2);
                mvwprintw(sub_win_top_.get(), 0, pad_size, "%s%s", program_name_text.data(), treenote_version_string.data());
                use_padding = true;
            }
            else if (line_length >= filename_len + 2 * pad_size + strings_.modified.length() + 1)
            {
                /* not enough space to show name of program, but enough to show entirety of filename */
                filename_x_pos = std::max(0, (line_length - filename_len - strings_.modified.length()) / 2);
                mvwprintw(sub_win_top_.get(), 0, filename_x_pos, "%s", filename_str.c_str());
                use_padding = true;
            }
            else if (not show_modified)
            {
                /* maybe not enough space for filename, but modified does not need to be shown */
                if (line_length < filename_len)
                {
                    /* remove characters from the start of filename_str to fit in line */
                    const std::ptrdiff_t offset{ std::clamp(filename_len + 3 - line_length, 0, filename_len) };
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
                else
                {
                    /* enough space for filename: center in absence of space for the modified text */
                    filename_x_pos = std::max(0, (line_length - filename_len) / 2);
                }
            }
            else if (line_length < filename_len + (strings_.modified.length() + 1))
            {
                /* remove characters from the start of filename_str to fit in line (with the modified text shown) */
                const std::ptrdiff_t offset{ std::clamp(filename_len + 3 - line_length + (strings_.modified.length() + 1), 0, filename_len) };
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
            
            mvwprintw(sub_win_top_.get(), 0, filename_x_pos, "%s", filename_str.c_str());
            
            if (show_modified)
            {
                const int mod_text_x_pos{ std::max(0, sub_win_top_.size().x - strings_.modified.length() - (pad_size * static_cast<int>(use_padding))) };
                mvwprintw(sub_win_top_.get(), 0, mod_text_x_pos, "%s", strings_.modified.c_str());
            }
            
            touchline(stdscr, sub_win_top_.pos().y, sub_win_top_.pos().y);
            wnoutrefresh(sub_win_top_.get());
        }
    }

    /* Called via window::update_window();
     * doupdate() must be called after calling this function. */
    void window::draw_status()
    {
        using detail::bounded_cast;
        using detail::status_bar_mode;
        
        if (sub_win_status_)
        {
            wclear(sub_win_status_.get());
            
            if (status_mode_ == status_bar_mode::DEFAULT)
            {
                /* status bar is in notification mode */
                set_default_color(detail::CONTENT_COLOR, sub_win_status_);
                
                if (status_msg_.has_message())
                {
                    if (status_msg_.is_error())
                        set_color(detail::WARNING_MSG_COLOR, sub_win_status_);
                    else
                        set_color(detail::STATUS_MSG_COLOR, sub_win_status_);
                    
                    const int msg_len{ status_msg_.length() };
                    
                    if (msg_len + 4 <= sub_win_status_.size().x)
                    {
                        /* enough space for message in window: display normally */
                        const int x_pos{ std::max(0, (sub_win_status_.size().x - msg_len - 4) / 2) };
                        mvwprintw(sub_win_status_.get(), 0, x_pos, "[ %s ]", status_msg_.c_str());
                    }
                    else
                    {
                        /* message too large: don't show angle brackets */
                        const int x_pos{ std::max(0, (sub_win_status_.size().x - msg_len) / 2) };
                        mvwprintw(sub_win_status_.get(), 0, x_pos, "%s", status_msg_.c_str());
                    }
                    
                    if (status_msg_.is_error())
                        unset_color(detail::WARNING_MSG_COLOR, sub_win_status_);
                    else
                        unset_color(detail::STATUS_MSG_COLOR, sub_win_status_);
                }
            }
            else
            {
                /* status bar is in prompt mode */
                set_default_color(detail::BORDER_COLOR, sub_win_status_);
                
                if (status_mode_ == status_bar_mode::PROMPT_CLOSE)
                {
                    wprintw(sub_win_status_.get(), "%s ", strings_.close_prompt.c_str());
                }
                else if (status_mode_ == status_bar_mode::PROMPT_FILENAME)
                {
                    /* handle long filenames with a scrolling system */
                    
                    int cursor_x{ bounded_cast<int>(prompt_info_.cursor_pos) };
                    int start_of_line_index{ 0 };
                    
                    const int line_start_pos{ std::max(std::min(strings_.file_prompt.length() + 2, sub_win_status_.size().x - 4), 2) };
                    const int space_available{ sub_win_status_.size().x - line_start_pos };
                    const int cursor_limit{ space_available - 2 };
                    const int page_offset{ space_available - 2};
                    
                    while (cursor_x > cursor_limit)
                    {
                        start_of_line_index += page_offset;
                        cursor_x -= page_offset;
                    }
                    
                    wprintw(sub_win_status_.get(), "%s ", strings_.file_prompt.c_str());
                    mvwprintw(sub_win_status_.get(), 0, line_start_pos - 2, ": %s", prompt_info_.text.substr(start_of_line_index, space_available).c_str());
                    
                    if (start_of_line_index != 0)
                    {
                        /* line has been scrolled: replace first character with continuation */
                        set_color(detail::CONT_ARROW_COLOR, sub_win_status_);
                        mvwprintw(sub_win_status_.get(), 0, line_start_pos - 1, "<");
                        unset_color(detail::CONT_ARROW_COLOR, sub_win_status_);
                    }
                    
                    if (bounded_cast<int>(prompt_info_.text.size()) > start_of_line_index + space_available)
                    {
                        /* length of filename exceeds window size, replace final character with continuation */
                        set_color(detail::CONT_ARROW_COLOR, sub_win_status_);
                        mvwprintw(sub_win_status_.get(), 0, sub_win_status_.size().x - 1, ">");
                        unset_color(detail::CONT_ARROW_COLOR, sub_win_status_);
                    }
                }
                else
                {
                    std::unreachable();
                }
                
            }
            
            touchwin(sub_win_status_.get());
            wnoutrefresh(sub_win_status_.get());
        }
    }
    
    /* Called via window::update_window();
     * doupdate() must be called after calling this function. */
    void window::draw_help()
    {
        if (sub_win_help_)
        {
            wclear(sub_win_help_.get());
            set_default_color(detail::CONTENT_COLOR, sub_win_content_);
            
            // todo: implement properly later
            wprintw(sub_win_help_.get(), "  This is the help section, which has yet to be implemented  \n  :(  ");
            
            touchline(stdscr, sub_win_help_.pos().y, sub_win_help_.pos().y);
            wnoutrefresh(sub_win_help_.get());
        }
    }
    
    /* Called via window::draw_content() or window::draw_content_selective();
     * touchline(), wnoutrefresh(), and doupdate() must be called after calling this function. */
    void window::draw_content_current_line_no_wrap(const int display_line, const tce& entry, int& cursor_x)
    {
        using detail::bounded_cast;
        
        const std::size_t prefix_length{ current_file_.get_entry_prefix_length(entry) * 4 };
        const std::size_t line_length{ current_file_.get_entry_line_length(entry) };
        
        int start_of_line_index{ 0 };
        
        const int cursor_limit{ sub_win_content_.size().x - ((sub_win_content_.size().x > 8) ? 3 : 2) };
        const int page_offset{ std::max(9, sub_win_content_.size().x) - 8 };
        
        /* move the cursor position and text so both are on screen */
        while (cursor_x > cursor_limit)
        {
            start_of_line_index += page_offset;
            cursor_x -= page_offset;
        }
        
        if (start_of_line_index >= bounded_cast<int>(prefix_length))
        {
            /* no need to draw line prefix */
            const std::size_t start{ start_of_line_index - prefix_length };
            const std::string line_content{ current_file_.get_entry_content(entry, start, sub_win_content_.size().x) };
            
            mvwprintw(sub_win_content_.get(), display_line, 0, "%s", line_content.c_str());
        }
        else
        {
            /* disregard first n characters of prefix */
            std::string line_prefix{ treenote::make_line_string_default(current_file_.get_entry_prefix(entry)) };
            treenote::utf8::drop_first_n_chars(line_prefix, start_of_line_index);
            const std::size_t content_length{ sub_win_content_.size().x + start_of_line_index - prefix_length };
            const std::string line_content{ current_file_.get_entry_content(entry, 0, content_length) };
            
            mvwprintw(sub_win_content_.get(), display_line, 0, "%s%s", line_prefix.c_str(), line_content.c_str());
        }
        
        if (start_of_line_index != 0)
        {
            /* line has been scrolled: replace first character with continuation */
            set_color(detail::CONT_ARROW_COLOR, sub_win_content_);
            mvwprintw(sub_win_content_.get(), display_line, 0, "<");
            unset_color(detail::CONT_ARROW_COLOR, sub_win_content_);
        }
        
        if (bounded_cast<int>(line_length + prefix_length) - start_of_line_index > sub_win_content_.size().x)
        {
            /* length of line content and prefix exceeds window size, replace final character with continuation */
            set_color(detail::CONT_ARROW_COLOR, sub_win_content_);
            mvwprintw(sub_win_content_.get(), display_line, sub_win_content_.size().x - 1, ">");
            unset_color(detail::CONT_ARROW_COLOR, sub_win_content_);
        }
    }
    
    /* Called via window::draw_content() or window::draw_content_selective();
     * touchline(), wnoutrefresh(), and doupdate() must be called after calling this function. */
    void window::draw_content_non_current_line_no_wrap(const int display_line, const treenote::tree::cache_entry& entry)
    {
        using detail::bounded_cast;
        
        const std::size_t prefix_length{ current_file_.get_entry_prefix_length(entry) * 4 };
        const std::string line_prefix{ treenote::make_line_string_default(current_file_.get_entry_prefix(entry)) };
        const std::size_t line_length{ current_file_.get_entry_line_length(entry) };
        const std::string line_content{ current_file_.get_entry_content(entry, 0, sub_win_content_.size().x - prefix_length) };
        
        mvwprintw(sub_win_content_.get(), display_line, 0, "%s%s", line_prefix.c_str(), line_content.c_str());
        
        if (bounded_cast<int>(line_length + prefix_length) > sub_win_content_.size().x)
        {
            /* length of line content and prefix exceeds window size, replace final character with continuation */
            set_color(detail::CONT_ARROW_COLOR, sub_win_content_);
            mvwprintw(sub_win_content_.get(), display_line, sub_win_content_.size().x - 1, ">");
            unset_color(detail::CONT_ARROW_COLOR, sub_win_content_);
        }
    }
    
    /* Called via window::update_window();
     * doupdate() must be called after calling this function. */
    void window::draw_content(coord& default_cursor_pos)
    {
        using detail::bounded_cast;
        using detail::status_bar_mode;
        
        if (sub_win_content_)
        {
            wclear(sub_win_content_.get());
            set_default_color(detail::CONTENT_COLOR, sub_win_content_);
    
            int display_line{ 0 };
            auto lc{ current_file_.get_lc_range(line_start_y_, sub_win_content_.size().y) };
            
            for (const auto& entry: lc)
            {
                if (display_line == default_cursor_pos.y and status_mode_ == status_bar_mode::DEFAULT)
                    draw_content_current_line_no_wrap(display_line, entry, default_cursor_pos.x);
                else
                    draw_content_non_current_line_no_wrap(display_line, entry);
                
                ++display_line;
            }
            
            touchline(sub_win_content_.get(), 0, sub_win_content_.size().y);
            wnoutrefresh(sub_win_content_.get());
        }
    }
    
    /* Called via window::update_window();
     * doupdate() must be called after calling this function.
     * Redraws at most 2 lines instead of the whole screen.   */
    void window::draw_content_selective(coord& default_cursor_pos)
    {
        using detail::bounded_cast;
        using detail::status_bar_mode;
        
        if (sub_win_content_)
        {
            // todo: possible optimisation: check if lines need to be redrawn in first place
            // e.g. when scrolling horizontally and not crossing a page boundary
            
            set_default_color(detail::CONTENT_COLOR, sub_win_content_);
            
            auto lc{ current_file_.get_lc_range(line_start_y_, sub_win_content_.size().y) };
            
            if (static_cast<std::size_t>(std::max(default_cursor_pos.y, previous_cursor_y)) >= std::ranges::size(lc))
            {
                /* error: abort attempt to selectively redraw and redraw entire screen instead */
                return window::draw_content(default_cursor_pos);
            }
            
            /* clear line and replace it with line */
            wmove(sub_win_content_.get(), default_cursor_pos.y, 0);
            wclrtoeol(sub_win_content_.get());
            draw_content_current_line_no_wrap(default_cursor_pos.y, *(std::ranges::begin(lc) + default_cursor_pos.y), default_cursor_pos.x);
            touchline(sub_win_content_.get(), default_cursor_pos.y, 1);
            
            /* assume that the screen position has not moved, since if it has, redraw_mask::RD_CONTENT
             * will have been set, so this function should not have been called in the first place      */
            if (previous_cursor_y != default_cursor_pos.y)
            {
                wmove(sub_win_content_.get(), previous_cursor_y, 0);
                wclrtoeol(sub_win_content_.get());
                draw_content_non_current_line_no_wrap(previous_cursor_y, *(std::ranges::begin(lc) + previous_cursor_y));
                touchline(sub_win_content_.get(), previous_cursor_y, 1);
            }
            
            wnoutrefresh(sub_win_content_.get());
        }
    }

    
    /* Update functions */
    
    /* Called via window::update_window();
     * window::draw_content_current_line_no_wrap() must be called beforehand
     * if (status_mode_ == status_bar_mode::DEFAULT), so that the x component
     * of the cursor_pos is within bounds                                     */
    void window::update_cursor_pos(coord& default_cursor_pos)
    {
        using detail::bounded_cast;
        using detail::status_bar_mode;
        
        if (status_mode_ == status_bar_mode::DEFAULT)
        {
            /* prevent cursor from being outside content area */
            default_cursor_pos = { std::min(default_cursor_pos.y, sub_win_content_.size().y - 1), std::min(default_cursor_pos.x, sub_win_content_.size().x - 1) };
            
            move(sub_win_content_.pos().y + default_cursor_pos.y, sub_win_content_.pos().x + default_cursor_pos.x);
        }
        else if (status_mode_ == status_bar_mode::PROMPT_CLOSE)
        {
            move(sub_win_status_.pos().y, std::min(sub_win_status_.pos().x + strings_.close_prompt.length() + 1, sub_win_status_.size().x - 1));
        }
        else if (status_mode_ == status_bar_mode::PROMPT_FILENAME)
        {
            /* handle long filenames with a scrolling system */
            
            int cursor_x{ bounded_cast<int>(prompt_info_.cursor_pos) };
            
            const int line_start_pos{ std::max(std::min(strings_.file_prompt.length() + 2, sub_win_status_.size().x - 4), 2) };
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
        using detail::bounded_cast;
        
        status_msg_.clear(screen_redraw_);
        curs_set(0);
        
        coord cursor_pos{ bounded_cast<int>(current_file_.cursor_y()) - bounded_cast<int>(line_start_y_),
                          bounded_cast<int>(current_file_.cursor_x() + current_file_.cursor_current_indent_lvl() * 4) };
        
        if (screen_redraw_.has_mask(redraw_mask::RD_ALL))
            clear();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_TOP))
            draw_top();
        
        if (screen_redraw_.has_mask(redraw_mask::RD_CONTENT))
            draw_content(cursor_pos);
        else if (not word_wrap_enabled)
            draw_content_selective(cursor_pos);
        
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
        update_viewport_clamp_lower();
        
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
    
    void window::update_window_sizes()
    {
        using detail::sub_window;
        
        static constexpr int top_height{ 1 };
        static constexpr int help_height{ 2 };
        static constexpr int status_height{ 1 };
        static constexpr int threshold1{ 5 };
        static constexpr int threshold2{ 2 };
        static constexpr int threshold3{ 1 };
        
        screen_dimensions_ = { .y = getmaxy(stdscr), .x = getmaxx(stdscr) };
        bool show_status{ true };
        bool show_top{ true };
        bool show_help{ show_help_bar_ };
        
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
        content_height -= static_cast<int>(show_help) * help_height;
        
        if (show_top)
            sub_win_top_ = sub_window{ { top_height, screen_dimensions_.x },
                                       { 0,          0 } };
        else if (sub_win_top_)
            sub_win_top_ = sub_window{};
        
        sub_win_content_ = sub_window{ { content_height, screen_dimensions_.x - 1 },
                                       { static_cast<int>(show_top) * top_height, 1 } };
        
        if (show_status)
            sub_win_status_ = sub_window{ { 1, screen_dimensions_.x },
                                          { screen_dimensions_.y - status_height - (static_cast<int>(show_help) * help_height), 0 } };
        else if (sub_win_status_)
            sub_win_status_ = sub_window{};
        
        if (show_help)
            sub_win_help_ = sub_window{ { 2, screen_dimensions_.x },
                                        { screen_dimensions_.y - help_height, 0 } };
        else if (sub_win_help_)
            sub_win_help_ = sub_window{};
        
        update_viewport_pos();
        screen_redraw_.set_all();
    }


    /* Main function for main_window */
    
    int window::operator()(std::deque<std::string>& filenames)
    {
        using detail::redraw_mask;
        
        do
        {
            if (not filenames.empty())
            {
                current_filename_ = filenames.front();
                filenames.pop_front();
            }
            
            tree_open();
            update_screen();
            
            detail::char_read_helper crh{};
            
            for (bool exit{ false }; !exit;)
            {
                crh.extract_char();
                
                /* act on input */
                if (crh.is_resize())
                {
                    /* update overall window size information */
                    
                    update_window_sizes();
                    status_msg_.force_clear(screen_redraw_);
                }
                else if (crh.is_command())
                {
                    /* command key sent: execute instruction */
                    
                    crh.extract_second_char();
                    auto action{ crh.get_action(keymap_) };
                    
                    switch (action)
                    {
                        case actions::show_help:
                            // todo: implement
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
                        case actions::cut_node:
                            if (current_file_.node_cut() != 0)
                                status_msg_.set_message(screen_redraw_, strings_.cut_error);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::copy_node:
                            if (current_file_.node_copy() != 0)
                                status_msg_.set_message(screen_redraw_, strings_.copy_error);
                            break;
                        case actions::paste_node:
                            if (current_file_.node_paste_default() != 0)
                                status_msg_.set_warning(screen_redraw_, strings_.paste_error);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            break;
                        case actions::paste_node_abv:
                            if (current_file_.node_paste_above() != 0)
                                status_msg_.set_warning(screen_redraw_, strings_.paste_error);
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
                                    status_msg_.set_message(screen_redraw_, strings_.nothing_delete);
                                if (result == 2)
                                    status_msg_.set_warning(screen_redraw_, strings_.delete_prevent);
                                screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                                update_viewport_pos();
                            }
                            break;
                        case actions::delete_node_rec:
                            {
                                auto result = current_file_.node_delete_rec();
                                if (result == 1)
                                    status_msg_.set_message(screen_redraw_, strings_.nothing_delete);
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

                        case actions::cursor_pos:
                            display_tree_pos();
                            break;
                        case actions::cursor_left:
                            current_file_.cursor_mv_left();
                            break;
                        case actions::cursor_right:
                            current_file_.cursor_mv_right();
                            break;
                        case actions::cursor_up:
                            current_file_.cursor_mv_up(1 + crh.extract_multiple_of_same_action(actions::cursor_up, keymap_));
                            update_viewport_pos();
                            break;
                        case actions::cursor_down:
                            current_file_.cursor_mv_down(1 + crh.extract_multiple_of_same_action(actions::cursor_down, keymap_));
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
                            line_start_y_ -= std::min(line_start_y_, 1uz);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_cursor_pos();
                            break;
                        case actions::scroll_down:
                            line_start_y_ += 1;
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
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
                            line_start_y_ = current_file_.cursor_y() -
                                    std::min(current_file_.cursor_y(), static_cast<std::size_t>(sub_win_content_.size().y) / 2);
                            screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                            update_viewport_clamp_lower();
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
                            status_msg_.set_warning(screen_redraw_, strings_.dbg_unknwn_act(strings_.unbound_key.c_str(), crh.keyname()));
                            break;
                            
                        default:
                            // temporary code: todo: remove when done
                            status_msg_.set_warning(screen_redraw_, strings_.dbg_unimp_act(std::to_underlying(action), crh.keyname()));
                            break;
                    }

                }
                else
                {
                    /* key is readable input: send onwards */
                    
                    std::string inserted{ detail::wint_to_string(crh.value()) };
                    crh.extract_more_readable_chars(inserted);
    
                    current_file_.line_insert_text(inserted);
                    
                    // below code is for testing only
                    status_msg_.set_message(screen_redraw_, strings_.dbg_pressed(inserted));
                    
                    screen_redraw_.add_mask(redraw_mask::RD_CONTENT);
                }
                
                update_screen();
            }
            
        }
        while (not filenames.empty());
        
        return 0;
    }
}