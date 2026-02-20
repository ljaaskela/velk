#ifndef VELK_IMPL_H
#define VELK_IMPL_H

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
class VelkImpl final : public ext::ObjectCore<VelkImpl, IVelk>
{
public:
    VelkImpl();

    ReturnValue register_type(const IObjectFactory &factory) override;
    ReturnValue unregister_type(const IObjectFactory &factory) override;
    IInterface::Ptr create(Uid uid) const override;

    const ClassInfo* get_class_info(Uid classUid) const override;
    IAny::Ptr create_any(Uid type) const override;
    IProperty::Ptr create_property(Uid type, const IAny::Ptr &value, int32_t flags) const override;
    void queue_deferred_tasks(array_view<DeferredTask> tasks) const override;
    void update() const override;
    IFuture::Ptr create_future() const override;
    IFunction::Ptr create_callback(IFunction::CallableFn* fn) const override;
    IFunction::Ptr create_owned_callback(void* context,
                                         IFunction::BoundFn* fn,
                                         IFunction::ContextDeleter* deleter) const override;

private:
    /** @brief Registry entry mapping a class UID to its factory. */
    struct Entry {
        Uid uid;                        ///< Class UID.
        const IObjectFactory *factory;  ///< Factory that creates instances of this class.
        bool operator<(const Entry& o) const { return uid < o.uid; }
    };

    /** @brief Finds the factory for the given class UID, or nullptr if not registered. */
    const IObjectFactory* find(Uid uid) const;
    std::vector<Entry> types_;                      ///< Sorted registry of class factories.
    mutable std::mutex deferred_mutex_;             ///< Guards @c deferred_queue_.
    mutable std::vector<DeferredTask> deferred_queue_; ///< Tasks queued for the next update() call.
};

} // namespace velk

#endif // VELK_IMPL_H
