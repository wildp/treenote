// utf8.h

#pragma once

#include <iosfwd>
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
    
    [[maybe_unused]] void str_it_get_ext(std::string::const_iterator& it, const std::string::const_iterator& end, std::string& c);
    
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
    
}