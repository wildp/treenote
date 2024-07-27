// keymap.hpp

#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <concepts>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include <curses.h>

namespace treenote_tui
{
    namespace key
    {
        namespace detail
        {
            template<int Bits>
            struct [[maybe_unused]] double_width_int
            {
                using type = std::uintmax_t;
                using overflow [[maybe_unused]] = void;
                inline static constexpr int shift_amt{ sizeof(std::uintmax_t) * CHAR_BIT / 2 };
            };
        
            template<int Bits> requires (Bits <= 4)
            struct [[maybe_unused]] double_width_int<Bits>
            {
                using type = std::uint_least8_t;
                inline static constexpr int shift_amt{ Bits };
            };
        
            template<int Bits> requires (Bits > 4 && Bits <= 8)
            struct [[maybe_unused]] double_width_int<Bits>
            {
                using type = std::uint_least16_t;
                inline static constexpr int shift_amt{ Bits };
            };
        
            template<int Bits> requires (Bits > 8 && Bits <= 16)
            struct [[maybe_unused]] double_width_int<Bits>
            {
                using type = std::uint_least32_t;
                inline static constexpr int shift_amt{ Bits };
            };
        
            template<int Bits> requires (Bits > 16 && Bits <= 32)
            struct [[maybe_unused]] double_width_int<Bits>
            {
                using type = std::uint_least64_t;
                inline static constexpr int shift_amt{ Bits };
            };
        
            template<std::unsigned_integral T, typename U = double_width_int<sizeof(T) * CHAR_BIT>>
            struct double_width : U
            {
                inline static constexpr auto make(wint_t first, wint_t second) -> typename U::type
                {
                    return static_cast<typename U::type>(first) | (static_cast<typename U::type>(second) << U::shift_amt);
                }
            };
        }
    
        using input_t = detail::double_width<wint_t>::type;
        
        namespace detail
        {
            inline constexpr input_t control_modifier{ 0x1f };
            
            /* WARNING: This function must only be called after initscr() is called */
            inline input_t get_extended_key(const char* definition)
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
        }
        
        namespace code
        {
            inline constexpr input_t esc{ 27 };
            inline constexpr input_t enter{ 13 };
        }
    
        template<wint_t KEY_CODE>
        using id = decltype([] constexpr -> input_t { return KEY_CODE; });
        
        template<wint_t C>
        using ctrl = decltype([] constexpr -> input_t { return C & detail::control_modifier; });
    
        template<wint_t C>
        using alt = decltype([] constexpr -> input_t { return detail::double_width<wint_t>::make(code::esc, C); });
    
        template<wint_t C>
        using f = decltype([] constexpr -> input_t { return KEY_F(C); });
    
    
        using up            [[maybe_unused]]    = id<KEY_UP>;
        using shift_up      [[maybe_unused]]    = id<KEY_SR>;
        using ctrl_up       [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;5A"); });
        
        using down          [[maybe_unused]]    = id<KEY_DOWN>;
        using shift_down    [[maybe_unused]]    = id<KEY_SF>;
        using ctrl_down     [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;5B"); });

        using right         [[maybe_unused]]    = id<KEY_RIGHT>;
        using shift_right   [[maybe_unused]]    = id<KEY_SRIGHT>;
        using ctrl_right    [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;5C"); });
    
        using left          [[maybe_unused]]    = id<KEY_LEFT>;
        using shift_left    [[maybe_unused]]    = id<KEY_SLEFT>;
        using ctrl_left     [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;5D"); });
    
        using home          [[maybe_unused]]    = id<KEY_HOME>;
        using shift_home    [[maybe_unused]]    = id<KEY_SHOME>;
        using ctrl_home     [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;5H"); });
        using alt_home      [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;3H"); });
        
        using end           [[maybe_unused]]    = id<KEY_END>;
        using shift_end     [[maybe_unused]]    = id<KEY_SEND>;
        using ctrl_end      [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;5F"); });
        using alt_end       [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[1;3F"); });
        
        using page_down     [[maybe_unused]]    = key::id<KEY_NPAGE>;
        using page_up       [[maybe_unused]]    = key::id<KEY_PPAGE>;
        
        using ins           [[maybe_unused]]    = key::id<KEY_IC>;
        using shift_ins     [[maybe_unused]]    = key::id<KEY_SIC>;
        using ctrl_ins      [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[2;5~"); });
        using alt_ins       [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[2;3~"); });
        using shift_alt_ins [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[2;4~"); });
        
        using del           [[maybe_unused]]    = key::id<KEY_DC>;
        using shift_del     [[maybe_unused]]    = key::id<KEY_SDC>;
        using ctrl_del      [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[3;5~"); });
        using alt_del       [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[3;3~"); });
        using shift_alt_del [[maybe_unused]]    = decltype([] -> input_t { return detail::get_extended_key("\x1b[3;4~"); });
        
        using enter         [[maybe_unused]]    = key::id<KEY_ENTER>;
        using backspace     [[maybe_unused]]    = key::id<KEY_BACKSPACE>;
        
        
        /* note: do not use shift+ins, shift+del, alt+arrow_key, ctrl+alt+anything, shift+alt+anything */
        
        
        // todo: add more key definitions
    }
    
    inline constexpr key::input_t make_alt_code(wint_t c)
    {
        return key::detail::double_width<wint_t>::make(key::code::esc, c);
    }

    enum class actions
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
    
    namespace keymap
    {
        template<typename T>
        concept keycode_function = requires (T t)
        {
            { std::invoke(t) } -> std::same_as<key::input_t>;
        };
        
        template<typename T>
        concept keymap_entry = requires (T t)
        {
            { std::invoke(t) } -> std::same_as<keymap_t::value_type>;
        };
        
        template<actions Action, keycode_function KeycodeFunction>
        struct entry : decltype([] -> keymap_t::value_type { return { std::invoke(KeycodeFunction{}), Action }; }) {};
//        {
//            keymap_t::value_type operator()() const { return { std::invoke(KeycodeFunction{}), Action }; };
//        };
        
        template<keymap_entry... Entries>
        struct make_defaults : decltype([] -> keymap_t { return { (std::invoke(Entries{}))... }; }) {};
//        {
//            keymap_t operator()() const { return { std::invoke(Entries{})... }; };
//        };
        
    
        /* Key map has been taken from nano, with a few changes made for manipulating the tree
         * NOTE: std::invoke(defaults{}) should only be called after initscr() is called */
        
        using defaults = make_defaults<
                entry<  actions::show_help,         key::ctrl<'g'>      >,
                entry<  actions::show_help,         key::f<1>           >,
                entry<  actions::close_tree,        key::ctrl<'x'>      >,
                entry<  actions::close_tree,        key::f<2>           >,
                entry<  actions::write_tree,        key::ctrl<'o'>      >,
                entry<  actions::write_tree,        key::f<3>           >,

                entry<  actions::save_file,         key::ctrl<'s'>      >,
                
                entry<  actions::suspend,           key::ctrl<'z'>      >,

                entry<  actions::cut_node,          key::ctrl<'k'>      >,
                entry<  actions::cut_node,          key::f<9>           >,
                entry<  actions::copy_node,         key::alt<'6'>       >,
                entry<  actions::copy_node,         key::alt<'^'>       >,
                entry<  actions::paste_node,        key::ctrl<'u'>      >,
                entry<  actions::paste_node,        key::f<10>          >,
                
                entry<  actions::undo,              key::alt<'u'>       >,
                entry<  actions::redo,              key::alt<'r'>       >,
                
                entry<  actions::cursor_pos,        key::ctrl<'c'>      >,
                entry<  actions::cursor_pos,        key::f<11>          >,
                entry<  actions::go_to,             key::ctrl<'/'>      >,
                entry<  actions::go_to,             key::alt<'g'>       >,


                entry<  actions::raise_node,        key::shift_left     >,
                entry<  actions::lower_node,        key::shift_right    >,
                entry<  actions::reorder_backwards, key::shift_up       >,
                entry<  actions::reorder_forwards,  key::shift_down     >,
                // todo: add second set of key combinations for node movement (?)

                entry<  actions::insert_node_def,   key::ins            >,
                entry<  actions::insert_node_abv,   key::shift_ins      >,
                entry<  actions::insert_node_abv,   key::shift_alt_ins  >,
                entry<  actions::insert_node_chi,   key::ctrl_ins       >,
                entry<  actions::insert_node_bel,   key::alt_ins        >,

                entry<  actions::delete_node_chk,   key::shift_del      >,
                entry<  actions::delete_node_rec,   key::ctrl_del       >,
                entry<  actions::delete_node_spc,   key::alt_del        >,
                
                
                entry<  actions::cursor_left,       key::ctrl<'b'>      >,
                entry<  actions::cursor_left,       key::left           >,
                entry<  actions::cursor_right,      key::ctrl<'f'>      >,
                entry<  actions::cursor_right,      key::right          >,
                entry<  actions::cursor_up,         key::ctrl<'p'>      >,
                entry<  actions::cursor_up,         key::up             >,
                entry<  actions::cursor_down,       key::ctrl<'n'>      >,
                entry<  actions::cursor_down,       key::down           >,
                entry<  actions::cursor_prev_w,     key::alt<' '>       >,
                entry<  actions::cursor_next_w,     key::ctrl<' '>      >,
                entry<  actions::cursor_sol,        key::ctrl<'a'>      >,
                entry<  actions::cursor_sol,        key::home           >,
                entry<  actions::cursor_eol,        key::ctrl<'e'>      >,
                entry<  actions::cursor_eol,        key::end            >,
                entry<  actions::cursor_sof,        key::alt<'\\'>      >,
                entry<  actions::cursor_sof,        key::ctrl_home      >,
                entry<  actions::cursor_eof,        key::alt<'/'>       >,
                entry<  actions::cursor_eof,        key::ctrl_end       >,
                
                entry<  actions::scroll_up,         key::alt<'-'>       >,
                entry<  actions::scroll_down,       key::alt<'+'>       >,
                entry<  actions::page_up,           key::ctrl<'y'>      >,
                entry<  actions::page_up,           key::page_up        >,
                entry<  actions::page_down,         key::ctrl<'v'>      >,
                entry<  actions::page_down,         key::page_down      >,
                entry<  actions::center_view,       key::ctrl<'l'>      >,
                
                entry<  actions::node_parent,       key::alt<'b'>       >,
                entry<  actions::node_parent,       key::ctrl_left      >,
                entry<  actions::node_child,        key::alt<'f'>       >,
                entry<  actions::node_child,        key::ctrl_right     >,
                entry<  actions::node_prev,         key::alt<'p'>       >,
                entry<  actions::node_prev,         key::ctrl_up        >,
                entry<  actions::node_next,         key::alt<'n'>       >,
                entry<  actions::node_next,         key::ctrl_down      >,

                entry<  actions::newline,           key::ctrl<'m'>      >,
                entry<  actions::newline,           key::enter          >,
                entry<  actions::backspace,         key::ctrl<'h'>      >,
                entry<  actions::backspace,         key::backspace      >,
                entry<  actions::delete_char,       key::ctrl<'d'>      >,
                entry<  actions::delete_char,       key::del            >
                
        >;
    }
    
}

