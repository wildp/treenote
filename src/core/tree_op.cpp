// core/tree_op.cpp
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


#include "tree_op.hpp"

#include <algorithm>
#include <limits>

namespace treenote::core
{
    namespace detail
    {
        namespace
        {
            template<typename... Ts>
            struct overload : Ts ... { using Ts::operator()...; };
            
            constexpr std::size_t max_hist_size_{ std::numeric_limits<std::ptrdiff_t>::max() };
        }
    }
    
    
    operation_stack::return_t operation_stack::undo(tree& tree_root)
    {
        if (position_ != 0)
        {
            --position_;
            tree::invoke_reverse(tree_root, cmd_hist_[position_].cmd);
            return { 0, cmd_hist_[position_].before };
        }
        else
        {
            return { 1, empty_cursor_pos };
        }
    }
    
    operation_stack::return_t operation_stack::redo(tree& tree_root)
    {
        if (position_ < cmd_hist_.size())
        {
            tree::invoke(tree_root, cmd_hist_[position_].cmd);
            ++position_;
            return { 0, cmd_hist_[position_ - 1].after };
        }
        else
        {
            return { 1, empty_cursor_pos };
        }
    }
    
    void operation_stack::exec(tree& tree_root, command&& cmd, cursor_pos&& pos_before)
    {
        clean();
        
        cmd_hist_.emplace_back(std::move(cmd), std::make_optional(std::move(pos_before)), std::nullopt);
        
        /* since edit_contents is a placeholder command to tell a specific tree_string to undo/redo a pt_cmd,
         * the action denoted by edit_contents should be executed already, hence we don't call tree_invoke.   */
        
        if (std::holds_alternative<cmd::edit_contents>(cmd_hist_.back().cmd))
            ++position_;
        else
            redo(tree_root);
    }
    
    void operation_stack::append_multi(tree& tree_root, command&& cmd)
    {
        clean();
        tree::invoke(tree_root, cmd);
        
        if (std::holds_alternative<cmd::multi_cmd>(cmd_hist_.back().cmd))
        {
            cmd::multi_cmd& ref{ std::get<cmd::multi_cmd>(cmd_hist_.back().cmd) };
            ref.commands.push_back(std::move(cmd));
        }
        else
        {
            cmd::multi_cmd multi{};
            multi.commands.push_back(std::move(cmd_hist_.back().cmd));
            multi.commands.push_back(std::move(cmd));
            
            cursor_pos_opt before{ std::move(cmd_hist_.back().before )};
            
            cmd_hist_.pop_back();
            cmd_hist_.emplace_back(std::move(multi), std::move(before), std::nullopt);
        }
    }
    
    cmd_names operation_stack::get_current_cmd_name(const tree& tree_root) const
    {
        if (position_ == 0)
            return cmd_names::none;
        
        const command* cmd_ptr{ &(cmd_hist_[position_ - 1].cmd) };
        
        while (std::holds_alternative<cmd::multi_cmd>(*cmd_ptr))
        {
             const cmd::multi_cmd& multi{ std::get<cmd::multi_cmd>(*cmd_ptr) };
             
             if (not multi.commands.empty())
                 cmd_ptr = &(multi.commands.front());
             else
                 return cmd_names::error;
        }
        
        return std::visit(detail::overload{
                [](const cmd::move_node&)
                {
                    return cmd_names::move_node;
                },
                [&](const cmd::edit_contents& c)
                {
                    return get_const_by_index(tree_root, c.pos).transform([](const tree& te){
                        return te.get_content_const().get_current_cmd_name();
                    }).value_or(cmd_names::error);
                },
                [](const cmd::insert_node& c)
                {
                    if (c.is_paste)
                        return cmd_names::paste_node;
                    else
                        return cmd_names::insert_node;
                },
                [](const cmd::delete_node& c)
                {
                    if (c.is_cut)
                        return cmd_names::cut_node;
                    else
                        return cmd_names::delete_node;
                },
                [](const cmd::multi_cmd&)
                {
                    return cmd_names::error;
                }
        }, *cmd_ptr);
    }
    
    void operation_stack::clean()
    {
        if (position_ < cmd_hist_.size())
        {
            /* clear commands in cmd_hist_ after current */
            cmd_hist_.erase(std::ranges::begin(cmd_hist_) + static_cast<std::ptrdiff_t>(position_), std::ranges::end(cmd_hist_));
            cmd_hist_.shrink_to_fit();
        }
        else if (position_ == cmd_hist_.size())
        {
            /* current command is at end of cmd_hist_ : no work needed (normally) */
        
            if (position_ == detail::max_hist_size_)
            {
                /* cmd_hist_ is too big, reduce size of cmd_hist_ by 50% */
                std::vector<stack_elem> tmp{};
                auto range{ cmd_hist_ | std::views::drop(position_ / 2) };
                tmp.reserve(std::ranges::size(range));
                std::ranges::move(range, std::back_inserter(tmp));
                cmd_hist_ = std::move(tmp);
            }
        }
        else
        {
            throw std::runtime_error("Illegal position in tree_op");
        }
    }
}