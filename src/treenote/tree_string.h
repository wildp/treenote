// tree_string.h
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

#include <string>
#include <vector>

#include "table.hpp"
#include "tree_cmd.hpp"
#include "note_buffer.h"

namespace treenote
{
    class tree_string
    {
    public:
        using opt_idx_pair = std::optional<std::pair<std::size_t, std::size_t>>;
        
        /* note: in all functions below, `pos` and `len` refer to display lengths: */
        
        tree_string();
        tree_string(const extended_piece_table_entry& input);
        void add_line(const extended_piece_table_entry& more_input);
        [[nodiscard]] tree_string make_copy() const;
    
        tree_string(const tree_string&) = delete;
        tree_string(tree_string&&) = default;
        tree_string& operator=(const tree_string&) = delete;
        tree_string& operator=(tree_string&&) = default;
        ~tree_string() = default;
        
        /* the 5 functions below return true if a new edit_cmd should be added to tree_hist, and false otherwise: */
        
        bool insert_str(std::size_t line, std::size_t pos, const extended_piece_table_entry& inserted, std::size_t& cursor_inc_amt);
        bool delete_char_before(std::size_t line, std::size_t pos, std::size_t& cursor_dec_amt);
        bool delete_char_current(std::size_t line, std::size_t pos);
        bool make_line_break(std::size_t upper_line, std::size_t upper_line_pos);
        bool make_line_join(std::size_t upper_line);
        
        int undo();
        int redo();
        
        [[nodiscard]] std::size_t line_count() const noexcept;
        [[nodiscard]] std::size_t line_length(std::size_t line) const;
        
        [[nodiscard]] std::string to_str(std::size_t line) const;
        [[nodiscard]] std::string to_substr(std::size_t line, std::size_t pos, std::size_t len) const;
        
        void set_no_longer_current();
        
        [[nodiscard]] cmd_names get_current_cmd_name() const;
        
        [[nodiscard]] bool empty() const noexcept;

//        [[nodiscard]] std::string debug_get_table_entry_string(std::size_t line) const;
//        [[nodiscard]] const std::string& debug_get_buffer() const;
        
    private:
        
        void clear_hist_if_needed();
        void exec(table_command&& tc);
        
        void invoke(const table_command& tc);
        void invoke_reverse(const table_command& tc);
        
        [[nodiscard]] std::string index_of_char_within_entry(const piece_table_entry& entry, std::size_t pos_in_entry) const;
        [[nodiscard]] std::size_t entry_last_char_len(const piece_table_entry& entry);
        [[nodiscard]] std::size_t entry_first_char_len(const piece_table_entry& entry);
        
        piece_table_t                                               piece_table_vec_;
        
        std::vector<table_command>                                  piece_table_hist_;
        std::size_t                                                 piece_table_hist_pos_{ 0 };
        
        const note_buffer*                                          buffer_ptr_; /* non owning, can be null */
        tree_string_token                                           token_;
    };
    
    
    /* Inline function implementations */
    
    inline int tree_string::undo()
    {
        set_no_longer_current();
        
        if (piece_table_hist_pos_ != 0)
        {
            --piece_table_hist_pos_;
            invoke_reverse(piece_table_hist_[piece_table_hist_pos_]);
            return 0;
        }
        else
        {
            return 1;
        }
    }
    
    inline int tree_string::redo()
    {
        set_no_longer_current();
        
        if (piece_table_hist_pos_ < piece_table_hist_.size())
        {
            invoke(piece_table_hist_[piece_table_hist_pos_]);
            ++piece_table_hist_pos_;
            return 0;
        }
        else
        {
            return 1;
        }
    }
    
    inline void tree_string::exec(table_command&& tc)
    {
        clear_hist_if_needed();
        piece_table_hist_.push_back(std::move(tc));
        invoke(piece_table_hist_.back());
        ++piece_table_hist_pos_;
    }
    
    inline std::size_t tree_string::line_count() const noexcept
    {
        return piece_table_vec_.size();
    }
    
    inline void tree_string::set_no_longer_current()
    {
        token_.release();
    }
    
    inline std::size_t tree_string::entry_last_char_len(const piece_table_entry& entry)
    {
        if (entry_has_no_mb_char(entry))
            return 1;
        else
            return index_of_char_within_entry(entry, entry.display_length - 1).size();
    }

    inline std::size_t tree_string::entry_first_char_len(const piece_table_entry& entry)
    {
        if (entry_has_no_mb_char(entry))
            return 1;
        else
            return index_of_char_within_entry(entry, 0).size();
    }
}
