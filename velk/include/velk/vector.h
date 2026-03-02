#ifndef VELK_VECTOR_H
#define VELK_VECTOR_H

#include <velk/array_view.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace velk {

namespace detail {
class array_any_core_base;
}

/**
 * @brief Non-templated base for vector, holding raw buffer state and
 *        type-independent operations. Reduces per-instantiation code size.
 */
class vector_base
{
    friend class detail::array_any_core_base;

protected:
    vector_base() = default;

    /** @brief Allocates a raw buffer of @p bytes. Aborts on failure. */
    static void* alloc_raw(size_t bytes)
    {
        void* p = std::malloc(bytes);
        assert(p && "velk::vector allocation failed");
        return p;
    }

    /** @brief Frees the raw buffer. Does not destroy elements. */
    void free_raw() noexcept
    {
        std::free(data_);
        data_ = nullptr;
        capacity_ = 0;
    }

    /** @brief Computes the next capacity that can hold @p required elements. */
    static size_t grow_capacity(size_t current, size_t required) noexcept
    {
        size_t cap = current ? current : min_capacity;
        while (cap < required) {
            cap *= 2;
        }
        return cap;
    }

    /** @brief Swaps raw buffer state with @p other. */
    void swap_raw(vector_base& other) noexcept
    {
        void* td = data_;
        data_ = other.data_;
        other.data_ = td;
        size_t ts = size_;
        size_ = other.size_;
        other.size_ = ts;
        size_t tc = capacity_;
        capacity_ = other.capacity_;
        other.capacity_ = tc;
    }

    /** @brief Steals the buffer from @p other, zeroing it. */
    void steal_from(vector_base& other) noexcept
    {
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    /** @brief Grows the buffer via memcpy. */
    void grow_raw(size_t required, size_t elem_size)
    {
        size_t new_cap = grow_capacity(capacity_, required);
        void* new_buf = alloc_raw(new_cap * elem_size);
        if (size_ > 0) {
            std::memcpy(new_buf, data_, size_ * elem_size);
        }
        std::free(data_);
        data_ = new_buf;
        capacity_ = new_cap;
    }

    static constexpr size_t min_capacity = 8;

    void* data_{};
    size_t size_{};
    size_t capacity_{};
};

/**
 * @brief ABI-stable owning resizable array.
 *
 * Replacement for std::vector in public interface headers.
 * Uses malloc/free for raw buffer management, making it safe across
 * DLL boundaries.
 *
 * For trivially copyable types, all operations use memcpy/memmove/memset/memcmp.
 * For non-trivial types, uses placement new, explicit destructors, and move semantics.
 *
 * @tparam T The element type.
 */
template <class T>
class vector : private vector_base
{
    static constexpr bool trivial = std::is_trivially_copyable_v<T>;

public:
    using value_type = T; ///< The element type.

    /** @brief Default-constructs an empty vector. */
    vector() = default;

    /** @brief Constructs a vector with @p count value-initialized elements. */
    explicit vector(size_t count)
    {
        if (count == 0) {
            return;
        }
        if constexpr (trivial) {
            data_ = alloc_raw(count * sizeof(T));
            capacity_ = count;
            std::memset(data_, 0, count * sizeof(T));
        } else {
            grow_to(count);
            for (size_t i = 0; i < count; ++i) {
                new (typed_data() + i) T();
            }
        }
        size_ = count;
    }

    /** @brief Constructs a vector with @p count copies of @p value. */
    vector(size_t count, const T& value)
    {
        if (count == 0) {
            return;
        }
        grow_to(count);
        T* d = typed_data();
        for (size_t i = 0; i < count; ++i) {
            if constexpr (trivial) {
                d[i] = value;
            } else {
                new (d + i) T(value);
            }
        }
        size_ = count;
    }

    /** @brief Constructs a vector from a pointer range [@p first, @p last). */
    vector(const T* first, const T* last)
    {
        assert(last >= first);
        size_t count = static_cast<size_t>(last - first);
        if (count == 0) {
            return;
        }
        data_ = alloc_raw(count * sizeof(T));
        capacity_ = count;
        if constexpr (trivial) {
            std::memcpy(data_, first, count * sizeof(T));
        } else {
            for (size_t i = 0; i < count; ++i) {
                new (typed_data() + i) T(first[i]);
            }
        }
        size_ = count;
    }

    /** @brief Constructs a vector from an initializer list. */
    vector(std::initializer_list<T> init) : vector(init.begin(), init.end()) {}

    /** @brief Constructs a vector by copying elements from an array_view. */
    vector(array_view<T> view) : vector(view.begin(), view.end()) {}

    /** @brief Copy constructor. Allocates a tight buffer (capacity == size). */
    vector(const vector& other)
    {
        if (other.size_ == 0) {
            return;
        }
        data_ = alloc_raw(other.size_ * sizeof(T));
        capacity_ = other.size_;
        if constexpr (trivial) {
            std::memcpy(data_, other.data_, other.size_ * sizeof(T));
        } else {
            for (size_t i = 0; i < other.size_; ++i) {
                new (typed_data() + i) T(other.typed_data()[i]);
            }
        }
        size_ = other.size_;
    }

    /** @brief Move constructor. Steals the buffer from @p other. */
    vector(vector&& other) noexcept { steal_from(other); }

    /** @brief Copy assignment. */
    vector& operator=(const vector& other)
    {
        if (this != &other) {
            vector tmp(other);
            swap(tmp);
        }
        return *this;
    }

    /** @brief Move assignment. */
    vector& operator=(vector&& other) noexcept
    {
        if (this != &other) {
            destroy_all();
            free_raw();
            steal_from(other);
        }
        return *this;
    }

    /** @brief Destructor. Destroys all elements and frees the buffer. */
    ~vector()
    {
        destroy_all();
        free_raw();
    }

    /** @brief Returns a reference to the element at index @p i (unchecked). */
    T& operator[](size_t i) { return typed_data()[i]; }
    /** @brief Returns a const reference to the element at index @p i (unchecked). */
    const T& operator[](size_t i) const { return typed_data()[i]; }

    /** @brief Returns a reference to the first element. */
    T& front() { return typed_data()[0]; }
    /** @brief Returns a const reference to the first element. */
    const T& front() const { return typed_data()[0]; }
    /** @brief Returns a reference to the last element. */
    T& back() { return typed_data()[size_ - 1]; }
    /** @brief Returns a const reference to the last element. */
    const T& back() const { return typed_data()[size_ - 1]; }

    /** @brief Returns a pointer to the underlying data. */
    T* data() { return typed_data(); }
    /** @brief Returns a const pointer to the underlying data. */
    const T* data() const { return typed_data(); }

    /** @brief Returns an iterator to the first element. */
    T* begin() { return typed_data(); }
    /** @brief Returns a const iterator to the first element. */
    const T* begin() const { return typed_data(); }
    /** @brief Returns a past-the-end iterator. */
    T* end() { return typed_data() + size_; }
    /** @brief Returns a const past-the-end iterator. */
    const T* end() const { return typed_data() + size_; }

    /** @brief Returns true if the vector contains no elements. */
    bool empty() const { return size_ == 0; }
    /** @brief Returns the number of elements. */
    size_t size() const { return size_; }
    /** @brief Returns the number of elements that can be held without reallocation. */
    size_t capacity() const { return capacity_; }

    /**
     * @brief Reserves storage for at least @p new_cap elements.
     *
     * Does nothing if current capacity is already sufficient.
     */
    void reserve(size_t new_cap)
    {
        if (new_cap > capacity_) {
            grow_to(new_cap);
        }
    }

    /** @brief Reduces capacity to match size. */
    void shrink_to_fit()
    {
        if (capacity_ == size_) {
            return;
        }
        if (size_ == 0) {
            free_raw();
            return;
        }
        void* new_buf = alloc_raw(size_ * sizeof(T));
        if constexpr (trivial) {
            std::memcpy(new_buf, data_, size_ * sizeof(T));
        } else {
            T* dst = static_cast<T*>(new_buf);
            for (size_t i = 0; i < size_; ++i) {
                new (dst + i) T(std::move(typed_data()[i]));
            }
            destroy_all();
        }
        std::free(data_);
        data_ = new_buf;
        capacity_ = size_;
    }

    /** @brief Destroys all elements. Capacity is unchanged. */
    void clear()
    {
        destroy_all();
        size_ = 0;
    }

    /** @brief Appends a copy of @p value. Safe when @p value references this vector. */
    void push_back(const T& value)
    {
        T tmp(value);
        ensure_capacity(size_ + 1);
        if constexpr (trivial) {
            typed_data()[size_] = tmp;
        } else {
            new (typed_data() + size_) T(std::move(tmp));
        }
        ++size_;
    }

    /** @brief Appends @p value by move (non-trivial types only). */
    template <bool NT = !trivial, std::enable_if_t<NT, int> = 0>
    void push_back(T&& value)
    {
        ensure_capacity(size_ + 1);
        new (typed_data() + size_) T(std::move(value));
        ++size_;
    }

    /** @brief Constructs an element in-place at the end. */
    template <class... Args>
    T& emplace_back(Args&&... args)
    {
        T tmp(std::forward<Args>(args)...);
        ensure_capacity(size_ + 1);
        T* p = typed_data() + size_;
        if constexpr (trivial) {
            *p = tmp;
        } else {
            new (p) T(std::move(tmp));
        }
        ++size_;
        return *p;
    }

    /** @brief Removes the last element. */
    void pop_back()
    {
        assert(size_ > 0);
        --size_;
        if constexpr (!trivial) {
            typed_data()[size_].~T();
        }
    }

    /**
     * @brief Inserts a copy of @p value before the element at @p pos.
     * @return Pointer to the inserted element.
     */
    T* insert(const T* pos, const T& value)
    {
        size_t idx = static_cast<size_t>(pos - typed_data());
        assert(idx <= size_);
        T tmp(value);
        ensure_capacity(size_ + 1);
        T* d = typed_data();
        if constexpr (trivial) {
            insert_trivial(idx, &tmp, 1);
        } else {
            if (size_ > idx) {
                new (d + size_) T(std::move(d[size_ - 1]));
                for (size_t i = size_ - 1; i > idx; --i) {
                    d[i] = std::move(d[i - 1]);
                }
                d[idx] = std::move(tmp);
            } else {
                new (d + idx) T(std::move(tmp));
            }
            ++size_;
        }
        return typed_data() + idx;
    }

    /**
     * @brief Inserts elements from range [@p first, @p last) before @p pos.
     * @return Pointer to the first inserted element.
     */
    T* insert(const T* pos, const T* first, const T* last)
    {
        size_t idx = static_cast<size_t>(pos - typed_data());
        assert(idx <= size_ && last >= first);
        size_t count = static_cast<size_t>(last - first);
        if (count == 0) {
            return typed_data() + idx;
        }
        if constexpr (trivial) {
            void* tmp = alloc_raw(count * sizeof(T));
            std::memcpy(tmp, first, count * sizeof(T));
            ensure_capacity(size_ + count);
            insert_trivial(idx, tmp, count);
            std::free(tmp);
        } else {
            vector tmp(first, last);
            ensure_capacity(size_ + count);
            T* d = typed_data();
            size_t old_size = size_;
            size_t tail = old_size - idx;
            if (tail > 0) {
                for (size_t i = old_size + count - 1; i >= old_size && i != static_cast<size_t>(-1); --i) {
                    new (d + i) T(std::move(d[i - count]));
                }
                if (tail > count) {
                    for (size_t i = old_size - 1; i >= idx + count; --i) {
                        d[i] = std::move(d[i - count]);
                    }
                }
                T* src = tmp.typed_data();
                for (size_t i = 0; i < count; ++i) {
                    if (idx + i < old_size) {
                        d[idx + i] = std::move(src[i]);
                    } else {
                        new (d + idx + i) T(std::move(src[i]));
                    }
                }
            } else {
                T* src = tmp.typed_data();
                for (size_t i = 0; i < count; ++i) {
                    new (d + idx + i) T(std::move(src[i]));
                }
            }
            size_ = old_size + count;
        }
        return typed_data() + idx;
    }

    /**
     * @brief Erases the element at @p pos.
     * @return Pointer to the element following the removed one.
     */
    T* erase(const T* pos)
    {
        size_t idx = static_cast<size_t>(pos - typed_data());
        assert(idx < size_);
        if constexpr (trivial) {
            erase_trivial(idx, 1);
        } else {
            T* d = typed_data();
            for (size_t i = idx; i + 1 < size_; ++i) {
                d[i] = std::move(d[i + 1]);
            }
            --size_;
            d[size_].~T();
        }
        return typed_data() + idx;
    }

    /**
     * @brief Erases elements in the range [@p first, @p last).
     * @return Pointer to the element following the last removed one.
     */
    T* erase(const T* first, const T* last)
    {
        T* d = typed_data();
        size_t idx = static_cast<size_t>(first - d);
        size_t end_idx = static_cast<size_t>(last - d);
        assert(idx <= end_idx && end_idx <= size_);
        size_t count = end_idx - idx;
        if (count == 0) {
            return d + idx;
        }
        if constexpr (trivial) {
            erase_trivial(idx, count);
        } else {
            for (size_t i = idx; i + count < size_; ++i) {
                d[i] = std::move(d[i + count]);
            }
            for (size_t i = size_ - count; i < size_; ++i) {
                d[i].~T();
            }
            size_ -= count;
        }
        return typed_data() + idx;
    }

    /** @brief Resizes the vector to @p count elements, value-initializing new ones. */
    void resize(size_t count)
    {
        if (count < size_) {
            if constexpr (!trivial) {
                T* d = typed_data();
                for (size_t i = count; i < size_; ++i) {
                    d[i].~T();
                }
            }
        } else if (count > size_) {
            ensure_capacity(count);
            if constexpr (trivial) {
                std::memset(static_cast<char*>(data_) + size_ * sizeof(T), 0, (count - size_) * sizeof(T));
            } else {
                T* d = typed_data();
                for (size_t i = size_; i < count; ++i) {
                    new (d + i) T();
                }
            }
        }
        size_ = count;
    }

    /** @brief Resizes the vector to @p count elements, filling new ones with @p value. */
    void resize(size_t count, const T& value)
    {
        if (count < size_) {
            if constexpr (!trivial) {
                T* d = typed_data();
                for (size_t i = count; i < size_; ++i) {
                    d[i].~T();
                }
            }
        } else if (count > size_) {
            if constexpr (trivial) {
                ensure_capacity(count);
                T* d = typed_data();
                for (size_t i = size_; i < count; ++i) {
                    d[i] = value;
                }
            } else {
                T tmp(value);
                ensure_capacity(count);
                T* d = typed_data();
                for (size_t i = size_; i < count; ++i) {
                    new (d + i) T(tmp);
                }
            }
        }
        size_ = count;
    }

    /** @brief Swaps the contents of this vector with @p other. */
    void swap(vector& other) noexcept { swap_raw(other); }

    /** @brief Implicit conversion to a read-only array_view. */
    operator array_view<T>() const { return {typed_data(), size_}; }

    /** @brief Equality comparison (element-wise). */
    bool operator==(const vector& other) const
    {
        if (size_ != other.size_) {
            return false;
        }
        if constexpr (trivial) {
            return size_ == 0 || std::memcmp(data_, other.data_, size_ * sizeof(T)) == 0;
        } else {
            const T* a = typed_data();
            const T* b = other.typed_data();
            for (size_t i = 0; i < size_; ++i) {
                if (!(a[i] == b[i])) {
                    return false;
                }
            }
            return true;
        }
    }

    /** @brief Inequality comparison. */
    bool operator!=(const vector& other) const { return !(*this == other); }

    /** @brief Returns a reference to the non-template base for type-erased operations. */
    vector_base& base() { return *this; }
    /** @brief Returns a const reference to the non-template base. */
    const vector_base& base() const { return *this; }

private:
    T* typed_data() { return static_cast<T*>(data_); }
    const T* typed_data() const { return static_cast<const T*>(data_); }

    /** @brief Destroys all live elements without freeing the buffer. */
    void destroy_all()
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            T* d = typed_data();
            for (size_t i = 0; i < size_; ++i) {
                d[i].~T();
            }
        }
    }

    /**
     * @brief Grows the buffer to hold at least @p required elements.
     *
     * For trivial types uses memcpy; for non-trivial types moves elements.
     */
    void grow_to(size_t required)
    {
        size_t new_cap = grow_capacity(capacity_, required);
        void* new_buf = alloc_raw(new_cap * sizeof(T));
        if constexpr (trivial) {
            if (size_ > 0) {
                std::memcpy(new_buf, data_, size_ * sizeof(T));
            }
        } else {
            T* dst = static_cast<T*>(new_buf);
            T* old = typed_data();
            for (size_t i = 0; i < size_; ++i) {
                new (dst + i) T(std::move(old[i]));
            }
            destroy_all();
        }
        std::free(data_);
        data_ = new_buf;
        capacity_ = new_cap;
    }

    /** @brief Ensures capacity for at least @p required elements, growing if needed. */
    void ensure_capacity(size_t required)
    {
        if (required > capacity_) {
            grow_to(required);
        }
    }

    /** @brief Inserts @p count elements from @p src at index @p idx via memmove+memcpy. Trivial only. */
    void insert_trivial(size_t idx, const void* src, size_t count)
    {
        char* d = static_cast<char*>(data_);
        size_t tail = size_ - idx;
        if (tail > 0) {
            std::memmove(d + (idx + count) * sizeof(T), d + idx * sizeof(T), tail * sizeof(T));
        }
        std::memcpy(d + idx * sizeof(T), src, count * sizeof(T));
        size_ += count;
    }

    /** @brief Erases @p count elements at index @p idx via memmove. Trivial only. */
    void erase_trivial(size_t idx, size_t count)
    {
        char* d = static_cast<char*>(data_);
        size_t tail = size_ - idx - count;
        if (tail > 0) {
            std::memmove(d + idx * sizeof(T), d + (idx + count) * sizeof(T), tail * sizeof(T));
        }
        size_ -= count;
    }
};

} // namespace velk

#endif // VELK_VECTOR_H
