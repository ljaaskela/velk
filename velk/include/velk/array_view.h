#ifndef VELK_ARRAY_VIEW_H
#define VELK_ARRAY_VIEW_H

#include <cstddef>

namespace velk {

/** @brief A simple constexpr span-like view over contiguous const data. */
template<class T>
struct array_view {
    constexpr array_view() = default;
    /** @brief Constructs a view over @p size elements starting at @p data. */
    constexpr array_view(const T* data, size_t size) : data_(data), size_(size) {}

    /** @brief Returns the number of elements in the view. */
    constexpr size_t size() const { return size_; }
    /** @brief Returns true if the view contains no elements. */
    constexpr bool empty() const { return size_ == 0; }
    /** @brief Returns a pointer to the first element. */
    constexpr const T* begin() const { return data_; }
    /** @brief Returns a past-the-end pointer. */
    constexpr const T* end() const { return data_ + size_; }
    /** @brief Returns the element at index @p i (unchecked). */
    constexpr const T &operator[](size_t i) const { return data_[i]; }

private:
    const T *data_{};
    size_t size_{};
};

} // namespace velk

#endif // VELK_ARRAY_VIEW_H
