// utf8.cpp

#include "utf8.h"

#include <iostream>

namespace treenote::utf8
{
    /* bit masks for checking for multibyte unicode characters */
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
    
    std::istream& get_ext(std::istream& f, std::string& c)
    {
        char tmp{};
        c = "";
        
        if (!f.get(tmp))
            return f;
        
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
                if (!f.get(tmp))
                {
                    invalid = true;
                    counter = 0;
                }
                else if ((tmp & mask_cont) != test_cont)
                {
                    invalid = true;
                }
                else
                {
                    c += tmp;
                }
            }
            
            if (invalid)
                c = "\uFFFD"; // char invalid; set to unicode 'Replacement Character'
        }
        return f;
    }
    
    std::istream& unget(std::istream& f)
    {
        for (bool loop{ true }; loop;)
        {
            f.unget();
    
            if (f)
            {
                auto tmp{ f.peek() };
                if ((tmp & mask_cont) != test_cont)
                    loop = false;
            }
            else
            {
                f.clear();
                loop = false;
            }
        }
        return f;
    }
    
    std::string peek(std::istream& f)
    {
        std::string tmp;
        get_ext(f, tmp);
        
        for (std::size_t i{ 0 }; i < tmp.size(); ++i)
            f.unget();
        
        return tmp;
    }
    
    std::pair<std::string, std::size_t> getline_ext(std::istream& f, const std::string& delim)
    {
        std::string result{};
        std::size_t len{0};
        
        std::string tmp{};
        
        while (get_ext(f, tmp) && tmp != delim)
        {
            result += tmp;
            len += 1;
        }
        
        return { result, len };
    }
    
    void str_it_get_ext(std::string::const_iterator& it, const std::string::const_iterator& end, std::string& c)
    {
        c = "";
        
        if (it == end)
            return;
        
        char tmp{ *it };
        ++it;
        
        c += static_cast<char>(tmp);
        
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
    
    std::optional<std::size_t> length(const std::string& str)
    {
        std::size_t len{ 0 };
        int counter{ 0 };
    
        for (const auto& c: str)
        {
            if (counter == 0)
            {
                ++len;
                
                if ((c & mask1) == test1)
                    counter = 0;
                else if ((c & mask2) == test2)
                    counter = 1;
                else if ((c & mask3) == test3)
                    counter = 2;
                else if ((c & mask4) == test4)
                    counter = 3;
                else
                    return std::nullopt; // invalid utf-8 character
            }
            else
            {
                if ((c & mask_cont) == test_cont)
                    --counter;
                else
                    return std::nullopt; // invalid utf-8 character
            }
        }
        
        return len;
    }
    
    std::size_t length(const char* c_str)
    {
        std::size_t len{ 0 };
        int counter{ 0 };
        if (c_str != nullptr)
        {
            std::string_view const sv{ c_str };
            for (const auto& c: sv)
            {
                if (counter == 0)
                {
                    ++len;
            
                    if ((c & mask1) == test1)
                        counter = 0;
                    else if ((c & mask2) == test2)
                        counter = 1;
                    else if ((c & mask3) == test3)
                        counter = 2;
                    else if ((c & mask4) == test4)
                        counter = 3;
                }
                else
                {
                    if ((c & mask_cont) == test_cont)
                        --counter;
                    else
                    {
                        // invalid utf8 character
                        counter = 0;
                        len++;
                    }
                }
            }
        }
    
        return len;
    }
    
    void drop_first_n_chars(std::string& str, std::size_t count)
    {
        std::size_t extracted{ 0 };
        int counter{ 0 };
        
        auto it{ std::ranges::begin(str) };
        const auto it_end{ std::ranges::end(str) };
        for (; extracted < count && it != it_end; ++it)
        {
            if (counter == 0)
            {
                ++extracted;
                
                if ((*it & mask1) == test1)
                    counter = 0;
                else if ((*it & mask2) == test2)
                    counter = 1;
                else if ((*it & mask3) == test3)
                    counter = 2;
                else if ((*it & mask4) == test4)
                    counter = 3;
                else
                    it = it_end; // invalid utf-8 character
            }
            else
            {
                if ((*it & mask_cont) == test_cont)
                    --counter;
                else
                    it = it_end; // invalid utf-8 character
            }
        }
        
        str.erase(std::ranges::begin(str), it);
    }
}