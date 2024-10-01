// read_helper.h

#pragma once

#include <string>

#include <curses.h>

#include "keymap.h"

namespace treenote_tui
{
    /* Class to help with reading keys from ncurses */
    class char_read_helper
    {
    public:
        [[nodiscard]] key::input_t value() const noexcept;
        [[nodiscard]] std::string value_string() const;
        [[nodiscard]] std::string key_name() const;
        [[nodiscard]] bool is_resize() const noexcept;
        [[nodiscard]] bool is_command() const noexcept;
        [[nodiscard]] bool is_mouse() const noexcept;
        void extract_char();
        void extract_second_char();
        void extract_more_readable_chars(std::string& inserted);
        
        [[nodiscard]] actions get_action(const keymap::map_t& keymap) const noexcept;
        [[nodiscard]] std::size_t extract_multiple_of_same_action(actions target, const keymap::map_t& keymap);
        
        void clear();
    
    private:
        static constexpr wint_t key_escape{ 0x1b };
        void force_extract_char();
        
        wint_t input_{ 0 };
        wint_t second_input_{ 0 };
        int input_info_{ 0 };
        bool carry_over_{ false };
    };
}
