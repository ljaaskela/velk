#ifndef VELK_IMPL_H
#define VELK_IMPL_H

#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_velk.h>
#include <mutex>
#include <vector>

namespace velk {

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
    struct Entry {
        Uid uid;
        const IObjectFactory *factory;
        bool operator<(const Entry& o) const { return uid < o.uid; }
    };

    const IObjectFactory* find(Uid uid) const;
    std::vector<Entry> types_;
    mutable std::mutex deferred_mutex_;
    mutable std::vector<DeferredTask> deferred_queue_;
};

} // namespace velk

#endif // VELK_IMPL_H
