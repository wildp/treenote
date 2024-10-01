// read_helper.cpp

#include "read_helper.h"

#include <cuchar>

namespace treenote_tui
{
    namespace
    {
        /* Create new string with a single (mb) character */
        std::string wint_to_string(const wint_t char_input)
        {
            std::mbstate_t mbstate{};
            std::string buf(MB_CUR_MAX, '\0');
            const std::size_t len{ std::c32rtomb(buf.data(), static_cast<char32_t>(char_input), &mbstate) };
            buf.resize(len);
            return buf;
        }
        
        /* Append a single (mb) character to a std::string */
        void append_wint_to_string(std::string& str, const wint_t char_input)
        {
            std::mbstate_t mbstate{};
            const std::size_t old_len{ str.length() };
            str.resize(old_len + MB_CUR_MAX);
            const std::size_t len{ std::c32rtomb(&(str[old_len]), static_cast<char32_t>(char_input), &mbstate) };
            str.resize(old_len + len);
        }
    }
    
    /* Implementation for reading characters */
    
    key::input_t char_read_helper::value() const noexcept
    {
        if (second_input_ == 0)
            return input_;
        else
            return key::detail::input<wint_t>::make(input_, second_input_);
    }
    
    std::string char_read_helper::value_string() const
    {
       return wint_to_string(value());
    }
    
    std::string char_read_helper::key_name() const
    {
        return key::name_of(input_, second_input_);
    }
    
    /* this must always be checked first before is_command is checked */
    bool char_read_helper::is_resize() const noexcept
    {
        return (input_info_ == KEY_CODE_YES and input_ == KEY_RESIZE);
    }
    
    bool char_read_helper::is_command() const noexcept
    {
        return (input_ < ' ' or input_info_ == KEY_CODE_YES);
    }
    
    /* this should be checked after is_command is checked */
    bool char_read_helper::is_mouse() const noexcept
    {
        return (input_ == KEY_MOUSE);
    }
    
    /* Reads another char, blocking until a char is read */
    void char_read_helper::extract_char()
    {
        /* do not get new keycode if another key has been got but not acted on */
        if (carry_over_)
            carry_over_ = false;
        else
            force_extract_char();
    }
    
    void char_read_helper::extract_second_char()
    {
        /* extract second key if key press is alt or esc */
        if (input_ == key_escape)
        {
            timeout(0);
            if (get_wch(&second_input_) == ERR)
                second_input_ = 0;
            timeout(-1);
        }
    }
    
    /* Continues extracting readable chars, if any.
     * NOTE: This should only be called after extracting a readable char. */
    void char_read_helper::extract_more_readable_chars(std::string& inserted)
    {
        timeout(0);
        for (bool loop{ true }; loop;)
        {
            force_extract_char();
            if (input_info_ == ERR)
            {
                loop = false;
            }
            else if (is_resize() or is_command())
            {
                loop = false;
                carry_over_ = true;
            }
            else if (input_ == key_escape)
            {
                unget_wch('\x1b');
                loop = false;
            }
            else
            {
                append_wint_to_string(inserted, input_);
            }

        }
        timeout(-1);
    }
    
    actions char_read_helper::get_action(const keymap::map_t& keymap) const noexcept
    {
        actions action{};
        auto val{ value() };
        if (keymap.contains(val))
            action = keymap.at(val);
        return action;
    }
    
    std::size_t char_read_helper::extract_multiple_of_same_action(actions target, const keymap::map_t& keymap)
    {
        std::size_t count{ 0 };
        timeout(0);
        for (bool loop{ true }; loop;)
        {
            force_extract_char();
            if (input_info_ == ERR)
            {
                loop = false;
            }
            else if (input_ == key_escape)
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
        timeout(-1);
        return count;
    }
    
    void char_read_helper::force_extract_char()
    {
        input_info_ = get_wch(&input_);
        second_input_ = 0;
    }
    
    void char_read_helper::clear()
    {
        timeout(0);
        for (bool loop{ true }; loop;)
        {
            force_extract_char();
            if (input_info_ == ERR)
            {
                loop = false;
            }
            else if (is_resize())
            {
                loop = false;
                carry_over_ = true;
            }
        }
        timeout(-1);
    }

}
