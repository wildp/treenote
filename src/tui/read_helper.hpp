// tui/read_helper.hpp
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

#include <string>

#include <curses.h>

#include "keymap.hpp"

namespace tred::tui
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
