//tree_index.h

#pragma once

#include <concepts>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace treenote
{
    template<typename T, typename U>
    concept same_remove_cvref = std::same_as<typename std::remove_cvref_t<T>, U>;
    
    template<typename T>
    concept tree_index =
            std::ranges::random_access_range<T>
            and requires (T a)
            {
                { *std::ranges::begin(a) } -> same_remove_cvref<std::size_t>;
                { *std::ranges::cbegin(a) } -> same_remove_cvref<std::size_t>;
                { *std::ranges::rbegin(a) } -> same_remove_cvref<std::size_t>;
                { *std::ranges::crbegin(a) } -> same_remove_cvref<std::size_t>;
            };
    
    using modifiable_tree_index = std::vector<std::size_t>;
    using mti_t = modifiable_tree_index;
    
    [[nodiscard]] inline std::size_t last_index_of(const tree_index auto& ti)
    {
        if (std::ranges::size(ti) > 0)
            return *(std::ranges::crbegin(ti));
        else
            throw std::invalid_argument{ "last_index_of: tree_index has size 0" };
    }
        
    [[nodiscard]] inline auto parent_index_of(const tree_index auto& ti)
    {
        if (std::ranges::size(ti) > 0)
            return (ti | std::views::take(std::ranges::size(ti) - 1));
        else
            throw std::invalid_argument{ "parent_index_of: tree_index has size 0" };
    }
    
    [[nodiscard]] inline modifiable_tree_index make_index_copy_of(const tree_index auto& ti)
    {
        modifiable_tree_index result{ std::ranges::begin(ti), std::ranges::end(ti) };
        return result;
    }
    
    inline void increment_last_index_of(modifiable_tree_index& mti)
    {
        if (std::ranges::size(mti) > 0)
            ++mti.back();
        else
            throw std::invalid_argument{ "increment_last_index_of: tree_index has size 0" };
    }
    
    inline void decrement_last_index_of(modifiable_tree_index& mti)
    {
        if (std::ranges::size(mti) > 0)
            --mti.back();
        else
            throw std::invalid_argument{ "decrement_last_index_of: tree_index has size 0" };
    }
    
    inline void make_child_index_of(modifiable_tree_index& mti, std::size_t value)
    {
        mti.push_back(value);
    }

    inline void set_last_index_of(modifiable_tree_index& mti, std::size_t value)
    {
        if (std::ranges::size(mti) > 0)
            mti.back() = value;
        else
            throw std::invalid_argument{ "set_last_index_of: tree_index has size 0" };
    }
    
    inline std::size_t longest_common_position_of(const tree_index auto& a, const tree_index auto& b)
    {
        std::size_t ret_val{ 0 };
        auto a_beg{ std::ranges::cbegin(a) };
        auto b_beg{ std::ranges::cbegin(b) };
        auto a_end{ std::ranges::cend(a) };
        auto b_end{ std::ranges::cend(b) };
        
        while (a_beg != a_end and b_beg != b_end and *a_beg == *b_beg)
        {
            ++ret_val;
            ++a_beg;
            ++b_beg;
        }
        
        return ret_val;
    }
    
    inline auto longest_common_index_of(const tree_index auto& a, const tree_index auto& b)
    {
        return (a | std::views::take(longest_common_position_of(a, b)));
    }
    
   
}