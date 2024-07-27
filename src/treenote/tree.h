// tree.h

#pragma once

#include <concepts>
#include <functional>
#include <iosfwd>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tree_cmd.hpp"
#include "tree_index.hpp"
#include "tree_string.h"
#include "utf8.h"

namespace treenote
{
    class tree
    {
    public:
        using optional_const_ref = std::optional<std::reference_wrapper<const tree>>;
        
        struct cache_entry
        {
            std::vector<std::size_t>              index;
            std::size_t                           line_no;
            std::reference_wrapper<const tree>    ref;
        };
        
        using line_cache = std::vector<cache_entry>;
    
        tree();
        explicit tree(std::string_view sv);
        
        tree(const tree&) = delete;
        tree(tree&&) = default;
        tree& operator=(const tree&) = delete;
        tree& operator=(tree&&) = default;
        ~tree() = default;
    
        [[nodiscard]] const auto& get_content_const() const;
        [[nodiscard]] const auto& get_child_const(std::size_t i) const;
        [[nodiscard]] std::size_t line_count() const;
        [[nodiscard]] std::size_t child_count() const;
        
        static void invoke(tree& tree_root, command& cmd);
        static void invoke_reverse(tree& tree_root, command& cmd);
        
        [[nodiscard]] static tree make_empty();
        [[nodiscard]] static tree make_copy(const tree& tree_entry);
        [[nodiscard]] static tree parse(std::istream& is, std::string_view filename, std::size_t& node_count, std::size_t& line_count);
        static void write(std::ostream& os, const tree& tree_root, std::size_t& node_count, std::size_t& line_count);
        
        [[nodiscard]] static line_cache build_index_cache(const tree& tree_root);
        
        [[nodiscard]] static auto get_editable_tree_string(tree& tree_root, const tree_index auto& ti)
                -> std::optional<std::reference_wrapper<tree_string>>;
        
    private:
        explicit tree(std::pair<std::string, std::size_t>&& input);
    
        void add_line(std::pair<std::string, std::size_t>&& input);
        std::size_t add_child(tree&& te);
        
        void reorder_children(std::size_t src, std::size_t dst);
        void insert_child(tree&& te, std::size_t index);
        [[nodiscard]] tree detach_child(std::size_t index);
        
        static void move_node(tree& tree_root, const tree_index auto& src, const tree_index auto& dst);
        static void unmove_node(tree& tree_root, const tree_index auto& dst, const tree_index auto& src);
        static void insert_node(tree& tree_root, const tree_index auto& pos, std::optional<tree>& ins);
        static void delete_node(tree& tree_root, const tree_index auto& pos, std::optional<tree>& del);
        static void redo_edit_contents(tree& tree_root, const tree_index auto& pos);
        static void undo_edit_contents(tree& tree_root, const tree_index auto& pos);
    
        [[nodiscard]] static auto get_node(tree& tree_root, const tree_index auto& ti)
                -> std::optional<std::reference_wrapper<tree>>;
        
        tree_string       content_;
        std::vector<tree> children_;
    };
    
    
    /* Free functions relating to tree */
    
    enum class line_mode : char
    {
        blank,      /* normally corresponds to "    " */
        line,       /* normally corresponds to "│   " */
        entry,      /* normally corresponds to "├── " */
        last        /* normally corresponds to "└── " */
    };
    
    using indent_info = std::vector<line_mode>;

    [[nodiscard]] bool tree_index_exists(const tree& tree_root, const tree_index auto& ti);
    [[nodiscard]] tree::optional_const_ref get_const_by_index(const tree& tree_root, const tree_index auto& ti);
    [[nodiscard]] std::string make_line_string_default(const indent_info& ii);
    [[nodiscard]] indent_info get_indent_info_by_index(const tree& tree_root, const tree_index auto& ti, bool cont = false);
    [[nodiscard]] std::size_t get_tree_entry_depth(const tree_index auto& ti);
    
    
    /* Implementation of inline functions */

    inline tree::tree() :
            content_{ "" }, children_{}
    {
    }
    
    inline tree::tree(std::string_view sv) :
        content_{ sv }, children_{}
    {
    }
    
    inline tree::tree(std::pair<std::string, std::size_t>&& input) :
            content_{ std::move(input) }, children_{}
    {
    }
    
    [[nodiscard]] inline const auto& tree::get_content_const() const
    {
        return content_;
    }
    
    [[nodiscard]] inline const auto& tree::get_child_const(std::size_t i) const
    {
        return children_[i];
    }
    
    [[nodiscard]] inline std::size_t tree::line_count() const
    {
        return content_.line_count();
    }
    
    [[nodiscard]] inline std::size_t tree::child_count() const
    {
        return children_.size();
    }
    
    inline void tree::add_line(std::pair<std::string, std::size_t>&& input)
    {
        content_.add_line(std::move(input));
    }
    
    inline void tree::unmove_node(tree& tree_root, const tree_index auto& dst, const tree_index auto& src)
    {
        tree::move_node(tree_root, dst, src);
    }
    
    [[nodiscard]] inline auto tree::get_node(tree& tree_root, const tree_index auto& ti)
    -> std::optional<std::reference_wrapper<tree>>
    {
        tree* current{ &tree_root };
        for (const auto& index: ti)
        {
            if (index >= current->child_count())
                return std::nullopt;
            else
                current = &(current->children_[index]);
        }
        return { *current };
    }
    
    [[nodiscard]] inline auto tree::get_editable_tree_string(tree& tree_root, const tree_index auto& ti)
            -> std::optional<std::reference_wrapper<tree_string>>
    {
        auto tmp{ tree::get_node(tree_root, ti) };
        if (tmp.has_value())
            return { tmp->get().content_ };
        else
            return {};
    }
    
    
    
    
    /* Templated free function implementations */
    
    inline bool tree_index_exists(const tree& tree_root, const tree_index auto& ti)
    {
        return get_by_index(tree_root, ti).has_value();
    }
    
    inline tree::optional_const_ref get_const_by_index(const tree& tree_root, const tree_index auto& ti)
    {
        const tree* current{ &tree_root };
    
        for (const auto& index : ti)
        {
            if (index >= current->child_count())
                return std::nullopt;
            else
                current = &(current->get_child_const(index));
        }
        return { *current };
    }
    
    inline std::string make_line_string_default(const indent_info& ii)
    {
        std::string result{};
        result.reserve(std::ranges::size(ii) * 4);
        
        for (const auto& level : ii)
        {
            switch (level)
            {
                case line_mode::blank:
                    result += "    ";
                    break;
                case line_mode::line:
                    result += "│   ";
                    break;
                case line_mode::entry:
                    result += "├── ";
                    break;
                case line_mode::last:
                    result += "└── ";
                    break;
            }
        }

        return result;
    }
    
    inline indent_info get_indent_info_by_index(const tree& tree_root, const tree_index auto& ti, bool cont)
    {
        if (std::ranges::size(ti) < 2)
            return {};
    
        indent_info result{};
        const tree* current{ &tree_root.get_child_const(*std::ranges::cbegin(ti)) };
    
        result.reserve((std::ranges::size(ti) - 1));
    
        for (const auto& index: ti | std::views::take(std::ranges::size(ti) - 1) | std::views::drop(1))
        {
            if (index >= current->child_count())
                return {};
            else if (index < current->child_count() - 1)
                result.push_back(line_mode::line);
            else
                result.push_back(line_mode::blank);
        
            current = &(current->get_child_const(index));
        }
    
        const auto last_index{ *std::ranges::crbegin(ti) };
    
        if (last_index >= current->child_count())
        {
            return {};
        }
        else if (!cont)
        {
            if (last_index < current->child_count() - 1)
                result.push_back(line_mode::entry);
            else
                result.push_back(line_mode::last);
        }
        else
        {
            if (last_index < current->child_count() - 1)
                result.push_back(line_mode::line);
            else
                result.push_back(line_mode::blank);
        }
    
        return result;
    }
    
    inline std::size_t get_tree_entry_depth(const tree_index auto& ti)
    {
        return std::ranges::size(ti);
    }
}