#ifndef ARRAY_VIEW_H
#define ARRAY_VIEW_H

#include <cstddef>

/** @brief A simple constexpr span-like view over contiguous const data. */
template<class T>
struct array_view {
    const T* data_{};
    size_t size_{};

    constexpr size_t size() const { return size_; }
    constexpr bool empty() const { return size_ == 0; }
    constexpr const T* begin() const { return data_; }
    constexpr const T* end() const { return data_ + size_; }
    constexpr const T& operator[](size_t i) const { return data_[i]; }
};

#endif // ARRAY_VIEW_H
