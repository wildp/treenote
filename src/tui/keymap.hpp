// tui/keymap.hpp
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

#include <climits>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <curses.h>

namespace treenote::tui
{
    namespace key
    {
        namespace detail
        {
            template<std::size_t I>
            requires (I * 2 <= sizeof(std::uintmax_t))
            using double_width_int = std::conditional_t<I == 1, std::uint_least16_t,
                                        std::conditional_t<I == 2, std::uint_least32_t,
                                            std::conditional_t<I <= 4, std::uint_least64_t, std::uintmax_t>>>;
            
            template<std::unsigned_integral T>
            struct input
            {
                using type = double_width_int<sizeof(T)>;
                
                static constexpr int bit_count{ sizeof(T) * CHAR_BIT };
                
                static constexpr type make(T first, T second)
                {
                    return static_cast<type>(first) | (static_cast<type>(second) << bit_count);
                }
                
                static constexpr std::pair<T, T> unmake(type pair)
                {
                    return std::make_pair(pair & ((type{ 1 } << bit_count) - 1), pair >> bit_count);
                }
            };
        }
        
        using input_t = detail::input<wint_t>::type;
        
        [[nodiscard]] std::string name_of(wint_t first, wint_t second);
        [[nodiscard]] std::string name_of(input_t key);
    }
    
    enum class actions : std::int8_t
    {
        /* Special action to act as default */
        
        unknown = 0,
        
        /* General actions */
        
        show_help,
        close_tree,
        write_tree,
        
        save_file,
        
        suspend,
        
        /* Editing */
        
        cut_node,
        copy_node,
        paste_node,
        paste_node_abv,
        
        undo,
        redo,
        
        cursor_pos,
        go_to,

        indent_node,
        unindent_node,
        
        raise_node,
        lower_node,
        reorder_backwards,
        reorder_forwards,
        
        raise_node_spc,
        lower_node_spc,
        transfer_forwards,
        transfer_backwards,
        
        insert_node_def,
        insert_node_ent,
        insert_node_abv,
        insert_node_bel,
        insert_node_chi,
        
        delete_node_chk,
        delete_node_rec,
        delete_node_spc,
        
        /* Direct cursor movement */
        
        cursor_left,
        cursor_right,
        cursor_up,
        cursor_down,
        cursor_prev_w,
        cursor_next_w,
        cursor_sol,
        cursor_eol,
        cursor_sof,
        cursor_eof,
        
        /* Page movement (cursor is moved if necessary) */
        
        page_up,
        page_down,
        scroll_up,
        scroll_down,
        center_view,
        
        /* Node traversal cursor movement */
        
        node_parent,
        node_child,
        node_next,
        node_prev,
        
        /* Line-based input related */
        
        newline,
        backspace,
        delete_char,
        delete_word_b,
        delete_word_f,
        
        /* Prompt-exclusive actions */
        
        prompt_cancel,
        prompt_yes,
        prompt_no,
        
    };
    
    
    namespace detail
    {
        /* forward declaration for help_bar_content in window_detail */
        struct help_bar_content;
    }
    
    class keymap
    {
    public:
        using map_t = std::unordered_map<key::input_t, actions>;
        using bindings_t = std::vector<std::vector<std::string>>;
        
        keymap() = default;
        
        /* note: this function may only be called after initscr() is called */
        static keymap make_default();
        
        [[nodiscard]] std::string key_for(actions action) const;
        
        [[nodiscard]] map_t make_editor_keymap() const;
        [[nodiscard]] map_t make_quit_prompt_keymap() const;
        [[nodiscard]] map_t make_help_screen_keymap() const;
        [[nodiscard]] map_t make_filename_editor_keymap() const;
        [[nodiscard]] map_t make_goto_editor_keymap() const;
        
        [[nodiscard]] static detail::help_bar_content make_editor_help_bar();
        [[nodiscard]] static detail::help_bar_content make_quit_prompt_help_bar();
        [[nodiscard]] static detail::help_bar_content make_help_screen_help_bar();
        [[nodiscard]] static detail::help_bar_content make_filename_editor_help_bar();
        [[nodiscard]] static detail::help_bar_content make_goto_editor_help_bar();
        
        [[nodiscard]] bindings_t make_key_bindings() const;
        
    private:
        std::map<actions, std::vector<key::input_t>> map_;
    };
}