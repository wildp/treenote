// keymap.h

#pragma once

#include <cstdint>
#include <climits>
#include <type_traits>
#include <unordered_map>

#include <curses.h>

namespace treenote_tui
{
    namespace key
    {
        namespace detail
        {
            template<int I>
            requires (I * 2 <= sizeof(std::uintmax_t))
            using double_width_int = std::conditional_t<I == 1, std::uint_least16_t,
                                        std::conditional_t<I == 2, std::uint_least32_t,
                                            std::conditional_t<I <= 4, std::uint_least64_t, std::uintmax_t>>>;
            
            template<std::unsigned_integral T>
            struct input
            {
                using type = detail::double_width_int<sizeof(T)>;
                
                static constexpr type make(T first, T second)
                {
                    return static_cast<type>(first) | (static_cast<type>(second) << (sizeof(T) * CHAR_BIT));
                }
            };
        }
        
        using input_t = detail::input<wint_t>::type;
        
        
        constexpr input_t control_modifier{ 0x1f };
        constexpr input_t escape{ 0x1b };
        
        constexpr input_t down{ KEY_DOWN };
        constexpr input_t up{ KEY_UP };
        constexpr input_t left{ KEY_LEFT };
        constexpr input_t right{ KEY_RIGHT };
        
        constexpr input_t backspace{ KEY_BACKSPACE };
        
        constexpr input_t enter{ KEY_ENTER };
        constexpr input_t del{ KEY_DC };
        constexpr input_t insert{ KEY_IC };
        
        constexpr input_t ctrl(wint_t key)
        {
            return key & control_modifier;
        }
        
        constexpr input_t alt(wint_t key)
        {
            return detail::input<wint_t>::make(escape, key);
        }
        
        constexpr input_t f(wint_t no)
        {
            return KEY_F(no);
        }
    }
    
    enum class actions : std::int8_t
    {
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
        
        raise_node,
        lower_node,
        reorder_backwards,
        reorder_forwards,
        
        // todo: add indent and de-intent (using tab and shift tab)
        //       with slightly different behaviour to move:
        raise_node_spc,
        lower_node_spc,
        transfer_forwards,
        transfer_backwards,
        
        insert_node_def,
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
        
        /* Special action to act as default */
        
        unknown
    };
    
    using keymap_t = std::unordered_map<key::input_t, actions>;
    
    keymap_t make_default_keymap();
}