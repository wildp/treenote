// core/buffer.hpp
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

#include <compare>
#include <iterator>
#include <memory>
#include <vector>

#include "table.hpp"
#include "utf8.hpp"

namespace tred::core
{
    class buffer;
    using extended_piece_table_entry = std::pair<piece_table_entry, buffer*>;
    
    class buffer
    {
    public:
        struct proxy_index_iterator;
        using const_iterator = proxy_index_iterator;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        
        buffer();
        
        buffer(const buffer&) = delete;
        buffer(buffer&&) = delete;
        buffer& operator=(const buffer&) = delete;
        buffer& operator=(buffer&&) = delete;
        ~buffer() = default;
        
        extended_piece_table_entry append(std::ranges::input_range auto input_range);
        
        [[nodiscard]] char at(std::size_t pos) const;
        
        [[nodiscard]] std::vector<std::string_view> to_str_view(const piece_table_line& line) const;
        [[nodiscard]] std::vector<std::string_view> to_substr_view(const piece_table_line& line, std::size_t pos, std::size_t len) const;
        
        [[nodiscard]] const_iterator cbegin() const noexcept;
        [[nodiscard]] const_iterator cend() const;
        [[maybe_unused]] [[nodiscard]] const_reverse_iterator crbegin() const;
        [[maybe_unused]] [[nodiscard]] const_reverse_iterator crend() const noexcept;
        
        [[nodiscard]] const_iterator begin() const noexcept { return cbegin(); }
        [[nodiscard]] const_iterator end() const { return cend(); }
        [[maybe_unused]] [[nodiscard]] const_reverse_iterator rbegin() const { return crbegin(); }
        [[maybe_unused]] [[nodiscard]] const_reverse_iterator rend() const noexcept { return crend(); }
        
        struct proxy_index_iterator
        {
            /* hacky implementation to enable use of note_buffer with iterators */
            
            using iterator_category [[maybe_unused]]    = std::random_access_iterator_tag;
            using difference_type                       = std::ptrdiff_t;
            using value_type                            = char;
            
            constexpr proxy_index_iterator() noexcept = default;
            
            proxy_index_iterator(const buffer* data, const std::size_t index) noexcept :
                    data_{ data }, index_{ index }
            {
            }
            
            value_type operator*() const { return data_->at(index_); }
            [[nodiscard]] value_type operator[](const difference_type amt) const { return data_->at(index_ + amt); }
            
            proxy_index_iterator& operator++() noexcept { ++index_; return *this; }
            proxy_index_iterator& operator--() noexcept { --index_; return *this; }
            proxy_index_iterator operator++(int) noexcept { const auto old{ *this }; operator++(); return old; }
            proxy_index_iterator operator--(int) noexcept { const auto old{ *this }; operator--(); return old; }
            
            proxy_index_iterator& operator+=(const difference_type amt) { index_ += amt; return *this; }
            proxy_index_iterator& operator-=(const difference_type amt) { index_ -= amt; return *this; }
            
            [[nodiscard]] friend proxy_index_iterator operator+(proxy_index_iterator lhs, const difference_type amt) { lhs += amt; return lhs; }
            [[nodiscard]] friend proxy_index_iterator operator+(const difference_type amt, proxy_index_iterator rhs) { rhs += amt; return rhs; }
            [[nodiscard]] friend proxy_index_iterator operator-(proxy_index_iterator lhs, const difference_type amt) { lhs -= amt; return lhs; }
            
            [[nodiscard]] std::strong_ordering operator<=>(const proxy_index_iterator& other) const noexcept = default;
            
            [[nodiscard]] friend difference_type operator-(const proxy_index_iterator& lhs, const proxy_index_iterator& rhs)
            {
                /* to satisfy std::sized_sentinel_for<proxy_index_iterator, proxy_index_iterator> */
                return static_cast<difference_type>(lhs.index_) - static_cast<difference_type>(rhs.index_);
            }
        
        private:
            const buffer* data_{ nullptr };
            std::size_t index_{ 0 };
        };
        
        static_assert(std::random_access_iterator<proxy_index_iterator>); /* ensure that our proxy iterator is sufficiently valid */
        
    private:
        static constexpr std::size_t buf_size{ 1024 };
        
        class block
        {
        public:
            constexpr block() { data_.fill(0); }
            
            block(const block&) = delete;
            block(block&&) = default;
            block& operator=(const block&) = delete;
            block& operator=(block&&) = default;
            ~block() = default;
            
            friend class buffer;
            
        private:
            std::array<char, buf_size> data_{};
        };
        
        struct sv_helper_info
        {
            std::size_t block_index;
            std::size_t initial_offset;
            std::size_t bytes_to_extract;
        };
        
        void sv_helper(std::vector<std::string_view>& result, sv_helper_info info) const;
        [[nodiscard]] std::size_t sv_char_count_to_byte_count(sv_helper_info info, std::size_t chars_to_count) const;
        
        void increment_append_iter();
        void decrement_append_iter();
        [[nodiscard]] std::size_t index_of_append_iter() const;
        
        std::vector<std::unique_ptr<block>> blocks_;            /* note: pointers to blocks here are NEVER null                 */
        std::unique_ptr<block>              victim_block_;      /* stores the last block removed; this MAY be null              */
        
        decltype(block::data_.begin())      append_iter_;       /* this always points to somewhere in the last block            */
                                                                /* note: use increment_append_iter and decrement_append_iter
                                                                 *       to move the position                                   */
        
    };
    
    
    /* Generic buffer function implementation */
    
    inline extended_piece_table_entry buffer::append(std::ranges::input_range auto input_range)
    {
        piece_table_entry result{ .start_index = index_of_append_iter(),
                                  .display_length = 0,
                                  .byte_length = 0 };
        
        auto input_begin{ std::ranges::begin(input_range) };
        const auto input_end{ std::ranges::end(input_range) };
        
        while (input_begin != input_end)
        {
            *append_iter_ = *input_begin;
            
            if (*append_iter_ == '\n' or *append_iter_ == '\0')
                break; /* delimiter reached; stop extraction */
            
            ++input_begin;
            
            if ((*append_iter_ & utf8::mask1) != utf8::test1)
            {
                /* utf-8 character is multibyte: continue extracting */
                
                int char_length{ 1 };
                bool invalid{ false };
                
                if ((*append_iter_ & utf8::mask2) == utf8::test2)
                    char_length = 2;
                else if ((*append_iter_ & utf8::mask3) == utf8::test3)
                    char_length = 3;
                else if ((*append_iter_ & utf8::mask4) == utf8::test4)
                    char_length = 4;
                
                increment_append_iter();
                
                for (int i{ 1 }; i < char_length; ++i)
                {
                    if (input_begin == input_end)
                    {
                        char_length = i;
                        invalid = true;
                        break;
                    }
                    
                    *append_iter_ = *input_begin;
                    ++input_begin;
                    
                    if ((*append_iter_ & utf8::mask_cont) != utf8::test_cont)
                        invalid = true;
                    
                    increment_append_iter();
                }
                
                if (invalid)
                {
                    /* char invalid; set input to Unicode 'Replacement Character' */
                    static constexpr std::string_view replacement_char{ "\uFFFD" };
                    
                    for (int i{ 0 }; i < char_length; ++i)
                        decrement_append_iter();
                    
                    for (const auto& c: replacement_char)
                    {
                        *append_iter_ = c;
                        increment_append_iter();
                    }
                }
            }
            else
            {
                increment_append_iter();
            }
            
            ++result.display_length;
        }
        
        /* calculate at end */
        result.byte_length = index_of_append_iter() - result.start_index;
        return { result, this };
    }
    
    
    /* Inline function definitions */
    
    inline char buffer::at(const std::size_t pos) const
    {
        return blocks_.at(pos / buf_size)->data_.at(pos % buf_size);
    }
    
    inline buffer::const_iterator buffer::cbegin() const noexcept
    {
        return proxy_index_iterator{ this , 0 };
    }
    
    inline buffer::const_iterator buffer::cend() const
    {
        return proxy_index_iterator{ this, index_of_append_iter() };
    }
    
    [[maybe_unused]] inline buffer::const_reverse_iterator buffer::crbegin() const
    {
        return std::make_reverse_iterator(cend());
    }
    
    [[maybe_unused]] inline buffer::const_reverse_iterator buffer::crend() const noexcept
    {
        return std::make_reverse_iterator(cbegin());
    }
}
