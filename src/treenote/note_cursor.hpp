// note_cursor.hpp

#pragma once

#include "tree_op.h"
#include "note_cache.hpp"

namespace treenote
{
    class note;
    
    class note_cursor
    {
    public:
        [[nodiscard]] std::size_t x() const noexcept;
        [[nodiscard]] std::size_t y() const noexcept;

        void mv_left(const note_cache& cache, std::size_t amt);
        void mv_right(const note_cache& cache, std::size_t amt);
        void mv_up(const note_cache& cache, std::size_t amt);
        void mv_down(const note_cache& cache, std::size_t amt);
        void wd_forward(const note_cache& cache, std::size_t amt);
        void wd_backward(const note_cache& cache, std::size_t amt);
        void to_SOF(const note_cache& cache);
        void to_EOF(const note_cache& cache);
        void to_SOL(const note_cache& cache);
        void to_EOL(const note_cache& cache);
        void nd_parent(const note_cache& cache);
        void nd_child(const note_cache& cache);
        void nd_prev(const note_cache& cache);
        void nd_next(const note_cache& cache);
        
        void clamp_x(const note_cache& cache);
        void clamp_y(const note_cache& cache) noexcept;
    
        void reset() noexcept;
        
        void restore_pos(const note_cache& cache, const operation_stack::cursor_pos& pos);
        [[nodiscard]] operation_stack::cursor_pos get_saved_pos() const noexcept;
        
        [[nodiscard]] std::size_t get_mnd() const noexcept;
        void reset_mnd() noexcept;
        void update_intended_pos(const note_cache& cache);
        
    private:
        void move_up_impl(const note_cache& cache, std::size_t amt) noexcept;
        void move_down_impl(const note_cache& cache, std::size_t amt) noexcept;
        void node_prev_impl(const note_cache& cache);
        void node_next_impl(const note_cache& cache);
    
        void set_h_pos_after_v_move(const note_cache& cache);
        void set_intended_depth(const note_cache& cache, int offset = 0);
        void set_intended_index(const note_cache& cache);
        [[nodiscard]] std::size_t get_max_h_pos(const note_cache& cache) const;
        [[nodiscard]] static std::size_t get_max_v_pos(const note_cache& cache) noexcept;
        
        std::size_t                 y_{ 0 };
        std::size_t                 x_{ 0 };
        std::size_t                 x_intended_{ 0 };
        
        std::size_t                 node_depth_intended_{ 1 };
        std::vector<std::size_t>    node_index_intended_{ 0 };
        std::size_t                 move_node_depth_{ 1 };
    };
    
    
    /* Inline public getters */

    inline std::size_t note_cursor::y() const noexcept
    {
        return y_;
    }
    
    inline std::size_t note_cursor::x() const noexcept
    {
        return x_;
    }
    
    
    /* Inline public member functions to move the cursor */

    inline void note_cursor::mv_left(const note_cache& cache, std::size_t amt)
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

    inline void note_cursor::mv_right(const note_cache& cache, std::size_t amt)
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

    inline void note_cursor::mv_up(const note_cache& cache, std::size_t amt)
    {
        move_up_impl(cache, amt);
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void note_cursor::mv_down(const note_cache& cache, std::size_t amt)
    {
        move_down_impl(cache, amt);
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void note_cursor::wd_forward(const note_cache& /*cache*/, std::size_t /*amt*/)
    {
        // todo: implement
    }

    inline void note_cursor::wd_backward(const note_cache& /*cache*/, std::size_t /*amt*/)
    {
        // todo: implement
    }

    inline void note_cursor::to_EOF(const note_cache& cache)
    {
        y_ = get_max_v_pos(cache);
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void note_cursor::to_SOF(const note_cache& cache)
    {
        y_ = 0;
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    inline void note_cursor::to_SOL(const note_cache& /*cache*/)
    {
        x_ = 0;
        x_intended_ = 0;
    }

    inline void note_cursor::to_EOL(const note_cache& cache)
    {
        x_ = get_max_h_pos(cache);
        x_intended_ = x_;
    }

    inline void note_cursor::nd_parent(const note_cache& cache)
    {
        if (cache.entry_depth(y_) > 0)
        {
            set_intended_depth(cache, -1);
            node_prev_impl(cache);
        }
    }

    inline void note_cursor::nd_child(const note_cache& cache)
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

    inline void note_cursor::nd_prev(const note_cache& cache)
    {
        node_prev_impl(cache);
        set_h_pos_after_v_move(cache);
        set_intended_index(cache);
    }

    inline void note_cursor::nd_next(const note_cache& cache)
    {
        node_next_impl(cache);
        set_h_pos_after_v_move(cache);
        set_intended_index(cache);
    }
    
    
    /* Move node depth functions and other functions used for node movement */


    inline std::size_t note_cursor::get_mnd() const noexcept
    {
        return move_node_depth_;
    }
    
    inline void note_cursor::reset_mnd() noexcept
    {
        move_node_depth_ = node_depth_intended_;
    }

    inline void note_cursor::update_intended_pos(const note_cache& cache)
    {
        set_h_pos_after_v_move(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }

    
    /* Other public member functions */
    
    inline void note_cursor::clamp_x(const note_cache& cache)
    {
        x_ = std::min(x_, get_max_h_pos(cache));
    }
    
    inline void note_cursor::clamp_y(const note_cache& cache) noexcept
    {
        y_ = std::min(y_, get_max_v_pos(cache));
    }

    inline void note_cursor::reset() noexcept
    {
        y_ = 0;
        x_ = 0;
        x_intended_ = 0;
        node_depth_intended_ = 1;
        node_index_intended_ = { 0 };
    }
    
    inline void note_cursor::restore_pos(const note_cache& cache, const operation_stack::cursor_pos& pos)
    {
        y_ = pos.second;
        x_ = pos.first;
        x_intended_ = x_;
        clamp_y(cache);
        clamp_x(cache);
        set_intended_depth(cache);
        set_intended_index(cache);
    }
    
    inline operation_stack::cursor_pos note_cursor::get_saved_pos() const noexcept
    {
        return { x_, y_ };
    }
    

    /* Private members */
    
    inline void note_cursor::move_up_impl(const note_cache& /*cache*/, std::size_t amt) noexcept
    {
        y_ = (amt > y_) ? 0 : y_ - amt;
    }

    inline void note_cursor::move_down_impl(const note_cache& cache, std::size_t amt) noexcept
    {
        y_ = std::min(y_ + amt, get_max_v_pos(cache));
    }

    inline void note_cursor::node_prev_impl(const note_cache& cache)
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

    inline void note_cursor::node_next_impl(const note_cache& cache)
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

    inline void note_cursor::set_h_pos_after_v_move(const note_cache& cache)
    {
        const std::size_t max_horizontal_pos{ get_max_h_pos(cache) };

        if (x_intended_ > max_horizontal_pos)
            x_ = max_horizontal_pos;
        else
            x_ =  x_intended_;
    }

    inline void note_cursor::set_intended_depth(const note_cache& cache, int offset)
    {
        node_depth_intended_ = cache.entry_depth(y_) + offset;
    }

    inline void note_cursor::set_intended_index(const note_cache& cache)
    {
        node_index_intended_ = cache.index(y_);
    }

    inline std::size_t note_cursor::get_max_h_pos(const note_cache& cache) const
    {
        return cache.entry_line_length(y_);
    }

    inline std::size_t note_cursor::get_max_v_pos(const note_cache& cache) noexcept
    {
        return std::max(cache.size(), 1uz) - 1;
    }
}