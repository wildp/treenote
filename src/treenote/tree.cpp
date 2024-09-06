// tree.cpp

#include "tree.h"

#include <algorithm>
#include <iostream>
#include <stack>

#include "utf8.h"
#include "tree_op.h"

namespace treenote
{
    /* Implementation helpers */
    
    namespace detail
    {
        namespace
        {
            struct traverse_info
            {
                std::reference_wrapper<const tree> ref;
                std::size_t index;
            };
            
            struct traverse_stack : std::stack<traverse_info>
            {
                using std::stack<traverse_info>::stack;
                [[nodiscard]] const tree& top_tree() const { return top().ref; }
                [[nodiscard]] std::size_t& top_index() { return top().index; };
                [[nodiscard]] const std::size_t& top_index() const { return top().index; };
            };
            
            [[nodiscard]] inline std::size_t parse_helper(std::istream& is, bool& marker, bool& skip)
            {
                constexpr int tab_size{ 4 };
                std::string c{};
                unsigned int column{ 0 };
                
                for (bool loop{ true }; loop;)
                {
                    if (!utf8::get_ext(is, c))
                    {
                        /* error or reached eof */
                        loop = false;
                        skip = false;
                    }
                    else if (c == "│" || c == " ")
                    {
                        ++column;
                    }
                    else if (c == "├" || c == "└" || c == "─")
                    {
                        marker = true;
                        ++column;
                    }
                    else if (c == "\t")
                    {
                        column += tab_size - (column % tab_size);
                    }
                    else
                    {
                        /* readable character found */
                        utf8::unget(is);
                        loop = false;
                    }
                }
                
                return (column + tab_size / 2) / tab_size;
            }
            
            inline void write_helper(std::ostream& os, const tree& te, const std::vector<bool>& line_markers)
            {
                for (std::size_t line{ 0 }; line < te.line_count() || line == 0; ++line)
                {
                    for (std::size_t pos{ 0 }; pos < line_markers.size(); ++pos)
                    {
                        if (pos + 1 == line_markers.size() && line == 0)
                        {
                            if (line_markers[pos])
                                os << "├── ";
                            else
                                os << "└── ";
                        }
                        else
                        {
                            if (line_markers[pos])
                                os << "│   ";
                            else
                                os << "    ";
                        }
                    }
                    
                    if (te.line_count() == 0)
                        os << '\n';
                    else
                        os << te.get_content_const().to_str(line) << '\n';
                }
            }
            
            template<typename T>
            inline void vec_reorder(std::vector<T>& container, std::size_t src, std::size_t dst)
            {
                /* NOTE:
                 * If src or dst are greater than `std::numeric_limits<std::ptrdiff_t>::max()`
                 * (minimum of 2^16 - 1 as specified in standard), this function may behave
                 * incorrectly. On modern systems, this value is typically 2^63 - 1.
                 * This upper limit is never checked since it is extremely unlikely that
                 * `container.size()` will exceed `std::numeric_limits<std::ptrdiff_t>::max`
                 */
                
                if (src < container.size() && dst < container.size())
                {
                    auto tmp{ std::move(container[src]) };
                    
                    /* shift elements between src and dst in children_ to the right or left
                     * by 1 depending on the positions of src and dst                        */
                    if (src < dst)
                    {
                        std::move(std::ranges::begin(container) + static_cast<std::ptrdiff_t>(src) + 1,
                                  std::ranges::begin(container) + static_cast<std::ptrdiff_t>(dst) + 1,
                                  std::ranges::begin(container) + static_cast<std::ptrdiff_t>(src));
                        
                    }
                    else if (src > dst)
                    {
                        std::move_backward(std::ranges::begin(container) + static_cast<std::ptrdiff_t>(dst),
                                           std::ranges::begin(container) + static_cast<std::ptrdiff_t>(src),
                                           std::ranges::begin(container) + static_cast<std::ptrdiff_t>(src) + 1);
                    }
                    
                    container[dst] = std::move(tmp);
                }
                else
                {
                    // todo: improve error
                    throw std::out_of_range{ "treenote::detail::vec_reorder : index out of range" };
                }
            }
            
            template<typename T>
            inline void vec_insert(std::vector<T>& container, T&& item, std::size_t index)
            {
                /* NOTE:
                 * If index is greater than `std::numeric_limits<std::ptrdiff_t>::max()`
                 * (minimum of 2^16 - 1 as specified in standard), this function may behave
                 * incorrectly. On modern systems, this value is typically 2^63 - 1.
                 * This upper limit is never checked since it is extremely unlikely that
                 * `container.size()` will exceed `std::numeric_limits<std::ptrdiff_t>::max`
                 */
                
                if (index <= container.size())
                {
                    container.insert(std::ranges::begin(container) + static_cast<std::ptrdiff_t>(index), std::forward<T>(item));
                }
                else
                {
                    // todo: improve error
                    throw std::out_of_range{ "tree::insert_child(tree&&, std::size_t): Tree index out of range" };
                }
            }
            
            template<typename T>
            [[nodiscard]] inline T vec_detach(std::vector<T>& container, std::size_t index)
            {
                /* NOTE:
                  * If index is greater than `std::numeric_limits<std::ptrdiff_t>::max()`
                  * (minimum of 2^16 - 1 as specified in standard), this function may behave
                  * incorrectly. On modern systems, this value is typically 2^63 - 1.
                  * This upper limit is never checked since it is extremely unlikely that
                  * `container.size()` will exceed `std::numeric_limits<std::ptrdiff_t>::max`
                  */
                
                if (index < container.size())
                {
                    T ret{ std::move(container[index]) };
                    
                    std::move(std::ranges::begin(container) + static_cast<std::ptrdiff_t>(index) + 1,
                              std::ranges::end(container),
                              std::ranges::begin(container) + static_cast<std::ptrdiff_t>(index));
                    
                    container.pop_back();
                    
                    return ret;
                }
                else
                {
                    // todo: improve error
                    throw std::out_of_range{ "tree::detach_child(std::size_t): Tree index out of range" };
                }
            }
            
            template<typename... Ts>
            struct overload : Ts ... { using Ts::operator()...; };
            
            /* template deduction guide for overload struct; not actually needed in c++20 but clang complains otherwise */
            template<class... Ts> overload(Ts...) -> overload<Ts...>;
        }
    }
    
    /* Public facing functions */

    tree tree::make_empty()
    {
        tree root_node{};
        root_node.add_child(tree{});
        return root_node;
    }
    
    tree tree::make_copy(const tree& te)
    {
        tree copy{};
        copy.content_ = te.content_.make_copy();
        
        detail::traverse_stack                      stack{};
        std::stack<std::reference_wrapper<tree>>    result_stack{};
        
        result_stack.emplace(copy);
        
        for (const auto& c: te.children_)
        {
            stack.emplace(c, 0);
            
            while (!stack.empty())
            {
                /* copy tree entry content */
                tree tmp{};
                tmp.content_ = stack.top().ref.get().content_.make_copy();
                result_stack.top().get().add_child(std::move(tmp));
                result_stack.emplace(result_stack.top().get().children_.back());
                
                /* find next node */
                for (bool loop{ true }; loop;)
                {
                    if (stack.top_index() < stack.top_tree().child_count())
                    {
                        /* child tree entry found; traverse deeper */
                        stack.emplace(stack.top_tree().get_child_const(stack.top_index()), 0);
                        loop = false;
                    }
                    else
                    {
                        /* cannot go deeper; unwind stack and repeat
                         * until stack empty or next tree entry found */
                        stack.pop();
                        result_stack.pop();
                        
                        if (!stack.empty())
                        {
                            ++(stack.top_index());
                        }
                        else
                        {
                            loop = false;
                        }
                    }
                }
            }
        }
        
        return copy;
    }
    
    tree tree::parse(std::istream& is, std::string_view filename, note_buffer& buf, save_load_info& read_info)
    {
        std::noskipws(is); /* important! without this, only one line is produced */
        std::stack<std::reference_wrapper<tree>> tree_stack{};
        
        tree root_node{ buf.append(filename) };
        tree_stack.emplace(root_node);
        
        while (!is.eof())
        {
            bool marker{ false };
            bool skip_extract{ false };
            
            /* parse lines */
            const std::size_t indent_level{ detail::parse_helper(is, marker, skip_extract) };
            
            if (!skip_extract)
            {
                if (!marker && indent_level != 0)
                {
                    /* add line to existing tree entry instead of making new tree entry */
                    tree_stack.top().get().add_line(buf.append(std::views::istream<char>(is)));
                }
                else
                {
                    if (tree_stack.size() > indent_level + 1)
                    {
                        while (tree_stack.size() > indent_level + 1)
                            tree_stack.pop();
                    }
                    else if (tree_stack.size() < indent_level + 1)
                    {
                        /* insert additional tree nodes if necessary */
                        while (tree_stack.size() < indent_level + 1)
                        {
                            auto& tmp{ tree_stack.top().get() };
                            const std::size_t index{ tmp.add_child(tree{}) };
                            tree_stack.emplace(tmp.children_[index]);
                            ++read_info.node_count;
                        }
                    }
                    
                    /* add new entry to tree and push to top of stack */
                    auto& tmp{ tree_stack.top().get() };
                    const std::size_t index{ tmp.add_child(tree{ buf.append(std::views::istream<char>(is)) }) };
                    tree_stack.emplace(tmp.children_[index]);
                    ++read_info.node_count;
                }
                ++read_info.line_count;
            }
        }
        
        // todo: remove trailing new lines?
        
        return root_node;
    }
    
    void tree::write(std::ostream& os, const tree& tree_root, save_load_info& write_info)
    {
        detail::traverse_stack  stack{};
        std::vector<bool>       line_markers{};
    
        for (const auto& c: tree_root.children_)
        {
            stack.emplace(c, 0);
    
            while (!stack.empty())
            {
                /* write to stream */
                detail::write_helper(os, stack.top_tree(), line_markers);
                write_info.line_count += stack.top_tree().line_count();
                ++write_info.node_count;
                
                /* find next node */
                for (bool loop{ true }; loop;)
                {
                    if (stack.top_index() < stack.top_tree().child_count())
                    {
                        /* child tree entry found; traverse deeper */
                        
                        if (stack.top_index() + 1 == stack.top_tree().child_count())
                            line_markers.push_back(false);
                        else
                            line_markers.push_back(true);
                
                        stack.emplace(stack.top_tree().get_child_const(stack.top_index()), 0);
                        loop = false;
                    }
                    else
                    {
                        /* cannot go deeper; unwind stack and repeat
                         * until stack empty or next tree entry found */
                        stack.pop();
                
                        if (!line_markers.empty())
                            line_markers.pop_back();
                
                        if (!stack.empty())
                            ++(stack.top_index());
                        else
                            loop = false;
                    }
                }
            }
        }
    }
    
    void tree::invoke(tree& tree_root, command& cmd)
    {
        std::visit(detail::overload{
                [&](cmd::move_node& c) { move_node(tree_root, c.src, c.dst); },
                [&](cmd::edit_contents& c) { redo_edit_contents(tree_root, c.pos); },
                [&](cmd::insert_node& c) { insert_node(tree_root, c.pos, c.inserted); },
                [&](cmd::delete_node& c) { delete_node(tree_root, c.pos, c.deleted); },
                [&](cmd::multi_cmd& cs) { for (auto& c : cs.commands) tree::invoke(tree_root, c); },
        }, cmd);
    }
    
    void tree::invoke_reverse(tree& tree_root, command& cmd)
    {
        std::visit(detail::overload{
                [&](cmd::move_node& c) { unmove_node(tree_root, c.dst, c.src); },
                [&](cmd::edit_contents& c) { undo_edit_contents(tree_root, c.pos); },
                [&](cmd::insert_node& c) { delete_node(tree_root, c.pos, c.inserted); },
                [&](cmd::delete_node& c) { insert_node(tree_root, c.pos, c.deleted); },
                [&](cmd::multi_cmd& cs) { for (auto& c : cs.commands | std::views::reverse) tree::invoke_reverse(tree_root, c); },
        }, cmd);
    }
    
    tree::line_cache tree::build_index_cache(const tree& tree_root)
    {
        line_cache cache{};
    
        detail::traverse_stack      stack{};
        std::vector<std::size_t>    current_pos{ 0 };
    
        for (const auto& c: tree_root.children_)
        {
            stack.emplace(c, 0);
            
            while (!stack.empty())
            {
                /* add tree index to cache */
                for (std::size_t line{ 0 }; line < std::max(stack.top().ref.get().line_count(), 1uz); line++)
                    cache.emplace_back(current_pos, line, stack.top().ref.get());
                
                /* find next node */
                for (bool loop{ true }; loop;)
                {
                    if (stack.top_index() < stack.top_tree().child_count())
                    {
                        /* child tree entry found; traverse deeper */
                        current_pos.push_back(stack.top_index());
                        stack.emplace(stack.top_tree().get_child_const(stack.top_index()), 0);
                        loop = false;
                    }
                    else
                    {
                        /* cannot go deeper; unwind stack and repeat
                         * until stack empty or next tree entry found */
                        stack.pop();
                        
                        if (!stack.empty())
                        {
                            current_pos.pop_back();
                            ++(stack.top_index());
                        }
                        else
                        {
                            loop = false;
                        }
                    }
                }
            }
            ++(current_pos.back());
        }
        
        return cache;
    }
    
    
    /* Private member functions */
    
    std::size_t tree::add_child(tree&& str)
    {
        children_.push_back(std::move(str));
        return children_.size() - 1;
    }
    
    void tree::reorder_children(std::size_t src, std::size_t dst)
    {
        detail::vec_reorder(children_, src, dst);
    }
    
    void tree::insert_child(tree&& te, std::size_t index)
    {
        detail::vec_insert(children_, std::move(te), index);
    }
    
    tree tree::detach_child(std::size_t index)
    {
        return detail::vec_detach(children_, index);
    }
    
    
    /* Private static functions */
    
    void tree::move_node(tree& tree_root, const tree_index auto& src, const tree_index auto& dst)
    {
        auto lci{ longest_common_index_of(src, dst) };
        bool error{ false };
    
        if (std::ranges::size(lci) + 1 == std::ranges::size(src) && std::ranges::size(lci) + 1 == std::ranges::size(dst))
        {
            /* src and dst have the same parent; use reorder instead of detach + insert */
            auto common_parent{ get_node(tree_root, lci) };
        
            if (common_parent)
                common_parent->get().reorder_children(last_index_of(src), last_index_of(dst));
            else
                error = true;
        }
        else
        {
            auto src_parent{ get_node(tree_root, parent_index_of(src)) };
        
            if (src_parent)
            {
                tree tmp{ src_parent->get().detach_child(last_index_of(src)) };
                auto dst_parent{ get_node(tree_root, parent_index_of(dst)) };
            
                if (dst_parent)
                {
                    dst_parent->get().insert_child(std::move(tmp), last_index_of(dst));
                }
                else
                {
                    /* revert change and throw error */
                    src_parent->get().insert_child(std::move(tmp), last_index_of(src));
                    error = true;
                }
            }
        }
    
        if (error)
        {
            std::cerr << "Error in: " << __func__ << '\n';
            //throw std::out_of_range(std::to_string(std::stacktrace{}));
        }
    }
    
    void tree::insert_node(tree& tree_root, const tree_index auto& pos, std::optional<tree>& ins)
    {
        auto target{ get_node(tree_root, parent_index_of(pos)) };
        
        if (target)
        {
            if (ins)
                target->get().insert_child(std::move(*ins), last_index_of(pos));
            else
                target->get().insert_child(tree{}, last_index_of(pos));
        }
        else
        {
            std::cerr << "Error in: " << __func__ << '\n';
            //throw std::out_of_range(std::to_string(std::stacktrace{}));
        }
    }
    
    void tree::delete_node(tree& tree_root, const tree_index auto& pos, std::optional<tree>& del)
    {
        auto target{ get_node(tree_root, parent_index_of(pos)) };
        
        if (target)
        {
            del = target->get().detach_child(last_index_of(pos));
        }
        else
        {
            std::cerr << "Error in: " << __func__ << '\n';
            //throw std::out_of_range(std::to_string(std::stacktrace{}));
        }
    }
    
    void tree::redo_edit_contents(tree& tree_root, const tree_index auto& pos)
    {
        auto target{ get_node(tree_root, pos) };
        
        if (target)
        {
            target->get().content_.redo();
        }
        else
        {
            std::cerr << "Error in: " << __func__ << '\n';
            //throw std::out_of_range(std::to_string(std::stacktrace{}));
        }
    }
    
    void tree::undo_edit_contents(tree& tree_root, const tree_index auto& pos)
    {
        auto target{ get_node(tree_root, pos) };
        if (target)
        {
            target->get().content_.undo();
        }
        else
        {
            std::cerr << "Error in: " << __func__ << '\n';
            //throw std::out_of_range(std::to_string(std::stacktrace{}));
        }
    }

}