// note_edit.hpp

#pragma once

#include "tree_index.hpp"
#include "tree_string.h"
#include "tree.h"

namespace treenote
{
    class note_edit_info
    {
    public:
        tree_string& get(tree& tree_root, const tree_index auto& ti);
        void reset() noexcept;
        
    private:
        std::optional<std::reference_wrapper<tree_string>>  current_tree_string_ref_{};
        std::vector<std::size_t>                            current_tree_string_node_idx_{};
    };
    
    inline void note_edit_info::reset() noexcept
    {
        current_tree_string_ref_.reset();
        current_tree_string_node_idx_.clear();
    }
    
    inline tree_string& note_edit_info::get(tree& tree_root, const tree_index auto& ti)
    {
        if (!current_tree_string_ref_.has_value())
        {
            current_tree_string_node_idx_.assign(std::ranges::cbegin(ti), std::ranges::cend(ti));
            current_tree_string_ref_ = tree::get_editable_tree_string(tree_root, ti);
        }
        else if (!std::ranges::equal(current_tree_string_node_idx_, ti))
        {
            current_tree_string_ref_->get().set_no_longer_current();
            current_tree_string_node_idx_.assign(std::ranges::cbegin(ti), std::ranges::cend(ti));
            current_tree_string_ref_ = tree::get_editable_tree_string(tree_root, ti);
        }
        
        return current_tree_string_ref_->get();  // throws error if supplied tree index is invalid
    }
}