// core/tree_op.hpp
//
// Copyright (C) 2025 Peter Wild
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

#include <vector>

#include "tree.hpp"
#include "tree_cmd.hpp"

namespace treenote::core
{
    namespace cmd
    {
        struct move_node
        {
            std::vector<std::size_t>    src;
            std::vector<std::size_t>    dst;
        };
        
        struct edit_contents
        {
            std::vector<std::size_t>    pos;
            /* the actual edit command is stored and executed in tree_string
             * this command just stores the location of the node containing that tree_string */
        };
        
        struct insert_node
        {
            std::vector<std::size_t>    pos;
            std::optional<tree>         inserted;
            bool                        is_paste{ false };
        };
        
        struct delete_node
        {
            std::vector<std::size_t>    pos;
            std::optional<tree>         deleted;
            bool                        is_cut{ false };
        };
        
        struct multi_cmd
        {
            std::vector<command>        commands;
        };
    }
    
    class operation_stack
    {
    public:
        using cursor_pos = std::pair<std::size_t, std::size_t>;
        using cursor_pos_opt = std::optional<cursor_pos>;
        using return_t = std::pair<int, const cursor_pos_opt&>;
        
        struct stack_elem
        {
            command         cmd;
            cursor_pos_opt  before;
            cursor_pos_opt  after;
        };
    
        constexpr operation_stack() = default;
        return_t undo(tree& tree_root);
        return_t redo(tree& tree_root);
        void exec(tree& tree_root, command&& cmd, cursor_pos&& pos_before);
        void append_multi(tree& tree_root, command&& cmd);
        void set_after_pos(cursor_pos&& pos_after);
        void set_position_of_save() noexcept;
        
        [[nodiscard]] bool file_is_modified() const noexcept;
        [[nodiscard]] cmd_names get_current_cmd_name(const tree& tree_root) const;
        
    private:
        void clean();
        
        std::vector<stack_elem>             cmd_hist_;
        std::size_t                         position_{ 0 }; /* defined as (index of the current command position in tree_ref_) + 1 */
        std::size_t                         position_at_last_save_{ 0 };
        
        static constexpr cursor_pos_opt     empty_cursor_pos{};
    };
    
    
    /* Inline function definitions */
    
    inline void operation_stack::set_after_pos(cursor_pos&& pos_after)
    {
        if (not cmd_hist_.empty())
            cmd_hist_.back().after.emplace(std::move(pos_after));
    }
    
    inline bool operation_stack::file_is_modified() const noexcept
    {
        return position_ != position_at_last_save_;
    }
    
    inline void operation_stack::set_position_of_save() noexcept
    {
        position_at_last_save_ = position_;
    }
}