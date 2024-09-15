// keymap.h

#pragma once

#include <cstdint>
#include <climits>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

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
        
    };
    
    enum class prompt_actions : std::int8_t
    {
        /* Special action to act as default */
        
        unknown [[maybe_unused]] = 0,
        
        /* Special action to act as default */
        
        cancel,
        yes,
        no,
        force_quit,
        
    };
    
    class keymap
    {
    public:
        using action_type = std::variant<prompt_actions, actions>;
        using key_type = key::input_t;
        
        keymap() = default;
        
        /* note: this function may only be called after initscr() is called */
        static keymap make_default();
        
        [[nodiscard]] auto make_editor_keymap() const -> std::unordered_map<key::input_t, actions>;
        [[nodiscard]] auto make_quit_prompt_keymap() const -> std::map<key::input_t, prompt_actions>;
        [[nodiscard]] auto make_filename_editor_keymap() const -> std::map<key::input_t, action_type>;
        
    private:
        std::map<action_type, std::vector<key_type>> map_;
    };
    
    using editor_keymap_t = decltype(std::declval<keymap>().make_editor_keymap());
    using quit_prompt_keymap_t = decltype(std::declval<keymap>().make_quit_prompt_keymap());
    using filename_editor_keymap_t = decltype(std::declval<keymap>().make_quit_prompt_keymap());
}