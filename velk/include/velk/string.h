#ifndef VELK_STRING_H
#define VELK_STRING_H

#include <velk/string_view.h>

#include <cassert>
#include <cstdlib>
#include <cstring>

namespace velk {

/**
 * @brief ABI-stable owning string with small-string optimization.
 *
 * Replacement for std::string in public interface headers.
 * Uses malloc/free for heap buffer management, making it safe across
 * DLL boundaries. Always null-terminated.
 *
 * Strings of up to 22 characters are stored inline (no heap allocation).
 * The last byte of the 24-byte layout discriminates between modes:
 * in inline mode it holds the string size (0..22, high bit clear);
 * in heap mode it is the MSB of capacity_ (high bit set).
 */
class string
{
public:
    /** @brief Maximum characters storable inline without heap allocation. */
    static constexpr size_t sso_capacity = 22;

    /** @brief Default-constructs an empty string (inline, zero-length). */
    string() { std::memset(raw_bytes(), 0, sizeof(*this)); }

    /** @brief Constructs from a null-terminated C string. */
    string(const char* str)
    {
        std::memset(raw_bytes(), 0, sizeof(*this));
        if (str && str[0] != '\0') {
            assign_raw(str, std::strlen(str));
        }
    }

    /** @brief Constructs from a pointer and explicit length. */
    string(const char* data, size_t size)
    {
        std::memset(raw_bytes(), 0, sizeof(*this));
        if (size > 0) {
            assign_raw(data, size);
        }
    }

    /** @brief Constructs from a string_view. */
    string(string_view sv)
    {
        std::memset(raw_bytes(), 0, sizeof(*this));
        if (!sv.empty()) {
            assign_raw(sv.data(), sv.size());
        }
    }

    /** @brief Constructs a string with @p count copies of character @p ch. */
    string(size_t count, char ch)
    {
        std::memset(raw_bytes(), 0, sizeof(*this));
        if (count > 0) {
            ensure_capacity(count);
            std::memset(writable_data(), ch, count);
            set_size_and_null(count);
        }
    }

    /** @brief Copy constructor. */
    string(const string& other)
    {
        std::memset(raw_bytes(), 0, sizeof(*this));
        size_t s = other.size();
        if (s > 0) {
            assign_raw(other.data(), s);
        }
    }

    /** @brief Move constructor. Steals state from @p other. */
    string(string&& other) noexcept
    {
        std::memcpy(raw_bytes(), other.raw_bytes(), sizeof(*this));
        std::memset(other.raw_bytes(), 0, sizeof(other));
    }

    /** @brief Copy assignment. */
    string& operator=(const string& other)
    {
        if (this != &other) {
            string tmp(other);
            swap(tmp);
        }
        return *this;
    }

    /** @brief Move assignment. */
    string& operator=(string&& other) noexcept
    {
        if (this != &other) {
            if (is_heap()) {
                std::free(heap_.ptr_);
            }
            std::memcpy(raw_bytes(), other.raw_bytes(), sizeof(*this));
            std::memset(other.raw_bytes(), 0, sizeof(other));
        }
        return *this;
    }

    /** @brief Assignment from a null-terminated C string. */
    string& operator=(const char* str)
    {
        string tmp(str);
        swap(tmp);
        return *this;
    }

    /** @brief Assignment from a string_view. */
    string& operator=(string_view sv)
    {
        string tmp(sv);
        swap(tmp);
        return *this;
    }

    /** @brief Destructor. Frees heap buffer if allocated. */
    ~string()
    {
        if (is_heap()) {
            std::free(heap_.ptr_);
        }
    }

    /** @brief Returns the character at index @p i (unchecked). */
    char& operator[](size_t i) { return writable_data()[i]; }
    /** @brief Returns the character at index @p i (unchecked). */
    char operator[](size_t i) const { return data()[i]; }

    /** @brief Returns a reference to the first character. */
    char& front() { return writable_data()[0]; }
    /** @brief Returns the first character. */
    char front() const { return data()[0]; }

    /** @brief Returns a reference to the last character. */
    char& back() { return writable_data()[size() - 1]; }
    /** @brief Returns the last character. */
    char back() const { return data()[size() - 1]; }

    /** @brief Returns a pointer to the underlying character data. */
    char* data() { return writable_data(); }
    /** @brief Returns a const pointer to the underlying character data. */
    const char* data() const { return is_heap() ? heap_.ptr_ : raw_bytes(); }

    /** @brief Returns a null-terminated C string. */
    const char* c_str() const { return data(); }

    /** @brief Returns an iterator to the first character. */
    char* begin() { return writable_data(); }
    /** @brief Returns a const iterator to the first character. */
    const char* begin() const { return data(); }
    /** @brief Returns a past-the-end iterator. */
    char* end() { return writable_data() + size(); }
    /** @brief Returns a const past-the-end iterator. */
    const char* end() const { return data() + size(); }

    /** @brief Returns true if the string is empty. */
    bool empty() const { return size() == 0; }

    /** @brief Returns the number of characters. */
    size_t size() const { return is_heap() ? heap_.size_ : local_.size_; }

    /** @brief Returns the number of characters that can be held without reallocation. */
    size_t capacity() const { return is_heap() ? (heap_.capacity_ & ~heap_flag) : sso_capacity; }

    /**
     * @brief Reserves storage for at least @p new_cap characters.
     *
     * Does nothing if current capacity is already sufficient.
     */
    void reserve(size_t new_cap)
    {
        if (new_cap > capacity()) {
            grow_to(new_cap);
        }
    }

    /** @brief Reduces capacity to match size, possibly moving back to inline storage. */
    void shrink_to_fit()
    {
        if (!is_heap()) {
            return;
        }
        size_t s = heap_.size_;
        if (s <= sso_capacity) {
            // Transition back to inline.
            char* old_ptr = heap_.ptr_;
            std::memset(raw_bytes(), 0, sizeof(*this));
            if (s > 0) {
                std::memcpy(local_.buf_, old_ptr, s);
            }
            local_.buf_[s] = '\0';
            local_.size_ = static_cast<unsigned char>(s);
            std::free(old_ptr);
        } else {
            size_t real_cap = heap_.capacity_ & ~heap_flag;
            if (real_cap == s) {
                return;
            }
            char* new_buf = alloc_buffer(s + 1);
            std::memcpy(new_buf, heap_.ptr_, s + 1);
            std::free(heap_.ptr_);
            set_heap(new_buf, s, s);
        }
    }

    /** @brief Clears the string. Capacity is unchanged. */
    void clear()
    {
        if (is_heap()) {
            heap_.size_ = 0;
            heap_.ptr_[0] = '\0';
        } else {
            local_.size_ = 0;
            local_.buf_[0] = '\0';
        }
    }

    /** @brief Appends a single character. */
    void push_back(char ch)
    {
        size_t s = size();
        ensure_capacity(s + 1);
        writable_data()[s] = ch;
        set_size_and_null(s + 1);
    }

    /** @brief Removes the last character. */
    void pop_back()
    {
        assert(size() > 0);
        set_size_and_null(size() - 1);
    }

    /** @brief Appends a string_view. */
    string& append(string_view sv)
    {
        if (sv.empty()) {
            return *this;
        }
        size_t s = size();
        ensure_capacity(s + sv.size());
        char* d = writable_data();
        std::memcpy(d + s, sv.data(), sv.size());
        set_size_and_null(s + sv.size()); // NOLINT(clang-analyzer-unix.Malloc)
        return *this;
    }

    /** @brief Appends a null-terminated C string. */
    string& append(const char* str) { return append(string_view(str, std::strlen(str))); }

    /** @brief Appends @p count copies of character @p ch. */
    string& append(size_t count, char ch)
    {
        if (count == 0) {
            return *this;
        }
        size_t s = size();
        ensure_capacity(s + count);
        std::memset(writable_data() + s, ch, count);
        set_size_and_null(s + count);
        return *this;
    }

    /** @brief Appends a string_view. */
    string& operator+=(string_view sv) { return append(sv); }
    /** @brief Appends a single character. */
    string& operator+=(char ch)
    {
        push_back(ch);
        return *this;
    }

    /**
     * @brief Inserts @p sv before position @p pos.
     * @return Reference to this string.
     */
    string& insert(size_t pos, string_view sv)
    {
        size_t s = size();
        assert(pos <= s);
        if (sv.empty()) {
            return *this;
        }
        ensure_capacity(s + sv.size());
        char* d = writable_data();
        std::memmove(d + pos + sv.size(), d + pos, s - pos);
        std::memcpy(d + pos, sv.data(), sv.size());
        set_size_and_null(s + sv.size());
        return *this;
    }

    /**
     * @brief Erases @p count characters starting at @p pos.
     * @return Reference to this string.
     */
    string& erase(size_t pos, size_t count = npos)
    {
        size_t s = size();
        assert(pos <= s);
        if (count > s - pos) {
            count = s - pos;
        }
        char* d = writable_data();
        std::memmove(d + pos, d + pos + count, s - pos - count);
        set_size_and_null(s - count);
        return *this;
    }

    /** @brief Resizes the string to @p count characters, filling new ones with @p ch. */
    void resize(size_t count, char ch = '\0')
    {
        size_t s = size();
        if (count > s) {
            ensure_capacity(count);
            std::memset(writable_data() + s, ch, count - s);
        }
        set_size_and_null(count);
    }

    /** @brief Swaps the contents of this string with @p other. */
    void swap(string& other) noexcept
    {
        char tmp[sizeof(string)];
        std::memcpy(tmp, raw_bytes(), sizeof(string));
        std::memcpy(raw_bytes(), other.raw_bytes(), sizeof(string));
        std::memcpy(other.raw_bytes(), tmp, sizeof(string));
    }

    /**
     * @brief Returns a substring starting at @p pos with at most @p count characters.
     * @param pos   Start position (clamped to size()).
     * @param count Maximum length (clamped to remaining characters).
     */
    string substr(size_t pos, size_t count = npos) const
    {
        size_t s = size();
        if (pos > s) {
            pos = s;
        }
        if (count > s - pos) {
            count = s - pos;
        }
        return string(data() + pos, count);
    }

    /**
     * @brief Searches for the first occurrence of @p sv starting at @p pos.
     * @return Position of the match, or npos if not found.
     */
    size_t find(string_view sv, size_t pos = 0) const { return view().find(sv, pos); }

    /**
     * @brief Searches for the last occurrence of @p sv at or before @p pos.
     * @return Position of the match, or npos if not found.
     */
    size_t rfind(string_view sv, size_t pos = npos) const { return view().rfind(sv, pos); }

    /** @brief Implicit conversion to a read-only string_view. */
    operator string_view() const { return view(); }

    /** @brief Sentinel value returned by find/rfind on failure. */
    static constexpr size_t npos = static_cast<size_t>(-1);

    /** @brief Equality comparison with another string. */
    bool operator==(const string& other) const { return view() == other.view(); }
    /** @brief Inequality comparison with another string. */
    bool operator!=(const string& other) const { return view() != other.view(); }

    /** @brief Equality comparison with a string_view. */
    bool operator==(string_view sv) const { return view() == sv; }
    /** @brief Inequality comparison with a string_view. */
    bool operator!=(string_view sv) const { return view() != sv; }

    /** @brief Equality comparison with a C string. */
    bool operator==(const char* str) const { return view() == string_view(str, std::strlen(str)); }
    /** @brief Inequality comparison with a C string. */
    bool operator!=(const char* str) const { return !(*this == str); }

    /** @brief Stream output support. */
    template <class OStream>
    friend OStream& operator<<(OStream& os, const string& s)
    {
        os.write(s.c_str(), static_cast<decltype(os.width())>(s.size()));
        return os;
    }

    /** @brief Concatenation of two strings. */
    friend string operator+(const string& lhs, string_view rhs)
    {
        string result;
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs.view());
        result.append(rhs);
        return result;
    }

    /** @brief Concatenation of a string_view and a string. */
    friend string operator+(string_view lhs, const string& rhs)
    {
        string result;
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs);
        result.append(rhs.view());
        return result;
    }

private:
    static constexpr size_t heap_flag = size_t(1) << (sizeof(size_t) * 8 - 1);

    /** @brief Returns a char pointer to the raw storage (for memcpy/memset). */
    char* raw_bytes() { return reinterpret_cast<char*>(this); }
    const char* raw_bytes() const { return reinterpret_cast<const char*>(this); }

    /** @brief Returns true if the string is using heap storage. */
    bool is_heap() const { return local_.size_ & 0x80; }

    /** @brief Returns a string_view over the current contents. */
    string_view view() const { return {data(), size()}; }

    /** @brief Returns a writable pointer to character data. */
    char* writable_data() { return is_heap() ? heap_.ptr_ : raw_bytes(); }

    /** @brief Transitions to heap mode with the given buffer, size, and capacity. */
    void set_heap(char* buf, size_t s, size_t cap)
    {
        heap_.ptr_ = buf;
        heap_.size_ = s;
        heap_.capacity_ = cap | heap_flag;
    }

    /**
     * @brief Sets the string size and writes the null terminator.
     *
     * Must be called after any operation that changes the logical length.
     * The caller must ensure the buffer is large enough for @p s + 1 bytes.
     */
    void set_size_and_null(size_t s)
    {
        writable_data()[s] = '\0';
        if (is_heap()) {
            heap_.size_ = s;
        } else {
            local_.size_ = static_cast<unsigned char>(s);
        }
    }

    /** @brief Initializes from raw data and length. Chooses inline or heap. */
    void assign_raw(const char* src, size_t len)
    {
        if (len <= sso_capacity) {
            std::memcpy(local_.buf_, src, len);
            local_.buf_[len] = '\0';
            local_.size_ = static_cast<unsigned char>(len);
        } else {
            char* buf = alloc_buffer(len + 1);
            std::memcpy(buf, src, len);
            buf[len] = '\0';
            set_heap(buf, len, len);
        }
    }

    /** @brief Allocates a raw buffer of @p bytes. Aborts on failure. */
    static char* alloc_buffer(size_t bytes)
    {
        void* p = std::malloc(bytes);
        assert(p && "velk::string allocation failed");
        return static_cast<char*>(p);
    }

    /**
     * @brief Grows the buffer to hold at least @p required characters (plus null).
     *
     * Transitions from inline to heap if necessary. Preserves existing content.
     */
    void grow_to(size_t required)
    {
        size_t cur_cap = capacity();
        size_t new_cap = cur_cap;
        while (new_cap < required) {
            new_cap *= 2;
        }
        char* new_buf = alloc_buffer(new_cap + 1);
        size_t s = size();
        if (s > 0) {
            std::memcpy(new_buf, data(), s);
        }
        new_buf[s] = '\0';
        if (is_heap()) {
            std::free(heap_.ptr_);
        }
        set_heap(new_buf, s, new_cap);
    }

    /** @brief Ensures capacity for at least @p required characters, growing if needed. */
    void ensure_capacity(size_t required)
    {
        if (required > capacity()) {
            grow_to(required);
        }
    }

    union
    {
        struct
        {
            char* ptr_;
            size_t size_;
            size_t capacity_; ///< High bit set indicates heap mode.
        } heap_;
        struct
        {
            char buf_[sizeof(heap_) - 1]; ///< 23 bytes: up to 22 chars + null terminator.
            unsigned char size_;          ///< Inline string size (0..22, high bit always clear).
        } local_;
    };
};

static_assert(sizeof(string) == 3 * sizeof(void*), "velk::string must be 24 bytes");

} // namespace velk

#endif // VELK_STRING_H
