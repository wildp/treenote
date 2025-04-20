// core/editor.cpp
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


#include "editor.hpp"

#include <fstream>

#include "tree.hpp"

namespace treenote::core
{
    /* File related public member functions */

    void editor::make_empty()
    {
        tree_instance_ = tree::make_empty();
        init();
    }
    
    editor::return_t editor::load_file(const std::filesystem::path& path)
    {
        using std::filesystem::perms;
        
        auto msg{ file_msg::none };
        save_load_info sli{ .node_count = 0, .line_count = 0 };
        
        bool make_empty{ true };
        const auto fs{ std::filesystem::status(path) };
        
        if (not std::filesystem::exists(fs))
        {
            msg = file_msg::does_not_exist; /* (not actually an error) */
        }
        else if (std::filesystem::is_directory(fs))
        {
            msg = file_msg::is_directory;
        }
        else if (std::filesystem::is_character_file(fs) or std::filesystem::is_block_file(fs))
        {
            msg = file_msg::is_device_file;
        }
        else if (std::filesystem::is_fifo(fs) or std::filesystem::is_socket(fs) or std::filesystem::is_other(fs))
        {
            msg = file_msg::is_invalid_file;
        }
        else if (perms::none == (fs.permissions() & perms::owner_read))
        {
            msg = file_msg::is_unreadable;
        }
        else
        {
            if (perms::none == (fs.permissions() & perms::owner_write))
                msg = file_msg::is_unwritable; /* (not actually an error) */
            
            /* maybe perform more checks? */
            
            std::ifstream file{ path };
            
            if (not file)
            {
                msg = file_msg::unknown_error;
            }
            else
            {
                tree_instance_ = tree::parse(file, path.string(), buffer_, sli);
                make_empty = false;
            }
        }
        
        if (make_empty)
            tree_instance_ = tree::make_empty();
        
        init();
        return { msg, sli };
    }
    
    editor::return_t editor::save_file(const std::filesystem::path& path)
    {
        using std::filesystem::perms;
        
        auto msg{ file_msg::none };
        save_load_info sli{ .node_count = 0, .line_count = 0 };
        
        const auto fs{ std::filesystem::status(path) };
        
        bool save{ false };
        
        if (not std::filesystem::exists(fs))
        {
            msg = file_msg::none;
            save = true;
        }
        else if (std::filesystem::is_directory(fs))
        {
            msg = file_msg::is_directory;
        }
        else if (not std::filesystem::is_regular_file(fs))
        {
            msg = file_msg::is_invalid_file;
        }
        else if (perms::none == (fs.permissions() & perms::owner_write))
        {
            msg = file_msg::is_unwritable;
        }
        else /* maybe perform more checks? */
        {
            save = true;
        }
        
        if (save)
        {
            std::ofstream file{ path };
            
            if (not file)
            {
                msg = file_msg::unknown_error;
            }
            else
            {
                tree::write(file, tree_instance_, sli);
                op_hist_.set_position_of_save();
            }
        }
        
        return { msg, sli };
    }
    
    editor::file_msg editor::save_to_tmp(std::filesystem::path& path)
    {
        if (path.empty())
        {
            path = std::filesystem::current_path();
            path /= "treenote.";
            path += std::to_string(getpid());
        }
        
        path += ".save";
        
        if (not std::filesystem::exists(std::filesystem::status(path)))
            return save_file(path).first;
        
        path += ".0";
        
        for (int i{ 1 }; i < 100; ++i)
        {
            path.replace_extension("." + std::to_string(i));
            if (not std::filesystem::exists(std::filesystem::status(path)))
                return save_file(path).first;
        }
        
        return file_msg::unknown_error;
    }
    
    /* Line editing functions */

    /* implementation helper: call only from line_delete_char and line_forward_delete_word */
    void editor::delete_line_break_forward_impl()
    {
        /* preconditions:
         * cursor_x() >= cursor_max_x() and cursor_current_line() + 1 < cursor_max_line() */
        
        auto& e{ editor_.get(tree_instance_, cursor_current_index()) };
        
        if (e.make_line_join(cursor_current_line()))
        {
            op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
            rebuild_cache();
            save_cursor_pos_to_hist();
        }
    }

    /* implementation helper: call only from line_delete_char and line_forward_delete_word */
    void editor::delete_line_break_backward_impl()
    {
        /* preconditions:
         * cursor_x() == 0 and cursor_current_line() > 0 */
        
        auto& e{ editor_.get(tree_instance_, cursor_current_index()) };
        
        auto cursor_save{ cursor_make_save() };
        cursor_mv_up();
        cursor_to_EOL();
           
        if (e.make_line_join(cursor_current_line()))
        {
            op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, std::move(cursor_save));
            rebuild_cache();
            save_cursor_pos_to_hist();
        }
        else
        {
            cursor_mv_down();
            cursor_to_SOL();
        }
    }

    void editor::line_insert_text(const std::string_view input)
    {
        auto& e{ editor_.get(tree_instance_, cursor_current_index()) };
        std::size_t cursor_inc_amt{ 0 };
        
        /* maybe validate input string, including preventing input of newline chars
         * however this is not needed with ncurses and so doesn't really matter right now */
        
        if (e.insert_str(cursor_current_line(), cursor_x(), buffer_.append(input), cursor_inc_amt))
            op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());

        cursor_mv_right(cursor_inc_amt);
        save_cursor_pos_to_hist();
    }
    
    void editor::line_delete_char()
    {
        if (cursor_x() >= cursor_max_x() and cursor_current_line() + 1 < cursor_max_line())
        {
            /* delete line break */
            delete_line_break_forward_impl();
        }
        else if (cursor_x() < cursor_max_x())
        {
            auto& e{ editor_.get(tree_instance_, cursor_current_index()) };

            /* delete character */
            if (e.delete_char_current(cursor_current_line(), cursor_x()))
                op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
            save_cursor_pos_to_hist();
        }
    }
    
    void editor::line_backspace()
    {
        auto& e{ editor_.get(tree_instance_, cursor_current_index()) };
        
        if (cursor_x() == 0 and cursor_current_line() > 0)
        {
            /* delete line break */
            delete_line_break_backward_impl();
        }
        else if (cursor_x() > 0)
        {
            /* delete character */
            std::size_t cursor_dec_amt{ 0 };
            if (e.delete_char_before(cursor_current_line(), cursor_x(), cursor_dec_amt))
                op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
            cursor_mv_left(cursor_dec_amt);
            save_cursor_pos_to_hist();
        }
    }
    
    void editor::line_newline()
    {
        auto& e{ editor_.get(tree_instance_, cursor_current_index()) };
        
        if (e.make_line_break(cursor_current_line(), cursor_x()))
        {
            op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
            rebuild_cache();
            cursor_mv_down();
            cursor_to_SOL();
            save_cursor_pos_to_hist();
        }
    }

    void editor::line_forward_delete_word()
    {
        auto& e{ editor_.get(tree_instance_, cursor_current_index()) };
        
        if (cursor_x() >= cursor_max_x() and cursor_current_line() + 1 < cursor_max_line())
        {
            /* delete line break */
            delete_line_break_forward_impl();
        }
        else if (cursor_x() < cursor_max_x())
        {
            /* delete characters until next word boundary */
            while (true)
            {
                const auto cur{ cursor_current_char() };
                
                /* delete character */
                if (e.delete_char_current(cursor_current_line(), cursor_x()))
                    op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
                
                if (not utf8::is_word_constituent(cur))
                {
                    const auto next{ cursor_current_char() };
                    if (next.empty() or utf8::is_word_constituent(next))
                        break;
                }
            }
            
            save_cursor_pos_to_hist();
        }
    }
    
    void editor::line_backward_delete_word()
    {
        if (cursor_x() == 0 and cursor_current_line() > 0)
        {
            /* delete line break */
            delete_line_break_backward_impl();
        }
        else if (cursor_x() > 0)
        {
            /* delete character */

            auto& e{ editor_.get(tree_instance_, cursor_current_index()) };

            std::size_t cursor_dec_amt{ 0 };
            if (e.delete_char_before(cursor_current_line(), cursor_x(), cursor_dec_amt))
                op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
            cursor_mv_left(cursor_dec_amt);

            auto cur{ cursor_previous_char() };
            
            while (true)
            {
                if (cur.empty())
                    break;
                
                if (e.delete_char_before(cursor_current_line(), cursor_x(), cursor_dec_amt))
                    op_hist_.exec(tree_instance_, command{ cmd::edit_contents{ cursor_current_index() } }, cursor_make_save());
                cursor_mv_left(cursor_dec_amt);

                auto prev{ cursor_previous_char() };
                
                if (utf8::is_word_constituent(cur) and not utf8::is_word_constituent(prev))
                    break;

                cur = std::move(prev);
            }
            
            save_cursor_pos_to_hist();
        }
    }

    
    /* Node movement functions */

    // move node and all children up one depth level
    int editor::node_move_higher_rec()
    {
        cursor_.reset_mnd();
    
        /* prevent moving a node higher if the node is already at minimum depth */
        if (std::ranges::size(cursor_current_index()) <= 1)
            return 1;
        
        mti_t src_parent_index{ make_index_copy_of(parent_index_of(cursor_current_index())) };
        const auto src_parent_tmp{ get_const_by_index(tree_instance_, src_parent_index) };
        
        mti_t src_index{ make_index_copy_of(cursor_current_index()) };
        const auto src_tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
    
        op_hist_.exec(tree_instance_, command{ cmd::multi_cmd{} }, cursor_make_save());
        
        if (src_parent_tmp.has_value() and src_tmp.has_value())
        {
            const tree& src_parent_tree_tmp{ src_parent_tmp->get() };
            const tree& src_tree_tmp{ src_tmp->get() };
    
            /* Move src_index's parent's children below src_index to be children of src_index */
            
            mti_t alt_src_index{ make_index_copy_of(src_index) };
            increment_last_index_of(alt_src_index);
            
            mti_t alt_dst_index{ make_index_copy_of(src_index) };
            make_child_index_of(alt_dst_index, src_tree_tmp.child_count());
            
            while (src_parent_tree_tmp.child_count() > last_index_of(src_index) + 1)
            {
                op_hist_.append_multi(tree_instance_, cmd::move_node{ .src = alt_src_index, .dst = alt_dst_index });
                increment_last_index_of(alt_dst_index);
            }
            
            /* Then finally raise node at src_index */
            
            mti_t dst_index{ std::move(src_parent_index) };
            increment_last_index_of(dst_index);
            
            op_hist_.append_multi(tree_instance_, cmd::move_node{ .src = std::move(src_index), .dst = std::move(dst_index) });
    
            rebuild_cache();
            cursor_.update_intended_pos(cache_);
            cursor_.reset_mnd();
            save_cursor_pos_to_hist();
            return 0;
        }
        else
        {
            throw std::runtime_error("node_move_higher_rec: invalid tree");
        }
    }
    
    // move node and all children down one depth level
    int editor::node_move_lower_rec()
    {
        /* prevent moving a node lower if there is no possible new parent for it */
        if (last_index_of(cursor_current_index()) == 0)
            return 1;
        
        const auto src_index{ cursor_current_index() };
        
        mti_t dst_index{ make_index_copy_of(cursor_current_index()) };
        decrement_last_index_of(dst_index);
    
        const auto dst_parent_tmp{ get_const_by_index(tree_instance_, dst_index) };
    
        if (dst_parent_tmp.has_value())
        {
            const tree& dst_parent_tree_tmp{ dst_parent_tmp->get() };
            const std::size_t parent_child_count{ dst_parent_tree_tmp.child_count() };
    
            make_child_index_of(dst_index, parent_child_count);
    
            op_hist_.exec(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(dst_index) }, cursor_make_save());

            rebuild_cache();
            cursor_.update_intended_pos(cache_);
            cursor_.reset_mnd();
        }
        else
        {
            throw std::runtime_error("node_move_lower_rec: invalid tree");
        }
        
        save_cursor_pos_to_hist();
        return 0;
    }
    
    /* Move node and all children up on page */
    int editor::node_move_back_rec()
    {
        /* prevent moving the first node in the tree forwards */
        if (std::ranges::size(cursor_current_index()) <= 1 and last_index_of(cursor_current_index()) == 0)
            return 1;
        
        auto cursor_save{ cursor_make_save() };
        const auto src_index{ cursor_current_index() };
        
        if (last_index_of(src_index) == 0)
        {
            /* cannot move back; promote node instead and place before parent */
    
            mti_t parent_index{ make_index_copy_of(parent_index_of(cursor_current_index())) };
            
            cursor_.nd_parent(cache_);
            
            op_hist_.exec(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(parent_index) }, std::move(cursor_save));
    
            rebuild_cache();
        }
        else
        {
            mti_t dst_index{ make_index_copy_of(cursor_current_index()) };
            decrement_last_index_of(dst_index);
            
            if (get_tree_entry_depth(cursor_current_index()) < cursor_.get_mnd())
            {
                /* move node to be a child of the previous node
                 * (previous node is guaranteed to exist by outer if statement) */
    
                const auto dst_parent_tmp{ get_const_by_index(tree_instance_, dst_index) };
    
                if (dst_parent_tmp.has_value())
                {
                    const tree& dst_parent_tree_tmp{ dst_parent_tmp->get() };
                    const std::size_t parent_child_count{ dst_parent_tree_tmp.child_count() };
                    
                    make_child_index_of(dst_index, parent_child_count);
                    
                    op_hist_.exec(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(dst_index) }, std::move(cursor_save));
                    
                    rebuild_cache();
                    cursor_.update_intended_pos(cache_);
                }
                else
                {
                    throw std::runtime_error("node_move_back_rec: invalid tree");
                }
            }
            else
            {
                /* move node back within same level */
                
                cursor_.reset_mnd();
                cursor_.update_intended_pos(cache_);
                cursor_.nd_prev(cache_);
                
                op_hist_.exec(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(dst_index) }, std::move(cursor_save));
    
                rebuild_cache();
            }
        }
        
        save_cursor_pos_to_hist();
        return 0;
    }
    
    /* Move node and all children down on page */
    int editor::node_move_forward_rec()
    {
        /* prevent moving the last node in the tree (of minimum depth) forwards */
        if (std::ranges::size(cursor_current_index()) == 1 and last_index_of(cursor_current_index()) + 1 == tree_instance_.child_count())
            return 1;
        
        auto cursor_save{ cursor_make_save() };
        const auto parent_index{ parent_index_of(cursor_current_index()) };
        const auto parent_tmp{ get_const_by_index(tree_instance_, parent_index) };
        
        if (parent_tmp.has_value())
        {
            const tree& parent_tree_tmp{ parent_tmp->get() };
    
            const auto src_index{ cursor_current_index() };
            
            if (last_index_of(cursor_current_index()) + 1 >= parent_tree_tmp.child_count())
            {
                /* cannot move forward; promote node instead and place after parent */
    
                mti_t dst_index{ make_index_copy_of(parent_index) };
                increment_last_index_of(dst_index);
    
                op_hist_.exec(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(dst_index) }, std::move(cursor_save));
                
                rebuild_cache();
            }
            else
            {
                mti_t dst_index{ make_index_copy_of(cursor_current_index()) };
                
                if (get_tree_entry_depth(cursor_current_index()) < cursor_.get_mnd())
                {
                    /* move node to be a child of the next node
                     * (next node is guaranteed to exist by outer if statement) */
                    
                    make_child_index_of(dst_index, 0uz);
                }
                else
                {
                    /* move node forward within same level */
                    
                    increment_last_index_of(dst_index);
                    
                    cursor_.reset_mnd();
                    cursor_.update_intended_pos(cache_);
                }
                
                op_hist_.exec(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(dst_index) }, std::move(cursor_save));
                
                rebuild_cache();
                cursor_.nd_next(cache_);
            }
        }
        else
        {
            throw std::runtime_error("node_move_forward_rec: invalid tree");
        }
        
        save_cursor_pos_to_hist();
        return 0;
    }
    
    /* Moves a node to the left on the page by lowering it within the tree.
     * The children remain unmoved where possible. */
    int editor::node_move_lower_indent()
    {
        /* prevent moving a node lower if there is no possible new parent for it */
        if (last_index_of(cursor_current_index()) == 0)
            return 1;
        
        op_hist_.exec(tree_instance_, command{ cmd::multi_cmd{} }, cursor_make_save());
        
        const auto src_index{ cursor_current_index() };
        const auto src_parent_tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
        
        mti_t dst_index{ make_index_copy_of(cursor_current_index()) };
        decrement_last_index_of(dst_index);
        const auto dst_parent_tmp{ get_const_by_index(tree_instance_, dst_index) };
        
        if (src_parent_tmp.has_value() and dst_parent_tmp.has_value())
        {
            const tree& src_parent_tree_tmp{ src_parent_tmp->get() };
            const tree& dst_parent_tree_tmp{ dst_parent_tmp->get() };
            const std::size_t parent_child_count{ dst_parent_tree_tmp.child_count() };
            
            make_child_index_of(dst_index, parent_child_count);
            
            mti_t src_child_index{ make_index_copy_of(cursor_current_index()) };
            make_child_index_of(src_child_index, 0 /* value unimportant */);
            
            while (src_parent_tree_tmp.child_count() > 0)
            {
                set_last_index_of(src_child_index, src_parent_tree_tmp.child_count() - 1);
                op_hist_.append_multi(tree_instance_, cmd::move_node{ .src = src_child_index, .dst = dst_index });
            }
            
            op_hist_.append_multi(tree_instance_, cmd::move_node{ .src = src_index, .dst = std::move(dst_index) });
            
            rebuild_cache();
            cursor_.update_intended_pos(cache_);
            cursor_.reset_mnd();
        }
        else
        {
            throw std::runtime_error("node_move_lower_indent: invalid tree");
        }
        
        save_cursor_pos_to_hist();
        return 0;
    }
    
//    /* Moves a node to the right on the page by raising it within the tree.
//     * The children remain unmoved where possible  */
//    int note::node_move_higher_special()
//    {
//        return node_move_higher_rec();
//    }
//
//    /* Moves a node to the left on the page by lowering it within the tree.
//     * The children remain unmoved where possible. */
//    int note::node_move_lower_special()
//    {
//        return node_move_lower_indent();
//    }
//
//    /* Moves a node up on the page, by -_____-
//     * The children remain unmoved where possible. */
//    int note::node_move_back_special()
//    {
//        /* not implemented */
//
//        rebuild_cache();
//        return 0;
//    }
//
//    /* Moves a node down on the page, by -_____-
//     * The children remain unmoved where possible. */
//    int note::node_move_forward_special()
//    {
//        /* not implemented */
//
//        rebuild_cache();
//        return 0;
//    }

    /* Deletes a node without deleting its children.
     * The children are moved to either the previous node at the same depth, or are raised. */
    int editor::node_delete_special()
    {
        if (cursor_current_child_count() == 0)
        {
            return node_delete_rec();
        }
        else
        {
            /* if the last number of the deleted node's index is 0, raise children;
             * otherwise move them to previous node with the same level as deleted node */

            op_hist_.exec(tree_instance_, command{ cmd::multi_cmd{} }, cursor_make_save());

            const auto deleted_node_index{ cursor_current_index() };

            if (last_index_of(cursor_current_index()) > 0)
            {
                mti_t src_index{ cursor_current_index() };
                make_child_index_of(src_index, 0uz);

                mti_t dst_parent_index{ make_index_copy_of(cursor_current_index()) };
                decrement_last_index_of(dst_parent_index);

                const auto src_parent_tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
                const auto dst_parent_tmp{ get_const_by_index(tree_instance_, dst_parent_index) };

                if (src_parent_tmp.has_value() and dst_parent_tmp.has_value())
                {
                    const tree& src_parent_tree_tmp{ src_parent_tmp->get() };
                    const tree& dst_parent_tree_tmp{ dst_parent_tmp->get() };

                    mti_t dst_index{ make_index_copy_of(dst_parent_index) };
                    make_child_index_of(dst_index, dst_parent_tree_tmp.child_count());

                    while (src_parent_tree_tmp.child_count() > 0)
                    {
                        op_hist_.append_multi(tree_instance_, cmd::move_node{ .src = src_index, .dst = dst_index });
                        increment_last_index_of(dst_index);
                    }
                }
                else
                {
                    throw std::runtime_error("node_delete_special: invalid tree");
                }

            }
            else
            {
                mti_t dst_index{ make_index_copy_of(cursor_current_index()) };
                increment_last_index_of(dst_index);

                const auto src_parent_tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };

                if (src_parent_tmp.has_value())
                {
                    const tree& src_parent_tree_tmp{ src_parent_tmp->get() };

                    mti_t src_index{ make_index_copy_of(cursor_current_index())};
                    make_child_index_of(src_index, src_parent_tree_tmp.child_count());

                    while (src_parent_tree_tmp.child_count() > 0)
                    {
                        set_last_index_of(src_index, src_parent_tree_tmp.child_count() - 1);
                        op_hist_.append_multi(tree_instance_, cmd::move_node{ .src = src_index, .dst = dst_index });
                    }
                }
                else
                {
                    throw std::runtime_error("node_delete_special: invalid tree");
                }
            }

            op_hist_.append_multi(tree_instance_, cmd::delete_node{ .pos = deleted_node_index, .deleted = {} });

            rebuild_cache();
            cursor_clamp_x();
            save_cursor_pos_to_hist();
            return 0;
        }
    }
    
    /* Deletes a node and all of its children from the tree */
    int editor::node_delete_rec()
    {
        /* prevent deletion of an empty first node if it is the only node that exists */
        if (tree_instance_.child_count() == 1 and tree_instance_.get_child_const(0).get_content_const().line_length(0) == 0)
            return 1;
        
        op_hist_.exec(tree_instance_, cmd::delete_node{ .pos = cursor_current_index(), .deleted = {} }, cursor_make_save());
        
        /* ensure that tree nodes are not all empty by inserting a blank node if necessary */
        if (tree_instance_.child_count() == 0)
            op_hist_.append_multi(tree_instance_, cmd::insert_node{ .pos = cursor_current_index(), .inserted = tree{} });
        
        rebuild_cache();
        cursor_clamp_x();
        save_cursor_pos_to_hist();
        return 0;
    }
    
    
    /* Cut, Copy, and Paste implementations */
    
    int editor::node_cut()
    {
        /* don't perform deletion if unable to copy */
        if (int const result{ node_copy() }; result != 0)
            return result;
        
        /* the remainder of the function has been copied from node_delete_rec(), but modified slightly */
        
        /* prevent deletion of an empty first node if it is the only node that exists */
        if (tree_instance_.child_count() == 1 and tree_instance_.get_child_const(0).get_content_const().line_length(0) == 0)
            return 1;
        
        op_hist_.exec(tree_instance_, cmd::delete_node{ .pos = cursor_current_index(), .deleted = {}, .is_cut = true }, cursor_make_save());
        
        /* ensure that tree nodes are not all empty by inserting a blank node if necessary */
        if (tree_instance_.child_count() == 0)
            op_hist_.append_multi(tree_instance_, cmd::insert_node{ .pos = cursor_current_index(), .inserted= tree{} });
        
        rebuild_cache();
        cursor_clamp_x();
        save_cursor_pos_to_hist();
        return 0;
    }
    
    int editor::node_copy()
    {
        const auto tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
        
        if (not tmp.has_value())
            return 1;
        
        const tree& tree_temp{ tmp->get() };
        
        /* prevent copying of empty childless nodes
         * copy is redundant because performing an insert is more appropriate */
        if (tree_temp.child_count() == 0 and tree_temp.get_content_const().line_length(0) == 0)
            return 1;
        
        copied_tree_node_buffer_ = tree::make_copy(tree_temp);
        return 0;
    }
    
    int editor::node_paste_above()
    {
        if (not copied_tree_node_buffer_.has_value())
            return 1;
        
        /* the remainder of the function has been copied from node_insert_above(), but modified slightly */
        
        op_hist_.exec(tree_instance_,
                      cmd::insert_node{ .pos = cursor_current_index(), .inserted = tree::make_copy(*copied_tree_node_buffer_), .is_paste = true },
                      cursor_make_save());
        
        rebuild_cache();
        save_cursor_pos_to_hist();
        return 0;
    }
    
    int editor::node_paste_default()
    {
        if (not copied_tree_node_buffer_.has_value())
            return 1;
        
        /* the remainder of the function has been copied from node_insert_default(), but modified slightly */
        
        const auto tmp{ get_const_by_index(tree_instance_, cursor_current_index()) };
        
        if (not tmp.has_value())
            throw std::runtime_error("node_paste_default: cursor index does not exist");
        
        const tree& tree_temp{ tmp->get() };
        auto index{ cursor_current_index() };
        
        if (tree_temp.child_count() == 0)
        {
            if (std::ranges::size(index) == 0)
                throw std::runtime_error("node_paste_default: invalid cursor index");
            
            ++(*std::ranges::rbegin(index));
            op_hist_.exec(tree_instance_,
                          cmd::insert_node{ .pos=index, .inserted=tree::make_copy(*copied_tree_node_buffer_), .is_paste=true },
                          cursor_make_save());
            
            rebuild_cache();
            cursor_nd_next();
        }
        else
        {
            index.push_back(0uz);
            op_hist_.exec(tree_instance_,
                          cmd::insert_node{ .pos=index, .inserted=tree::make_copy(*copied_tree_node_buffer_), .is_paste=true },
                          cursor_make_save());
            
            rebuild_cache();
            cursor_mv_down();
        }
        
        save_cursor_pos_to_hist();
        return 0;
    }
}