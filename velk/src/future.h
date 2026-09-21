#ifndef FUTURE_H
#define FUTURE_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_future.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace velk::impl {

/**
 * @brief Default IFuture/IFutureInternal implementation.
 *
 * Thread-safe future that stores a cloned IAny result. Supports blocking
 * wait(), lock-free is_ready(), and continuation chaining via then().
 * Continuations attached before set_result() are stored and fired when
 * the result arrives. Continuations attached after are fired immediately
 * (Immediate type) or queued via instance().queue_deferred_tasks() (Deferred type).
 * Auto is resolved when a continuation fires: Immediate on the future's owner
 * thread, Deferred on any other thread.
 */
class Future final : public ext::ObjectCore<Future, IFutureInternal>
{
public:
    VELK_CLASS_UID(ClassId::Future, "Future");

    Future() = default;

public: // IFuture
    bool is_ready() const override;
    void wait() const override;
    const IAny* get_result() const override;
    void add_continuation(const IFunction::ConstPtr& fn, InvokeType type) override;
    IFuture::Ptr then(const IFunction::ConstPtr& fn, InvokeType type) override;

public: // IFutureInternal
    ReturnValue set_result(const IAny* result) override;

private:
    struct Continuation
    {
        IFunction::ConstPtr fn;
        InvokeType type;
    };

    void fire_continuation(const Continuation& cont, const IAny* result) const;

    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::atomic<bool> ready_{false};
    IAny::Ptr result_;
    vector<Continuation> pending_continuations_;
};

} // namespace velk::impl

#endif // FUTURE_H
