#ifndef VELK_EXT_PLUGIN_H
#define VELK_EXT_PLUGIN_H

#include <velk/ext/member_traits.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_plugin.h>

namespace velk::ext {

/** @brief Declares a static constexpr plugin UID from a UUID string literal.
    @note Convenience wrapper for VELK_CLASS_UID(str) */
#define VELK_PLUGIN_UID(str) VELK_CLASS_UID(str)

/** @brief Declares a static constexpr plugin name. */
#define VELK_PLUGIN_NAME(str) static constexpr string_view plugin_name{str};

/** @brief Declares a static constexpr plugin version. */
#define VELK_PLUGIN_VERSION(major, minor, patch) \
    static constexpr uint32_t plugin_version = ::velk::make_version(major, minor, patch);

/** @brief Declares a static constexpr list of plugin dependencies. */
#define VELK_PLUGIN_DEPS(...) static constexpr PluginDependency plugin_deps[] = {__VA_ARGS__};

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
template <class FinalClass>
class Plugin : public Object<FinalClass, IPlugin>
{
    static constexpr string_view resolve_name()
    {
        if constexpr (detail::has_plugin_name<FinalClass>::value) {
            return FinalClass::plugin_name;
        } else {
            return FinalClass::class_name();
        }
    }

    static constexpr uint32_t resolve_version()
    {
        if constexpr (detail::has_plugin_version<FinalClass>::value) {
            return FinalClass::plugin_version;
        } else {
            return 0;
        }
    }

    static constexpr array_view<PluginDependency> resolve_deps()
    {
        if constexpr (detail::has_plugin_deps<FinalClass>::value) {
            return {FinalClass::plugin_deps, sizeof(FinalClass::plugin_deps) / sizeof(PluginDependency)};
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
        static const PluginInfo info{
            FinalClass::get_factory(), resolve_name(), resolve_version(), resolve_deps()};
        return info;
    }

    /** @brief Returns the static plugin descriptor. */
    const PluginInfo& get_plugin_info() const override { return plugin_info(); }

    /** @brief Default no-op update handler. Override to receive update notifications. */
    void update(const UpdateInfo&) override {}
};

} // namespace velk::ext

#ifdef _MSC_VER
#define VELK_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VELK_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Exports the plugin entry point for a shared library.
 * @param PluginClass The concrete plugin class (must derive from ext::Plugin<T>).
 */
#define VELK_PLUGIN(PluginClass)                                               \
    extern "C" VELK_PLUGIN_EXPORT const ::velk::PluginInfo* velk_plugin_info() \
    {                                                                          \
        return &PluginClass::plugin_info();                                    \
    }

namespace velk::detail {

/** @brief Signature of the velk_plugin_info entry point exported by plugin shared libraries. */
using PluginInfoFn = const PluginInfo*();

} // namespace velk::detail

#endif // VELK_EXT_PLUGIN_H
