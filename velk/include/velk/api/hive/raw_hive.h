#ifndef VELK_API_RAW_HIVE_H
#define VELK_API_RAW_HIVE_H

#include <velk/interface/hive/intf_hive_store.h>

#include <type_traits>

namespace velk {

/**
 * @brief Typed wrapper around IRawHive that handles construction and destruction.
 *
 * Provides emplace() for placement-new construction and deallocate() for
 * destruction + slot reclamation. The underlying IRawHive only manages raw
 * memory; this wrapper adds type safety and RAII-style object lifetime.
 *
 * @tparam T The element type to store.
 */
template <class T>
class RawHive
{
public:
    /** @brief Wraps an existing IRawHive. */
    explicit RawHive(IRawHive::Ptr hive) : hive_(std::move(hive)) {}

    /** @brief Creates a RawHive for type T from a hive store. */
    explicit RawHive(IHiveStore& store) : hive_(store.get_raw_hive<T>()) {}

    /** @brief Destroys all live elements and releases the underlying hive. */
    ~RawHive() { clear(); }

    RawHive(const RawHive&) = delete;
    RawHive& operator=(const RawHive&) = delete;
    RawHive(RawHive&&) = default;
    RawHive& operator=(RawHive&&) = default;

    /** @brief Returns true if the underlying IRawHive is valid. */
    operator bool() const { return hive_.operator bool(); }

    /** @brief Constructs a T in the hive with the given arguments. Returns nullptr if invalid. */
    template <class... Args>
    T* emplace(Args&&... args)
    {
        if (!hive_) {
            return nullptr;
        }
        void* slot = hive_->allocate();
        return new (slot) T(static_cast<Args&&>(args)...);
    }

    /** @brief Destroys the object and reclaims its slot. */
    void deallocate(T* ptr)
    {
        if (ptr) {
            ptr->~T();
            if (hive_) {
                hive_->deallocate(ptr);
            }
        }
    }

    /** @brief Returns true if the pointer belongs to this hive. */
    bool contains(const T* ptr) const { return hive_ && hive_->contains(ptr); }

    /** @brief Returns the number of live elements. */
    size_t size() const { return hive_ ? hive_->size() : 0; }

    /** @brief Returns true if the hive is empty. */
    bool empty() const { return !hive_ || hive_->empty(); }

    /**
     * @brief Iterates all live elements with a typed callback.
     * @param fn Callable as void(T&) or bool(T&). Return false to stop early.
     */
    template <class Fn>
    void for_each(Fn&& fn) const
    {
        static_assert(std::is_invocable_v<std::decay_t<Fn>, T&>,
                      "RawHive::for_each visitor must be callable as void(T&) or bool(T&)");
        if (!hive_) {
            return;
        }
        hive_->for_each(&fn, [](void* ctx, void* elem) -> bool {
            auto& f = *static_cast<std::decay_t<Fn>*>(ctx);
            T& obj = *static_cast<T*>(elem);
            if constexpr (std::is_same_v<decltype(f(obj)), bool>) {
                return f(obj);
            } else {
                f(obj);
                return true;
            }
        });
    }

    /** @brief Destroys all live elements and resets the hive to empty. */
    void clear()
    {
        if (!hive_) {
            return;
        }
        if constexpr (std::is_trivially_destructible_v<T>) {
            hive_->clear(nullptr, nullptr);
        } else {
            hive_->clear(nullptr, [](void*, void* elem) { static_cast<T*>(elem)->~T(); });
        }
    }

    /** @brief Returns the underlying IRawHive. */
    IRawHive& raw() { return *hive_; }
    /** @brief Returns the underlying IRawHive (const). */
    const IRawHive& raw() const { return *hive_; }

private:
    IRawHive::Ptr hive_;
};

} // namespace velk

#endif // VELK_API_RAW_HIVE_H
