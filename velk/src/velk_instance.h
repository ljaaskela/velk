#ifndef VELK_INSTANCE_H
#define VELK_INSTANCE_H

#include "plugin_registry.h"
#include "type_registry.h"

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
class VelkInstance final : public ext::ObjectCore<VelkInstance, IVelk, ILog>
{
public:
    VelkInstance();
    ~VelkInstance();

    ITypeRegistry& type_registry() override { return type_registry_; }
    const ITypeRegistry& type_registry() const override { return type_registry_; }

    IPluginRegistry& plugin_registry() override { return plugin_registry_; }
    const IPluginRegistry& plugin_registry() const override { return plugin_registry_; }

    ILog& log() override { return *this; }
    const ILog& log() const override { return const_cast<VelkInstance&>(*this); }

    IInterface::Ptr create(Uid uid) const override;
    IAny::Ptr create_any(Uid type) const override;
    IProperty::Ptr create_property(Uid type, const IAny::Ptr& value, int32_t flags) const override;
    void queue_deferred_tasks(array_view<DeferredTask> tasks) const override;
    void queue_deferred_property(DeferredPropertySet task) const override;
    void update(Duration time) const override;
    IFuture::Ptr create_future() const override;
    IFunction::Ptr create_callback(IFunction::CallableFn* fn) const override;
    IFunction::Ptr create_owned_callback(void* context, IFunction::BoundFn* fn,
                                         IFunction::ContextDeleter* deleter) const override;

    void set_sink(const ILogSink::Ptr& sink) override;
    void set_level(LogLevel level) override;
    LogLevel get_level() const override;
    void dispatch(LogLevel level, const char* file, int line, const char* message) override;

private:
    /** @brief Coalesces and applies queued deferred property sets (last-write-wins). */
    void flush_deferred_properties(std::vector<DeferredPropertySet>& propSets) const;

    TypeRegistry type_registry_;                       ///< Registry of class factories.
    PluginRegistry plugin_registry_;                   ///< Registry of loaded plugins.
    mutable std::mutex deferred_mutex_;                ///< Guards @c deferred_queue_.
    mutable std::vector<DeferredTask> deferred_queue_; ///< Tasks queued for the next update() call.
    mutable std::vector<DeferredPropertySet>
        deferred_property_queue_;    ///< Property sets queued for the next update() call.
    ILogSink::Ptr sink_;             ///< Custom log sink (empty = default stderr).
    LogLevel level_{LogLevel::Info}; ///< Minimum log level.
};

} // namespace velk

#endif // VELK_INSTANCE_H
