// note.h

#pragma once

#include <cstdint>
#include <filesystem>
#include <ranges>

#include "tree.h"
#include "tree_op.h"
#include "note_cursor.hpp"
#include "note_edit_info.hpp"

namespace treenote
{
    class note
    {
    public:
        enum class file_msg: std::int8_t
        {
            none = 0,
            does_not_exist,
            is_directory,
            is_device_file,
            is_invalid_file,
            is_unreadable,
            is_unwritable,
            
            unknown_error = -1
        };
        
        struct save_load_info
        {
            std::size_t nodes;
            std::size_t lines;
        };
        
        using return_t = std::pair<file_msg, save_load_info>;
        
        note();
        ~note();
    
        [[maybe_unused]] void make_empty();
        [[maybe_unused]] void close_file();
        [[maybe_unused]] [[nodiscard]] return_t load_file(const std::filesystem::path& path);
        [[maybe_unused]] [[nodiscard]] return_t save_file(const std::filesystem::path& path);
        
        [[maybe_unused]] [[nodiscard]] bool modified() const;
        
        [[maybe_unused]] [[nodiscard]] auto get_lc_range(std::size_t pos, std::size_t size);
        [[maybe_unused]] [[nodiscard]] auto get_entry_prefix(const tree::cache_entry& tce);
        [[maybe_unused]] [[nodiscard]] static auto get_entry_prefix_length(const tree::cache_entry& tce);
        [[maybe_unused]] [[nodiscard]] static auto get_entry_content(const tree::cache_entry& tce);
        [[maybe_unused]] [[nodiscard]] static auto get_entry_content(const tree::cache_entry& tce, std::size_t begin, std::size_t len);
        [[maybe_unused]] [[nodiscard]] static auto get_entry_line_length(const tree::cache_entry& tce);
        
        /* line editing functions */
        
        [[maybe_unused]] void line_insert_text(const std::string& input);
        [[maybe_unused]] void line_delete_char();
        [[maybe_unused]] void line_backspace();
        [[maybe_unused]] void line_newline();
    
        
        /* functions to alter tree structure */

        [[maybe_unused]] [[nodiscard]] cmd_names undo();
        [[maybe_unused]] [[nodiscard]] cmd_names redo();
        
        [[maybe_unused]] int node_move_higher_rec();
        [[maybe_unused]] int node_move_lower_rec();
        [[maybe_unused]] int node_move_back_rec();
        [[maybe_unused]] int node_move_forward_rec();
//        [[maybe_unused]] int node_move_higher_special();
//        [[maybe_unused]] int node_move_lower_special();
//        [[maybe_unused]] int node_move_back_special();
//        [[maybe_unused]] int node_move_forward_special();
        [[maybe_unused]] void node_insert_default();
        [[maybe_unused]] void node_insert_above();
        [[maybe_unused]] void node_insert_below();
        [[maybe_unused]] void node_insert_child();
        [[maybe_unused]] int node_delete_check();
        [[maybe_unused]] int node_delete_special();
        [[maybe_unused]] int node_delete_rec();
        [[maybe_unused]] int node_cut();
        [[maybe_unused]] int node_copy();
        [[maybe_unused]] int node_paste_above();
        [[maybe_unused]] int node_paste_default();
        
        /* wrapper functions to for cursor */
        
        [[maybe_unused]] void cursor_mv_left(std::size_t amt = 1);
        [[maybe_unused]] void cursor_mv_right(std::size_t amt = 1);
        [[maybe_unused]] void cursor_mv_up(std::size_t amt = 1);
        [[maybe_unused]] void cursor_mv_down(std::size_t amt = 1);
        [[maybe_unused]] void cursor_wd_forward(std::size_t amt = 1);
        [[maybe_unused]] void cursor_wd_backward(std::size_t amt = 1);
        [[maybe_unused]] void cursor_to_SOF();
        [[maybe_unused]] void cursor_to_EOF();
        [[maybe_unused]] void cursor_to_SOL();
        [[maybe_unused]] void cursor_to_EOL();
        [[maybe_unused]] void cursor_nd_parent(std::size_t amt = 1);
        [[maybe_unused]] void cursor_nd_child(std::size_t amt = 1);
        [[maybe_unused]] void cursor_nd_prev(std::size_t amt = 1);
        [[maybe_unused]] void cursor_nd_next(std::size_t amt = 1);
        
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_y() const noexcept;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_x() const noexcept;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_current_indent_lvl() const;
        [[maybe_unused]] [[nodiscard]] const auto& cursor_current_index() const;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_current_line() const;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_current_child_count() const;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_max_y() const noexcept;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_max_x() const;
        [[maybe_unused]] [[nodiscard]] std::size_t cursor_max_line() const;
        
        note(const note&) = delete;
        note(note&&) = delete;
        note& operator=(const note&) = delete;
        note& operator=(note&&) = delete;
        
//        [[nodiscard]] const tree_string& debug_get_current_tree_string() const;
        
    private:
        void init();
        void rebuild_cache();
        void cursor_clamp_x();
        [[nodiscard]] operation_stack::cursor_pos cursor_make_save();
        void cursor_restore(const operation_stack::cursor_pos& pos);
        void save_cursor_pos_to_hist();
        
        
        
        tree                        tree_instance_;
        operation_stack             op_hist_;
        note_cursor                 cursor_;
        note_cache                  cache_;
        note_edit_info              editor_;
        
        std::optional<tree>         copied_tree_node_buffer_;
        std::optional<std::string>  copied_string_buffer_;
        
    };
    
    
    /* Inline function implementations */
    
    inline note::note() :
            cache_{ tree_instance_ }
    {
        init();
    }
    
    inline note::~note()
    {
        if (op_hist_.file_is_modified())
        {
            // todo: save file as temporary (used in case of crashes)
        }
    }
    
    inline bool note::modified() const
    {
        return op_hist_.file_is_modified();
    }
    
    inline void note::close_file()
    {
        make_empty();
    }
    
    inline auto note::get_lc_range(std::size_t pos, std::size_t size)
    {
        if (cache_.size() == 0)
            throw std::runtime_error("Note Cache is empty");
        
        const std::size_t begin{ std::min(cache_.size() - 1, pos) };
        const std::size_t count{ std::min(cache_.size() - begin, size) };
        return cache_() | std::views::drop(begin) | std::views::take(count);
    }
    
    inline auto note::get_entry_prefix(const tree::cache_entry& tce)
    {
        return get_indent_info_by_index(tree_instance_, tce.index, tce.line_no != 0);
    }
    
    inline auto note::get_entry_prefix_length(const tree::cache_entry& tce)
    {
        return std::ranges::size(tce.index) - 1;
    }
    
    
    inline auto note::get_entry_content(const tree::cache_entry& tce)
    {
        return tce.ref.get().get_content_const().to_str(tce.line_no);
    }
    
    inline auto note::get_entry_content(const tree::cache_entry& tce, std::size_t begin, std::size_t len)
    {
        return tce.ref.get().get_content_const().to_substr(tce.line_no, begin, len);
    }
    
    inline auto note::get_entry_line_length(const tree::cache_entry& tce)
    {
        return tce.ref.get().get_content_const().line_length(tce.line_no);
    }
    
    
    /* Inline private member functions */
    
    inline void note::init()
    {
        op_hist_ = operation_stack{};
        editor_.reset();
        cursor_.reset();
        rebuild_cache();
    }
    
    inline void note::rebuild_cache()
    {
        cache_.rebuild(tree_instance_);
        cursor_.clamp_y(cache_);
    }
    
    inline void note::cursor_clamp_x()
    {
        cursor_.clamp_x(cache_);
    }
    
    inline operation_stack::cursor_pos note::cursor_make_save()
    {
        return cursor_.get_saved_pos();
    }
    
    inline void note::cursor_restore(const operation_stack::cursor_pos& pos)
    {
        cursor_.restore_pos(cache_, pos);
    }
    
    /* this function must be called every time after a change made to the tree */
    inline void note::save_cursor_pos_to_hist()
    {
        op_hist_.set_after_pos(cursor_make_save());
    }
    
    /* Inline cursor movement functions */
    
    inline void note::cursor_mv_left(std::size_t amt)
    {
        cursor_.mv_left(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void note::cursor_mv_right(std::size_t amt)
    {
        cursor_.mv_right(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void note::cursor_mv_up(std::size_t amt)
    {
        cursor_.mv_up(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void note::cursor_mv_down(std::size_t amt)
    {
        cursor_.mv_down(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void note::cursor_wd_forward(std::size_t amt)
    {
        cursor_.wd_forward(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void note::cursor_wd_backward(std::size_t amt)
    {
        cursor_.wd_backward(cache_, amt);
        cursor_.reset_mnd();
    }

    inline void note::cursor_to_SOF()
    {
        cursor_.to_SOF(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_to_EOF()
    {
        cursor_.to_EOF(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_to_SOL()
    {
        cursor_.to_SOL(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_to_EOL()
    {
        cursor_.to_EOL(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_nd_parent(std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_parent(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_nd_child(std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_child(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_nd_prev(std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_prev(cache_);
        cursor_.reset_mnd();
    }

    inline void note::cursor_nd_next(std::size_t amt)
    {
        for (std::size_t i{ 0 }; i < amt; ++i)
            cursor_.nd_next(cache_);
        cursor_.reset_mnd();
    }

    inline std::size_t note::cursor_y() const noexcept
    {
        return cursor_.y();
    }

    inline std::size_t note::cursor_x() const noexcept
    {
        return cursor_.x();
    }

    inline std::size_t note::cursor_current_indent_lvl() const
    {
        return std::max(get_tree_entry_depth(cache_.index(cursor_y())), 1uz) - 1;
    }

    inline const auto& note::cursor_current_index() const
    {
        return cache_.index(cursor_y());
    }

    inline std::size_t note::cursor_current_line() const
    {
        return cache_.line_no(cursor_y());
    }

    inline std::size_t note::cursor_current_child_count() const
    {
        return cache_.entry_child_count(cursor_y());
    }

    inline std::size_t note::cursor_max_y() const noexcept
    {
        return cache_.size();
    }
    
    inline std::size_t note::cursor_max_x() const
    {
        return cache_.entry_line_length(cursor_y());
    }
    
    inline std::size_t note::cursor_max_line() const
    {
        return cache_.entry_line_count(cursor_y());
    }
    
    
    /* Tree structure editing */

    inline cmd_names note::undo()
    {
        auto ret_val{ op_hist_.get_current_cmd_name(tree_instance_) };
        const auto result{ op_hist_.undo(tree_instance_) };
        rebuild_cache();
        if (result.first != 0)
            ret_val = cmd_names::error;
        else if (result.second.has_value())
            cursor_restore(*result.second);
        return ret_val;
    }
    
    inline cmd_names note::redo()
    {
        const auto result{ op_hist_.redo(tree_instance_) };
        auto ret_val{ op_hist_.get_current_cmd_name(tree_instance_) };
        rebuild_cache();
        if (result.first != 0)
            ret_val = cmd_names::error;
        else if (result.second.has_value())
            cursor_restore(*result.second);
        return ret_val;
    }

    inline void note::node_insert_above()
    {
        op_hist_.exec(tree_instance_, command{ cmd::insert_node{ cursor_current_index(), tree{} } }, cursor_make_save());
        
        rebuild_cache();
        save_cursor_pos_to_hist();
    }
    
    inline void note::node_insert_default()
    {
        const auto tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
    
        if (!tmp.has_value())
            throw std::runtime_error("node_delete_check: cursor index does not exist");
        
        const tree& tree_temp{ tmp->get() };
    
        if (tree_temp.child_count() == 0)
            node_insert_below();
        else
            node_insert_child();
    }
    
    inline void note::node_insert_below()
    {
        auto index{ cursor_current_index() };
        
        if (std::ranges::size(index) == 0)
            return;
        
        ++(*std::ranges::rbegin(index));
        op_hist_.exec(tree_instance_, command{ cmd::insert_node{ index, tree{} } }, cursor_make_save());
    
        rebuild_cache();
        cursor_nd_next();
        save_cursor_pos_to_hist();
    }

    inline void note::node_insert_child()
    {
        auto index{ cursor_current_index() };
        index.push_back(0uz);
        op_hist_.exec(tree_instance_, command{ cmd::insert_node{ index, tree{} } }, cursor_make_save());
    
        rebuild_cache();
        cursor_mv_down();
        save_cursor_pos_to_hist();
    }
    
    inline int note::node_delete_check()
    {
        const auto tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
    
        if (!tmp.has_value())
            throw std::runtime_error("node_delete_check: cursor index does not exist");
        
        const tree& tree_temp{ tmp->get() };
    
        if (tree_temp.child_count() == 0)
            return node_delete_rec();
        else
            return 2;
    }


//    /* Debug functions */
//
//    inline const tree_string& note::debug_get_current_tree_string() const
//    {
//        return get_const_by_index(tree_instance_, cursor_current_index())->get().get_content_const();
//    }
    
}