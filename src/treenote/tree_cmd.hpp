// tree_cmd.hpp

#pragma once

#include <cstdint>
#include <variant>

namespace treenote
{
    enum class cmd_names : std::int8_t
    {
        none = 0,
        move_node,
        insert_node,
        delete_node,
        cut_node,
        paste_node,
        insert_text,
        delete_text,
        line_break,
        line_join,
        
        error = -1
    };
    
    namespace cmd
    {
        struct move_node;
        struct edit_contents;
        struct insert_node;
        struct delete_node;
        struct multi_cmd;
    }
    
    namespace pt_cmd
    {
        /* Insertion */
        struct split_insert;
        struct grow_rhs;
        struct insert_entry;
        
        /* Deletion */
        struct split_delete;
        struct shrink_rhs;
        struct shrink_lhs;
        struct delete_entry;
        
        /* Other */
        struct line_break;
        struct line_join;
        
        struct multi_cmd;
        
        /* also:
         * - grow_rhs should be combinable with instances of itself or split_insert
         * - shrink_lhs and shrink_rhs should be combinable with instances of themselves of split_delete
         * - split_delete should be replaceable by shrink_lhs / shrink_rhs / delete_entry
         * - delete_entry can be moved into a multi_cmd to delete across several piece table entries      */
    }
    
    using command = std::variant<
            cmd::move_node,
            cmd::edit_contents,
            cmd::insert_node,
            cmd::delete_node,
            cmd::multi_cmd
    >;
    
    using table_command = std::variant<
            pt_cmd::split_insert,
            pt_cmd::split_delete,
            pt_cmd::grow_rhs,
            pt_cmd::shrink_rhs,
            pt_cmd::shrink_lhs,
            pt_cmd::insert_entry,
            pt_cmd::delete_entry,
            pt_cmd::line_break,
            pt_cmd::line_join,
            pt_cmd::multi_cmd
    >;
    
    enum class pt_cmd_type : std::int8_t
    {
        none = 0,
        insertion,
        deletion_b,
        deletion_c,
        linebreak,
        linejoin,
        
        error = -1
    };
}