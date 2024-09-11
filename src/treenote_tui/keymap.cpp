// keymap.cpp

#include "keymap.h"

#include <cstdlib>

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
                
                
                constexpr key_name operator|(key_name lhs, key_name rhs)
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
                    
                    char* str;
                    while ( (str = keybound(key_code_generator, 0)) != nullptr)
                    {
                        // todo: update code to be more modern
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
                        return -1; // TODO: throw error
                }
            }
            
        }
        
    }
    
    keymap_t make_default_keymap()
    {
        using namespace key;
        
        keymap_t map;
        
        map[    ctrl('g')                               ] = actions::show_help;
        map[    f(1)                                    ] = actions::show_help;
        map[    ctrl('x')                               ] = actions::close_tree;
        map[    f(2)                                    ] = actions::close_tree;
        map[    ctrl('o')                               ] = actions::write_tree;
        map[    f(3)                                    ] = actions::write_tree;
    
        map[    ctrl('s')                               ] = actions::save_file;
        map[    ctrl('z')                               ] = actions::suspend;
    
        map[    ctrl('k')                               ] = actions::cut_node;
        map[    f(9)                                    ] = actions::cut_node;
        map[    alt('6')                                ] = actions::copy_node;
        map[    alt('^')                                ] = actions::copy_node;
        map[    ctrl('u')                               ] = actions::paste_node;
        map[    f(10)                                   ] = actions::paste_node;
        
        map[    alt('u')                                ] = actions::undo;
        map[    alt('r')                                ] = actions::redo;
        
        map[    ctrl('c')                               ] = actions::cursor_pos;
        map[    f(11)                                   ] = actions::cursor_pos;
        map[    ctrl('/')                               ] = actions::go_to;
        map[    ctrl('g')                               ] = actions::go_to;
        
        map[    get(spc::shift | spc::left)             ] = actions::raise_node;
        map[    get(spc::shift | spc::right)            ] = actions::lower_node;
        map[    get(spc::shift | spc::up)               ] = actions::reorder_backwards;
        map[    get(spc::shift | spc::down)             ] = actions::reorder_forwards;
        
        map[    get(spc::ins)                           ] = actions::insert_node_def;
        map[    get(spc::shift | spc::ins)              ] = actions::insert_node_abv;
        map[    get(spc::shift | spc::alt | spc::ins)   ] = actions::insert_node_abv;
        map[    get(spc::ctrl | spc::ins)               ] = actions::insert_node_chi;
        map[    get(spc::alt | spc::ins)                ] = actions::insert_node_bel;
        
        map[    get(spc::shift | spc::del)              ] = actions::delete_node_chk;
        map[    get(spc::ctrl | spc::del)               ] = actions::delete_node_rec;
        map[    get(spc::alt | spc::del)                ] = actions::delete_node_spc;
        
        map[    ctrl('b')                               ] = actions::cursor_left;
        map[    get(spc::left)                          ] = actions::cursor_left;
        map[    ctrl('f')                               ] = actions::cursor_right;
        map[    get(spc::right)                         ] = actions::cursor_right;
        map[    ctrl('p')                               ] = actions::cursor_up;
        map[    get(spc::up)                            ] = actions::cursor_up;
        map[    ctrl('n')                               ] = actions::cursor_down;
        map[    get(spc::down)                          ] = actions::cursor_down;
        map[    alt(' ')                                ] = actions::cursor_prev_w;
        map[    ctrl(' ')                               ] = actions::cursor_next_w;
        map[    ctrl('a')                               ] = actions::cursor_sol;
        map[    get(spc::home)                          ] = actions::cursor_sol;
        map[    ctrl('e')                               ] = actions::cursor_eol;
        map[    get(spc::end)                           ] = actions::cursor_eol;
        map[    alt('\\')                               ] = actions::cursor_sof;
        map[    get(spc::ctrl | spc::home)              ] = actions::cursor_sof;
        map[    alt('/')                                ] = actions::cursor_eof;
        map[    get(spc::ctrl | spc::end)               ] = actions::cursor_eof;
        
        map[    alt('-')                                ] = actions::scroll_up;
        map[    alt('_')                                ] = actions::scroll_up;
        map[    alt('+')                                ] = actions::scroll_down;
        map[    alt('=')                                ] = actions::scroll_down;
        map[    ctrl('y')                               ] = actions::page_up;
        map[    get(spc::page_up)                       ] = actions::page_up;
        map[    ctrl('v')                               ] = actions::page_down;
        map[    get(spc::page_down)                     ] = actions::page_down;
        map[    ctrl('l')                               ] = actions::center_view;
        
        map[    alt('b')                                ] = actions::node_parent;
        map[    get(spc::ctrl | spc::left)              ] = actions::node_parent;
        map[    alt('f')                                ] = actions::node_child;
        map[    get(spc::ctrl | spc::right)             ] = actions::node_child;
        map[    alt('p')                                ] = actions::node_prev;
        map[    get(spc::ctrl | spc::up)                ] = actions::node_prev;
        map[    alt('n')                                ] = actions::node_next;
        map[    get(spc::ctrl | spc::down)              ] = actions::node_next;
        
        map[    ctrl('m')                               ] = actions::newline;
        map[    get(spc::enter)                         ] = actions::newline;
        map[    ctrl('h')                               ] = actions::backspace;
        map[    get(spc::backspace)                     ] = actions::backspace;
        map[    ctrl('d')                               ] = actions::delete_char;
        map[    get(spc::del)                           ] = actions::delete_char;
        
        return map;
    }
}