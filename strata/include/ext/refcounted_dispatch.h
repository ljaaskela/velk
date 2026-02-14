#ifndef EXT_REFCOUNTED_DISPATCH_H
#define EXT_REFCOUNTED_DISPATCH_H

#include <atomic>
#include <ext/interface_dispatch.h>

namespace strata::ext {

/**
 * @brief Adds intrusive reference counting to InterfaceDispatch.
 *
 * @tparam Interfaces The interfaces this object implements.
 */
template<class... Interfaces>
class RefCountedDispatch : public InterfaceDispatch<Interfaces...>
{
public:
    /** @brief Atomically increments the reference count. */
    void ref() override { data_.refCount++; }
    /** @brief Atomically decrements the reference count; deletes the object at zero. */
    void unref() override
    {
        if (--data_.refCount == 0) {
            delete this;
        }
    }

public:
    RefCountedDispatch() = default;
    ~RefCountedDispatch() override = default;

protected:
    struct ObjectData
    {
        alignas(4) std::atomic<int32_t> refCount{1};
        alignas(4) int32_t flags{};
    };
    constexpr ObjectData &get_object_data() noexcept { return data_; }

private:
    ObjectData data_;
};

} // namespace strata::ext

#endif // EXT_REFCOUNTED_DISPATCH_H
