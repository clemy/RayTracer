#pragma once

#include <algorithm>
#include <istream>
#include <iterator>
#include <numeric>
#include <stdexcept>

#include <zlib.h>

#include "types.h"


/* This is a helper for reading binary files in blocks.
   begin(limit) returns an iterator valid for 'limit' bytes.
   Other helper functions are for reading different data types.
   It will throw if the file ends prematurely */
class BinaryInputStream {
public:
    using value_type = u8;

    class iterator {
    public:
        friend class BinaryInputStream;

        // some iterator typedefs
        using iterator_category = std::input_iterator_tag;
        using value_type = BinaryInputStream::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type *;
        using reference = value_type &;

        // input iterator interface
        value_type operator*() const { return m_stream.get(); }
        iterator &operator++() {
            m_stream.advance();
            --m_limit;
            return *this;
        }
        iterator operator++(int) { iterator i{ *this }; ++ *this; return i; }
        // only compares end of block
        // does not stop at end of file (exception triggered intentionally)
        bool operator==(const iterator &rhs) const { return isAtEnd() == rhs.isAtEnd(); }
        bool operator!=(const iterator &rhs) const { return !operator==(rhs); }
        bool isAtEnd() const { return m_limit == 0; }

    private:
        iterator(BinaryInputStream &stream, size_t limit) :
            m_stream{ stream },
            m_limit{ limit }
        {}

        BinaryInputStream &m_stream;
        size_t m_limit;
    };

    // constructs end of file stream
    BinaryInputStream() {}
    // construct this based on iterators or streams
    explicit BinaryInputStream(std::istreambuf_iterator<char> it) : m_it{ it } {}
    explicit BinaryInputStream(std::istream &in) : m_it{ std::istreambuf_iterator<char>(in) } {}
    // disallow copy (gets the CRC wrong)
    BinaryInputStream(const BinaryInputStream &) = delete;
    BinaryInputStream &operator=(const BinaryInputStream &) = delete;
    // but allow move
    BinaryInputStream(BinaryInputStream &&) = default;
    BinaryInputStream &operator=(BinaryInputStream &&) = default;

    // gets an iterator good for 'limit' bytes; after that it equals the end iterator
    iterator begin(size_t limit = std::numeric_limits<size_t>::max()) {
        return iterator(*this, limit);
    }
    iterator end() {
        return iterator(*this, 0);
    }

    value_type get() const {
        checkEof();
        return static_cast<value_type>(*m_it);
    }
    void advance(size_t count = 1) {
        for (size_t i = 0; i < count; i++) {
            value_type value = get();
            m_crc = crc32(m_crc, reinterpret_cast<const unsigned char *>(&value), 1L);
            ++m_it;
        }
    }

    // gets and resets the CRC checksum
    u32 getAndResetCRC() {
        u32 crc = m_crc;
        m_crc = crc32(0L, Z_NULL, 0);
        return crc;
    }

    // checks if the next bytes contain the expected values (expected must be iterable)
    template <typename T>
    bool match(const T &expected) {
        return std::equal(std::cbegin(expected), std::cend(expected), begin());
    }

    // reads in an integer value in network byte order
    // specify type as template parameter, like
    //   u32 v = it.read<u32>();
    template <typename T>
    T read() {
        return std::accumulate(begin(sizeof(T)), end(), T{ 0 }, [] (const T &a, const value_type v) {
            return a << 8 | static_cast<value_type>(v);
        });
    }

    // reads in an container (array, vector, ..)
    // container must have preallocated size
    template <typename T>
    void read(T &container) {
        std::copy_n(begin(), container.size(), std::begin(container));
        advance(); // copy_n does not increment iterator after last byte
    }

    // reads n bytes in an output iterator (back_inserter, ..)
    template <typename OutputIt>
    void read(OutputIt d_first, size_t n) {
        std::copy_n(begin(), n, d_first);
        advance(); // copy_n does not increment iterator after last byte
    }

private:
    void checkEof() const {
        if (m_it == std::istreambuf_iterator<char>{}) {
            throw std::runtime_error("file ended prematurely");
        }
    }

    std::istreambuf_iterator<char> m_it;
    uLong m_crc = crc32(0L, Z_NULL, 0);
};
