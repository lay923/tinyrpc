#pragma once

#include <algorithm>
#include <memory>
#include "logging.h"

#undef LOGGING_COMPONENT
#define LOGGING_COMPONENT "StreamBuffer"

namespace TinyRPC
{
    class StreamBuffer
    {
        const static bool SHRINK_WITH_GET = false;
        const static size_t GROW_SIZE = 1024;
        const static size_t RESERVED_HEADER_SPACE = 64;
    public:
        /// <summary>
        /// Since we might further push some header information such as message ID
        /// into this buffer, we would like to reserve some space for the header info.
        /// Here we allocate 128 bytes and reserve the first 64 bytes as header space.
        /// </summary>
        StreamBuffer()
            : buf_((char*)malloc(RESERVED_HEADER_SPACE * 2)),
            const_buf_(false),
            pend_(RESERVED_HEADER_SPACE * 2),
            gpos_(RESERVED_HEADER_SPACE),
            ppos_(RESERVED_HEADER_SPACE)
        {
        }

        /// <summary>
        /// Create a new instance using an existing buffer. Since the buffer is already
        /// managed outside StreamBuffer, we don't want to free that space in destructor.
        /// </summary>
        /// <param name="buf">The buffer.</param>
        /// <param name="size">Buffer size.</param>
        StreamBuffer(const char * buf, size_t size)
            : buf_(const_cast<char*>(buf)),
            const_buf_(true),
            pend_(size),
            gpos_(0),
            ppos_(size)
        {
        }

        ~StreamBuffer()
        {
            if (!const_buf_)
            {
                free(buf_);
            }             
        }

        void swap(StreamBuffer & rhs)
        {
            std::swap(const_buf_, rhs.const_buf_);
            std::swap(buf_, rhs.buf_);
            std::swap(pend_, rhs.pend_);
            std::swap(gpos_, rhs.ppos_);
            std::swap(ppos_, rhs.ppos_);
        }

        char * get_buf()
        {
            return buf_;
        }

        void set_buf(const char * buf, size_t size)
        {
            const_buf_ = true;
            buf_ = const_cast<char*>(buf);
            ppos_ = size;
            gpos_ = 0;
            pend_ = size;
        }

        size_t get_size()
        {
            return ppos_ - gpos_;
        }

        void write(const char * buf, size_t size)
        {
            ASSERT(!const_buf_, "writing into a const buffer is not allowed.");
            size_t new_size = size + ppos_;
            if (new_size > pend_)
            {
                // reallocate buffer
                LOG("buffer is full, reallocating. pend_ = %d, new_size = %d", pend_, new_size);
                new_size = std::max(new_size, ppos_ + GROW_SIZE);
                char * new_buf = (char *)realloc(buf_, new_size);
                ASSERT(new_buf, "realloc failed");
                buf_ = new_buf;
                pend_ = new_size;
            }
            memcpy(buf_ + ppos_, buf, size);
            ppos_ += size;
        }

        void read(char * buf, size_t size)
        {
            ASSERT(gpos_ + size <= ppos_,
                "reading beyond the array: required size = %d, actual size = %d", size, ppos_ - gpos_);
            memcpy(buf, buf_ + gpos_, size);
            gpos_ += size;
            if (gpos_ > GROW_SIZE && SHRINK_WITH_GET && !const_buf_)
            {
                memmove(buf_, buf_ + gpos_, ppos_ - gpos_);
                char * new_buf = (char *)realloc(buf_, pend_ - gpos_);
                ASSERT(new_buf, "realloc failed");
                buf_ = new_buf;
                pend_ -= gpos_;
                ppos_ -= gpos_;
                gpos_ = 0;
            }
        }

        void write_head(const char * buf, size_t size)
        {
            ASSERT(!const_buf_, "writing into a const buffer is not allowed.");
            if (gpos_ < size)
            {
                // this should rarely happen, since we already have 64-byte reserved
                WARN("reallocating due to write_head, possible performance loss. gpos_ = %d, size = %d", gpos_, size);
                size_t new_size = std::max(size + ppos_, ppos_ + RESERVED_HEADER_SPACE);
                char * new_buf = (char *)malloc(new_size);
                ASSERT(new_buf, "realloc failed");
                // copy existing contents to the new buffer
                size_t new_gpos = new_size - (ppos_ - gpos_);
                memcpy(new_buf + new_gpos, buf_ + gpos_, ppos_ - gpos_);
                free(buf_);
                buf_ = new_buf;
                gpos_ = new_gpos;
                ppos_ = pend_ = new_size;
            }
            gpos_ -= size;
            memcpy(buf_ + gpos_, buf, size);
        }

    private:
        StreamBuffer(const StreamBuffer & rhs){};
        StreamBuffer & operator = (const StreamBuffer & rhs){ return *this; }

    private:
        char * buf_;
        bool const_buf_;// const buffers should not be written into
        size_t pend_;   // end of buffer0
        size_t gpos_;   // start of get
        size_t ppos_;   // start of put
    };
}