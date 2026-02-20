#ifndef VELK_API_CALLBACK_H
#define VELK_API_CALLBACK_H

#include <velk/api/any.h>
#include <velk/api/velk.h>
#include <velk/api/traits.h>
#include <velk/common.h>

#include <type_traits>
#include <utility>

namespace velk {

namespace detail {

/** @brief Typed parameter unpacking for auto-unpack Callback constructor */
template<class Callable, class ArgsTuple, size_t... Is>
decltype(auto) invoke_typed_impl(Callable& callable, FnArgs args, std::index_sequence<Is...>)
{
    return callable(Any<const decay_param_t<std::tuple_element_t<Is, ArgsTuple>>>(args[Is]).get_value()...);
}

template<class Callable, class Traits>
struct typed_trampoline {
    using ArgsTuple = typename Traits::args_tuple;
    static constexpr size_t Arity = Traits::arity;

    static IAny::Ptr invoke(void* c, FnArgs args)
    {
        if constexpr (Arity > 0) {
            if (args.count < Arity) return nullptr;
        }
        auto& fn = *static_cast<Callable*>(c);
        using R = typename Traits::return_type;
        if constexpr (std::is_void_v<R> || std::is_same_v<R, ReturnValue>) {
            invoke_typed_impl<Callable, ArgsTuple>(fn, args, std::make_index_sequence<Arity>{});
            return nullptr;
        } else if constexpr (std::is_same_v<R, IAny::Ptr>) {
            return invoke_typed_impl<Callable, ArgsTuple>(fn, args, std::make_index_sequence<Arity>{});
        } else {
            auto result = invoke_typed_impl<Callable, ArgsTuple>(fn, args, std::make_index_sequence<Arity>{});
            return Any<R>(result).clone();
        }
    }

    static void destroy(void* c)
    {
        delete static_cast<Callable*>(c);
    }
};

} // namespace detail

/** @brief Convenience wrapper that creates and owns an IFunction with a callback. */
class Callback
{
public:
    /** @brief Type alias for the native callback signature. */
    using CallbackFn = IFunction::CallableFn;

private:
    // SFINAE helpers for constructor overload resolution.
    //   is_callable_v      — not a copy/move of Callback, not convertible to raw CallbackFn*
    //   is_fnargs_callable — also invocable as (FnArgs) -> ReturnValue
    //   is_typed_callable  — not fnargs-invocable, but has a detectable operator() with typed params
    template<class F> using Decay = std::decay_t<F>;

    template<class F>
    static constexpr bool is_callable_v =
        !std::is_same_v<Decay<F>, Callback> &&
        !std::is_convertible_v<Decay<F>, CallbackFn*>;

    template<class F>
    static constexpr bool is_fnargs_callable_v =
        is_callable_v<F> && (std::is_invocable_r_v<ReturnValue, Decay<F>, FnArgs> ||
                             std::is_invocable_r_v<IAny::Ptr, Decay<F>, FnArgs>);

    template<class F>
    static constexpr bool is_typed_callable_v =
        is_callable_v<F> && !is_fnargs_callable_v<F> &&
        detail::has_callable_traits_v<Decay<F>>;

public:
    Callback() = delete;
    /** @brief Creates a Callback backed by the given callback. */
    Callback(CallbackFn *cb)
        : fn_(instance().create_callback(cb))
    {}

    /**
     * @brief Creates a Callback from a capturing callable (lambda, functor, etc.).
     *
     * The callable is heap-allocated and owned by the underlying IFunction.
     * Only raw function pointers cross the DLL boundary.
     */
    template<class F, detail::require<is_fnargs_callable_v<F>> = 0>
    Callback(F&& callable)
    {
        using Callable = std::decay_t<F>;
        auto* ctx = new Callable(std::forward<F>(callable));
        auto* trampoline = +[](void* c, FnArgs args) -> IAny::Ptr {
            if constexpr (std::is_invocable_r_v<IAny::Ptr, Callable, FnArgs>) {
                return (*static_cast<Callable*>(c))(args);
            } else {
                (*static_cast<Callable*>(c))(args);
                return nullptr;
            }
        };
        auto* deleter = +[](void* c) {
            delete static_cast<Callable*>(c);
        };
        fn_ = instance().create_owned_callback(ctx, trampoline, deleter);
    }

    /**
     * @brief Creates a Callback from a callable with typed parameters.
     *
     * Automatically unpacks FnArgs into typed values using Any<const T>.
     * Supports both void and ReturnValue return types.
     */
    template<class F, detail::require<is_typed_callable_v<F>> = 0>
    Callback(F&& callable)
    {
        using Callable = std::decay_t<F>;
        using Traits = detail::callable_traits<Callable>;
        using Trampoline = detail::typed_trampoline<Callable, Traits>;

        auto* ctx = new Callable(std::forward<F>(callable));
        fn_ = instance().create_owned_callback(ctx, &Trampoline::invoke, &Trampoline::destroy);
    }

    /** @brief Implicit conversion to IFunction::Ptr. */
    operator IFunction::Ptr() { return fn_; }
    operator const IFunction::ConstPtr() const { return fn_; }

    /** @brief Invokes the function with no arguments.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    IAny::Ptr invoke(InvokeType type = Immediate) const { return fn_->invoke({}, type); }
    /** @brief Invokes the function with the given @p args.
     *  @param args Arguments for invocation.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    IAny::Ptr invoke(FnArgs args, InvokeType type = Immediate) const { return fn_->invoke(args, type); }

private:
    IFunction::Ptr fn_;
};

} // namespace velk

#endif // VELK_API_CALLBACK_H
