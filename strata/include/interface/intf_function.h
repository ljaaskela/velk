#ifndef INTF_FUNCTION_H
#define INTF_FUNCTION_H

#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_interface.h>

namespace strata {

/** @brief Specifies whether an invocation should execute immediately or be deferred to update(). */
enum InvokeType : uint8_t { Immediate = 0, Deferred = 1 };

/** @brief Interface for an invocable function object. */
class IFunction : public Interface<IFunction>
{
public:
    /** @brief Function pointer type for invoke callbacks. */
    using CallableFn = ReturnValue(const IAny *);
    /**
     * @brief Called to invoke the function.
     * @param args Call args. Actual content dependent on the function implementation.
     * @param type Immediate executes now; Deferred queues for the next update() call.
     * @return Function return value.
     */
    virtual ReturnValue invoke(const IAny *args, InvokeType type = Immediate) const = 0;
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
    using BoundFn = ReturnValue(void* context, const IAny*);

    /** @brief Sets the callback that will be called when IFunction::invoke is called. */
    virtual void set_invoke_callback(IFunction::CallableFn *fn) = 0;

    /**
     * @brief Binds a context pointer and trampoline function for virtual dispatch.
     * @param context Pointer to the interface subobject (passed as first arg to fn).
     * @param fn Static trampoline that casts context and calls the virtual method.
     */
    virtual void bind(void* context, BoundFn* fn) = 0;
};

/**
 * @brief Invokes a function with null safety.
 * @param fn Function to invoke.
 * @param args Arguments for invocation.
 * @param type Immediate executes now; Deferred queues for the next update() call.
 */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::Ptr &fn, const IAny *args = nullptr, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IFunction::ConstPtr &fn, const IAny *args = nullptr, InvokeType type = Immediate)
{
    return fn ? fn->invoke(args, type) : ReturnValue::INVALID_ARGUMENT;
}

} // namespace strata

#endif // INTF_FUNCTION_H
