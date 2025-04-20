// core/edit_info.hpp
//
// Copyright (C) 2025 Peter Wild
//
// This file is part of tred.
//
// tred is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// tred is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with tred.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include "tree_index.hpp"
#include "tree_string.hpp"
#include "tree.hpp"

namespace tred::core
{
    class edit_info
    {
    public:
        tree_string& get(tree& tree_root, const tree_index auto& ti);
        void reset() noexcept;
        
    private:
        std::optional<std::reference_wrapper<tree_string>>  current_tree_string_ref_;
        std::vector<std::size_t>                            current_tree_string_node_idx_;
    };
    
    inline void edit_info::reset() noexcept
    {
        tree_string_token::reset();
        current_tree_string_ref_.reset();
        current_tree_string_node_idx_.clear();
    }
    
    inline tree_string& edit_info::get(tree& tree_root, const tree_index auto& ti)
    {
        if (not current_tree_string_ref_.has_value())
        {
            current_tree_string_node_idx_.assign(std::ranges::cbegin(ti), std::ranges::cend(ti));
            current_tree_string_ref_ = tree::get_editable_tree_string(tree_root, ti);
        }
        else if (not std::ranges::equal(current_tree_string_node_idx_, ti))
        {
            current_tree_string_ref_->get().set_no_longer_current();
            current_tree_string_node_idx_.assign(std::ranges::cbegin(ti), std::ranges::cend(ti));
            current_tree_string_ref_ = tree::get_editable_tree_string(tree_root, ti);
        }
        
        return current_tree_string_ref_->get();  // throws error if supplied tree index is invalid
    }
}