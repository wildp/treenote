// keymap.cpp

#include "keymap.h"

#include <cstdlib>
#include <utility>
#include <stdexcept>

namespace treenote_tui
{
    namespace key
    {
        namespace
        {
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

                    shift       = 0b1000'0000,
                    ctrl        = 0b0100'0000,
                    alt         = 0b0010'0000
                };
                
                
                constexpr key_name operator|(key_name lhs, key_name rhs) noexcept
                {
                    return static_cast<key_name>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
                }
            }
            
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
            
            constexpr input_t get(spc::key_name key)
            {
                // todo: maybe refactor this ?
                
                switch (static_cast<std::uint8_t>(key))
                {
                    case spc::up:
                        return KEY_UP;
                    case spc::shift | spc::up:
                        return KEY_SR;
                    case spc::ctrl | spc::up:
                        return extended_key("\x1b[1;5A");

                    case spc::down:
                        return KEY_DOWN;
                    case spc::shift | spc::down:
                        return KEY_SF;
                    case spc::ctrl | spc::down:
                        return extended_key("\x1b[1;5B"); 
                    
                    case spc::right:
                        return KEY_RIGHT;
                    case spc::shift | spc::right:
                        return KEY_SRIGHT;
                    case spc::ctrl | spc::right:
                        return extended_key("\x1b[1;5C");
                    
                    case spc::left:
                        return KEY_LEFT;
                    case spc::shift | spc::left:
                        return KEY_SLEFT;
                    case spc::ctrl | spc::left:
                        return extended_key("\x1b[1;5D");
                    
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
                    case spc::page_up:
                        return KEY_PPAGE;
                        
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
                    case spc::backspace:
                        return KEY_BACKSPACE;
                    
                    default:
                        if not consteval
                        {
                            throw std::runtime_error("Invalid key_name combination");
                        }
                }
            }
            
        }
        
    }
    
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
        k.map_[actions::paste_node]         = { alt('u'), f(10) };

        k.map_[actions::undo]               = { alt('u') };
        k.map_[actions::redo]               = { alt('r') };
        
        k.map_[actions::cursor_pos]         = { ctrl('c'), f(11) };
        k.map_[actions::go_to]              = { ctrl('/'), alt('g') };
        
        k.map_[actions::raise_node]         = { get(spc::shift | spc::left) };
        k.map_[actions::lower_node]         = { get(spc::shift | spc::right) };
        k.map_[actions::reorder_backwards]  = { get(spc::shift | spc::up) };
        k.map_[actions::reorder_forwards]   = { get(spc::shift | spc::down) };
        
        k.map_[actions::insert_node_def]    = { get(spc::ins) };
        k.map_[actions::insert_node_abv]    = { get(spc::shift | spc::ins) };
        k.map_[actions::insert_node_abv]    = { get(spc::shift | spc::alt | spc::ins) };
        k.map_[actions::insert_node_chi]    = { get(spc::ctrl | spc::ins) };
        k.map_[actions::insert_node_bel]    = { get(spc::alt | spc::ins) };
        
        k.map_[actions::delete_node_chk]    = { get(spc::shift | spc::del) };
        k.map_[actions::delete_node_rec]    = { get(spc::ctrl | spc::del) };
        k.map_[actions::delete_node_spc]    = { get(spc::alt | spc::del) };
        
        k.map_[actions::cursor_left]        = { ctrl('b'), get(spc::left) };
        k.map_[actions::cursor_right]       = { ctrl('f'), get(spc::right) };
        k.map_[actions::cursor_up]          = { ctrl('p'), get(spc::up) };
        k.map_[actions::cursor_down]        = { ctrl('n'), get(spc::down) };
        k.map_[actions::cursor_prev_w]      = { alt(' ') };
        k.map_[actions::cursor_next_w]      = { ctrl(' ') };
        k.map_[actions::cursor_sol]         = { ctrl('a'), get(spc::home) };
        k.map_[actions::cursor_eol]         = { ctrl('e'), get(spc::end) };
        k.map_[actions::cursor_sof]         = { alt('\\'), get(spc::ctrl | spc::home) };
        k.map_[actions::cursor_eof]         = { alt('/'), get(spc::ctrl | spc::end) };
        
        k.map_[actions::scroll_up]          = { alt('-'), alt('_') };
        k.map_[actions::scroll_down]        = { alt('+'), alt('=') };
        k.map_[actions::page_up]            = { ctrl('y'), get(spc::page_up) };
        k.map_[actions::page_down]          = { ctrl('v'), get(spc::page_down) };
        
        k.map_[actions::center_view]        = { ctrl('l') };
        
        k.map_[actions::node_parent]        = { alt('b'), get(spc::ctrl | spc::left), };
        k.map_[actions::node_child]         = { alt('f'), get(spc::ctrl | spc::right)  };
        k.map_[actions::node_prev]          = { alt('p'), get(spc::ctrl | spc::up),};
        k.map_[actions::node_next]          = { alt('n'), get(spc::ctrl | spc::down) };

        k.map_[actions::newline]            = { ctrl('m'), get(spc::enter) };
        k.map_[actions::backspace]          = { ctrl('h'), get(spc::backspace) };
        k.map_[actions::delete_char]        = { ctrl('d'), get(spc::del) };
        
        k.map_[prompt_actions::cancel]      = { ctrl('c') };
        k.map_[prompt_actions::yes]         = { 'Y', 'y' };
        k.map_[prompt_actions::no]          = { 'N', 'n' };
        k.map_[prompt_actions::force_quit]  = { ctrl('q') };
        
        return k;
    }
    
    auto keymap::make_editor_keymap() const -> std::unordered_map<key::input_t, actions>
    {
        std::unordered_map<key::input_t, actions> result;
        
        for (const auto& [key, val]: map_)
            if (const auto* action{ std::get_if<actions>(&key) }; action != nullptr)
                for (const auto& input: val)
                    result[input] = *action;
        
        return result;
    }
    
    auto keymap::make_filename_editor_keymap() const -> std::map<key::input_t, action_type>
    {
        std::map<key::input_t, action_type> result;
        
        const std::vector<action_type> a_vec{ actions::newline, actions::backspace, actions::delete_char,
                                              actions::cursor_left, actions::cursor_right, prompt_actions::cancel };
        
        for (const auto& action: a_vec)
            for (const auto& key: map_.at(action))
                result[key] = action;
        
        return result;
    }
    
    auto keymap::make_quit_prompt_keymap() const -> std::map<key::input_t, prompt_actions>
    {
        std::map<key::input_t, prompt_actions> result;
        
        const std::vector<prompt_actions> a_vec{ prompt_actions::force_quit, prompt_actions::yes, prompt_actions::no,
                                                 prompt_actions::cancel };
        
        for (const auto& action: a_vec)
            for (const auto& key: map_.at(action))
                result[key] = action;
        
        return result;
    }
}
