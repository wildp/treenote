// core/cursor.hpp
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

#include <numeric>

#include "cache.hpp"
#include "tree_op.hpp"

namespace tred::core
{
    class editor;
    
    class cursor
    {
    public:
        [[nodiscard]] std::size_t x() const noexcept;
        [[nodiscard]] std::size_t y() const noexcept;

        void mv_left(const cache& cache, std::size_t amt);
        void mv_right(const cache& cache, std::size_t amt);
        void mv_up(const cache& cache, std::size_t amt);
        void mv_down(const cache& cache, std::size_t amt);
        void wd_forward(const cache& cache);
        void wd_backward(const cache& cache);
        void to_SOF(const cache& cache);
        void to_EOF(const cache& cache);
        void to_SOL(const cache& cache);
        void to_EOL(const cache& cache);
        void nd_parent(const cache& cache);
        void nd_child(const cache& cache);
        void nd_prev(const cache& cache);
        void nd_next(const cache& cache);
        
        void clamp_x(const cache& cache);
        void clamp_y(const cache& cache) noexcept;
    
        void reset() noexcept;
        
        void restore_pos(const cache& cache, const operation_stack::cursor_pos& pos);
        [[nodiscard]] operation_stack::cursor_pos get_saved_pos() const noexcept;
        
        [[nodiscard]] std::size_t get_mnd() const noexcept;
        void reset_mnd() noexcept;
        void update_intended_pos(const cache& cache);
        
    private:
        void move_up_impl(const cache& cache, std::size_t amt) noexcept;
        void move_down_impl(const cache& cache, std::size_t amt) noexcept;
        void node_prev_impl(const cache& cache);
        void node_next_impl(const cache& cache);
    
        void set_h_pos_after_v_move(const cache& cache);
        void set_intended_depth(const cache& cache, int offset = 0);
        void set_intended_index(const cache& cache);
        [[nodiscard]] std::size_t get_max_h_pos(const cache& cache) const;
        [[nodiscard]] static std::size_t get_max_v_pos(const cache& cache) noexcept;
        [[nodiscard]] std::string get_current_char(const cache& cache) const;
        
        std::size_t                 y_{ 0 };
        std::size_t                 x_{ 0 };
        std::size_t                 x_intended_{ 0 };
        
        std::size_t                 node_depth_intended_{ 1 };
        std::vector<std::size_t>    node_index_intended_{ 0 };
        std::size_t                 move_node_depth_{ 1 };
    };
    
    
    /* Inline public getters */

    inline std::size_t cursor::y() const noexcept
    {
        return y_;
    }
    
    inline std::size_t cursor::x() const noexcept
    {
        return x_;
    }
    
    
    /* Inline public member functions to move the cursor */

    inline void cursor::mv_left(const cache& cache, std::size_t amt)
    {
        while (amt > 0)
        {
            if (amt > x_)
            {
                if (cache.line_no(y_) > 0)
                {
                    amt -= x_ + 1;
                    move_up_impl(cache, 1);
                    x_ = get_max_h_pos(cache);
                }
                else
                {
                    x_ = 0;
                    amt = 0;
                }
            }
            else
            {
                x_ -= amt;
                amt = 0;
            }
        }
        
        x_intended_ = x_;
    }

    inline void cursor::mv_right(const cache& cache, std::size_t amt)
    {
        while (amt > 0)
        {
            if (const auto mhp{ get_max_h_pos(cache) }; x_ + amt > mhp)
            {
                if (cache.line_no(y_) + 1 < cache.entry_line_count(y_))
                {
                    amt -= (mhp - x_) + 1;
                    move_down_impl(cache, 1);
                    x_ = 0;
                }
                else
                {
                    x_ = mhp;
                    amt = 0;
                }
            }
            else
            {
                x_ += amt;
                amt = 0;
            }
        }
        
        x_intended_ = x_;
    }

    inline void cursor::mv_up(const cache& cache, const std::size_t amt)
    {
        move_up_impl(cache, amt);
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void cursor::mv_down(const cache& cache, const std::size_t amt)
    {
        move_down_impl(cache, amt);
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void cursor::wd_forward(const cache& cache)
    {
        for (bool done{ false }; not done;)
        {
            if (const auto mhp{ get_max_h_pos(cache) }; x_ + 1 > mhp)
            {
                if (cache.line_no(y_) + 1 < cache.entry_line_count(y_))
                {
                    move_down_impl(cache, 1);
                    x_ = 0;
                    
                    const auto cur{ get_current_char(cache) };
                    if (utf8::is_word_constituent(cur))
                        done = true;
                }
                else
                {
                    x_ = mhp;
                    done = true;
                }   
            }
            else
            {
                const auto cur{ get_current_char(cache) };
                
                ++x_;
                
                if (not utf8::is_word_constituent(cur))
                {
                    const auto next{ get_current_char(cache) };
                    if (utf8::is_word_constituent(next))
                        done = true;
                }
            }
        }
        
        x_intended_ = x_;
    }

    inline void cursor::wd_backward(const cache& cache)
    {
        mv_left(cache, 1);
        
        for (bool done{ false }; not done;)
        {
            const auto cur{ get_current_char(cache) };
            
            if (x_ == 0)
            {
                if (cache.line_no(y_) > 0 and (not utf8::is_word_constituent(cur)))
                {
                    move_up_impl(cache, 1);
                    x_ = std::sub_sat<std::size_t>(get_max_h_pos(cache), 1);
                }
                else
                {
                    done = true;
                }
            }
            else
            {
                --x_;
                
                if (utf8::is_word_constituent(cur))
                {
                    const auto prev{ get_current_char(cache) };
                    if (not utf8::is_word_constituent(prev))
                    {
                        done = true;
                        ++x_;
                    }
                }
            }
        }
        
        x_intended_ = x_;
    }

    inline void cursor::to_EOF(const cache& cache)
    {
        y_ = get_max_v_pos(cache);
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void cursor::to_SOF(const cache& cache)
    {
        y_ = 0;
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void cursor::to_SOL(const cache& /*cache*/)
    {
        x_ = 0;
        x_intended_ = 0;
    }

    inline void cursor::to_EOL(const cache& cache)
    {
        x_ = get_max_h_pos(cache);
        x_intended_ = x_;
    }

    inline void cursor::nd_parent(const cache& cache)
    {
        if (cache.entry_depth(y_) > 1)
        {
            set_intended_depth(cache, -1);
            node_prev_impl(cache);
        }
    }

    inline void cursor::nd_child(const cache& cache)
    {
        const std::size_t child_count{ cache.entry_child_count(y_) };

        if (child_count > 0)
        {
            set_intended_depth(cache, 1);
            node_next_impl(cache);

            if (get_tree_entry_depth(node_index_intended_) >= node_depth_intended_)
            {
                for (std::size_t i{ 0 }; i < node_index_intended_[node_depth_intended_] and i < child_count; ++i)
                    node_next_impl(cache);
            }
        }
    }

    inline void cursor::nd_prev(const cache& cache)
    {
        node_prev_impl(cache);
        set_h_pos_after_v_move(cache);
        set_intended_index(cache);
    }

    inline void cursor::nd_next(const cache& cache)
    {
        node_next_impl(cache);
        set_h_pos_after_v_move(cache);
        set_intended_index(cache);
    }
    
    
    /* Move node depth functions and other functions used for node movement */

    inline std::size_t cursor::get_mnd() const noexcept
    {
        return move_node_depth_;
    }
    
    inline void cursor::reset_mnd() noexcept
    {
        move_node_depth_ = node_depth_intended_;
    }

    inline void cursor::update_intended_pos(const cache& cache)
    {
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    
    /* Other public member functions */
    
    inline void cursor::clamp_x(const cache& cache)
    {
        x_ = std::min(x_, get_max_h_pos(cache));
    }
    
    inline void cursor::clamp_y(const cache& cache) noexcept
    {
        y_ = std::min(y_, get_max_v_pos(cache));
    }

    inline void cursor::reset() noexcept
    {
        y_ = 0;
        x_ = 0;
        x_intended_ = 0;
        node_depth_intended_ = 1;
        node_index_intended_ = { 0 };
    }
    
    inline void cursor::restore_pos(const cache& cache, const operation_stack::cursor_pos& pos)
    {
        y_ = pos.second;
        x_ = pos.first;
        x_intended_ = x_;
        clamp_y(cache);
        clamp_x(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }
    
    inline operation_stack::cursor_pos cursor::get_saved_pos() const noexcept
    {
        return { x_, y_ };
    }
    

    /* Private members */
    
    inline void cursor::move_up_impl(const cache& /*cache*/, const std::size_t amt) noexcept
    {
        y_ = (amt > y_) ? 0 : y_ - amt;
    }

    inline void cursor::move_down_impl(const cache& cache, const std::size_t amt) noexcept
    {
        y_ = std::min(y_ + amt, get_max_v_pos(cache));
    }

    inline void cursor::node_prev_impl(const cache& cache)
    {
        move_up_impl(cache, cache.line_no(y_));

        if (y_ != 0)
        {
            do
            {
                move_up_impl(cache, 1);
                move_up_impl(cache, cache.line_no(y_));
            }
            while (y_ != 0 and cache.entry_depth(y_) > node_depth_intended_);
        }
    }

    inline void cursor::node_next_impl(const cache& cache)
    {
        const std::size_t max_v_pos{ get_max_v_pos(cache) };

        if (y_ <= max_v_pos - cache.entry_line_count(y_))
        {
            do
            {
                move_down_impl(cache, cache.entry_line_count(y_) - cache.line_no(y_));
            }
            while (y_ <= max_v_pos - cache.entry_line_count(y_) and cache.entry_depth(y_) > node_depth_intended_);
        }
    }

    inline void cursor::set_h_pos_after_v_move(const cache& cache)
    {
        const std::size_t max_horizontal_pos{ get_max_h_pos(cache) };

        if (x_intended_ > max_horizontal_pos)
            x_ = max_horizontal_pos;
        else
            x_ =  x_intended_;
    }

    inline void cursor::set_intended_depth(const cache& cache, const int offset)
    {
        node_depth_intended_ = cache.entry_depth(y_) + offset;
    }

    inline void cursor::set_intended_index(const cache& cache)
    {
        node_index_intended_ = cache.index(y_);
    }

    inline std::size_t cursor::get_max_h_pos(const cache& cache) const
    {
        return cache.entry_line_length(y_);
    }

    inline std::size_t cursor::get_max_v_pos(const cache& cache) noexcept
    {
        return std::max(cache.size(), 1uz) - 1;
    }
    
    inline std::string cursor::get_current_char(const cache& cache) const
    {
        return cache.entry_content(y_).to_substr(cache.line_no(y_), x_, 1);
    }
}