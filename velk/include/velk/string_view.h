#ifndef VELK_STRING_VIEW_H
#define VELK_STRING_VIEW_H

#include <cstddef>

namespace velk {

/**
 * @brief ABI-stable string view. POD layout: {const char*, size_t}.
 *
 * Replacement for std::string_view in public interface headers.
 * Fully constexpr-compatible.
 */
class string_view
{
public:
    /** @brief Default-constructs an empty view (null data, zero size). */
    constexpr string_view() = default;

    /** @brief Constructs from a pointer and explicit length. */
    constexpr string_view(const char* data, size_t size) : data_(data), size_(size) {}

    /** @brief Constructs from a string literal (deduces length at compile time). */
    template <size_t N>
    constexpr string_view(const char (&str)[N]) : data_(str),
                                                  size_(N - 1)
    {}

    /** @brief Returns a pointer to the underlying character data. */
    constexpr const char* data() const { return data_; }
    /** @brief Returns the number of characters in the view. */
    constexpr size_t size() const { return size_; }
    /** @brief Returns true if the view is empty. */
    constexpr bool empty() const { return size_ == 0; }

    /** @brief Returns the character at index @p i (unchecked). */
    constexpr char operator[](size_t i) const { return data_[i]; }

    /** @brief Returns an iterator to the first character. */
    constexpr const char* begin() const { return data_; }
    /** @brief Returns a past-the-end iterator. */
    constexpr const char* end() const { return data_ + size_; }

    /**
     * @brief Returns a substring starting at @p pos with at most @p count characters.
     * @param pos   Start position (clamped to size()).
     * @param count Maximum length (clamped to remaining characters).
     */
    constexpr string_view substr(size_t pos, size_t count = npos) const
    {
        if (pos > size_) {
            pos = size_;
        }
        if (count > size_ - pos) {
            count = size_ - pos;
        }
        return {data_ + pos, count};
    }

    /**
     * @brief Searches for the first occurrence of @p sv starting at @p pos.
     * @return Position of the match, or npos if not found.
     */
    constexpr size_t find(string_view sv, size_t pos = 0) const
    {
        if (sv.size_ == 0) {
            return pos <= size_ ? pos : npos;
        }
        if (sv.size_ > size_) {
            return npos;
        }
        for (size_t i = pos; i <= size_ - sv.size_; ++i) {
            bool match = true;
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[i + j] != sv.data_[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Searches for the last occurrence of @p sv at or before @p pos.
     * @return Position of the match, or npos if not found.
     */
    constexpr size_t rfind(string_view sv, size_t pos = npos) const
    {
        if (sv.size_ > size_) {
            return npos;
        }
        size_t last = size_ - sv.size_;
        if (pos < last) {
            last = pos;
        }
        for (size_t i = last + 1; i > 0; --i) {
            size_t idx = i - 1;
            bool match = true;
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[idx + j] != sv.data_[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return idx;
            }
        }
        return npos;
    }

    /** @brief Equality comparison (element-wise). */
    constexpr bool operator==(string_view o) const
    {
        if (size_ != o.size_) {
            return false;
        }
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] != o.data_[i]) {
                return false;
            }
        }
        return true;
    }

    /** @brief Inequality comparison. */
    constexpr bool operator!=(string_view o) const { return !(*this == o); }

    /** @brief Sentinel value returned by find/rfind on failure. */
    static constexpr size_t npos = static_cast<size_t>(-1);

    /** @brief Stream output support. */
    template <class OStream>
    friend OStream& operator<<(OStream& os, string_view sv)
    {
        os.write(sv.data_, static_cast<decltype(os.width())>(sv.size_));
        return os;
    }

private:
    const char* data_{}; ///< Pointer to character data (not necessarily null-terminated).
    size_t size_{};      ///< Number of characters in the view.
};

} // namespace velk

#endif // VELK_STRING_VIEW_H
