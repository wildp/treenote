// window_help.hpp

#pragma once

#include "keys.hpp"
#include "window_detail.hpp"

namespace treenote_tui
{
    
    namespace detail
    {
        constexpr const char* help_text =
                "Treenote is a tree-based text editor"
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor  \n"
                "incididunt ut labore et dolore magna aliqua. A scelerisque purus semper eget    \n"
                "duis at tellus. Mattis vulputate enim nulla aliquet porttitor lacus luctus.     \n"
                "Morbi tristique senectus et netus et malesuada fames ac turpis. Porttitor eget  \n"
                "dolor morbi non arcu. Elit ullamcorper dignissim cras tincidunt lobortis feugiat\n"
                "vivamus. Volutpat blandit aliquam etiam erat velit scelerisque in dictum non.   \n"
                "Integer malesuada nunc vel risus commodo viverra. Cras adipiscing enim eu turpis\n"
                "egestas pretium aenean pharetra. Dolor sit amet consectetur adipiscing elit.    ";
        
    }
    
    namespace keymap
    {
        template<actions, typename...>
        struct keymap_find
        {
            using type = void;  /* action not found */
        };
        
        template<actions A, keycode_function F, keymap_entry... Es>
        struct keymap_find<A, entry<A, F>, Es...>
        {
            using type = std::pair<F, typename keymap_find<A, Es...>::type>;
        };
        
        template<actions A, actions B, keycode_function F, keymap_entry... Es>
        requires (A != B)
        struct keymap_find<A, entry<B, F>, Es...> : keymap_find<A, Es...> {};
        
        template<typename>
        struct [[maybe_unused]]  list_size {};
        
        template<>
        struct [[maybe_unused]]  list_size<void>
        {
            [[maybe_unused]] inline static constexpr std::size_t value{ 0 };
        };
        
        template<typename F, typename Ts>
        struct [[maybe_unused]] list_size<std::pair<F, Ts>>
        {
            [[maybe_unused]] inline static constexpr std::size_t value{ 1 + list_size<Ts>::value };
        };
        
        template<typename, typename...>
        struct unpack_list {};
        
        template<typename... Unpacked>
        struct unpack_list<void, Unpacked...>
        {
            using type = std::tuple<Unpacked...>;
        };
        
        template<typename Head, typename Cons, typename... Unpacked>
        struct unpack_list<std::pair<Head, Cons>, Unpacked...> : unpack_list<Cons, Unpacked..., Head> {};
        
        template<actions A, typename T>
        struct keymap_find_helper {};
        
        template<actions A, keymap_entry... Es>
        struct keymap_find_helper<A, make_defaults<Es...>> : keymap_find<A, Es...> {};
        
        template<typename>
        struct make_keybind_vec {};
        
        template<keymap_entry... Es>
        struct make_keybind_vec<std::tuple<Es...>> : decltype([] -> std::vector<key::input_t> { return { std::invoke(Es{})... }; } ) {};
        
        template<actions A>
        struct find_keybindings : decltype([]{ return std::invoke(make_keybind_vec<typename unpack_list<typename keymap_find_helper<A, defaults>::type>::type>{}); }) {};
    }
 
    
    namespace detail
    {
        class action_help
        {
        public:
            action_help(const char* str, std::vector<key::input_t> keycodes) :
                help_text{ str }
            {
            
            }
            
            
        private:
            std::vector<std::string>    keys;
            text_string                 help_text;
        };
        
        template<actions A>
        inline action_help make_action_help(const char* description)
        {
            return action_help(description, std::invoke(keymap::find_keybindings<A>{}));
        }
        
//        struct help_text
//        {
//            action_help help_preamble{ make_action_help };
//            text_string help_pream
//        };
    }
    
}
