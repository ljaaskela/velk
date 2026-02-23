#ifndef VELK_INTF_FUNCTION_H
#define VELK_INTF_FUNCTION_H

#include <velk/common.h>
#include <velk/interface/intf_any.h>
#include <velk/interface/intf_interface.h>

namespace velk {

/** @brief Specifies whether an invocation should execute immediately or be deferred to update(). */
enum InvokeType : uint8_t
{
    Immediate = 0,
    Deferred = 1
};

/**
 * @brief Non-owning view of function arguments.
 *
 * Passed by value (16 bytes). Constructed by callers from stack arrays of @c const @c IAny*.
 * Supports bounds-checked indexing and range-for iteration.
 */
struct FnArgs
{
    const IAny* const* data = nullptr; ///< Pointer to contiguous array of IAny pointers.
    size_t count = 0;                  ///< Number of arguments.

    /** @brief Returns the argument at index @p i, or nullptr if out of range. */
    const IAny* operator[](size_t i) const { return i < count ? data[i] : nullptr; }
    /** @brief Returns true if there are no arguments. */
    bool empty() const { return count == 0; }

    /** @brief Returns an iterator to the first argument. */
    const IAny* const* begin() const { return data; }
    /** @brief Returns an iterator past the last argument. */
    const IAny* const* end() const { return data + count; }
};

/** @brief Interface for an invocable function object. */
class IFunction : public Interface<IFunction>
{
public:
    /** @brief Function pointer type for invoke callbacks. */
    using CallableFn = IAny::Ptr(FnArgs);
    /** @brief Function pointer type for bound trampoline callbacks. */
    using BoundFn = IAny::Ptr(void* context, FnArgs);
    /** @brief Function pointer type for deleting an owned context. */
    using ContextDeleter = void(void*);
    /**
     * @brief Called to invoke the function.
     * @param args Call args as a non-owning view.
     * @param type Immediate executes now; Deferred queues for the next update() call.
     * @return Typed result (nullptr = void/no result).
     */
    virtual IAny::Ptr invoke(FnArgs args, InvokeType type = Immediate) const = 0;
};

/**
 * @brief Invokes a function with null safety.
 * @param fn Function to invoke.
 * @param args Arguments for invocation.
 * @param type Immediate executes now; Deferred queues for the next update() call.
 */
inline IAny::Ptr invoke_function(const IFunction::ConstPtr& fn, FnArgs args = {}, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : nullptr;
}

/** @brief Invokes a function with a single IAny argument. */
inline IAny::Ptr invoke_function(const IFunction::ConstPtr& fn, const IAny* arg, InvokeType type = Immediate)
{
    FnArgs args{&arg, 1};
    return fn ? fn->invoke(args, type) : nullptr;
}

} // namespace velk

#endif // VELK_INTF_FUNCTION_H
