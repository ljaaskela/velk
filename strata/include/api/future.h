#ifndef API_FUTURE_H
#define API_FUTURE_H

#include <api/any.h>
#include <api/callback.h>
#include <api/strata.h>
#include <interface/intf_future.h>
#include <interface/types.h>

namespace strata {

// Forward declarations: then() is defined out-of-class after Promise.
class Promise;
inline Promise make_promise();

namespace detail {

/** @brief Maps a callable return type to the corresponding Future value type. */
template<class R>
using future_return_t = std::conditional_t<
    std::is_void_v<R> || std::is_same_v<R, ReturnValue>, void, R>;

} // namespace detail

/**
 * @brief Typed read-side wrapper for IFuture.
 * @tparam T The expected result type. Use void for valueless futures.
 */
template<class T>
class Future
{
public:
    /** @brief Wraps an existing IFuture pointer. */
    explicit Future(IFuture::Ptr future) : future_(std::move(future)) {}

    /** @brief Returns true if the underlying IFuture is valid. */
    operator bool() const { return future_ != nullptr; }

    /** @brief Returns true if the result has been set. */
    bool is_ready() const { return future_ && future_->is_ready(); }

    /** @brief Blocks until the result is available. */
    void wait() const { if (future_) future_->wait(); }

    /** @brief Blocks until ready, then returns the typed result. */
    Any<const T> get_result() const
    {
        return Any<const T>(future_ ? future_->get_result() : nullptr);
    }

    /**
     * @brief Adds a continuation and returns a Future that resolves with its return value.
     *
     * Supports the same callable types as Callback (FnArgs lambdas, typed lambdas, etc.).
     * The returned Future resolves when the continuation completes.
     * Callables returning void or ReturnValue produce Future<void>.
     *
     * @param callable A callable compatible with Callback.
     * @param type Immediate fires synchronously; Deferred queues for update().
     * @return A Future that resolves with the continuation's return value.
     */
    template<class F>
    auto then(F&& callable, InvokeType type = Immediate);

    /** @brief Implicit conversion to IFuture::Ptr. */
    operator IFuture::Ptr() { return future_; }
    /** @copydoc operator IFuture::Ptr() */
    operator const IFuture::ConstPtr() const { return future_; }

private:
    IFuture::Ptr future_;
};

/** @brief Specialization for void futures (no result value). */
template<>
class Future<void>
{
public:
    /** @brief Wraps an existing IFuture pointer. */
    explicit Future(IFuture::Ptr future) : future_(std::move(future)) {}

    /** @brief Returns true if the underlying IFuture is valid. */
    operator bool() const { return future_ != nullptr; }
    /** @brief Returns true if the result has been set. */
    bool is_ready() const { return future_ && future_->is_ready(); }
    /** @brief Blocks until the future is resolved. */
    void wait() const { if (future_) future_->wait(); }

    /** @copydoc Future::then */
    template<class F>
    auto then(F&& callable, InvokeType type = Immediate);

    /** @brief Implicit conversion to IFuture::Ptr. */
    operator IFuture::Ptr() { return future_; }
    /** @copydoc operator IFuture::Ptr() */
    operator const IFuture::ConstPtr() const { return future_; }

private:
    IFuture::Ptr future_;
};

/**
 * @brief Write-side handle for resolving a future.
 *
 * Wraps both IFuture (to hand out Future<T>) and IFutureInternal (to set the result).
 */
class Promise
{
public:
    /** @brief Wraps an existing IFuture pointer (must also support IFutureInternal). */
    explicit Promise(IFuture::Ptr future) : future_(std::move(future)) {}

    /** @brief Returns true if the underlying pointer is valid. */
    operator bool() const { return future_ != nullptr; }

    /**
     * @brief Resolves the future with a typed value.
     * @tparam T The value type.
     * @param value The value to resolve with (cloned internally).
     * @return SUCCESS on first call, NOTHING_TO_DO if already resolved.
     */
    template<class T>
    ReturnValue set_value(const T& value)
    {
        auto* internal = interface_cast<IFutureInternal>(future_);
        return internal ? internal->set_result(Any<const T>(value)) : ReturnValue::FAIL;
    }

    /**
     * @brief Resolves a void future (no value).
     * @return SUCCESS on first call, NOTHING_TO_DO if already resolved.
     */
    ReturnValue complete()
    {
        auto* internal = interface_cast<IFutureInternal>(future_);
        return internal ? internal->set_result(nullptr) : ReturnValue::FAIL;
    }

    /**
     * @brief Returns a typed Future for the consumer side.
     * @tparam T The expected result type. Use void for valueless futures.
     */
    template<class T>
    Future<T> get_future() const
    {
        return Future<T>(future_);
    }

private:
    IFuture::Ptr future_;
};

/**
 * @brief Creates a new Promise backed by a FutureImpl.
 * @return A Promise that can be resolved and whose Future can be handed to consumers.
 */
inline Promise make_promise()
{
    return Promise(instance().create_future());
}

// then() implementation (needs Promise and make_promise)

namespace detail {

/**
 * @brief Shared implementation for Future<T>::then and Future<void>::then.
 *
 * Creates a new promise, wraps the callable so that resolving happens
 * automatically when the continuation completes, and returns the chained future.
 */
template<class F>
auto future_then(const IFuture::Ptr& future, F&& callable, InvokeType type)
{
    using Callable = std::decay_t<F>;

    if constexpr (std::is_invocable_r_v<ReturnValue, Callable, FnArgs> ||
                   std::is_invocable_r_v<IAny::Ptr, Callable, FnArgs>) {
        // FnArgs callable: chain as Future<void>
        auto promise = make_promise();
        auto result = promise.get_future<void>();
        if (future) {
            Callback cb([p = std::move(promise), f = Callable(std::forward<F>(callable))]
                        (FnArgs args) mutable -> IAny::Ptr {
                f(args);
                p.complete();
                return nullptr;
            });
            future->add_continuation(cb, type);
        }
        return result;
    } else {
        static_assert(has_callable_traits_v<Callable>, "callable must have a detectable operator()");
        using Traits = callable_traits<Callable>;
        using R = typename Traits::return_type;
        using FR = future_return_t<R>;

        auto promise = make_promise();
        auto result = promise.get_future<FR>();
        if (future) {
            Callback cb([p = std::move(promise), f = Callable(std::forward<F>(callable))]
                        (FnArgs args) mutable -> IAny::Ptr {
                if constexpr (std::is_void_v<R> || std::is_same_v<R, ReturnValue>) {
                    invoke_typed_impl<Callable, typename Traits::args_tuple>(
                        f, args, std::make_index_sequence<Traits::arity>{});
                    p.complete();
                } else {
                    p.set_value(invoke_typed_impl<Callable, typename Traits::args_tuple>(
                        f, args, std::make_index_sequence<Traits::arity>{}));
                }
                return nullptr;
            });
            future->add_continuation(cb, type);
        }
        return result;
    }
}

} // namespace detail

template<class T>
template<class F>
auto Future<T>::then(F&& callable, InvokeType type)
{
    return detail::future_then(future_, std::forward<F>(callable), type);
}

template<class F>
auto Future<void>::then(F&& callable, InvokeType type)
{
    return detail::future_then(future_, std::forward<F>(callable), type);
}

} // namespace strata

#endif // API_FUTURE_H
