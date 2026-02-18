#ifndef FUTURE_H
#define FUTURE_H

#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_future.h>
#include <interface/types.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace velk {

class FutureImpl final : public ext::ObjectCore<FutureImpl, IFutureInternal>
{
public:
    VELK_CLASS_UID(ClassId::Future);

    FutureImpl() = default;

public: // IFuture
    bool is_ready() const override;
    void wait() const override;
    const IAny* get_result() const override;
    void add_continuation(const IFunction::ConstPtr& fn, InvokeType type) override;
    IFuture::Ptr then(const IFunction::ConstPtr& fn, InvokeType type) override;

public: // IFutureInternal
    ReturnValue set_result(const IAny* result) override;

private:
    struct Continuation {
        IFunction::ConstPtr fn;
        InvokeType type;
    };

    void fire_continuation(const Continuation& cont, const IAny* result) const;

    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::atomic<bool> ready_{false};
    IAny::Ptr result_;
    std::vector<Continuation> pending_continuations_;
};

} // namespace velk

#endif // FUTURE_H
