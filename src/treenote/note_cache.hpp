// note_cache.hpp
//
// Copyright (C) 2024 Peter Wild
//
// This file is part of Treenote.
//
// Treenote is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Treenote is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Treenote.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include <algorithm>
#include <compare>

#include "tree.h"

namespace treenote
{
    class note_cache
    {
    public:
        explicit note_cache(const tree& tree_root);
        void rebuild(const tree& tree_root);
        
        [[nodiscard]] const tree::cache_entry& operator[](std::size_t i) const;
        [[nodiscard]] const tree::line_cache& operator()() const noexcept;
        [[nodiscard]] const auto& index(std::size_t i) const;
        [[nodiscard]] const auto& line_no(std::size_t i) const;
        [[nodiscard]] std::size_t entry_depth(std::size_t i) const;
        [[nodiscard]] std::size_t size() const noexcept;
        
        [[nodiscard]] std::size_t entry_line_length(std::size_t i) const;
        [[nodiscard]] std::size_t entry_line_count(std::size_t i) const;
        [[nodiscard]] std::size_t entry_child_count(std::size_t i) const;
        [[nodiscard]] const auto& entry_content(std::size_t i) const;
        
        [[nodiscard]] std::size_t approx_pos_of_tree_idx(const tree_index auto& ti, std::size_t line) const;
        
    private:
        [[nodiscard]] const tree& get_tree_entry(std::size_t i) const;
        
        tree::line_cache        tree_index_cache_;
    };
    
    
    /* Implementations */
    
    inline note_cache::note_cache(const tree& tree_root)
    {
        rebuild(tree_root);
    }
    
    inline void note_cache::rebuild(const tree& tree_root)
    {
        tree_index_cache_ = tree::build_index_cache(tree_root);
    }
    
    inline const tree::cache_entry& note_cache::operator[](const std::size_t i) const
    {
        return tree_index_cache_.at(i);
    }
    
    inline const tree::line_cache& note_cache::operator()() const noexcept
    {
        return tree_index_cache_;
    }
    
    inline const auto& note_cache::index(const std::size_t i) const
    {
        return operator[](i).index;
    }
    
    inline const auto& note_cache::line_no(const std::size_t i) const
    {
        return operator[](i).line_no;
    }
    
    inline std::size_t note_cache::entry_depth(const std::size_t i) const
    {
        return get_tree_entry_depth(index(i));
    }
    
    inline std::size_t note_cache::size() const noexcept
    {
        return tree_index_cache_.size();
    }
    
    inline std::size_t note_cache::entry_line_length(const std::size_t i) const
    {
        return get_tree_entry(i).get_content_const().line_length(line_no(i));
    }
    
    inline std::size_t note_cache::entry_line_count(const std::size_t i) const
    {
        return get_tree_entry(i).line_count();
    }
    
    inline std::size_t note_cache::entry_child_count(const std::size_t i) const
    {
        return get_tree_entry(i).child_count();
    }
    
    inline const auto& note_cache::entry_content(const std::size_t i) const
    {
        return get_tree_entry(i).get_content_const();
    }
    
    inline const tree& note_cache::get_tree_entry(const std::size_t i) const
    {
        return operator[](i).ref.get();
    }
    
    inline std::size_t note_cache::approx_pos_of_tree_idx(const tree_index auto& ti, const std::size_t line) const
    {
        /* note: if tree_index does not exist, this function returns the pos of the nearest */
        
        std::size_t lo{ 0 };
        std::size_t hi{ tree_index_cache_.size() };

        while (hi - lo > 1)
        {
            const std::size_t mid{ lo + ((hi - lo) / 2) };
            const auto& mid_entry{ tree_index_cache_.at(mid) };
            
            const auto compare_result{ std::lexicographical_compare_three_way(std::ranges::begin(ti), std::ranges::end(ti),
                                                                              std::ranges::begin(mid_entry.index),
                                                                              std::ranges::end(mid_entry.index)) };
            
            if (std::is_eq(compare_result))
            {
                if (line == mid_entry.line_no)
                    lo = hi = mid;
                else if (line < mid_entry.line_no)
                    hi = mid;
                else
                    lo = mid;
            }
            else
            {
                if (std::is_lt(compare_result))
                    hi = mid;
                else
                    lo = mid;
            }
        }

        return lo;
    }
    
}