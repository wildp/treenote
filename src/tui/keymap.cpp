// tui/keymap.cpp
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


#include "keymap.hpp"

#include <algorithm>
#include <cstdlib>
#include <ranges>
#include <stdexcept>

#include "strings.hpp"
#include "window_detail.hpp"

namespace tred::tui::key
{
    namespace
    {
        /* Constants and modifier keys */
        
        constexpr input_t control_modifier{ 0x1f };
        constexpr input_t escape{ 0x1b };
        
        constexpr input_t ctrl(const wint_t key)
        {
            return key & control_modifier;
        }
        
        constexpr input_t alt(const wint_t key)
        {
            return detail::input<wint_t>::make(escape, key);
        }

        /* WARNING: this should generally be avoided except where necessary (e.g. for Alt + Enter) */
        constexpr input_t ctrl_alt(const wint_t key)
        {
            return detail::input<wint_t>::make(escape, key & control_modifier);
        }
        
        constexpr input_t f(const wint_t no)
        {
            return KEY_F(no);
        }

        /* Keys following this are hidden from the help screen */
        constexpr input_t hide_keys{ 0 };
        
        /* Definitions for special keys */
        
        namespace spc
        {
            enum key_name : std::uint8_t
            {
                up          = 0x1,
                down        = 0x2,
                right       = 0x3,
                left        = 0x4,

                home        = 0x5,
                end         = 0x6,

                page_down   = 0x7,
                page_up     = 0x8,

                ins         = 0x9,
                del         = 0xA,

                enter       = 0xB,
                backspace   = 0xC,
                tab         = 0xD,

                shift       = 0b1000'0000,
                ctrl        = 0b0100'0000,
                alt         = 0b0010'0000
            };
            
            
            constexpr key_name operator|(const key_name lhs, const key_name rhs) noexcept
            {
                return static_cast<key_name>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
            }
        }
        
        
        /* Implementation for special keys */
        
        /* WARNING: This function must only be called after initscr() is called */
        input_t extended_key(const char* definition)
        {
            static int key_code_generator{ 0 };
            auto keycode{ key_defined(definition) };
            
            if (keycode <= 0)
            {
                if (keycode < 0)
                    define_key(definition, 0);
                
                while (true)
                {
                    char* str{ keybound(key_code_generator, 0) };
                    
                    if (str == nullptr)
                        break;
                    
                    std::free(str);
                    ++key_code_generator;
                }
                
                define_key(definition, key_code_generator);
                keycode = key_defined(definition);
            }
            
            return keycode;
        }
        
        constexpr input_t get(const spc::key_name key)
        {
            switch (static_cast<std::uint8_t>(key))
            {
                case spc::up:
                    return KEY_UP;
                case spc::shift | spc::up:
                    return KEY_SR;
                case spc::ctrl | spc::up:
                    return extended_key("\x1b[1;5A");
                case spc::alt | spc::up:
                    return extended_key("\x1b[1;3A");
                case spc::shift | spc::alt | spc::up:
                    return extended_key("\x1b[1;4A");
                    
                case spc::down:
                    return KEY_DOWN;
                case spc::shift | spc::down:
                    return KEY_SF;
                case spc::ctrl | spc::down:
                    return extended_key("\x1b[1;5B"); 
                case spc::alt | spc::down:
                    return extended_key("\x1b[1;3B"); 
                case spc::shift | spc::alt | spc::down:
                    return extended_key("\x1b[1;4B");
                    
                case spc::right:
                    return KEY_RIGHT;
                case spc::shift | spc::right:
                    return KEY_SRIGHT;
                case spc::ctrl | spc::right:
                    return extended_key("\x1b[1;5C");
                case spc::alt | spc::right:
                    return extended_key("\x1b[1;3C");
                case spc::shift | spc::alt | spc::right:
                    return extended_key("\x1b[1;4C");
                    
                case spc::left:
                    return KEY_LEFT;
                case spc::shift | spc::left:
                    return KEY_SLEFT;
                case spc::ctrl | spc::left:
                    return extended_key("\x1b[1;5D");
                case spc::alt | spc::left:
                    return extended_key("\x1b[1;3D");
                case spc::shift | spc::alt | spc::left:
                    return extended_key("\x1b[1;4D");
                    
                case spc::home:
                    return KEY_HOME;
                case spc::shift | spc::home:
                    return KEY_SHOME;
                case spc::ctrl | spc::home:
                    return extended_key("\x1b[1;5H");
                case spc::alt | spc::home:
                    return extended_key("\x1b[1;3H");
                
                case spc::end:
                    return KEY_END;
                case spc::shift | spc::end:
                    return KEY_SEND;
                case spc::ctrl | spc::end:
                    return extended_key("\x1b[1;5F");
                case spc::alt | spc::end:
                    return extended_key("\x1b[1;3F");
                    
                case spc::page_down:
                    return KEY_NPAGE;
                case spc::shift | spc::page_down:
                    return KEY_SNEXT;
                    
                case spc::page_up:
                    return KEY_PPAGE;
                case spc::shift | spc::page_up:
                    return KEY_SPREVIOUS;
                    
                case spc::ins:
                    return KEY_IC;
                case spc::shift | spc::ins:
                    return KEY_SIC;
                case spc::ctrl | spc::ins:
                    return extended_key("\x1b[2;5~");
                case spc::alt | spc::ins:
                    return extended_key("\x1b[2;3~");
                case spc::shift | spc::alt | spc::ins:
                    return extended_key("\x1b[2;4~");
                
                case spc::del:
                    return KEY_DC;
                case spc::shift | spc::del:
                    return KEY_SDC;
                case spc::ctrl | spc::del:
                    return extended_key("\x1b[3;5~");
                case spc::alt | spc::del:
                    return extended_key("\x1b[3;3~");
                case spc::shift | spc::alt | spc::del:
                    return extended_key("\x1b[3;4~");
                
                case spc::enter:
                    return KEY_ENTER;
                case spc::alt | spc::enter:
                    return alt(KEY_ENTER);

                case spc::backspace:
                    return KEY_BACKSPACE;
                case spc::alt | spc::backspace:
                    return alt(KEY_BACKSPACE);
                
                case spc::tab:
                    return KEY_STAB;
                case spc::shift | spc::tab:
                    return KEY_BTAB;
                
                default:
                    throw std::runtime_error("Invalid key_name combination");
            }
        }
        
    }
}       
        
namespace tred::tui
{   
    keymap keymap::make_default()
    {
        using namespace key;
        
        keymap k{};
        
        k.map_[actions::show_help]          = { ctrl('g'), f(1) };
        k.map_[actions::close_tree]         = { ctrl('x'), f(2) };
        k.map_[actions::write_tree]         = { ctrl('o'), f(3) };
        
        k.map_[actions::save_file]          = { ctrl('s') };
        k.map_[actions::suspend]            = { ctrl('z') };
        
        k.map_[actions::cut_node]           = { ctrl('k'), f(9) };
        k.map_[actions::copy_node]          = { alt('6'), alt('^') };
        k.map_[actions::paste_node]         = { ctrl('u'), f(10) };

        k.map_[actions::undo]               = { alt('u') };
        k.map_[actions::redo]               = { alt('r') };
        
        k.map_[actions::cursor_pos]         = { ctrl('c'), f(11) };
        k.map_[actions::go_to]              = { ctrl('_'), alt('g') };
        
        k.map_[actions::indent_node]        = { ctrl('I') , get(spc::tab) };
        k.map_[actions::unindent_node]      = { alt('I') , get(spc::shift | spc::tab) };
        
        k.map_[actions::raise_node]         = { get(spc::alt | spc::left),  alt('b') };
        k.map_[actions::lower_node]         = { get(spc::alt | spc::right), alt('f') };
        k.map_[actions::reorder_backwards]  = { get(spc::alt | spc::up),    alt('p') };
        k.map_[actions::reorder_forwards]   = { get(spc::alt | spc::down),  alt('n') };

        // k.map_[actions::raise_node_spc]     = { get(spc::alt | spc::shift | spc::left) };
        // k.map_[actions::lower_node_spc]     = { get(spc::alt | spc::shift | spc::right) };
        // k.map_[actions::transfer_forwards]  = { get(spc::alt | spc::shift | spc::up) };
        // k.map_[actions::transfer_backwards] = { get(spc::alt | spc::shift | spc::down) };

        k.map_[actions::insert_node_ent]    = { get(spc::alt | spc::enter), hide_keys, ctrl_alt('m') };
        k.map_[actions::insert_node_def]    = { get(spc::ins) };
        k.map_[actions::insert_node_chi]    = { get(spc::ctrl | spc::ins) };
        k.map_[actions::insert_node_abv]    = { get(spc::shift | spc::alt | spc::ins) }; /* Shift-Ins is usually terminal paste */
        k.map_[actions::insert_node_bel]    = { get(spc::alt | spc::ins) };
        
        k.map_[actions::delete_node_chk]    = { get(spc::shift | spc::del) };
        k.map_[actions::delete_node_rec]    = { get(spc::alt | spc::del) };
        k.map_[actions::delete_node_spc]    = { get(spc::shift | spc::alt | spc::del) };
        
        k.map_[actions::cursor_left]        = { get(spc::left),  ctrl('b') };
        k.map_[actions::cursor_right]       = { get(spc::right), ctrl('f') };
        k.map_[actions::cursor_up]          = { get(spc::up),    ctrl('p') };
        k.map_[actions::cursor_down]        = { get(spc::down),  ctrl('n') };
        k.map_[actions::cursor_prev_w]      = { get(spc::ctrl | spc::left),  alt(' ') };
        k.map_[actions::cursor_next_w]      = { get(spc::ctrl | spc::right), ctrl(' ') };
        k.map_[actions::cursor_sol]         = { ctrl('a'), get(spc::home) };
        k.map_[actions::cursor_eol]         = { ctrl('e'), get(spc::end) };
        k.map_[actions::cursor_sof]         = { alt('\\'), get(spc::ctrl | spc::home) };
        k.map_[actions::cursor_eof]         = { alt('/'),  get(spc::ctrl | spc::end) };
        
        k.map_[actions::scroll_up]          = { get(spc::ctrl | spc::up),   alt('-'), alt('_') };
        k.map_[actions::scroll_down]        = { get(spc::ctrl | spc::down), alt('+'), alt('=') };
        k.map_[actions::page_up]            = { ctrl('y'), get(spc::page_up) };
        k.map_[actions::page_down]          = { ctrl('v'), get(spc::page_down) };
        
        k.map_[actions::center_view]        = { ctrl('l') };
        
        k.map_[actions::node_parent]        = { get(spc::shift | spc::left),  };
        k.map_[actions::node_child]         = { get(spc::shift | spc::right), };
        k.map_[actions::node_prev]          = { get(spc::shift | spc::up),    };
        k.map_[actions::node_next]          = { get(spc::shift | spc::down),  };

        k.map_[actions::newline]            = { ctrl('m'), get(spc::enter) };
        k.map_[actions::backspace]          = { ctrl('h'), get(spc::backspace) };
        k.map_[actions::delete_char]        = { ctrl('d'), get(spc::del) };
        k.map_[actions::delete_word_b]      = { get(spc::alt | spc::backspace), hide_keys, ctrl_alt('h') };
        k.map_[actions::delete_word_f]      = { get(spc::ctrl | spc::del) };
        
        k.map_[actions::prompt_cancel]      = { ctrl('c') };
        k.map_[actions::prompt_yes]         = { 'Y', 'y' };
        k.map_[actions::prompt_no]          = { 'N', 'n' };
        
        return k;
    }
    
    keymap::map_t keymap::make_editor_keymap() const
    {
        map_t result;
        
        for (const auto& [action, val]: map_)
            if (action != actions::prompt_cancel and action != actions::prompt_yes and action != actions::prompt_no)
                for (const auto input: val)
                    if (input != key::hide_keys)
                        result[input] = action;
        
        return result;
    }
    
    keymap::map_t keymap::make_filename_editor_keymap() const
    {
        map_t result;
        
        const std::vector a_vec{ actions::newline, actions::backspace, actions::delete_char,
                                 actions::cursor_left, actions::cursor_right, actions::prompt_cancel };
        
        for (const auto action: a_vec)
            for (const auto key: map_.at(action))
                if (key != key::hide_keys)
                    result[key] = action;
        
        return result;
    }
    
    keymap::map_t keymap::make_quit_prompt_keymap() const
    {
        map_t result;
        
        const std::vector a_vec{ actions::prompt_yes, actions::prompt_no, actions::prompt_cancel };
        
        for (const auto action: a_vec)
            for (const auto& key: map_.at(action))
                if (key != key::hide_keys)
                    result[key] = action;
        
        return result;
    }
    
    keymap::map_t keymap::make_help_screen_keymap() const
    {
        map_t result;
        
        const std::vector a_vec{ actions::cursor_up, actions::cursor_down, actions::page_up, actions::page_down,
                                 actions::scroll_up, actions::scroll_down, actions::cursor_sof, actions::cursor_eof,
                                 actions::close_tree, actions::center_view  };
        
        for (const auto action: a_vec)
            for (const auto key: map_.at(action))
                if (key != key::hide_keys)
                    result[key] = action;
        
        return result;
    }
    
    keymap::map_t keymap::make_goto_editor_keymap() const
    {
        map_t result;
        
        const std::vector a_vec{ actions::newline, actions::backspace, actions::delete_char,
                                 actions::cursor_left, actions::cursor_right, actions::prompt_cancel };
        
        for (const auto& action: a_vec)
            for (const auto key: map_.at(action))
                if (key != key::hide_keys)
                    result[key] = action;
        
        return result;
    }
    
    namespace key
    {
        namespace
        {
            [[nodiscard]] std::string decode_extended_ctrl_key(std::string_view input)
            {
                std::string new_result;
                input.remove_prefix(1);
                
                if (input.ends_with("1"))
                    new_result = "";
                else if (input.ends_with("2"))
                    new_result += "S-";
                else if (input.ends_with("3"))
                    new_result += "M-";
                else if (input.ends_with("4"))
                    new_result += "S-M-";
                else if (input.ends_with("5"))
                    new_result += "^";
                else if (input.ends_with("6"))
                    new_result += "S-^";
                else if (input.ends_with("7"))
                    new_result += "M-^";
                else if (input.ends_with("8"))
                    new_result += "S-M-^";
                else
                    return "";
                
                if (input.starts_with("UP"))
                    new_result += "▲";
                else if (input.starts_with("DN"))
                    new_result += "▼";
                else if (input.starts_with("RIT"))
                    new_result += "▶";
                else if (input.starts_with("LFT"))
                    new_result += "◀";
                else if (input.starts_with("HOM"))
                    new_result += "Home";
                else if (input.starts_with("END"))
                    new_result += "End";
                else if (input.starts_with("NXT"))
                    new_result += "PgDn";
                else if (input.starts_with("PRV"))
                    new_result += "PgUp";
                else if (input.starts_with("IC"))
                    new_result += "Ins";
                else if (input.starts_with("DC"))
                    new_result += "Del";
                else
                    return "";
                
                // other codes: "Tab", "Space", "Enter", "Bsp"
                
                return new_result;
            }
            
            [[nodiscard]] std::string short_name_of(const wint_t key)
            {
                switch (key)
                {
                    /* add more keys as necessary */
                    
                    case KEY_UP:
                        return "▲";
                    case KEY_SR:
                        return "S-▲";
                    case KEY_DOWN:
                        return "▼";
                    case KEY_SF:
                        return "S-▼";
                    case KEY_LEFT:
                        return "◀";
                    case KEY_SLEFT:
                        return "S-◀";
                    case KEY_RIGHT:
                        return "▶";
                    case KEY_SRIGHT:
                        return "S-▶";
                    
                    case KEY_NPAGE:
                        return "PgDn";
                    case KEY_SNEXT:
                        return "S-PgDn";
                        
                    case KEY_PPAGE:
                        return "PgUp";
                    case KEY_SPREVIOUS:
                        return "S-PgUp";
                    
                    case KEY_HOME:
                        return "Home";
                    case KEY_SHOME:
                        return "S-Home";
                    
                    case KEY_END:
                        return "End";
                    case KEY_SEND:
                        return "S-End";
                    
                    case KEY_ENTER:
                        return "Enter";
                    case KEY_BACKSPACE:
                        return "Bsp";
                    
                    case KEY_IC:
                        return "Ins";
                    case KEY_SIC:
                        return "S-Ins";
                    
                    case KEY_DC:
                        return "Del";
                    case KEY_SDC:
                        return "S-Del";
                    
                    case KEY_STAB:
                        return "Tab";
                    case KEY_BTAB:
                        return "BTab";
                        
                    default:
                        break;
                }
                
                if (KEY_F0 <= key and key <= KEY_F(63))
                {
                    return "F" + std::to_string(key - KEY_F0);
                }
                
                return "";
            }
        }
        
        std::string name_of(const wint_t first, const wint_t second)
        {
            if (first == 0 and second == 0)
                return "";
            
            std::string result;
            
            if (first == escape and second != 0)
            {
                std::locale l{};
                result += "M-";
                result += keyname(static_cast<int>(second));
                std::ranges::for_each(result.begin(), result.end(), [&l](char& c) { c = std::toupper(c, l); });
            }
            else
            {
                result += keyname(static_cast<int>(first));
                if (second != 0)
                    result += keyname(static_cast<int>(second));
            }
            
            if (result.ends_with(' '))
            {
                result.pop_back();
                result += "Space";
            }
            else if (result.starts_with("k") and result.size() > 1)
            {
                auto new_result{ decode_extended_ctrl_key(result) };
                
                if (not new_result.empty())
                    return new_result;
                else
                    return "(unrecognised1: " + result + ")";
            }
            else if (result.starts_with("KEY_"))
            {
                auto new_result{ short_name_of(first) };
                
                if (not new_result.empty())
                    return new_result;
                else
                    return "(unrecognised2: " + result + ")";
            }
            else if (result.starts_with("M-KEY_"))
            {
                std::string new_result{ "M-" };
                new_result += short_name_of(second);
                
                if (new_result != "M-")
                    return new_result;
                else
                    return "(unrecognised3: " + result + ")";
            }
            
            return result;
        }
        
        std::string name_of(const input_t key)
        {
            const auto [first, second]{ detail::input<wint_t>::unmake(key) };
            return name_of(first, second);
        }
    }
     
    std::string keymap::key_for(const actions action) const
    {
        if (not map_.contains(action))
            return "";
         
        const auto& vec{ map_.at(action) };
        
        if (vec.empty())
            return "";
        
        return key::name_of(vec[0]);
    }
    
    detail::help_bar_content keymap::make_editor_help_bar()
    {
        detail::help_bar_content bar;
        
        bar.entries.emplace_back(actions::show_help, strings::action_help);
        bar.entries.emplace_back(actions::close_tree, strings::action_exit);
        bar.entries.emplace_back(actions::write_tree, strings::action_write);
        bar.entries.emplace_back(actions::save_file, strings::action_save);
        
        bar.entries.emplace_back(actions::cut_node, strings::action_cut);
        bar.entries.emplace_back(actions::paste_node, strings::action_paste);
        
        bar.entries.emplace_back(actions::cursor_pos, strings::action_location);
        bar.entries.emplace_back(actions::go_to, strings::action_go_to);
        
        bar.entries.emplace_back(actions::undo, strings::action_undo);
        bar.entries.emplace_back(actions::redo, strings::action_redo);
        
        bar.entries.emplace_back(actions::copy_node, strings::action_copy);
        bar.entries.emplace_back(actions::insert_node_def, strings::action_insert_node);
        
        bar.entries.emplace_back(actions::insert_node_chi, strings::action_insert_child);
        bar.entries.emplace_back(actions::delete_node_chk, strings::action_delete_node);
        
        return bar;
    }
    
    detail::help_bar_content keymap::make_quit_prompt_help_bar()
    {
        detail::help_bar_content bar;
        bar.last_is_bottom = true;
        bar.min_width = 9;
        bar.max_width = 16;
        
        bar.entries.emplace_back(actions::prompt_yes, strings::action_yes);
        bar.entries.emplace_back(actions::prompt_no, strings::action_no);
        bar.entries.emplace_back(actions::prompt_cancel, strings::action_cancel);
        
        return bar;
    }
     
    detail::help_bar_content keymap::make_help_screen_help_bar()
    {
        detail::help_bar_content bar;
        
        bar.entries.emplace_back(actions::center_view, strings::action_refresh);
        bar.entries.emplace_back(actions::close_tree, strings::action_close);
        bar.entries.emplace_back(actions::cursor_up, strings::action_previous_line);
        bar.entries.emplace_back(actions::cursor_down, strings::action_next_line);
        bar.entries.emplace_back(actions::page_up, strings::action_previous_page);
        bar.entries.emplace_back(actions::page_down, strings::action_next_page);
        bar.entries.emplace_back(actions::cursor_sof, strings::action_first_line);
        bar.entries.emplace_back(actions::cursor_eof, strings::action_last_line);
        
        return bar;
    }
    
    detail::help_bar_content keymap::make_filename_editor_help_bar()
    {
        detail::help_bar_content bar;
        
        bar.entries.emplace_back(actions::prompt_cancel, strings::action_cancel);
        
        return bar;
    }
    
    detail::help_bar_content keymap::make_goto_editor_help_bar()
    {
        detail::help_bar_content bar;
        
        bar.entries.emplace_back(actions::prompt_cancel, strings::action_cancel);
        
        return bar;
    }
     
    keymap::bindings_t keymap::make_key_bindings() const
    {
        bindings_t result;
        
        for (const auto& entry : strings::help_strings)
        {
            result.emplace_back();
            
            if (entry and map_.contains(entry.action()))
                for (const auto k: map_.at(entry.action()) | std::views::take_while([](const auto i){ return i != key::hide_keys; }))
                    result.back().emplace_back(key::name_of(k));
        }
        
        return result;
    }

}
