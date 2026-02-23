#ifndef VELK_API_TRAITS_H
#define VELK_API_TRAITS_H

#include <velk/interface/intf_object.h>

#include <tuple>
#include <type_traits>

namespace velk {

class IAny; // forward declaration for convertibility checks

/**
 * @brief Internal type traits and SFINAE helpers shared across the Velk API.
 *
 * This namespace contains callable introspection traits, parameter decay helpers,
 * IAny convertibility checks, and SFINAE gate aliases used by Callback, Function,
 * Future, Any, and the VELK_INTERFACE macro machinery.
 */
namespace detail {

// IObject introspection

/** @brief Returns true if T is IObject or has IObject as an ancestor via ParentInterface. */
template <class T>
constexpr bool has_iobject_in_chain()
{
    if constexpr (std::is_same_v<T, IObject>) {
        return true;
    } else if constexpr (std::is_same_v<T, IInterface>) {
        return false;
    } else {
        return has_iobject_in_chain<typename T::ParentInterface>();
    }
}

// Callable introspection

/**
 * @brief Base for callable_traits specializations.
 *
 * Provides the return type, parameter tuple, and arity extracted from a
 * member-function-pointer signature.
 *
 * @tparam R    Return type of the callable.
 * @tparam Args Parameter types of the callable.
 */
template <class R, class... Args>
struct callable_traits_base
{
    using return_type = R;                           ///< The callable's return type.
    using args_tuple = std::tuple<Args...>;          ///< Parameter types as a std::tuple.
    static constexpr size_t arity = sizeof...(Args); ///< Number of parameters.
};

/**
 * @brief Extracts return type, parameter types, and arity from a callable.
 *
 * SFINAE-friendly: the primary template is deliberately left undefined so that
 * substitution failure on unsupported types is not an error. Specializations
 * cover @c const / non-const and @c noexcept member-function pointers, plus a
 * @c void_t-based catch-all that delegates through @c operator().
 *
 * @tparam F The callable type to introspect.
 *
 * @par Supported callable forms
 * - <tt>R(C::*)(Args...) const</tt>
 * - <tt>R(C::*)(Args...)</tt>
 * - <tt>R(C::*)(Args...) const noexcept</tt>
 * - <tt>R(C::*)(Args...) noexcept</tt>
 * - Any type @p F with a detectable @c operator() (lambdas, functors)
 *
 * @par Example
 * @code
 * auto fn = [](int x, float y) -> bool { return x > y; };
 * using Traits = detail::callable_traits<decltype(fn)>;
 * // Traits::return_type == bool
 * // Traits::arity == 2
 * @endcode
 */
template <class F, class = void>
struct callable_traits; // SFINAE-friendly primary (undefined)

/// @cond INTERNAL
template <class R, class C, class... Args>
struct callable_traits<R (C::*)(Args...) const> : callable_traits_base<R, Args...>
{};
template <class R, class C, class... Args>
struct callable_traits<R (C::*)(Args...)> : callable_traits_base<R, Args...>
{};
template <class R, class C, class... Args>
struct callable_traits<R (C::*)(Args...) const noexcept> : callable_traits_base<R, Args...>
{};
template <class R, class C, class... Args>
struct callable_traits<R (C::*)(Args...) noexcept> : callable_traits_base<R, Args...>
{};

template <class F>
struct callable_traits<F, std::void_t<decltype(&F::operator())>> : callable_traits<decltype(&F::operator())>
{};
/// @endcond

/**
 * @brief Detects whether @c callable_traits<F> is well-formed.
 *
 * @c true if @p F has a detectable @c operator() (or is a member-function pointer)
 * from which @c callable_traits can extract a @c return_type.  @c false otherwise.
 *
 * @tparam F The type to test.
 */
template <class F, class = void>
inline constexpr bool has_callable_traits_v = false;

/// @cond INTERNAL
template <class F>
inline constexpr bool has_callable_traits_v<F, std::void_t<typename callable_traits<F>::return_type>> = true;
/// @endcond

// Parameter decay

/**
 * @brief Strips @c const and applies @c std::decay to a parameter type.
 *
 * Equivalent to <tt>std::remove_const_t<std::decay_t<T>></tt>.  Used when
 * extracting typed arguments from @c FnArgs via @c Any<const T>::get_value(),
 * where the parameter type may carry const or reference qualifiers that must
 * be removed before constructing the @c Any wrapper.
 *
 * @tparam T The (possibly const/ref-qualified) parameter type.
 */
template <class T>
using decay_param_t = std::remove_const_t<std::decay_t<T>>;

// IAny convertibility checks

/**
 * @brief @c true when @c sizeof...(Args) >= 2 and every argument type is
 *        implicitly convertible to <tt>const IAny*</tt>.
 *
 * Used to select the @c invoke_function overload that passes arguments
 * through as raw @c IAny pointers without wrapping.
 *
 * @tparam Args Variadic argument types to check.
 */
template <class... Args>
inline constexpr bool all_any_convertible_v =
    (sizeof...(Args) >= 2) && (std::is_convertible_v<const Args&, const IAny*> && ...);

/**
 * @brief @c true when @c sizeof...(Args) >= 2 and no argument type is
 *        convertible to <tt>const IAny*</tt>.
 *
 * Used to select the @c invoke_function overload that wraps each argument
 * in @c Any<T> before invocation.
 *
 * @tparam Args Variadic argument types to check.
 */
template <class... Args>
inline constexpr bool none_any_convertible_v =
    (sizeof...(Args) >= 2) && (!std::is_convertible_v<const Args&, const IAny*> && ...);

// SFINAE gate aliases

/**
 * @brief SFINAE gate: substitution succeeds (as @c int) when @p B is @c true.
 *
 * Shorthand for <tt>std::enable_if_t<B, int></tt>.  Intended for use as a
 * non-type template parameter default:
 * @code
 * template<bool RW = IsReadWrite, detail::require<RW> = 0>
 * void set_value(const T& v);
 * @endcode
 *
 * @tparam B Boolean condition.
 */
template <bool B>
using require = std::enable_if_t<B, int>;

/**
 * @brief SFINAE gate for variadic @c invoke_function with IAny-convertible arguments.
 * @tparam Args Variadic argument types; gate opens when @c all_any_convertible_v is @c true.
 * @see all_any_convertible_v
 */
template <class... Args>
using require_any_args = std::enable_if_t<all_any_convertible_v<Args...>, int>;

/**
 * @brief SFINAE gate for variadic @c invoke_function with value arguments.
 * @tparam Args Variadic argument types; gate opens when @c none_any_convertible_v is @c true.
 * @see none_any_convertible_v
 */
template <class... Args>
using require_value_args = std::enable_if_t<none_any_convertible_v<Args...>, int>;

} // namespace detail
} // namespace velk

#endif // VELK_API_TRAITS_H
