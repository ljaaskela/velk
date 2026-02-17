#ifndef API_FUNCTION_H
#define API_FUNCTION_H

#include <api/any.h>
#include <common.h>
#include <interface/intf_function.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace strata {

/** @brief Lightweight wrapper around an existing IFunction pointer. */
class Function
{
public:
    /** @brief Wraps an existing IFunction pointer. */
    explicit Function(IFunction::Ptr existing) : fn_(std::move(existing)) {}

    /** @brief Implicit conversion to IFunction::Ptr. */
    operator IFunction::Ptr() { return fn_; }
    operator const IFunction::ConstPtr() const { return fn_; }

    /** @brief Returns true if the underlying IFunction is valid. */
    operator bool() const { return fn_.operator bool(); }

    /** @brief Returns the underlying IFunction pointer. */
    IFunction::Ptr get_function_interface() { return fn_; }
    /** @copydoc get_function_interface() */
    IFunction::ConstPtr get_function_interface() const { return fn_; }

    /** @brief Invokes the function with no arguments (null-safe).
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    IAny::Ptr invoke(InvokeType type = Immediate) const
    {
        return fn_ ? fn_->invoke({}, type) : nullptr;
    }

    /** @brief Invokes the function with the given @p args (null-safe).
     *  @param args Arguments for invocation.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    IAny::Ptr invoke(FnArgs args, InvokeType type = Immediate) const
    {
        return fn_ ? fn_->invoke(args, type) : nullptr;
    }

private:
    IFunction::Ptr fn_;
};

// Variadic invoke_function: IAny-convertible args

/**
 * @brief Invokes a function with multiple IAny-convertible arguments.
 * @param fn Function to invoke.
 * @param args Two or more IAny-convertible arguments.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
IAny::Ptr invoke_function(const IFunction::ConstPtr& fn, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Args)}) : nullptr;
}

// Variadic invoke_function: value args (auto-wrapped in Any<T>)

namespace detail {

template<class FnPtr, class Tuple, size_t... Is>
IAny::Ptr invoke_with_any_tuple(const FnPtr& fn, Tuple& tup, std::index_sequence<Is...>)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(std::get<Is>(tup))...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Is)}) : nullptr;
}

} // namespace detail

/**
 * @brief Invokes a function with multiple value arguments.
 *
 * Each argument is wrapped in Any<T> and passed as FnArgs.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
IAny::Ptr invoke_function(const IFunction::ConstPtr& fn, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    return detail::invoke_with_any_tuple(fn, tup, std::index_sequence_for<Args...>{});
}

} // namespace strata

#endif // API_FUNCTION_H
