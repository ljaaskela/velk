#ifndef INTF_FUNCTION_H
#define INTF_FUNCTION_H

#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_interface.h>

namespace strata {

/** @brief Specifies whether an invocation should execute immediately or be deferred to update(). */
enum InvokeType : uint8_t { Immediate = 0, Deferred = 1 };

/**
 * @brief Non-owning view of function arguments.
 *
 * Passed by value (16 bytes). Constructed by callers from stack arrays of @c const @c IAny*.
 * Supports bounds-checked indexing and range-for iteration.
 */
struct FnArgs {
    const IAny* const* data = nullptr;  ///< Pointer to contiguous array of IAny pointers.
    size_t count = 0;                   ///< Number of arguments.

    /** @brief Returns the argument at index @p i, or nullptr if out of range. */
    const IAny* operator[](size_t i) const { return i < count ? data[i] : nullptr; }
    /** @brief Returns true if there are no arguments. */
    bool empty() const { return count == 0; }

    /** @brief Returns an iterator to the first argument. */
    const IAny *const *begin() const { return data; }
    /** @brief Returns an iterator past the last argument. */
    const IAny *const *end() const { return data + count; }

};

/** @brief Interface for an invocable function object. */
class IFunction : public Interface<IFunction>
{
public:
    /** @brief Function pointer type for invoke callbacks. */
    using CallableFn = ReturnValue(FnArgs);
    /**
     * @brief Called to invoke the function.
     * @param args Call args as a non-owning view.
     * @param type Immediate executes now; Deferred queues for the next update() call.
     * @return Function return value.
     */
    virtual ReturnValue invoke(FnArgs args, InvokeType type = Immediate) const = 0;
};

/**
 * @brief Internal interface for configuring an IFunction's invoke callback.
 *
 * Supports two dispatch mechanisms:
 * - @c set_invoke_callback() for explicit function pointer callbacks (highest priority).
 * - @c bind() for trampoline-based virtual dispatch, used by STRATA_INTERFACE
 *   to route @c invoke() calls to @c fn_Name() virtual methods on the interface.
 */
class IFunctionInternal : public Interface<IFunctionInternal>
{
public:
    /** @brief Function pointer type for bound trampoline callbacks. */
    using BoundFn = ReturnValue(void* context, FnArgs);
    /** @brief Function pointer type for deleting an owned context. */
    using ContextDeleter = void(void*);

    /** @brief Sets the callback that will be called when IFunction::invoke is called. */
    virtual void set_invoke_callback(IFunction::CallableFn *fn) = 0;

    /**
     * @brief Binds a context pointer and trampoline function for virtual dispatch.
     * @param context Pointer to the interface subobject (passed as first arg to fn).
     * @param fn Static trampoline that casts context and calls the virtual method.
     */
    virtual void bind(void* context, BoundFn* fn) = 0;

    /**
     * @brief Sets an owned callback with a heap-allocated context.
     *
     * Takes ownership of @p context. The @p deleter is called on @p context
     * when the function is destroyed or a new callback is set.
     *
     * @param context Heap-allocated callable (ownership transferred).
     * @param fn Static trampoline that casts context and invokes it.
     * @param deleter Static function that deletes context.
     */
    virtual void set_owned_callback(void* context, BoundFn* fn, ContextDeleter* deleter) = 0;
};

/**
 * @brief Invokes a function with null safety.
 * @param fn Function to invoke.
 * @param args Arguments for invocation.
 * @param type Immediate executes now; Deferred queues for the next update() call.
 */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::Ptr &fn, FnArgs args = {}, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::ConstPtr &fn, FnArgs args = {}, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @brief Invokes a function with a single IAny argument. */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::Ptr &fn, const IAny *arg, InvokeType type = Immediate)
{
    FnArgs args{&arg, 1};
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::ConstPtr &fn, const IAny *arg, InvokeType type = Immediate)
{
    FnArgs args{&arg, 1};
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

} // namespace strata

#endif // INTF_FUNCTION_H
