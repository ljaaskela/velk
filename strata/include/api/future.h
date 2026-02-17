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

/**
 * @brief Maps a callable type to the corresponding Future value type.
 *
 * FnArgs callables (returning ReturnValue or IAny::Ptr) produce void.
 * Typed callables returning void or ReturnValue produce void.
 * Other typed callables produce their return type.
 */
template<class Callable, class = void>
struct future_return { using type = void; };

template<class Callable>
struct future_return<Callable, std::enable_if_t<
    !std::is_invocable_v<Callable, FnArgs> &&
    has_callable_traits_v<Callable> &&
    !std::is_void_v<typename callable_traits<Callable>::return_type> &&
    !std::is_same_v<typename callable_traits<Callable>::return_type, ReturnValue>>>
{
    using type = typename callable_traits<Callable>::return_type;
};

template<class Callable>
using future_return_t = typename future_return<Callable>::type;

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

template<class T>
template<class F>
auto Future<T>::then(F&& callable, InvokeType type)
{
    using FR = detail::future_return_t<std::decay_t<F>>;
    Callback cb(std::forward<F>(callable));
    return Future<FR>(future_ ? future_->then(cb, type) : nullptr);
}

template<class F>
auto Future<void>::then(F&& callable, InvokeType type)
{
    using FR = detail::future_return_t<std::decay_t<F>>;
    Callback cb(std::forward<F>(callable));
    return Future<FR>(future_ ? future_->then(cb, type) : nullptr);
}

} // namespace strata

#endif // API_FUTURE_H
