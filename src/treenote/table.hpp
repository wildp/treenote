// table.hpp
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

#include <optional>
#include <vector>

#include "tree_cmd.hpp"

namespace treenote
{
    struct piece_table_entry
    {
        std::size_t start_index;
        std::size_t display_length;
        std::size_t byte_length;
    };
    
    using piece_table_line    = std::vector<piece_table_entry>;
    using piece_table_t       = std::vector<piece_table_line>;
    
    inline bool entry_has_no_mb_char(const piece_table_entry& entry) noexcept
    {
        return entry.display_length == entry.byte_length;
    }
    
    
    /* Definitions of string commands necessary for implementation */
    
    namespace pt_cmd
    {
        struct split_insert
        {
            std::size_t                 line;
            std::size_t                 original_entry_index;
            std::size_t                 pos_in_entry;
            piece_table_entry           inserted;
        };
        
        struct split_delete
        {
            std::size_t                 line;
            std::size_t                 original_entry_index;
            std::size_t                 l_boundary_pos;
            std::size_t                 r_boundary_pos;
        };
        
        struct grow_rhs
        {
            std::size_t                 line;
            std::size_t                 entry_index;
            std::size_t                 display_amt;
            std::size_t                 byte_amt;
        };
        
        struct shrink_rhs
        {
            std::size_t                 line;
            std::size_t                 entry_index;
            std::size_t                 display_amt;
            std::size_t                 byte_amt;
        };
        
        struct shrink_lhs
        {
            std::size_t                 line;
            std::size_t                 entry_index;
            std::size_t                 display_amt;
            std::size_t                 byte_amt;
        };
        
        struct insert_entry
        {
            std::size_t                 line;
            std::size_t                 entry_index;
            piece_table_entry           inserted;
        };
        
        struct delete_entry
        {
            using merge_info = std::optional<std::size_t>;
            
            std::size_t                 line;
            std::size_t                 entry_index;
            piece_table_entry           deleted;
            merge_info                  merge_pos_in_prev;
        };
        
        struct line_break
        {
            std::size_t                 line_before;
            std::size_t                 pos_before;
        };
        
        struct line_join
        {
            std::size_t                 line_after;
            std::size_t                 pos_after;
        };
        
        struct multi_cmd
        {
            std::vector<table_command>  commands;
        };
    }
    
    class tree_string_token
    {
    public:
        [[nodiscard]] bool check(pt_cmd_type action, std::size_t line, std::size_t pos) const noexcept
        {
            return current_ptr == this and last_action == action and line_ == line and position_ == pos;
        }
        
        void acquire(pt_cmd_type action, std::size_t line, std::size_t pos) noexcept
        {
            current_ptr = this;
            last_action = action;
            line_ = line;
            position_ = pos;
        }
        
        void release() noexcept
        {
            current_ptr = nullptr;
            last_action = pt_cmd_type::none;
            position_ = 0;
            line_ = 0;
        }
        
        static void reset() noexcept
        {
            current_ptr = nullptr;
        }
        
    private:
        inline static const tree_string_token* current_ptr{ nullptr };
        inline static pt_cmd_type last_action{ pt_cmd_type::none };
        std::size_t line_{ 0 };
        std::size_t position_{ 0 };
    };
}