#ifndef API_FUNCTION_H
#define API_FUNCTION_H

#include <api/any.h>
#include <api/function_context.h>
#include <api/strata.h>
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_function.h>
#include <interface/intf_metadata.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace strata {

namespace detail {

// --- callable_traits: extract return type and parameter types from a callable's operator() ---

template<class F, class = void>
struct callable_traits; // SFINAE-friendly primary (undefined)

template<class R, class C, class... Args>
struct callable_traits<R(C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<class R, class C, class... Args>
struct callable_traits<R(C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<class R, class C, class... Args>
struct callable_traits<R(C::*)(Args...) const noexcept> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<class R, class C, class... Args>
struct callable_traits<R(C::*)(Args...) noexcept> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<class F>
struct callable_traits<F, std::void_t<decltype(&F::operator())>>
    : callable_traits<decltype(&F::operator())> {};

template<class F, class = void>
inline constexpr bool has_callable_traits_v = false;

template<class F>
inline constexpr bool has_callable_traits_v<F, std::void_t<typename callable_traits<F>::return_type>> = true;

// --- Typed parameter unpacking for auto-unpack Function constructor ---

template<class T>
using decay_param_t = std::remove_const_t<std::decay_t<T>>;

template<class Callable, class ArgsTuple, size_t... Is>
decltype(auto) invoke_typed_impl(Callable& callable, FnArgs args, std::index_sequence<Is...>)
{
    return callable(Any<const decay_param_t<std::tuple_element_t<Is, ArgsTuple>>>(args[Is]).get_value()...);
}

template<class Callable, class Traits>
struct typed_trampoline {
    using ArgsTuple = typename Traits::args_tuple;
    static constexpr size_t Arity = Traits::arity;

    static ReturnValue invoke(void* c, FnArgs args)
    {
        if constexpr (Arity > 0) {
            if (args.count < Arity) return ReturnValue::INVALID_ARGUMENT;
        }
        auto& fn = *static_cast<Callable*>(c);
        if constexpr (std::is_void_v<typename Traits::return_type>) {
            invoke_typed_impl<Callable, ArgsTuple>(fn, args, std::make_index_sequence<Arity>{});
            return ReturnValue::SUCCESS;
        } else {
            return invoke_typed_impl<Callable, ArgsTuple>(fn, args, std::make_index_sequence<Arity>{});
        }
    }

    static void destroy(void* c)
    {
        delete static_cast<Callable*>(c);
    }
};

} // namespace detail

/** @brief Convenience wrapper that creates and owns an IFunction with a callback. */
class Function : public ext::ObjectCore<Function>
{
public:
    /** @brief Type alias for the native callback signature. */
    using CallbackFn = IFunction::CallableFn;

private:
    // SFINAE helpers for constructor overload resolution.
    //   is_callable_v      — not a copy/move of Function, not convertible to raw CallbackFn*
    //   is_fnargs_callable — also invocable as (FnArgs) -> ReturnValue
    //   is_typed_callable  — not fnargs-invocable, but has a detectable operator() with typed params
    template<class F> using Decay = std::decay_t<F>;

    template<class F>
    static constexpr bool is_callable_v =
        !std::is_same_v<Decay<F>, Function> &&
        !std::is_convertible_v<Decay<F>, CallbackFn*>;

    template<class F>
    static constexpr bool is_fnargs_callable_v =
        is_callable_v<F> && std::is_invocable_r_v<ReturnValue, Decay<F>, FnArgs>;

    template<class F>
    static constexpr bool is_typed_callable_v =
        is_callable_v<F> && !std::is_invocable_r_v<ReturnValue, Decay<F>, FnArgs> &&
        detail::has_callable_traits_v<Decay<F>>;

public:
    Function() = delete;
    /** @brief Creates a Function backed by the given callback. */
    Function(CallbackFn *cb)
    {
        fn_ = instance().create<IFunction>(ClassId::Function);
        if (auto internal = interface_cast<IFunctionInternal>(fn_); internal && cb) {
            internal->set_invoke_callback(cb);
        }
    }

    /**
     * @brief Creates a Function from a capturing callable (lambda, functor, etc.).
     *
     * The callable is heap-allocated and owned by the underlying IFunction.
     * Only raw function pointers cross the DLL boundary.
     */
    template<class F, std::enable_if_t<is_fnargs_callable_v<F>, int> = 0>
    Function(F&& callable)
    {
        using Callable = std::decay_t<F>;
        fn_ = instance().create<IFunction>(ClassId::Function);
        if (auto internal = interface_cast<IFunctionInternal>(fn_)) {
            auto* ctx = new Callable(std::forward<F>(callable));
            auto* trampoline = +[](void* c, FnArgs args) -> ReturnValue {
                return (*static_cast<Callable*>(c))(args);
            };
            auto* deleter = +[](void* c) {
                delete static_cast<Callable*>(c);
            };
            internal->set_owned_callback(ctx, trampoline, deleter);
        }
    }

    /**
     * @brief Creates a Function from a callable with typed parameters.
     *
     * Automatically unpacks FnArgs into typed values using Any<const T>.
     * Supports both void and ReturnValue return types.
     */
    template<class F, std::enable_if_t<is_typed_callable_v<F>, int> = 0>
    Function(F&& callable)
    {
        using Callable = std::decay_t<F>;
        using Traits = detail::callable_traits<Callable>;
        using Trampoline = detail::typed_trampoline<Callable, Traits>;

        fn_ = instance().create<IFunction>(ClassId::Function);
        if (auto internal = interface_cast<IFunctionInternal>(fn_)) {
            auto* ctx = new Callable(std::forward<F>(callable));
            internal->set_owned_callback(ctx, &Trampoline::invoke, &Trampoline::destroy);
        }
    }

    /** @brief Implicit conversion to IFunction::Ptr. */
    operator IFunction::Ptr() { return fn_; }
    operator const IFunction::ConstPtr() const { return fn_; }

    /** @brief Invokes the function with no arguments.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    ReturnValue invoke(InvokeType type = Immediate) const { return fn_->invoke({}, type); }
    /** @brief Invokes the function with the given @p args.
     *  @param args Arguments for invocation.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    ReturnValue invoke(FnArgs args, InvokeType type = Immediate) const { return fn_->invoke(args, type); }

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
ReturnValue invoke_function(const IFunction::Ptr& fn, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Args)}) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::ConstPtr& fn, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Args)}) : ReturnValue::INVALID_ARGUMENT;
}

/**
 * @brief Invokes a named function with multiple IAny-convertible arguments.
 * @param o The object to query for the function.
 * @param name Name of the function to query.
 * @param args Two or more IAny-convertible arguments.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface* o, std::string_view name, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    auto* meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), FnArgs{ptrs, sizeof...(Args)}) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::Ptr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::ConstPtr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

// Variadic invoke_function: value args (auto-wrapped in Any<T>)

namespace detail {

template<class Tuple, size_t... Is>
ReturnValue invoke_with_any_tuple(const IFunction::Ptr& fn, Tuple& tup, std::index_sequence<Is...>)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(std::get<Is>(tup))...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Is)}) : ReturnValue::INVALID_ARGUMENT;
}

template<class Tuple, size_t... Is>
ReturnValue invoke_with_any_tuple(const IFunction::ConstPtr& fn, Tuple& tup, std::index_sequence<Is...>)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(std::get<Is>(tup))...};
    return fn ? fn->invoke(FnArgs{ptrs, sizeof...(Is)}) : ReturnValue::INVALID_ARGUMENT;
}

template<class Tuple, size_t... Is>
FnArgs make_fn_args(Tuple& tup, const IAny** ptrs, std::index_sequence<Is...>)
{
    ((ptrs[Is] = static_cast<const IAny*>(std::get<Is>(tup))), ...);
    return {ptrs, sizeof...(Is)};
}

} // namespace detail

/**
 * @brief Invokes a function with multiple value arguments.
 *
 * Each argument is wrapped in Any<T> and passed as FnArgs.
 */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::Ptr& fn, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    return detail::invoke_with_any_tuple(fn, tup, std::index_sequence_for<Args...>{});
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IFunction::ConstPtr& fn, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    return detail::invoke_with_any_tuple(fn, tup, std::index_sequence_for<Args...>{});
}

/** @brief Invokes a named function with multiple value arguments. */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface* o, std::string_view name, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    const IAny* ptrs[sizeof...(Args)];
    auto fnArgs = detail::make_fn_args(tup, ptrs, std::index_sequence_for<Args...>{});
    auto* meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), fnArgs) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::Ptr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

/** @copydoc invoke_function */
template<class... Args, std::enable_if_t<
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...), int> = 0>
ReturnValue invoke_function(const IInterface::ConstPtr& o, std::string_view name, const Args&... args)
{
    return invoke_function(o.get(), name, args...);
}

} // namespace strata

#endif // API_FUNCTION_H
