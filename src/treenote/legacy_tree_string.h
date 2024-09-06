// legacy_tree_string.h

#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "table.hpp"
#include "tree_cmd.hpp"
#include "tree_string.h"
#include "utf8.h"

namespace treenote
{
    class legacy_tree_string
    {
    public:
        using opt_idx_pair  = std::optional<std::pair<std::size_t, std::size_t>>;
        
        /* note: in all functions below, `pos` and `len` refer to display lengths: */
        
        legacy_tree_string() = delete;
        
        explicit legacy_tree_string(std::string_view input_sv);
        explicit legacy_tree_string(std::pair<std::string, std::size_t>&& input);
        void add_line(std::pair<std::string, std::size_t>&& more_input);
        [[maybe_unused]] [[nodiscard]] legacy_tree_string make_copy() const;
    
        legacy_tree_string(const legacy_tree_string&) = delete;
        legacy_tree_string(legacy_tree_string&&) = default;
        legacy_tree_string& operator=(const legacy_tree_string&) = delete;
        legacy_tree_string& operator=(legacy_tree_string&&) = default;
        ~legacy_tree_string() = default;
        
        /* the 5 functions below return true if a new edit_cmd should be added to tree_hist, and false otherwise: */
        
        bool insert_str(std::size_t line, std::size_t pos, const std::string& str, std::size_t& cursor_inc_amt);
        bool delete_char_before(std::size_t line, std::size_t pos, std::size_t& cursor_dec_amt);
        bool delete_char_current(std::size_t line, std::size_t pos);
        [[maybe_unused]] bool make_line_break(std::size_t upper_line, std::size_t upper_line_pos);
        [[maybe_unused]] bool make_line_join(std::size_t upper_line);
        
        [[maybe_unused]] int undo();
        [[maybe_unused]] int redo();
        
        [[nodiscard]] std::size_t line_count() const noexcept;
        [[nodiscard]] std::size_t line_length(std::size_t line) const;
        
        [[nodiscard]] std::string to_str(std::size_t line) const;
        [[nodiscard]] std::string to_substr(std::size_t line, std::size_t pos, std::size_t len) const;
        
        [[maybe_unused]] void set_no_longer_current();
        
        [[nodiscard]] cmd_names get_current_cmd_name() const;
        
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
        
        std::string                 buffer_;        /* UTF-8 string buffer (storing text possibly out of order) */
        std::size_t                 buffer_len_;    /* display length of string */
        
        piece_table_t               piece_table_vec_;
        
        std::vector<table_command>  piece_table_hist_;
        std::size_t                 piece_table_hist_pos_{ 0 };
        
        /* last cmd type added to piece_table_hist_ while this legacy_tree_string is currently active */
        pt_cmd_type                 last_action_{ pt_cmd_type::none };
        
        
        /* the optionals below are used for optimisation when repeatedly inserting or deleting characters: */
        
        opt_idx_pair                last_interacted_pos_;   /* display position of most recently inserted/deleted character: (line, pos) */
        opt_idx_pair                last_inserted_te_idx_;  /* location of most recently edited table entry: (line, entry)               */
    };
    
    /* Inline function implementations */
    
    [[maybe_unused]] inline int legacy_tree_string::undo()
    {
        last_action_ = pt_cmd_type::none;
        
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
    
    [[maybe_unused]] inline int legacy_tree_string::redo()
    {
        last_action_ = pt_cmd_type::none;
        
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
    
    inline void legacy_tree_string::exec(table_command&& tc)
    {
        clear_hist_if_needed();
        piece_table_hist_.push_back(std::move(tc));
        invoke(piece_table_hist_.back());
        ++piece_table_hist_pos_;
    }
    
    inline std::size_t legacy_tree_string::line_count() const noexcept
    {
        return piece_table_vec_.size();
    }
    
    [[maybe_unused]] inline void legacy_tree_string::set_no_longer_current()
    {
        last_action_ = pt_cmd_type::none;
    }
    
    inline std::size_t legacy_tree_string::entry_last_char_len(const piece_table_entry& entry)
    {
        return index_of_char_within_entry(entry, entry.display_length - 1).size();
    }
    
    inline std::size_t legacy_tree_string::entry_first_char_len(const piece_table_entry& entry)
    {
        return index_of_char_within_entry(entry, 0).size();
    }
}
