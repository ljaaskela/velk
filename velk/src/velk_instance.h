#ifndef VELK_INSTANCE_H
#define VELK_INSTANCE_H

#include "library_handle.h"

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_velk.h>

#include <mutex>
#include <vector>

namespace velk {

/**
 * @brief Singleton implementation of IVelk.
 *
 * Manages a sorted registry of object factories (keyed by class UID) and
 * provides type creation, metadata lookup, deferred task queuing, and
 * factory methods for properties, functions, futures, and any values.
 */
class VelkInstance final : public ext::ObjectCore<VelkInstance, IVelk, ITypeRegistry, IPluginRegistry, ILog>
{
public:
    VelkInstance();
    ~VelkInstance();

    ITypeRegistry& type_registry() override { return *this; }
    const ITypeRegistry& type_registry() const override { return *this; }

    IPluginRegistry& plugin_registry() override { return *this; }
    const IPluginRegistry& plugin_registry() const override { return *this; }

    ILog& log() override { return *this; }
    const ILog& log() const override { return const_cast<VelkInstance&>(*this); }

    ReturnValue register_type(const IObjectFactory& factory) override;
    ReturnValue unregister_type(const IObjectFactory& factory) override;
    IInterface::Ptr create(Uid uid) const override;

    const ClassInfo* get_class_info(Uid classUid) const override;
    IAny::Ptr create_any(Uid type) const override;
    IProperty::Ptr create_property(Uid type, const IAny::Ptr& value, int32_t flags) const override;
    void queue_deferred_tasks(array_view<DeferredTask> tasks) const override;
    void update(Duration time) const override;
    IFuture::Ptr create_future() const override;
    IFunction::Ptr create_callback(IFunction::CallableFn* fn) const override;
    IFunction::Ptr create_owned_callback(void* context, IFunction::BoundFn* fn,
                                         IFunction::ContextDeleter* deleter) const override;

    ReturnValue load_plugin(const IPlugin::Ptr& plugin) override;
    ReturnValue load_plugin_from_path(const char* path) override;
    ReturnValue unload_plugin(Uid pluginId) override;
    IPlugin* find_plugin(Uid pluginId) const override;
    size_t plugin_count() const override;

    void set_sink(const ILogSink::Ptr& sink) override;
    void set_level(LogLevel level) override;
    LogLevel get_level() const override;
    void dispatch(LogLevel level, const char* file, int line, const char* message) override;

private:
    /** @brief Registry entry mapping a class UID to its factory. */
    struct Entry
    {
        Uid uid;                       ///< Class UID.
        const IObjectFactory* factory; ///< Factory that creates instances of this class.
        Uid owner;                     ///< Plugin that registered this type (Uid{} = builtin).
        bool operator<(const Entry& o) const { return uid < o.uid; }
    };

    /** @brief Plugin registry entry. */
    struct PluginEntry
    {
        Uid uid;
        IPlugin::Ptr plugin;
        LibraryHandle library; ///< Non-empty when plugin was loaded from a shared library.
        PluginConfig config;   ///< Configuration set by the plugin during initialize.
        bool operator<(const PluginEntry& o) const { return uid < o.uid; }
    };

    /** @brief Finds the factory for the given class UID, or nullptr if not registered. */
    const IObjectFactory* find(Uid uid) const;
    /** @brief Checks that all dependencies declared in info are loaded. Logs and returns Fail if not. */
    ReturnValue check_dependencies(const PluginInfo& info);
    std::vector<Entry> types_;                         ///< Sorted registry of class factories.
    std::vector<PluginEntry> plugins_;                 ///< Sorted registry of loaded plugins.
    Uid current_owner_;                                ///< Owner context for type registration.
    mutable std::mutex deferred_mutex_;                ///< Guards @c deferred_queue_.
    mutable std::vector<DeferredTask> deferred_queue_; ///< Tasks queued for the next update() call.
    ILogSink::Ptr sink_;                               ///< Custom log sink (empty = default stderr).
    LogLevel level_{LogLevel::Info};                   ///< Minimum log level.
    std::vector<IPlugin*> update_plugins_;             ///< Plugins that opted into update notifications.
    int64_t init_time_us_;                             ///< System time at construction (microseconds).
    mutable int64_t first_update_us_{};                ///< Time of the first update() call.
    mutable int64_t last_update_us_{};                 ///< Time of the previous update() call.
    mutable bool last_update_was_explicit_{};          ///< Whether previous update used explicit time.
};

} // namespace velk

#endif // VELK_INSTANCE_H
