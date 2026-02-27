#ifndef VELK_EXT_MEMBER_TRAITS_H
#define VELK_EXT_MEMBER_TRAITS_H

#include <type_traits>

namespace velk::detail {

/** @brief Check if class has a static 'class_id()' method. */
template <class T, class = void>
struct has_class_id : std::false_type
{};
template <class T>
struct has_class_id<T, std::void_t<decltype(T::class_id())>> : std::true_type
{};

/** @brief Check if class has 'class_uid' member. */
template <class T, class = void>
struct has_class_uid : std::false_type
{};
template <class T>
struct has_class_uid<T, std::void_t<decltype(T::class_uid)>> : std::true_type
{};

/** @brief Check if class has 'plugin_name' member. */
template <class T, class = void>
struct has_plugin_name : std::false_type
{};
template <class T>
struct has_plugin_name<T, std::void_t<decltype(T::plugin_name)>> : std::true_type
{};

/** @brief Check if class has 'plugin_version' member. */
template <class T, class = void>
struct has_plugin_version : std::false_type
{};
template <class T>
struct has_plugin_version<T, std::void_t<decltype(T::plugin_version)>> : std::true_type
{};

/** @brief Check if class has 'plugin_deps' member. */
template <class T, class = void>
struct has_plugin_deps : std::false_type
{};
template <class T>
struct has_plugin_deps<T, std::void_t<decltype(T::plugin_deps)>> : std::true_type
{};

} // namespace velk::detail

#endif // VELK_EXT_MEMBER_TRAITS_H
