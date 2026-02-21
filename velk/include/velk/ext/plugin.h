#ifndef VELK_EXT_PLUGIN_H
#define VELK_EXT_PLUGIN_H

#include <velk/ext/object.h>
#include <velk/interface/intf_plugin.h>

namespace velk::ext {

/** @brief Declares a static constexpr plugin UID from a UUID string literal.
    @note Convenience wrapper for VELK_CLASS_UID(str) */
#define VELK_PLUGIN_UID(str) VELK_CLASS_UID(str)

/** @brief Declares a static constexpr plugin name. */
#define VELK_PLUGIN_NAME(str) static constexpr string_view plugin_name { str };

/** @brief Declares a static constexpr list of the Uids the plugin depends on. */
#define VELK_PLUGIN_DEPS(...) static constexpr Uid plugin_deps[] = { __VA_ARGS__ };

/**
 * @brief CRTP base for plugin implementations.
 *
 * Inherits ext::Object to get ref counting, shared_ptr support, factory
 * registration, and IObject identity (get_class_uid, get_class_name, get_self).
 * Use VELK_CLASS_UID to specify a stable plugin identifier.
 *
 * Static metadata is collected automatically from optional members on FinalClass:
 *  - `static constexpr string_view plugin_name` (defaults to class_name())
 *  - `static constexpr Uid plugin_deps[]` (defaults to none)
 *
 * Derived classes implement initialize and shutdown.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 */
template<class FinalClass>
class Plugin : public Object<FinalClass, IPlugin>
{
    template<class T, class = void>
    struct has_plugin_name : std::false_type {};
    template<class T>
    struct has_plugin_name<T, std::void_t<decltype(T::plugin_name)>> : std::true_type {};

    template<class T, class = void>
    struct has_plugin_deps : std::false_type {};
    template<class T>
    struct has_plugin_deps<T, std::void_t<decltype(T::plugin_deps)>> : std::true_type {};

    static constexpr string_view resolve_name()
    {
        if constexpr (has_plugin_name<FinalClass>::value) {
            return FinalClass::plugin_name;
        } else {
            return FinalClass::class_name();
        }
    }

    static constexpr array_view<Uid> resolve_deps()
    {
        if constexpr (has_plugin_deps<FinalClass>::value) {
            return { FinalClass::plugin_deps,
                     sizeof(FinalClass::plugin_deps) / sizeof(Uid) };
        } else {
            return {};
        }
    }

public:
    /**
     * @brief Returns the static plugin descriptor, accessible without an instance.
     *
     * Deferred to a function (rather than a static constexpr member) because
     * FinalClass is incomplete at template instantiation time.
     */
    static const PluginInfo& plugin_info()
    {
        static const PluginInfo info {
            FinalClass::get_factory(),
            resolve_name(),
            resolve_deps()
        };
        return info;
    }

    /** @brief Returns the static plugin descriptor. */
    const PluginInfo& get_plugin_info() const override { return plugin_info(); }
};

} // namespace velk::ext

#ifdef _MSC_VER
#  define VELK_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define VELK_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Exports the plugin entry point for a shared library.
 * @param PluginClass The concrete plugin class (must derive from ext::Plugin<T>).
 */
#define VELK_PLUGIN(PluginClass) \
    extern "C" VELK_PLUGIN_EXPORT const ::velk::PluginInfo* velk_plugin_info() { \
        return &PluginClass::plugin_info(); \
    }

namespace velk::detail {

/** @brief Signature of the velk_plugin_info entry point exported by plugin shared libraries. */
using PluginInfoFn = const PluginInfo*();

} // namespace velk::detail

#endif // VELK_EXT_PLUGIN_H
