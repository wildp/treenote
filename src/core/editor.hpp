// core/editor.hpp
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

#include <filesystem>
#include <ranges>

#include "cursor.hpp"
#include "edit_info.hpp"
#include "tree.hpp"
#include "tree_op.hpp"

namespace tred::core
{
    class editor
    {
    public:
        enum class file_msg: std::int8_t
        {
            none,
            does_not_exist,
            is_directory,
            is_device_file,
            is_invalid_file,
            is_unreadable,
            is_unwritable,
            
            unknown_error
        };
        
        using return_t = std::pair<file_msg, save_load_info>;
        
        editor();
        ~editor();
    
        void make_empty();
        void close_file();
        [[nodiscard]] return_t load_file(const std::filesystem::path& path);
        [[nodiscard]] return_t save_file(const std::filesystem::path& path);
        file_msg save_to_tmp(std::filesystem::path& path);
        
        [[nodiscard]] bool modified() const noexcept;
        
        [[nodiscard]] auto get_lc_range(std::size_t pos, std::size_t size) const;
        [[nodiscard]] auto get_entry_prefix(const tree::cache_entry& tce) const;
        [[nodiscard]] static auto get_entry_prefix_length(const tree::cache_entry& tce);
        [[nodiscard]] static auto get_entry_content(const tree::cache_entry& tce, std::size_t begin, std::size_t len);
        [[nodiscard]] static auto get_entry_line_length(const tree::cache_entry& tce);
        
        /* line editing functions */
        
        void line_insert_text(std::string_view input);
        void line_delete_char();
        void line_backspace();
        void line_newline();
        void line_forward_delete_word();
        void line_backward_delete_word();
        
        /* functions to alter tree structure */

        [[nodiscard]] cmd_names undo();
        [[nodiscard]] cmd_names redo();
        
        int node_move_higher_rec();
        int node_move_lower_rec();
        int node_move_back_rec();
        int node_move_forward_rec();
        int node_move_lower_indent();
        // int node_move_higher_special();
        // int node_move_lower_special();
        // int node_move_back_special();
        // int node_move_forward_special();
        void node_insert_default();
        void node_insert_enter();
        void node_insert_above();
        void node_insert_below();
        void node_insert_child();
        int node_delete_check();
        int node_delete_special();
        int node_delete_rec();
        int node_cut();
        int node_copy();
        int node_paste_above();
        int node_paste_default();
        
        /* wrapper functions to for cursor */
        
        void cursor_mv_left(std::size_t amt = 1);
        void cursor_mv_right(std::size_t amt = 1);
        void cursor_mv_up(std::size_t amt = 1);
        void cursor_mv_down(std::size_t amt = 1);
        void cursor_wd_forward();
        void cursor_wd_backward();
        void cursor_to_SOF();
        void cursor_to_EOF();
        void cursor_to_SOL();
        void cursor_to_EOL();
        void cursor_nd_parent(std::size_t amt = 1);
        void cursor_nd_child(std::size_t amt = 1);
        void cursor_nd_prev(std::size_t amt = 1);
        void cursor_nd_next(std::size_t amt = 1);
        
        void cursor_go_to(const tree_index auto& idx, std::size_t line, std::size_t col);
        void cursor_go_to(std::size_t cache_entry_pos, std::size_t col);
        
        [[nodiscard]] std::size_t cursor_y() const noexcept;
        [[nodiscard]] std::size_t cursor_x() const noexcept;
        [[nodiscard]] std::size_t cursor_current_indent_lvl() const;
        [[nodiscard]] const auto& cursor_current_index() const;
        [[nodiscard]] std::size_t cursor_current_line() const;
        [[nodiscard]] std::size_t cursor_current_child_count() const;
        [[nodiscard]] std::size_t cursor_max_y() const noexcept;
        [[nodiscard]] std::size_t cursor_max_x() const;
        [[nodiscard]] std::size_t cursor_max_line() const;
        
        editor(const editor&) = delete;
        editor(editor&&) = delete;
        editor& operator=(const editor&) = delete;
        editor& operator=(editor&&) = delete;
        
    private:
        void init();
        void rebuild_cache();
        void cursor_clamp_x();
        void delete_line_break_forward_impl();
        void delete_line_break_backward_impl();
        std::string cursor_current_char() const;
        std::string cursor_previous_char() const;
        [[nodiscard]] operation_stack::cursor_pos cursor_make_save() const;
        void cursor_restore(const operation_stack::cursor_pos& pos);
        void save_cursor_pos_to_hist();
        
        tree                        tree_instance_;
        operation_stack             op_hist_;
        cursor                 cursor_;
        cache                  cache_;
        edit_info              editor_;
        buffer                 buffer_;
        
        std::optional<tree>         copied_tree_node_buffer_;
        
    };
    
    
    /* Inline function implementations */
    
    inline editor::editor() :
            cache_{ tree_instance_ }
    {
        init();
    }
    
    inline editor::~editor()
    {
        if (modified())
        {
            /* save file as temporary (used in case of crashes) 
             * (not used if program is sent SIGTERM) */
            
            std::filesystem::path path{ tree_instance_.get_content_const().to_str(0) };
            save_to_tmp(path);
        }
    }
    
    inline bool editor::modified() const noexcept
    {
        return op_hist_.file_is_modified();
    }
    
    inline void editor::close_file()
    {
        make_empty();
    }
    
    inline auto editor::get_lc_range(const std::size_t pos, const std::size_t size) const
    {
        if (cache_.size() == 0)
            throw std::runtime_error("Note Cache is empty");
        
        const std::size_t begin{ std::min(cache_.size() - 1, pos) };
        const std::size_t count{ std::min(cache_.size() - begin, size) };
        return cache_() | std::views::drop(begin) | std::views::take(count);
    }
    
    inline auto editor::get_entry_prefix(const tree::cache_entry& tce) const
    {
        return get_indent_info_by_index(tree_instance_, tce.index, tce.line_no != 0);
    }
    
    inline auto editor::get_entry_prefix_length(const tree::cache_entry& tce)
    {
        return std::ranges::size(tce.index) - 1;
    }
    
    inline auto editor::get_entry_content(const tree::cache_entry& tce, const std::size_t begin, const std::size_t len)
    {
        return tce.ref.get().get_content_const().to_substr(tce.line_no, begin, len);
    }
    
    inline auto editor::get_entry_line_length(const tree::cache_entry& tce)
    {
        return tce.ref.get().get_content_const().line_length(tce.line_no);
    }
    
    
    /* Inline private member functions */
    
    inline void editor::init()
    {
        op_hist_ = operation_stack{};
        editor_.reset();
        cursor_.reset();
        rebuild_cache();
    }
    
    inline void editor::rebuild_cache()
    {
        cache_.rebuild(tree_instance_);
        cursor_.clamp_y(cache_);
        editor_.reset();
    }
    
    inline void editor::cursor_clamp_x()
    {
        cursor_.clamp_x(cache_);
    }
    
    inline operation_stack::cursor_pos editor::cursor_make_save() const
    {
        return cursor_.get_saved_pos();
    }

    inline std::string editor::cursor_current_char() const
    {
        return cache_.entry_content(cursor_y()).to_substr(cache_.line_no(cursor_y()), cursor_x(), 1);
    }
    
    inline std::string editor::cursor_previous_char() const
    {
        if (cursor_x() > 0)
            return cache_.entry_content(cursor_y()).to_substr(cache_.line_no(cursor_y()), cursor_x() - 1, 1);
        else
            return "";
    }
    
    inline void editor::cursor_restore(const operation_stack::cursor_pos& pos)
    {
        cursor_.restore_pos(cache_, pos);
    }
    
    /* this function must be called every time after a change made to the tree */
    inline void editor::save_cursor_pos_to_hist()
    {
        op_hist_.set_after_pos(cursor_make_save());
    }
    
    
    /* Inline cursor movement functions */
    
    inline void editor::cursor_mv_left(const std::size_t amt)
    {
        cursor_.mv_left(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_mv_right(const std::size_t amt)
    {
        cursor_.mv_right(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_mv_up(const std::size_t amt)
    {
        cursor_.mv_up(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_mv_down(const std::size_t amt)
    {
        cursor_.mv_down(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_wd_forward()
    {
        cursor_.wd_forward(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_wd_backward()
    {
        cursor_.wd_backward(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_to_SOF()
    {
        cursor_.to_SOF(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_to_EOF()
    {
        cursor_.to_EOF(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_to_SOL()
    {
        cursor_.to_SOL(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_to_EOL()
    {
        cursor_.to_EOL(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_nd_parent(const std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_parent(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_nd_child(const std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_child(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_nd_prev(const std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_prev(cache_);
        cursor_.reset_mnd();
    }

    inline void editor::cursor_nd_next(const std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_next(cache_);
        cursor_.reset_mnd();
    }
    
    inline void editor::cursor_go_to(const tree_index auto& idx, std::size_t line, std::size_t col)
    {
        cursor_.restore_pos(cache_, { /* x = */ col, /* y = */ cache_.approx_pos_of_tree_idx(idx, line) });
    }
    
    inline void editor::cursor_go_to(std::size_t cache_entry_pos, std::size_t col)
    {
        cursor_.restore_pos(cache_, { /* x = */ col, /* y = */ cache_entry_pos });
    }

    inline std::size_t editor::cursor_y() const noexcept
    {
        return cursor_.y();
    }

    inline std::size_t editor::cursor_x() const noexcept
    {
        return cursor_.x();
    }

    inline std::size_t editor::cursor_current_indent_lvl() const
    {
        return std::max(get_tree_entry_depth(cache_.index(cursor_y())), 1uz) - 1;
    }

    inline const auto& editor::cursor_current_index() const
    {
        return cache_.index(cursor_y());
    }

    inline std::size_t editor::cursor_current_line() const
    {
        return cache_.line_no(cursor_y());
    }

    inline std::size_t editor::cursor_current_child_count() const
    {
        return cache_.entry_child_count(cursor_y());
    }

    inline std::size_t editor::cursor_max_y() const noexcept
    {
        return cache_.size();
    }
    
    inline std::size_t editor::cursor_max_x() const
    {
        return cache_.entry_line_length(cursor_y());
    }
    
    inline std::size_t editor::cursor_max_line() const
    {
        return cache_.entry_line_count(cursor_y());
    }
    
    
    /* Tree structure editing */

    inline cmd_names editor::undo()
    {
        auto ret_val{ op_hist_.get_current_cmd_name(tree_instance_) };
        const auto [undo_result, saved_cursor_pos]{ op_hist_.undo(tree_instance_) };
        rebuild_cache();
        if (undo_result != 0)
            ret_val = cmd_names::error;
        else if (saved_cursor_pos.has_value())
            cursor_restore(*saved_cursor_pos);
        return ret_val;
    }
    
    inline cmd_names editor::redo()
    {
        const auto [redo_rv, saved_cursor_pos]{ op_hist_.redo(tree_instance_) };
        auto ret_val{ op_hist_.get_current_cmd_name(tree_instance_) };
        rebuild_cache();
        if (redo_rv != 0)
            ret_val = cmd_names::error;
        else if (saved_cursor_pos.has_value())
            cursor_restore(*saved_cursor_pos);
        return ret_val;
    }
    
    inline void editor::node_insert_default()
    {
        const auto tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
    
        if (not tmp.has_value())
            throw std::runtime_error("node_insert_default: cursor index does not exist");
        
        const tree& tree_temp{ tmp->get() };
    
        if (tree_temp.child_count() == 0)
            node_insert_below();
        else
            node_insert_child();
    }

    inline void editor::node_insert_enter()
    {
        // TODO: node_insert_enter should move contents of current node into new node
        
        if (std::ranges::size(cursor_current_index()) <= 1)
            node_insert_child();
        else
            node_insert_default();
    }

    inline void editor::node_insert_above()
    {
        op_hist_.exec(tree_instance_, cmd::insert_node{ .pos = cursor_current_index(), .inserted = tree{} }, cursor_make_save());
        
        rebuild_cache();
        cursor_mv_down();
        cursor_nd_prev();
        save_cursor_pos_to_hist();
    }
    
    inline void editor::node_insert_below()
    {
        auto index{ cursor_current_index() };
        
        if (std::ranges::size(index) == 0)
            return;
        
        ++(*std::ranges::rbegin(index));
        op_hist_.exec(tree_instance_, cmd::insert_node{ .pos = index, .inserted = tree{} }, cursor_make_save());
    
        rebuild_cache();
        cursor_nd_next();
        save_cursor_pos_to_hist();
    }

    inline void editor::node_insert_child()
    {
        auto index{ cursor_current_index() };
        index.push_back(0uz);
        op_hist_.exec(tree_instance_, cmd::insert_node{ .pos = index, .inserted = tree{} }, cursor_make_save());
    
        rebuild_cache();
        cursor_mv_down();
        save_cursor_pos_to_hist();
    }
    
    inline int editor::node_delete_check()
    {
        const auto tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
    
        if (not tmp.has_value())
            throw std::runtime_error("node_delete_check: cursor index does not exist");
        
        const tree& tree_temp{ tmp->get() };
    
        if (tree_temp.child_count() == 0)
            return node_delete_rec();
        else
            return 2;
    }
}