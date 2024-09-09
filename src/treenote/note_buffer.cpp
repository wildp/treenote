// note_buffer.cpp

#include "note_buffer.h"

#include "utf8.h"

namespace treenote
{
    /* Constructors */
    
    note_buffer::note_buffer():
        victim_block_{ std::make_unique<block>() },
        append_iter_{ victim_block_->data_.begin() }
    {
        blocks_.emplace_back(std::move(victim_block_)); /* bit hacky */
    }
    
    
    /* Implementation of append_iter increment and decrement */
    
    void note_buffer::increment_append_iter()
    {
        append_iter_++;
        
        if (append_iter_ == blocks_.back()->data_.end())
        {
            /* create and insert new block (or reinsert victim block) */
            if (victim_block_)
                blocks_.emplace_back(std::move(victim_block_));
            else
                blocks_.emplace_back(std::make_unique<block>());
            
            append_iter_ = blocks_.back()->data_.begin();
        }
    }
    
    void note_buffer::decrement_append_iter()
    {
        if (append_iter_ == blocks_.back()->data_.begin())
        {
            /* ensure there is always one block present */
            if (blocks_.size() <= 1)
                return;
            
            /* remove end block and move to victim block */
            victim_block_ = std::move(blocks_.back());
            blocks_.pop_back();
            
            append_iter_ = blocks_.back()->data_.end();

        }
        
        --append_iter_;
    }
    
    [[nodiscard]] std::size_t note_buffer::index_of_append_iter() const
    {
        return std::distance(blocks_.back()->data_.begin(), append_iter_) + (blocks_.size() - 1) * buf_size;
    }
    
    
    /* Buffer reading function implementation */
    
    void note_buffer::sv_helper(std::vector<std::string_view>& result, sv_helper_info info) const
    {
        while (info.bytes_to_extract != 0)
        {
            const auto& data{ blocks_[info.block_index]->data_};
            
            const char* begin{ std::ranges::next(std::ranges::cbegin(data), static_cast<std::ptrdiff_t>(info.initial_offset)) };
            const char* end{ std::ranges::next(begin, static_cast<std::ptrdiff_t>(info.bytes_to_extract), std::ranges::cend(data)) };
            result.emplace_back(begin, end);
            
            info.initial_offset = 0;
            info.bytes_to_extract -= result.back().size();
            ++info.block_index;
        }
    }
    
    [[nodiscard]] std::size_t note_buffer::sv_char_count_to_byte_count(sv_helper_info info, std::size_t chars_to_count) const
    {
        const char* begin{ std::ranges::next(std::ranges::cbegin(blocks_[info.block_index]->data_), static_cast<std::ptrdiff_t>(info.initial_offset)) };
        const char* end{ std::ranges::cend(blocks_[info.block_index]->data_) };
        info.bytes_to_extract = 0;
        
        int char_length = 1;
        
        while (chars_to_count > 0)
        {
            if ((*begin & utf8::mask1) != utf8::test1)
            {
                if ((*begin & utf8::mask2) == utf8::test2)
                    char_length = 2;
                else if ((*begin & utf8::mask3) == utf8::test3)
                    char_length = 3;
                else if ((*begin & utf8::mask4) == utf8::test4)
                    char_length = 4;
            }
            
            if (char_length == 1)
                --chars_to_count;
            else
                --char_length;
            
            ++info.bytes_to_extract;
            begin = std::next(begin);
            
            if (begin == end)
            {
                ++info.block_index;
                begin = std::ranges::cbegin(blocks_[info.block_index]->data_);
                end = std::ranges::cend(blocks_[info.block_index]->data_);
            }
        }
        
        return info.bytes_to_extract;
    }
    
    [[nodiscard]] std::vector<std::string_view> note_buffer::to_str_view(const piece_table_line& line) const
    {
        std::vector<std::string_view> result;
        
        for (const auto& entry : line)
        {
            sv_helper(result, { .block_index = entry.start_index / buf_size,
                    .initial_offset = entry.start_index % buf_size,
                    .bytes_to_extract = entry.byte_length });
        }
        
        return result;
    }
    
    [[nodiscard]] std::vector<std::string_view> note_buffer::to_substr_view(const piece_table_line& line, std::size_t pos, std::size_t len) const
    {
        std::vector<std::string_view> result;
        
        std::size_t ignored_count{ 0 };
        std::size_t result_count{ 0 };
        
        for (const auto& entry: line)
        {
            if (ignored_count >= pos)
            {
                if (result_count > len)
                    break; /* don't extract string any further; we're done */
                
                if (result_count + entry.display_length <= len)
                {
                    /* extract entire string fragment */
                    sv_helper(result, { .block_index = entry.start_index / buf_size,
                                        .initial_offset = entry.start_index % buf_size,
                                        .bytes_to_extract = entry.byte_length });
                    
                    result_count += entry.display_length;
                }
                else
                {
                    /* extract string fragment until we have len characters in total */
                    
                    sv_helper_info info{ .block_index = entry.start_index / buf_size,
                                         .initial_offset = entry.start_index % buf_size,
                                         .bytes_to_extract = len - result_count };
                    
                    if (not entry_has_no_mb_char(entry))
                    {
                        /* string fragment contains multibyte characters: proceed carefully */
                        info.bytes_to_extract = sv_char_count_to_byte_count(info, len - result_count);
                    }
                    
                    sv_helper(result, info);
                    
                    result_count += len - result_count;
                }
            }
            else if (ignored_count + entry.display_length > pos)
            {
                /* discard string fragment until ignored_count > pos */
                
                const std::size_t chars_skipped{ pos - ignored_count };
                std::size_t bytes_skipped{ chars_skipped };
                
                if (not entry_has_no_mb_char(entry))
                {
                    /* string fragment contains multibyte characters: proceed carefully */
                    bytes_skipped = sv_char_count_to_byte_count({ .block_index = entry.start_index / buf_size,
                                                                  .initial_offset = entry.start_index % buf_size,
                                                                  .bytes_to_extract = 0 /* this value doesn't matter */
                                                                }, chars_skipped);
                }
                
                ignored_count = pos;
                const std::size_t chars_to_extract{ std::min(len - result_count, entry.display_length - chars_skipped) };
                
                sv_helper_info info{ .block_index = (entry.start_index + bytes_skipped) / buf_size,
                                     .initial_offset = (entry.start_index + bytes_skipped) % buf_size,
                                     .bytes_to_extract = chars_to_extract };
                
                if (not entry_has_no_mb_char(entry))
                {
                    /* string fragment contains multibyte characters: proceed carefully */
                    info.bytes_to_extract = sv_char_count_to_byte_count(info, chars_to_extract);
                }
                
                sv_helper(result, info);
                
                result_count += chars_to_extract;
            }
            else
            {
                ignored_count += entry.display_length;
            }
        }
        
        return result;
    }
}