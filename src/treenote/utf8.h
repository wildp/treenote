// utf8.h

#pragma once

#include <concepts>
#include <iosfwd>
#include <iterator>
#include <optional>
#include <string>
#include <utility>


namespace treenote::utf8
{
    /* Free functions for std::istream to interact with utf-8 characters */
    /* getline_ext returns the string and computed length of that string */
    
    [[maybe_unused]] std::istream& get_ext(std::istream& f, std::string& c);
    [[maybe_unused]] std::string peek(std::istream& f);
    [[maybe_unused]] std::istream& unget(std::istream& f);
    [[maybe_unused]] std::pair<std::string, std::size_t> getline_ext(std::istream& f, const std::string& delim = "\n");
    
    /* Free functions to interact with strings */
    
    template<std::bidirectional_iterator It>
    requires std::same_as<std::remove_cvref_t<decltype( *std::declval<It>() )>, char>
    void str_it_get_ext(It& it, const It& end, std::string& c);
    
    /* Free functions for std::string containing utf-8 characters */
    
    [[maybe_unused]] std::optional<std::size_t> length(const std::string& str);
    [[maybe_unused]] void drop_first_n_chars(std::string& str, std::size_t count);
    
    /* Free functions for char* containing utf-8 characters */

    [[maybe_unused]] std::size_t length(const char* c_str);
    
    /* Convenience wrappers for utf-8 std::iostream functions */
    
    [[maybe_unused]] inline std::string get(std::istream& f)
    {
        std::string tmp;
        get_ext(f, tmp);
        return tmp;
    }
    
    [[maybe_unused]] inline std::string getline(std::istream& f, const std::string& delim = "\n")
    {
        return getline_ext(f, delim).first;
    }
    
    /* bit masks for checking for multibyte Unicode characters */
    /* source: https://en.wikipedia.org/wiki/UTF-8#Encoding */
    
    constexpr int mask1{ 0b1000'0000 };
    constexpr int mask2{ 0b1110'0000 };
    constexpr int mask3{ 0b1111'0000 };
    constexpr int mask4{ 0b1111'1000 };
    
    constexpr int test1{ 0b0000'0000 };
    constexpr int test2{ 0b1100'0000 };
    constexpr int test3{ 0b1110'0000 };
    constexpr int test4{ 0b1111'0000 };
    
    constexpr int mask_cont{ 0b1100'0000 };
    constexpr int test_cont{ 0b1000'0000 };
    
    
    /* Inline implementations of templated functions */
    
    template<std::bidirectional_iterator It>
    requires std::same_as<std::remove_cvref_t<decltype( *std::declval<It>() )>, char>
    void str_it_get_ext(It& it, const It& end, std::string& c)
    {
        c = "";
        
        if (it == end)
            return;
        
        char tmp{ *it };
        ++it;
        
        c += tmp;
        
        if ((tmp & mask1) != test1)
        {
            int counter{ 0 };
            bool invalid{ false };
            
            if ((tmp & mask2) == test2)
                counter = 1;
            else if ((tmp & mask3) == test3)
                counter = 2;
            else if ((tmp & mask4) == test4)
                counter = 3;
            
            for (; counter > 0; --counter)
            {
                if (it == end)
                {
                    invalid = true;
                    counter = 0;
                }
                else
                {
                    tmp = *it;
                    ++it;
                    
                    if ((tmp & mask_cont) != test_cont)
                    {
                        invalid = true;
                    }
                    else
                    {
                        c += tmp;
                    }
                }
            }
            
            if (invalid)
                c = "\uFFFD"; // char invalid; set to unicode 'Replacement Character'
        }
    }
    
}