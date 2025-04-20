// core/tree_string.cpp
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


#include "tree_string.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>

namespace treenote::core
{
    /* Implementation helpers */
    
    namespace detail
    {
        namespace
        {
            template<typename... Ts>
            struct overload : Ts ... { using Ts::operator()...; };
            
            constexpr std::size_t max_hist_size_{ std::numeric_limits<std::ptrdiff_t>::max() };
            
            inline piece_table_entry& get_entry(piece_table_t& pt, const std::size_t line, const std::size_t entry_index)
            {
                /* assume: line < pt.size() and entry_index < pt[line].size() */
                
                return pt.at(line).at(entry_index);
            }
            
            tree_string::opt_idx_pair entry_index_within_table_line(const piece_table_line& line, const std::size_t pos)
            {
                for (std::size_t i{ 0 }, accumulated_len{ 0 }; i < line.size(); ++i)
                {
                    if (pos >= accumulated_len and pos < accumulated_len + line[i].display_length)
                        return std::make_optional<tree_string::opt_idx_pair::value_type>(i, pos - accumulated_len);
                    
                    accumulated_len += line[i].display_length;
                }
                
                return {}; /* pos probably refers to the space at the end of line */
            }
            
            inline pt_cmd::delete_entry::merge_info make_merge_info(piece_table_t& pt, const std::size_t line, const std::size_t entry_index)
            {
                auto& table_line{ pt.at(line) };
                
                /* check if entry to be deleted is at start or end of line (where no merging is possible) */
                if (entry_index == 0 or entry_index == table_line.size() - 1)
                    return {};
                
                auto& before{ table_line[entry_index - 1] };
                auto& after{ table_line[entry_index + 1] };
                
                /* check if merging will take place */
                if (before.start_index + before.byte_length == after.start_index)
                    return { before.display_length };
                else
                    return {};
            }
            
            void grow_entry_rhs(piece_table_entry& entry, const std::size_t display_amt, const std::size_t byte_amt)
            {
                entry.display_length += display_amt;
                entry.byte_length += byte_amt;
            }
            
            void shrink_entry_rhs(piece_table_entry& entry, const std::size_t display_amt, const std::size_t byte_amt)
            {
                /* assume: display_amt <= entry.display_length and byte_amt <= entry.byte_length */
                
                entry.display_length -= display_amt;
                entry.byte_length -= byte_amt;
            }
            
            void shrink_entry_lhs(piece_table_entry& entry, const std::size_t display_amt, const std::size_t byte_amt)
            {
                /* assume: display_amt <= entry.display_length and byte_amt <= entry.byte_length */
                
                entry.start_index += byte_amt;
                entry.display_length -= display_amt;
                entry.byte_length -= byte_amt;
            }
            
            void unshrink_entry_lhs(piece_table_entry& entry, const std::size_t display_amt, const std::size_t byte_amt)
            {
                /* assume: entry.start_index >= byte_amt */
                
                entry.start_index -= byte_amt;
                entry.display_length += display_amt;
                entry.byte_length += byte_amt;
            }
            
            void insert_entry_naive(piece_table_t& pt, const std::size_t line, const std::size_t entry_index, const piece_table_entry& entry)
            {
                auto& table_line{ pt.at(line) };
                table_line.insert(std::ranges::begin(table_line) + static_cast<ptrdiff_t>(entry_index), entry);
            }
            
            void delete_entry_and_merge(piece_table_t& pt, const std::size_t line, const std::size_t entry_index)
            {
                auto& table_line{ pt.at(line) };
                bool erase_entry_after_pos{ false };
                
                /* attempt to merge entries from before */
                if (entry_index > 0 and entry_index + 1 < table_line.size())
                {
                    auto& before{ table_line[entry_index - 1] };
                    auto& after{ table_line[entry_index + 1] };
                    
                    if (before.start_index + before.byte_length == after.start_index)
                    {
                        /* merge back table_entry of first line and front table entry of second line if adjacent */
                        before.display_length += after.display_length;
                        before.byte_length += after.byte_length;
                        erase_entry_after_pos = true;
                    }
                }

                const auto erase_pos{ std::ranges::begin(table_line) + static_cast<ptrdiff_t>(entry_index) };
                
                if (erase_entry_after_pos)
                    table_line.erase(erase_pos, erase_pos + 2);
                else
                    table_line.erase(erase_pos);
            }
            
            void split_entry_remove_inside(piece_table_t& pt, const buffer* buffer_ptr, const std::size_t line, const std::size_t original_entry_index, const std::size_t l_boundary_pos, const std::size_t r_boundary_pos)
            {
                /* assume: l_boundary_pos <= r_boundary_pos and r_boundary_pos < pt.at(line).at(original_entry_index).display_length
                 * note: if l_boundary_pos == 0, then shrink_lhs should be called instead of this                                   */
                
                auto& table_line{ pt.at(line) };
                auto& original{ table_line.at(original_entry_index) };
                
                std::size_t left_bytes{ l_boundary_pos };
                std::size_t skipped_bytes{ r_boundary_pos };
                
                if (not entry_has_no_mb_char(original))
                {
                    /* string fragment contains multibyte characters: proceed carefully */
                    
                    auto buf_begin{ std::ranges::cbegin(*buffer_ptr) + static_cast<std::ptrdiff_t>(original.start_index) };
                    const auto buf_end{ buf_begin + static_cast<std::ptrdiff_t>(original.byte_length) };
                    
                    std::string tmp{};
                    std::size_t skipped{ 0 };
                    left_bytes = 0;
                    
                    for (; skipped < l_boundary_pos; ++skipped)
                    {
                        utf8::str_it_get_ext(buf_begin, buf_end, tmp);
                        left_bytes += tmp.size();
                    }
                    
                    skipped_bytes = left_bytes;
                    
                    for (; skipped < r_boundary_pos; ++skipped)
                    {
                        utf8::str_it_get_ext(buf_begin, buf_end, tmp);
                        skipped_bytes += tmp.size();
                    }
                }
                
                const piece_table_entry right{ .start_index = original.start_index + skipped_bytes,
                                               .display_length = original.display_length - r_boundary_pos,
                                               .byte_length = original.byte_length - skipped_bytes };
                
                original.display_length = l_boundary_pos;
                original.byte_length = left_bytes;

                const auto insert_pos{ std::ranges::begin(table_line) + static_cast<ptrdiff_t>(original_entry_index) + 1 };
                table_line.insert(insert_pos, right);
            }
            
            void undo_split_entry_remove_inside(piece_table_t& pt, const std::size_t line, const std::size_t original_entry_index, const std::size_t r_boundary_pos)
            {
                /* assume: pt.at(line).size() > 1 and original_entry_index < (pt.at(line).size() - 1) */
                
                auto& table_line{ pt.at(line) };
                auto& original{ table_line.at(original_entry_index) };
                auto& snd_half{ table_line.at(original_entry_index + 1) };
                
                /* assume: original.start_index + original.byte_length == snd_half.start_index
                 * this should be true since this function should only be called to undo a spit_entry_and_insert */
                
                original.display_length = r_boundary_pos + snd_half.display_length;
                original.byte_length = (snd_half.start_index - original.start_index) + snd_half.byte_length;
                
                table_line.erase(std::ranges::begin(table_line) + static_cast<std::ptrdiff_t>(original_entry_index) + 1);
            }
            
            void split_entry_and_insert(piece_table_t& pt, const buffer* buffer_ptr, const std::size_t line, const std::size_t original_entry_index, const std::size_t pos_in_entry, const piece_table_entry& entry)
            {
                /* assume: pos_in_entry < pt.at(line).at(original_entry_index).display_length
                 * note: if pos_in_entry == 0 ,then insert_entry_naive should be called instead of this */

                auto& table_line{ pt.at(line) };
                auto& original{ table_line.at(original_entry_index) };
                
                std::size_t left_bytes{ 0 };
                
                if (entry_has_no_mb_char(original))
                {
                    left_bytes = pos_in_entry;
                }
                else
                {
                    /* string fragment contains multibyte characters: proceed carefully */
                    
                    auto buf_begin{ std::ranges::cbegin(*buffer_ptr) + static_cast<std::ptrdiff_t>(original.start_index) };
                    const auto buf_end{ buf_begin + static_cast<std::ptrdiff_t>(original.byte_length) };
                    
                    std::string tmp{};
                    
                    for (std::size_t skipped{ 0 }; skipped < pos_in_entry; ++skipped)
                    {
                        utf8::str_it_get_ext(buf_begin, buf_end, tmp);
                        left_bytes += tmp.size();
                    }
                }
                
                std::vector to_insert{ entry };
                to_insert.emplace_back(original.start_index + left_bytes, original.display_length - pos_in_entry, original.byte_length - left_bytes);
                original.display_length = pos_in_entry;
                original.byte_length = left_bytes;
                
                const auto insert_pos{ std::ranges::begin(table_line) + static_cast<ptrdiff_t>(original_entry_index) + 1 };
                table_line.insert(insert_pos, std::ranges::begin(to_insert), std::ranges::end(to_insert));
            }
            
            void undo_split_entry_and_insert(piece_table_t& pt, const std::size_t line, const std::size_t original_entry_index)
            {
                /* basically a modified version of delete_entry, performing: delete_entry(pt, line, original_entry_index + 1) */
                delete_entry_and_merge(pt, line, original_entry_index + 1);
            }
            
            void undo_delete_entry_and_merge(piece_table_t& pt, const buffer* buffer_ptr, const std::size_t line, const std::size_t idx, const piece_table_entry& entry, const pt_cmd::delete_entry::merge_info& merge_pos)
            {
                if (idx == 0 or not merge_pos.has_value())
                    insert_entry_naive(pt, line, idx, entry);
                else
                    split_entry_and_insert(pt, buffer_ptr, line, idx - 1, *merge_pos, entry);
            }
            
            void split_lines(piece_table_t& pt, const buffer* buffer_ptr, const std::size_t line, const std::size_t pos)
            {
                /* assume: line + 1 != 0 and line + 1 < pt.size()
                 * (no assumptions required on pos being valid)  */
                
                if (pos == 0)
                {
                    pt.emplace(std::ranges::cbegin(pt) + static_cast<ptrdiff_t>(std::min(line, pt.size())));
                }
                else
                {
                    pt.emplace(std::ranges::cbegin(pt) + static_cast<ptrdiff_t>(std::min(line + 1, pt.size())));
                    
                    auto& fst{ pt.at(line) };
                    auto& snd{ pt.at(line + 1) };
                    
                    if (not fst.empty())
                    {
                        std::size_t ignored_count{ 0 };
                        
                        auto fst_begin{ std::ranges::begin(fst) };
                        const auto fst_end{ std::ranges::end(fst) };
                        for (; fst_begin != fst_end; ++fst_begin)
                        {
                            if (ignored_count >= pos)
                            {
                                break;
                            }
                            else if (ignored_count + fst_begin->display_length > pos)
                            {
                                /* discard string fragment until we reach pos */
                                
                                std::size_t ignored_bytes{ 0 };
                                std::size_t ignored_current{ 0 };
                                
                                if (fst_begin->display_length == fst_begin->byte_length)
                                {
                                    ignored_current = pos - ignored_count;
                                    ignored_bytes = ignored_current;
                                }
                                else
                                {
                                    /* string fragment contains multibyte characters: proceed carefully */
                                    
                                    auto buf_begin{ std::ranges::cbegin(*buffer_ptr) + static_cast<std::ptrdiff_t>(fst_begin->start_index) };
                                    const auto buf_end{buf_begin + static_cast<std::ptrdiff_t>(fst_begin->byte_length) };
                                    
                                    std::string tmp{};
                                    const std::size_t to_ignore{ pos - ignored_count };
                                    
                                    while (ignored_current < to_ignore)
                                    {
                                        utf8::str_it_get_ext(buf_begin, buf_end, tmp);
                                        ++ignored_current;
                                        ignored_bytes += tmp.size();
                                    }
                                }
                                
                                /* now split final piece table entry into two */
                                snd.emplace_back(fst_begin->start_index + ignored_bytes,
                                                 fst_begin->display_length - ignored_current,
                                                 fst_begin->byte_length - ignored_bytes);
                                
                                fst_begin->display_length = ignored_current;
                                fst_begin->byte_length = ignored_bytes;
                                
                                
                                ++fst_begin;
                                break; /* otherwise set ignore_count to pos */
                            }
                            else
                            {
                                ignored_count += fst_begin->display_length;
                            }
                        }
                        
                        /* now move remainder of fst into snd */
                        snd.reserve(snd.size() + std::ranges::distance(fst_begin, fst_end));
                        std::move(fst_begin, fst_end, std::back_inserter(snd));
                        fst.erase(fst_begin, fst_end);
                    }
                }
            }
            
            void join_lines(piece_table_t& pt, const std::size_t line_after)
            {
                /* line_after is the index of the joined line
                 * assume: line_after + 1 != 0 and line_after + 1 < pt.size() */
                
                auto& fst{ pt.at(line_after) };
                auto& snd{ pt.at(line_after + 1) };
                
                if (not snd.empty())
                {
                    if (not fst.empty())
                    {
                        if (fst.back().start_index + fst.back().byte_length == snd.front().start_index)
                        {
                            /* merge back table_entry of first line and front table entry of second line if adjacent */
                            fst.back().display_length += snd.front().display_length;
                            fst.back().byte_length += snd.front().byte_length;
                            
                            if (snd.size() > 1)
                            {
                                fst.reserve(fst.size() + std::ranges::size(snd) - 1);
                                std::ranges::move(snd | std::views::drop(1), std::back_inserter(fst));
                            }
                        }
                        else
                        {
                            fst.reserve(fst.size() + std::ranges::size(snd));
                            std::ranges::move(snd, std::back_inserter(fst));
                        }
                    }
                    else
                    {
                        fst = std::move(snd);
                    }
                }
                
                pt.erase(std::ranges::begin(pt) + static_cast<std::ptrdiff_t>(line_after) + 1);
            }
            
        }
    }
    
    
    /* Constructor implementation */
    
    tree_string::tree_string() :
        buffer_ptr_{ nullptr }
    {
        piece_table_vec_.emplace_back();
    }
    
    tree_string::tree_string(const extended_piece_table_entry& input) :
            buffer_ptr_{ input.second }
    {
        piece_table_vec_.emplace_back();
        
        if (input.first.display_length > 0)
            piece_table_vec_.back().emplace_back(input.first);
    }
    
    
    /* Not actually a constructor but used during loading files */
    
    void tree_string::add_line(const extended_piece_table_entry& more_input)
    {
        if (not buffer_ptr_)
            buffer_ptr_ = more_input.second;
        else if (buffer_ptr_ != more_input.second)
            throw std::logic_error("table_string cannot contain entries from more than one note_buffer");
        
        piece_table_vec_.emplace_back();
        
        if (more_input.first.display_length > 0)
            piece_table_vec_.back().emplace_back(more_input.first);
    }
    
    
    /* Not actually a constructor but used to construct copies of tree_string */
    
    tree_string tree_string::make_copy() const
    {
        tree_string_token::reset();
        tree_string result{};
        result.piece_table_vec_ = piece_table_vec_;
        result.buffer_ptr_ = buffer_ptr_;
        return result;
    }
    
    
    /* Public string operation functions */
    
    bool tree_string::insert_str(std::size_t line, std::size_t pos, const extended_piece_table_entry& ext_inserted, std::size_t& cursor_inc_amt)
    {
        if (not buffer_ptr_)
            buffer_ptr_ = ext_inserted.second;
        else if (buffer_ptr_ != ext_inserted.second)
            throw std::logic_error("tree_string::insert_str(): table_string cannot contain entries from more than one note_buffer");
        
        const auto& inserted{ ext_inserted.first };
        
        /* precondition: str does not contain newlines */
        
        if (inserted.display_length == 0)
        {
            /* no text inserted */
            cursor_inc_amt = 0;
            return false;
        }
        
        std::optional<std::size_t> merge_insert_entry_idx{};    /* contains the index of the table entry to merge insert with;
                                                                 * if empty, issue a new command instead                      */
        
        /* ensure that line is within bounds. */
        
        if (line >= piece_table_vec_.size())
        {
            line = std::min(piece_table_vec_.size(), 1uz) - 1;
            pos = -1; /* we don't actually care if pos is within bounds, since if pos is too large we insert and end of line anyway */
        }
        
        /* identify whether we can join this insertion with the previous one:
         * (as to alter piece table entry and hist instead of generating new command) */
        
        if (token_.check(pt_cmd_type::insertion, line, pos) and not piece_table_hist_.empty())
        {
            /* identify previously inserted-into piece table entry */
            
            for (std::size_t entry_idx{ 0 }, sum_pos{ 0 }; entry_idx < piece_table_vec_[line].size(); ++entry_idx)
            {
                const auto& entry{ piece_table_vec_[line][entry_idx] };
                
                sum_pos += entry.display_length;
                
                if (pos < sum_pos)
                {
                    break;
                }
                else if (pos == sum_pos)
                {
                    if (entry.start_index + entry.byte_length == inserted.start_index)
                        merge_insert_entry_idx = entry_idx;
                    break;
                }
            }
            
            /* then edit history if found */
            
            if (merge_insert_entry_idx)
            {
                bool cancel{ true };
                
                if (not piece_table_hist_.empty())
                {
                    table_command& last_cmd{ piece_table_hist_.back() };
                    
                    /* alter top of piece_table_hist_ in place */
                    if (std::holds_alternative<pt_cmd::split_insert>(last_cmd))
                    {
                        pt_cmd::split_insert& hist_top{ std::get<pt_cmd::split_insert>(last_cmd) };
                        
                        if (hist_top.line == line)
                        {
                            detail::grow_entry_rhs(hist_top.inserted, inserted.display_length, inserted.byte_length);
                            cancel = false;
                        }
                    }
                    else if (std::holds_alternative<pt_cmd::grow_rhs>(last_cmd))
                    {
                        pt_cmd::grow_rhs& hist_top{ std::get<pt_cmd::grow_rhs>(last_cmd) };
                        
                        if (hist_top.line == line)
                        {
                            hist_top.display_amt += inserted.display_length;
                            hist_top.byte_amt += inserted.byte_length;
                            cancel = false;
                        }
                    }
                    else if (std::holds_alternative<pt_cmd::insert_entry>(last_cmd))
                    {
                        pt_cmd::insert_entry& hist_top{ std::get<pt_cmd::insert_entry>(last_cmd) };
                        
                        if (hist_top.line == line)
                        {
                            detail::grow_entry_rhs(hist_top.inserted, inserted.display_length, inserted.byte_length);
                            cancel = false;
                        }
                    }
                }
                
                if (cancel)
                {
                    /* cannot edit history, revert to generating a new command */
                    merge_insert_entry_idx = {};
                }
            }
        }
        
        /* then, generate command to update piece table depending on the location of the input
         * (or alter existing piece table entry in place)                                      */
        
        if (merge_insert_entry_idx)
        {
            /* alter piece table entry and hist */
            auto& entry{ piece_table_vec_[line][*merge_insert_entry_idx] };
            detail::grow_entry_rhs(entry, inserted.display_length, inserted.byte_length);
        }
        else if (pos == 0)
        {
            /* insert table entry at start of line */
            exec(table_command{ pt_cmd::insert_entry{ .line = line,
                                                      .entry_index = 0,
                                                      .inserted = inserted } });
        }
        else
        {
            const auto& table_line{ piece_table_vec_[line] };
            
            for (std::size_t i{ 0 }, accumulated_len{ 0 }; i < table_line.size(); ++i)
            {
                if (pos < accumulated_len + table_line[i].display_length)
                {
                    /* inserted position lies within table entry i, make a split. */
                    exec(table_command{ pt_cmd::split_insert{ .line = line,
                                                              .original_entry_index = i,
                                                              .pos_in_entry = pos - accumulated_len,
                                                              .inserted = inserted } });
                    break;
                }
                else if (pos == accumulated_len + table_line[i].display_length or i + 1 == table_line.size())
                {
                    /* inserted position is immediately after table entry i; check if possible to grow_rhs instead of insert
                     * if piece_table_vec_[i] is the last entry in table_line, we append the string regardless of the value of pos */
                    if (table_line[i].start_index + table_line[i].byte_length == inserted.start_index)
                    {
                        /* piece table entry points to last string fragment in buffer; grow rhs of entry */
                        exec(table_command{ pt_cmd::grow_rhs{ .line = line,
                                                              .entry_index = i,
                                                              .display_amt = inserted.display_length,
                                                              .byte_amt = inserted.byte_length } });
                    }
                    else
                    {
                        /* unable to grow table entry; insert new entry afterwards instead */
                        exec(table_command{ pt_cmd::insert_entry{ .line = line,
                                                                  .entry_index = i + 1,
                                                                  .inserted = inserted } });
                    }
                    break;
                }
                
                accumulated_len += table_line[i].display_length;
            }
        }
        
        token_.acquire(pt_cmd_type::insertion, line, pos + inserted.display_length);
        cursor_inc_amt = inserted.display_length;
        return (not merge_insert_entry_idx);
    }
    
    bool tree_string::delete_char_before(std::size_t line, const std::size_t pos, std::size_t& cursor_dec_amt)
    {
        /* generate and exec command, or extend top command to update piece table */

        cursor_dec_amt = 0;
        
        if (pos == 0)
        {
            /* cannot delete before first char in line */
            return false;
        }
        
        bool command_merged{ false };
        bool new_command_issued{ false };
        
        auto& table_line{ piece_table_vec_.at(line) };
        
        /* identify whether we can join this deletion with the previous one:
         * (as to alter piece table entry and hist instead of generating new command) */
        
        if (token_.check(pt_cmd_type::deletion_b, line, pos) and not piece_table_hist_.empty() and pos > 0)
        {
            const auto eiwtl{ detail::entry_index_within_table_line(table_line, pos - 1) };
            
            if (eiwtl.has_value())
            {
                bool success{ true };

                const std::size_t entry_idx{ eiwtl->first };
                const std::size_t pos_in_entry{ eiwtl->second };
                
                std::reference_wrapper last_sub_cmd{ piece_table_hist_.back() };
                
                /* make last_sub_cmd be a reference to the last command invoked
                 * (we assume that we can't have multi_cmd of multi_cmd)         */
                if (std::holds_alternative<pt_cmd::multi_cmd>(last_sub_cmd.get()))
                {
                    pt_cmd::multi_cmd& hist_top{ std::get<pt_cmd::multi_cmd>(last_sub_cmd.get()) };
                    if (not hist_top.commands.empty())
                        last_sub_cmd = std::ref(hist_top.commands.back());
                }
                
                /* alter top of piece_table_hist_ in place */
                
                if (std::holds_alternative<pt_cmd::split_delete>(last_sub_cmd.get()))
                {
                    if (table_line[entry_idx].display_length == 1)
                    {
                        if (entry_idx + 1 < table_line.size())
                        {
                            /* replace split_delete command with shrink_lhs */
                            const piece_table_entry before_copy{ table_line[entry_idx + 1] };
                            invoke_reverse(last_sub_cmd.get());
                            const piece_table_entry after_copy{ table_line[entry_idx] };
                            last_sub_cmd.get().emplace<pt_cmd::shrink_lhs>(line, entry_idx,
                                                                           after_copy.display_length - before_copy.display_length,
                                                                           after_copy.byte_length - before_copy.byte_length);
                            invoke(last_sub_cmd.get());
                        }
                        else
                        {
                            /* error with replacing split_delete with shrink_lhs */
                            success = false;
                        }
                    }
                    else
                    {
                        /* shrink rhs of entry further */
                        auto& entry{ table_line[entry_idx] };
                        pt_cmd::split_delete& hist_top{ std::get<pt_cmd::split_delete>(last_sub_cmd.get()) };
                        std::size_t byte_amt{ 1 };

                        if (not entry_has_no_mb_char(entry))
                            byte_amt = entry_last_char_len(entry);

                        detail::shrink_entry_rhs(entry, 1, byte_amt);
                        hist_top.l_boundary_pos -= 1;
                    }
                }
                else if (std::holds_alternative<pt_cmd::shrink_rhs>(last_sub_cmd.get()))
                {
                    if (table_line[entry_idx].display_length == 1)
                    {
                        /* replace shrink_rhs command with delete_entry */
                        invoke_reverse(last_sub_cmd.get());
                        last_sub_cmd.get().emplace<pt_cmd::delete_entry>(line, entry_idx, table_line[entry_idx],
                                                                         detail::make_merge_info(piece_table_vec_, line, entry_idx));
                        invoke(last_sub_cmd.get());
                    }
                    else
                    {
                        /* shrink rhs of entry further */
                        auto& entry{ table_line[entry_idx] };
                        pt_cmd::shrink_rhs& hist_top{ std::get<pt_cmd::shrink_rhs>(last_sub_cmd.get()) };
                        std::size_t byte_amt{ 1 };
                        
                        if (not entry_has_no_mb_char(entry))
                            byte_amt = entry_last_char_len(entry);
                        
                        detail::shrink_entry_rhs(entry, 1, byte_amt);
                        hist_top.display_amt += 1;
                        hist_top.byte_amt += byte_amt;
                    }
                }
                else if (std::holds_alternative<pt_cmd::shrink_lhs>(last_sub_cmd.get()) or
                         std::holds_alternative<pt_cmd::delete_entry>(last_sub_cmd.get()))
                {
                    /* convert last command into a multi_cmd if necessary */
                    if (not std::holds_alternative<pt_cmd::multi_cmd>(piece_table_hist_.back()))
                    {
                        pt_cmd::multi_cmd new_top{};
                        new_top.commands.emplace_back(piece_table_hist_.back());
                        piece_table_hist_.back() = std::move(new_top);
                    }
                    
                    pt_cmd::multi_cmd& cmd_vec{ std::get<pt_cmd::multi_cmd>(piece_table_hist_.back()) };
                    
                    if (table_line[entry_idx].display_length == 1)
                    {
                        /* add command to delete table entry */
                        cmd_vec.commands.emplace_back(pt_cmd::delete_entry{ .line = line,
                                                                            .entry_index = entry_idx,
                                                                            .deleted = table_line[entry_idx],
                                                                            .merge_pos_in_prev = detail::make_merge_info(piece_table_vec_, line, entry_idx) });
                        invoke(cmd_vec.commands.back());
                    }
                    else if (pos_in_entry == 0)
                    {
                        /* no merging happened; add command to shrink_lhs table entry */
                        std::size_t byte_amt{ 1 };
                        
                        if (not entry_has_no_mb_char(table_line[entry_idx]))
                            byte_amt = entry_first_char_len(table_line[entry_idx]);
                        
                        cmd_vec.commands.emplace_back(pt_cmd::shrink_lhs{ .line = line,
                                                                          .entry_index = entry_idx,
                                                                          .display_amt = 1,
                                                                          .byte_amt = byte_amt });
                        invoke(cmd_vec.commands.back());
                    }
                    else if (pos_in_entry + 1 < table_line[entry_idx].display_length)
                    {
                        /* merge happened; insert split command and then shrink lhs */
                        cmd_vec.commands.emplace_back(pt_cmd::split_delete{ .line = line,
                                                                            .original_entry_index = entry_idx,
                                                                            .l_boundary_pos = pos_in_entry,
                                                                            .r_boundary_pos = pos_in_entry + 1 });
                        invoke(cmd_vec.commands.back());
                    }
                    else if (pos_in_entry + 1 == table_line[entry_idx].display_length)
                    {
                        /* merge happened, but pos is now at end of table entry; shrink rhs */
                        cmd_vec.commands.emplace_back(pt_cmd::shrink_rhs{ .line = line,
                                                                          .entry_index = entry_idx,
                                                                          .display_amt = 1,
                                                                          .byte_amt = entry_last_char_len(table_line[entry_idx]) });
                        invoke(cmd_vec.commands.back());
                    }
                    else
                    {
                        /* pos is invalid, revert to generating a new command */
                        success = false;
                    }
                }
                else
                {
                    /* cannot edit history, revert to generating a new command */
                    success = false;
                }
                
                if (success)
                {
                    cursor_dec_amt = 1;
                    command_merged = true;
                    new_command_issued = true; /* set this to skip inserting new command */
                }
            }
        }
        
        /* then, generate command to update piece table depending on the location of the input */
        
        for (std::size_t i{ 0 }, accumulated_len{ 0 }; not new_command_issued and i < table_line.size(); ++i)
        {
            if (table_line[i].display_length > 0)
            {
                if (pos == accumulated_len + table_line[i].display_length)
                {
                    /* delete last char from pt entry by shrinking rhs of entry */
                    if (table_line[i].display_length == 1)
                    {
                        /* delete table entry instead of shrinking */
                        exec(table_command{ pt_cmd::delete_entry{ .line = line,
                                                                  .entry_index = i,
                                                                  .deleted = table_line[i],
                                                                  .merge_pos_in_prev = detail::make_merge_info(piece_table_vec_, line, i) } });
                    }
                    else
                    {
                        /* delete last char from pt entry by shrinking rhs of entry */
                        if (not entry_has_no_mb_char(table_line[i]))
                            exec(table_command{ pt_cmd::shrink_rhs{ .line = line,
                                                                    .entry_index = i,
                                                                    .display_amt = 1,
                                                                    .byte_amt = 1 } });
                        else
                            exec(table_command{ pt_cmd::shrink_rhs{ .line = line,
                                                                    .entry_index = i,
                                                                    .display_amt = 1,
                                                                    .byte_amt = entry_last_char_len(table_line[i]) } });
                    }
                    
                    cursor_dec_amt = 1;
                    new_command_issued = true;
                }
                else if (pos == accumulated_len + 1)
                {
                    /* delete first char from pt entry by shrinking lhs of entry */
                    if (not entry_has_no_mb_char(table_line[i]))
                        exec(table_command{ pt_cmd::shrink_lhs{ .line = line,
                                                                .entry_index = i,
                                                                .display_amt = 1,
                                                                .byte_amt = 1 } });
                    else
                        exec(table_command{ pt_cmd::shrink_lhs{ .line = line,
                                                                .entry_index = i,
                                                                .display_amt = 1,
                                                                .byte_amt = entry_first_char_len(table_line[i]) } });
                    
                    cursor_dec_amt = 1;
                    new_command_issued = true;
                }
                else if (pos < accumulated_len + table_line[i].display_length)
                {
                    /* perform split operation and shrink rhs of left side */
                    exec(table_command{ pt_cmd::split_delete{ .line = line,
                                                              .original_entry_index = i,
                                                              .l_boundary_pos = pos - 1 - accumulated_len,
                                                              .r_boundary_pos = pos - accumulated_len } });
                    
                    cursor_dec_amt = 1;
                    new_command_issued = true;
                }
                
                accumulated_len += table_line[i].display_length;
            }
        }
        
        token_.acquire(pt_cmd_type::deletion_b, line, pos - cursor_dec_amt);
        return (not command_merged and new_command_issued);
    }
    
    bool tree_string::delete_char_current(const std::size_t line, const std::size_t pos)
    {
        /* generate and exec command, or extend top command to update piece table */
        
        bool command_merged{ false };
        bool new_command_issued{ false };
        
        auto& table_line{ piece_table_vec_[line] };
        
        /* identify whether we can join this insertion with the previous one:
         * (as to alter piece table entry and hist instead of generating new command) */
        
        
        if (token_.check(pt_cmd_type::deletion_c, line, pos) and not piece_table_hist_.empty())
        {
            const auto eiwtl{ detail::entry_index_within_table_line(table_line, pos) };
            
            if (eiwtl.has_value())
            {
                bool success{ true };
                
                const std::size_t entry_idx{ eiwtl->first };
                const std::size_t pos_in_entry{ eiwtl->second };
                
                std::reference_wrapper last_sub_cmd{ piece_table_hist_.back() };
                
                /* make last_sub_cmd be a reference to the last command invoked
                 * (we assume that we can't have multi_cmd of multi_cmd)         */
                if (std::holds_alternative<pt_cmd::multi_cmd>(last_sub_cmd.get()))
                {
                    pt_cmd::multi_cmd& hist_top{ std::get<pt_cmd::multi_cmd>(last_sub_cmd.get()) };
                    if (not hist_top.commands.empty())
                        last_sub_cmd = std::ref(hist_top.commands.back());
                }
                
                /* alter top of piece_table_hist_ in place */
                
                if (std::holds_alternative<pt_cmd::split_delete>(last_sub_cmd.get()))
                {
                    if (table_line[entry_idx].display_length == 1)
                    {
                        if (entry_idx > 0)
                        {
                            const piece_table_entry before_copy{ table_line[entry_idx - 1] };
                            
                            /* replace split_delete command with shrink_rhs */
                            invoke_reverse(last_sub_cmd.get());
                            const piece_table_entry after_copy{ table_line[entry_idx - 1] };
                            last_sub_cmd.get().emplace<pt_cmd::shrink_rhs>(line, entry_idx - 1,
                                                                           after_copy.display_length - before_copy.display_length,
                                                                           after_copy.byte_length - before_copy.byte_length);
                            invoke(last_sub_cmd.get());
                        }
                        else
                        {
                            /* error with replacing split_delete with shrink_rhs */
                            success = false;
                        }
                    }
                    else
                    {
                        /* shrink lhs of entry further */
                        auto& entry{ table_line[entry_idx] };
                        pt_cmd::split_delete& hist_top{ std::get<pt_cmd::split_delete>(last_sub_cmd.get()) };
                        std::size_t byte_amt{ 1 };
                        
                        if (not entry_has_no_mb_char(entry))
                            byte_amt = entry_first_char_len(entry);
                        
                        detail::shrink_entry_lhs(entry, 1, byte_amt);
                        hist_top.r_boundary_pos += 1;
                    }
                    
                }
                else if (std::holds_alternative<pt_cmd::shrink_lhs>(last_sub_cmd.get()))
                {
                    if (table_line[entry_idx].display_length == 1)
                    {
                        /* replace shrink_lhs command with delete_entry */
                        invoke_reverse(last_sub_cmd.get());
                        last_sub_cmd.get().emplace<pt_cmd::delete_entry>(line, entry_idx, table_line[entry_idx],
                                                                         detail::make_merge_info(piece_table_vec_, line, entry_idx));
                        invoke(last_sub_cmd.get());
                    }
                    else
                    {
                        /* shrink lhs of entry further */
                        auto& entry{ table_line[entry_idx] };
                        pt_cmd::shrink_lhs& hist_top{ std::get<pt_cmd::shrink_lhs>(last_sub_cmd.get()) };
                        std::size_t byte_amt{ 1 };
                        
                        if (not entry_has_no_mb_char(entry))
                            byte_amt = entry_first_char_len(entry);
                        
                        detail::shrink_entry_lhs(entry, 1, byte_amt);
                        hist_top.display_amt += 1;
                        hist_top.byte_amt += byte_amt;
                    }
                }
                else if (std::holds_alternative<pt_cmd::shrink_rhs>(last_sub_cmd.get()) or
                         std::holds_alternative<pt_cmd::delete_entry>(last_sub_cmd.get()))
                {
                    /* convert last command into a multi_cmd if necessary */
                    if (not std::holds_alternative<pt_cmd::multi_cmd>(piece_table_hist_.back()))
                    {
                        pt_cmd::multi_cmd new_top{};
                        new_top.commands.emplace_back(piece_table_hist_.back());
                        piece_table_hist_.back() = std::move(new_top);
                    }
                    
                    pt_cmd::multi_cmd& cmd_vec{ std::get<pt_cmd::multi_cmd>(piece_table_hist_.back()) };
                    
                    if (table_line[entry_idx].display_length == 1)
                    {
                        /* add command to delete table entry */
                        cmd_vec.commands.emplace_back(pt_cmd::delete_entry{ .line = line,
                                                                            .entry_index = entry_idx,
                                                                            .deleted = table_line[entry_idx],
                                                                            .merge_pos_in_prev=detail::make_merge_info(piece_table_vec_, line, entry_idx) });
                        invoke(cmd_vec.commands.back());
                    }
                    else if (pos_in_entry == 0)
                    {
                        /* no merging happened; add command to shrink_lhs table entry */
                        std::size_t byte_amt{ 1 };
                        
                        if (not entry_has_no_mb_char(table_line[entry_idx]))
                            byte_amt = entry_first_char_len(table_line[entry_idx]);
                        
                        cmd_vec.commands.emplace_back(pt_cmd::shrink_lhs{ .line = line,
                                                                          .entry_index = entry_idx,
                                                                          .display_amt = 1,
                                                                          .byte_amt = byte_amt });
                        invoke(cmd_vec.commands.back());
                    }
                    else if (pos_in_entry + 1 < table_line[entry_idx].display_length)
                    {
                        /* merge happened; insert split command and then shrink lhs */
                        cmd_vec.commands.emplace_back(pt_cmd::split_delete{ .line = line,
                                                                            .original_entry_index = entry_idx,
                                                                            .l_boundary_pos = pos_in_entry,
                                                                            .r_boundary_pos = pos_in_entry + 1 });
                        invoke(cmd_vec.commands.back());
                    }
                    else if (pos_in_entry + 1 == table_line[entry_idx].display_length)
                    {
                        /* merge happened, but pos is now at end of table entry; shrink rhs */
                        cmd_vec.commands.emplace_back(pt_cmd::shrink_rhs{ .line = line,
                                                                          .entry_index = entry_idx,
                                                                          .display_amt = 1,
                                                                          .byte_amt = entry_last_char_len(table_line[entry_idx]) });
                        invoke(cmd_vec.commands.back());
                    }
                    else
                    {
                        /* pos is invalid, revert to generating a new command */
                        success = false;
                    }
                }
                else
                {
                    /* cannot edit history, revert to generating a new command */
                    success = false;
                }
                
                if (success)
                {
                    command_merged = true;
                    new_command_issued = true; /* set this to skip inserting new command */
                }
            }
        }
        
        /* then, generate command to update piece table depending on the location of the input */
        
        for (std::size_t i{ 0 }, accumulated_len{ 0 }; not new_command_issued and i < table_line.size(); ++i)
        {
            if (table_line[i].display_length > 0)
            {
                if (pos == accumulated_len)
                {
                    if (table_line[i].display_length == 1)
                    {
                        /* delete table entry instead of shrinking */
                        exec(table_command{ pt_cmd::delete_entry{ .line = line,
                                                                  .entry_index = i,
                                                                  .deleted = table_line[i],
                                                                  .merge_pos_in_prev = detail::make_merge_info(piece_table_vec_, line, i) } });
                    }
                    else
                    {
                        /* delete first char from pt entry by shrinking lhs of entry */
                        if (entry_has_no_mb_char(table_line[i]))
                            exec(table_command{ pt_cmd::shrink_lhs{ .line = line,
                                                                    .entry_index = i,
                                                                    .display_amt = 1,
                                                                    .byte_amt = 1 } });
                        else
                            exec(table_command{ pt_cmd::shrink_lhs{ .line = line,
                                                                    .entry_index = i,
                                                                    .display_amt = 1,
                                                                    .byte_amt = entry_first_char_len(table_line[i]) } });
                    }
                    
                    new_command_issued = true;
                }
                else if (pos == accumulated_len + table_line[i].display_length - 1)
                {
                    /* delete last char from pt entry by shrinking rhs of entry */
                    if (entry_has_no_mb_char(table_line[i]))
                        exec(table_command{ pt_cmd::shrink_rhs{ .line = line,
                                                                .entry_index = i,
                                                                .display_amt = 1,
                                                                .byte_amt = 1 } });
                    else
                        exec(table_command{ pt_cmd::shrink_rhs{ .line = line,
                                                                .entry_index = i ,
                                                                .display_amt = 1,
                                                                .byte_amt = entry_last_char_len(table_line[i]) } });
                        
                    new_command_issued = true;
                }
                else if (pos < accumulated_len + table_line[i].display_length - 1)
                {
                    /* perform split operation and shrink lhs of right side */
                    exec(table_command{ pt_cmd::split_delete{ .line = line,
                                                              .original_entry_index = i,
                                                              .l_boundary_pos = pos - accumulated_len,
                                                              .r_boundary_pos = pos + 1 - accumulated_len } });
                    new_command_issued = true;
                }
                
                accumulated_len += table_line[i].display_length;
            }
        }
        
        token_.acquire(pt_cmd_type::deletion_c, line, pos);
        return (not command_merged and new_command_issued);
    }
    
    bool tree_string::make_line_break(const std::size_t upper_line, const std::size_t upper_line_pos)
    {
        if (upper_line >= line_count() or upper_line_pos > line_length(upper_line))
            return false;
        
        /* generate and exec command to update piece table */
        exec(table_command{ pt_cmd::line_break{ .line_before = upper_line, .pos_before = upper_line_pos } });
        
        token_.acquire(pt_cmd_type::linebreak, upper_line, upper_line_pos);
        return true;
    }
    
    bool tree_string::make_line_join(const std::size_t upper_line)
    {
        if (upper_line + 1 >= line_count() or upper_line + 1 == 0)
            return false;
        
        /* generate and exec command to update piece table */
        const std::size_t pos_after{ line_length(upper_line) };
        exec(table_command{ pt_cmd::line_join{ .line_after = upper_line, .pos_after = pos_after } });
        
        token_.acquire(pt_cmd_type::linejoin, upper_line, pos_after);
        return true;
    }
    
    
    /* Public string indexing functions */
    
    std::string tree_string::to_str(const std::size_t line) const
    {
        if (not buffer_ptr_)
        {
            if (piece_table_vec_.at(line).empty())
                return ""; // <- this may need to be updated at some point
            else
                throw std::runtime_error("tree_string::to_str(): non-empty tree_string must have an associated note_buffer");
        }
        
        auto result{ buffer_ptr_->to_str_view(piece_table_vec_.at(line)) };
//        return { std::from_range, result | std::views::join };
        
        // std::string constructor with std::from_range_t hasn't been implemented in GCC yet
        auto tmp{ result | std::views::join };
        return { std::ranges::begin(tmp), std::ranges::end(tmp) };

    }
    
    std::string tree_string::to_substr(const std::size_t line, const std::size_t pos, const std::size_t len) const
    {
        if (not buffer_ptr_)
        {
            if (piece_table_vec_.at(line).empty())
                return ""; // <- this may need to be updated at some point
            else
                throw std::runtime_error("tree_string::to_substr(): non-empty tree_string must have an associated note_buffer");
        }
        
        auto result{ buffer_ptr_->to_substr_view(piece_table_vec_.at(line), pos, len) };
        return result | std::views::join | std::ranges::to<std::string>();
    }
    
    /* Misc public function implementations */
    
    std::size_t tree_string::line_length(const std::size_t line) const
    {
        if (line < piece_table_vec_.size())
            return std::transform_reduce(std::ranges::cbegin(piece_table_vec_[line]), std::ranges::cend(piece_table_vec_[line]),
                                         0uz, std::plus{}, [](const piece_table_entry& te){ return te.display_length; });
        else
            return 0;
    }
    
    cmd_names tree_string::get_current_cmd_name() const
    {
        if (piece_table_hist_pos_ == 0)
            return cmd_names::none;
        
        std::reference_wrapper cmd{ piece_table_hist_[piece_table_hist_pos_ - 1] };
        
        while (std::holds_alternative<pt_cmd::multi_cmd>(cmd.get()))
        {
            const pt_cmd::multi_cmd& multi{ std::get<pt_cmd::multi_cmd>(cmd.get()) };
            
            if (not multi.commands.empty())
                cmd = std::ref(multi.commands.front());
            else
                return cmd_names::error;
        }
        
        using namespace pt_cmd;
        
        return std::visit(detail::overload{
                [](const split_insert&) { return cmd_names::insert_text; },
                [](const split_delete&) { return cmd_names::delete_text; },
                [](const grow_rhs&) { return cmd_names::insert_text; },
                [](const shrink_rhs&) { return cmd_names::delete_text; },
                [](const shrink_lhs&) { return cmd_names::delete_text; },
                [](const insert_entry&) { return cmd_names::insert_text; },
                [](const delete_entry&) { return cmd_names::delete_text; },
                [](const line_break&) { return cmd_names::line_break; },
                [](const line_join&) { return cmd_names::line_join; },
                [](const multi_cmd&) { return cmd_names::error; },
        }, cmd.get());
    }
    
    /* Private member functions */
    
    void tree_string::invoke(const table_command& tc)
    {
        using namespace detail;
        using namespace pt_cmd;
        
        if (not buffer_ptr_)
        {
            const bool success{ std::visit(overload{
                    [&, this](const line_break& c)
                    {
                        if (c.pos_before != 0)
                            return false;
                        split_lines(piece_table_vec_, buffer_ptr_, c.line_before, c.pos_before);
                        return true;
                    },
                    [&, this](const line_join& c) { join_lines(piece_table_vec_, c.line_after); return true; },
                    [&, this](const multi_cmd& cs)
                    {
                        for (const auto& c: cs.commands)
                            invoke(c);
                        return true;
                    },
                    [](const auto&) { return false; }
            }, tc) };
            
            if (not success)
                throw std::runtime_error("tree_string::invoke(): non-empty tree_string must have an associated note_buffer");
            return;
        }
        
        
        // if (not buffer_ptr_)
        //     throw std::runtime_error("tree_string::invoke(): non-empty tree_string must have an associated note_buffer");
        
        std::visit(overload{
            [&, this](const split_insert& c) { split_entry_and_insert(piece_table_vec_, buffer_ptr_, c.line, c.original_entry_index, c.pos_in_entry, c.inserted); },
            [&, this](const split_delete& c) { split_entry_remove_inside(piece_table_vec_, buffer_ptr_, c.line, c.original_entry_index, c.l_boundary_pos, c.r_boundary_pos); },
            [&, this](const grow_rhs& c) { grow_entry_rhs(get_entry(piece_table_vec_, c.line, c.entry_index), c.display_amt, c.byte_amt); },
            [&, this](const shrink_rhs& c) { shrink_entry_rhs(get_entry(piece_table_vec_, c.line, c.entry_index), c.display_amt, c.byte_amt); },
            [&, this](const shrink_lhs& c) { shrink_entry_lhs(get_entry(piece_table_vec_, c.line, c.entry_index), c.display_amt, c.byte_amt); },
            [&, this](const insert_entry& c) { insert_entry_naive(piece_table_vec_, c.line, c.entry_index, c.inserted); },
            [&, this](const delete_entry& c) { delete_entry_and_merge(piece_table_vec_, c.line, c.entry_index); },
            [&, this](const line_break& c) { split_lines(piece_table_vec_, buffer_ptr_, c.line_before, c.pos_before); },
            [&, this](const line_join& c) { join_lines(piece_table_vec_, c.line_after); },
            [&, this](const multi_cmd& cs) { for (const auto& c: cs.commands) invoke(c); }
        }, tc);
    }
    
    void tree_string::invoke_reverse(const table_command& tc)
    {
        using namespace detail;
        using namespace pt_cmd;
        
        if (not buffer_ptr_)
        {
            const bool success{ std::visit(overload{
                    [&, this](const line_break& c) { join_lines(piece_table_vec_, c.line_before); return true; },
                    [&, this](const line_join& c)
                    {
                        if (c.pos_after != 0)
                            return false;
                        split_lines(piece_table_vec_, buffer_ptr_, c.line_after, c.pos_after);
                        return true;
                    },
                    [&, this](const multi_cmd& cs)
                    {
                        for (const auto& c: cs.commands | std::views::reverse)
                            invoke_reverse(c);
                        return true;
                    },
                    [](const auto&) { return false; }
            }, tc) };
            
            if (not success)
                throw std::runtime_error("tree_string::invoke_reverse(): non-empty tree_string must have an associated note_buffer");
            return;
        }

        std::visit(overload{
                [&, this](const split_insert& c) { undo_split_entry_and_insert(piece_table_vec_, c.line, c.original_entry_index); },
                [&, this](const split_delete& c) { undo_split_entry_remove_inside(piece_table_vec_, c.line, c.original_entry_index, c.r_boundary_pos); },
                [&, this](const grow_rhs& c) { shrink_entry_rhs(get_entry(piece_table_vec_, c.line, c.entry_index), c.display_amt, c.byte_amt); },
                [&, this](const shrink_rhs& c) { grow_entry_rhs(get_entry(piece_table_vec_, c.line, c.entry_index), c.display_amt, c.byte_amt); },
                [&, this](const shrink_lhs& c) { unshrink_entry_lhs(get_entry(piece_table_vec_, c.line, c.entry_index), c.display_amt, c.byte_amt); },
                [&, this](const insert_entry& c) { delete_entry_and_merge(piece_table_vec_, c.line, c.entry_index); },
                [&, this](const delete_entry& c) { undo_delete_entry_and_merge(piece_table_vec_, buffer_ptr_, c.line, c.entry_index, c.deleted, c.merge_pos_in_prev); },
                [&, this](const line_break& c) { join_lines(piece_table_vec_, c.line_before); },
                [&, this](const line_join& c) { split_lines(piece_table_vec_, buffer_ptr_, c.line_after, c.pos_after); },
                [&, this](const multi_cmd& cs) { for (const auto& c: cs.commands | std::views::reverse) invoke_reverse(c); }
        }, tc);
    }
    
    void tree_string::clear_hist_if_needed()
    {
        /* copied and pasted from tree_op.cpp, with variable and type names changed */
        
        if (piece_table_hist_pos_ < piece_table_hist_.size())
        {
            /* clear commands in piece_table_hist_ after current */
            piece_table_hist_.erase(std::ranges::begin(piece_table_hist_) + static_cast<std::ptrdiff_t>(piece_table_hist_pos_),
                                    std::ranges::end(piece_table_hist_));
            piece_table_hist_.shrink_to_fit();
        }
        else if (piece_table_hist_pos_ == piece_table_hist_.size())
        {
            /* current command is at end of piece_table_hist_ : no work needed (normally) */
            
            if (piece_table_hist_pos_ == detail::max_hist_size_)
            {
                /* piece_table_hist_ is too big, reduce size of piece_table_hist_ by 50% */
                std::vector<table_command> tmp{};
                auto range{ piece_table_hist_ | std::views::drop(piece_table_hist_pos_ / 2) };
                tmp.reserve(std::ranges::size(range));
                std::ranges::move(range, std::back_inserter(tmp));
                piece_table_hist_ = std::move(tmp);
            }
        }
        else
        {
            throw std::runtime_error("Illegal position in piece table hist of tree_string");
        }
    }
    
    std::string tree_string::index_of_char_within_entry(const piece_table_entry& entry, const std::size_t pos_in_entry) const
    {
        if (pos_in_entry >= entry.display_length)
            throw std::invalid_argument("tree_string::index_of_char_within_entry: pos_in_entry is larger than entry.display_length");
        
        if (entry_has_no_mb_char(entry))
        {
            /* trivial case: no multibyte characters */
            return { buffer_ptr_->at(entry.start_index + pos_in_entry) };
        }
        
        auto buf_begin{ std::ranges::cbegin(*buffer_ptr_) + static_cast<std::ptrdiff_t>(entry.start_index) };
        const auto buf_end{ buf_begin + static_cast<std::ptrdiff_t>(entry.byte_length) };
        
        std::string tmp{};
        
        for (std::size_t i{ 0 }; i <= pos_in_entry; ++i)
            utf8::str_it_get_ext(buf_begin, buf_end, tmp);
        
        if (tmp.empty())
            throw std::runtime_error("tree_string::index_of_char_within_entry: cannot index buffer");
        
        return tmp;
    }
    
    bool tree_string::empty() const noexcept
    {
        return piece_table_hist_.empty() and std::ranges::all_of(piece_table_vec_, [](const piece_table_line& l) { return l.empty(); });
    }
    
    
//    /* Debug functions */
//
//    std::string tree_string::debug_get_table_entry_string(std::size_t line) const
//    {
//        std::string result{};
//
//        const auto& l{ piece_table_vec_.at(line) };
//        for (const auto& pte : l)
//        {
//            result += "(" + std::to_string(pte.start_index) + "," +
//                      std::to_string(pte.display_length) + "," + std::to_string(pte.byte_length) + "),";
//        }
//
//        if (not result.empty())
//            result.pop_back();
//
//        return result;
//    }
//
//    const std::string& tree_string::debug_get_buffer() const
//    {
//        return buffer_;
//    }
    
}