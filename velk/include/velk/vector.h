#ifndef VELK_VECTOR_H
#define VELK_VECTOR_H

#include <velk/array_view.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>

namespace velk {

/**
 * @brief Non-templated base for vector, holding raw buffer state and
 *        type-independent operations. Reduces per-instantiation code size.
 */
class vector_base
{
protected:
    vector_base() = default;

    vector_base(void* data, size_t size, size_t capacity) noexcept
        : data_(data), size_(size), capacity_(capacity)
    {}

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

    /**
     * @brief Computes the next capacity that can hold @p required elements.
     *
     * Doubles from current (or min_capacity) until sufficient.
     */
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
        void*  td = data_;      data_ = other.data_;      other.data_ = td;
        size_t ts = size_;      size_ = other.size_;       other.size_ = ts;
        size_t tc = capacity_;  capacity_ = other.capacity_; other.capacity_ = tc;
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

    static constexpr size_t min_capacity = 8;

    void*  data_{};
    size_t size_{};
    size_t capacity_{};
};

/**
 * @brief ABI-stable owning resizable array.
 *
 * Replacement for std::vector in public interface headers.
 * Uses malloc/free for raw buffer management with placement new/explicit
 * destructor calls, making it safe across DLL boundaries.
 */
template <class T>
class vector : private vector_base
{
public:
    using value_type = T; ///< The element type.

    /** @brief Default-constructs an empty vector. */
    vector() = default;

    /** @brief Constructs a vector with @p count default-constructed elements. */
    explicit vector(size_t count)
    {
        if (count == 0) {
            return;
        }
        grow_to(count);
        for (size_t i = 0; i < count; ++i) {
            new (typed_data() + i) T();
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
        for (size_t i = 0; i < count; ++i) {
            new (typed_data() + i) T(value);
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
        grow_to(count);
        for (size_t i = 0; i < count; ++i) {
            new (typed_data() + i) T(first[i]);
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
        for (size_t i = 0; i < other.size_; ++i) {
            new (typed_data() + i) T(other.typed_data()[i]);
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
        T* new_buf = static_cast<T*>(alloc_raw(size_ * sizeof(T)));
        for (size_t i = 0; i < size_; ++i) {
            new (new_buf + i) T(std::move(typed_data()[i]));
        }
        destroy_all();
        free_raw();
        data_ = new_buf;
        capacity_ = size_;
    }

    /** @brief Destroys all elements. Capacity is unchanged. */
    void clear()
    {
        destroy_all();
        size_ = 0;
    }

    /** @brief Appends a copy of @p value. */
    void push_back(const T& value)
    {
        // Copy before potential realloc in case value references this vector's storage.
        T tmp(value);
        ensure_capacity(size_ + 1);
        new (typed_data() + size_) T(std::move(tmp));
        ++size_;
    }

    /** @brief Appends @p value by move. */
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
        ensure_capacity(size_ + 1);
        T* p = new (typed_data() + size_) T(std::forward<Args>(args)...);
        ++size_;
        return *p;
    }

    /** @brief Removes the last element. */
    void pop_back()
    {
        assert(size_ > 0);
        --size_;
        typed_data()[size_].~T();
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
        // Shift elements right by one.
        if (size_ > idx) {
            new (d + size_) T(std::move(d[size_ - 1]));
            for (size_t i = size_ - 1; i > idx; --i) {
                d[i] = std::move(d[i - 1]);
            }
            d[idx] = std::move(tmp);
        }
        else {
            new (d + idx) T(std::move(tmp));
        }
        ++size_;
        return typed_data() + idx;
    }

    /**
     * @brief Inserts elements from range [@p first, @p last) before @p pos.
     * @return Pointer to the first inserted element.
     */
    T* insert(const T* pos, const T* first, const T* last)
    {
        size_t idx = static_cast<size_t>(pos - typed_data());
        assert(idx <= size_);
        assert(last >= first);
        size_t count = static_cast<size_t>(last - first);
        if (count == 0) {
            return typed_data() + idx;
        }
        // Copy input range in case it overlaps with our buffer.
        vector tmp(first, last);
        ensure_capacity(size_ + count);
        T* d = typed_data();
        // Shift existing elements right by count.
        size_t old_size = size_;
        size_t tail = old_size - idx;
        if (tail > 0) {
            // Move-construct the tail end into uninitialized territory.
            for (size_t i = old_size + count - 1; i >= old_size && i != static_cast<size_t>(-1); --i) {
                new (d + i) T(std::move(d[i - count]));
            }
            // Move-assign the overlap region.
            if (tail > count) {
                for (size_t i = old_size - 1; i >= idx + count; --i) {
                    d[i] = std::move(d[i - count]);
                }
            }
            // Place new elements.
            T* src = tmp.typed_data();
            for (size_t i = 0; i < count; ++i) {
                if (idx + i < old_size) {
                    d[idx + i] = std::move(src[i]);
                }
                else {
                    new (d + idx + i) T(std::move(src[i]));
                }
            }
        }
        else {
            // Inserting at end.
            T* src = tmp.typed_data();
            for (size_t i = 0; i < count; ++i) {
                new (d + idx + i) T(std::move(src[i]));
            }
        }
        size_ = old_size + count;
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
        T* d = typed_data();
        for (size_t i = idx; i + 1 < size_; ++i) {
            d[i] = std::move(d[i + 1]);
        }
        --size_;
        d[size_].~T();
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
        // Shift remaining elements left.
        for (size_t i = idx; i + count < size_; ++i) {
            d[i] = std::move(d[i + count]);
        }
        // Destroy the vacated tail.
        for (size_t i = size_ - count; i < size_; ++i) {
            d[i].~T();
        }
        size_ -= count;
        return typed_data() + idx;
    }

    /** @brief Resizes the vector to @p count elements, default-constructing new ones. */
    void resize(size_t count)
    {
        T* d = typed_data();
        if (count < size_) {
            for (size_t i = count; i < size_; ++i) {
                d[i].~T();
            }
        }
        else if (count > size_) {
            ensure_capacity(count);
            d = typed_data();
            for (size_t i = size_; i < count; ++i) {
                new (d + i) T();
            }
        }
        size_ = count;
    }

    /** @brief Resizes the vector to @p count elements, filling new ones with @p value. */
    void resize(size_t count, const T& value)
    {
        T* d = typed_data();
        if (count < size_) {
            for (size_t i = count; i < size_; ++i) {
                d[i].~T();
            }
        }
        else if (count > size_) {
            // Copy value before potential realloc.
            T tmp(value);
            ensure_capacity(count);
            d = typed_data();
            for (size_t i = size_; i < count; ++i) {
                new (d + i) T(tmp);
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
        const T* a = typed_data();
        const T* b = other.typed_data();
        for (size_t i = 0; i < size_; ++i) {
            if (!(a[i] == b[i])) {
                return false;
            }
        }
        return true;
    }

    /** @brief Inequality comparison. */
    bool operator!=(const vector& other) const { return !(*this == other); }

private:
    /** @brief Casts the void* buffer to typed pointer. */
    T* typed_data() { return static_cast<T*>(data_); }
    const T* typed_data() const { return static_cast<const T*>(data_); }

    /** @brief Destroys all live elements without freeing the buffer. */
    void destroy_all()
    {
        T* d = typed_data();
        for (size_t i = 0; i < size_; ++i) {
            d[i].~T();
        }
    }

    /**
     * @brief Grows the buffer to hold at least @p required elements.
     *
     * Moves existing elements to the new buffer.
     */
    void grow_to(size_t required)
    {
        size_t new_cap = grow_capacity(capacity_, required);
        T* new_buf = static_cast<T*>(alloc_raw(new_cap * sizeof(T)));
        T* old = typed_data();
        for (size_t i = 0; i < size_; ++i) {
            new (new_buf + i) T(std::move(old[i]));
        }
        destroy_all();
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
};

} // namespace velk

#endif // VELK_VECTOR_H
