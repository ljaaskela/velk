#ifndef VELK_INTF_FUTURE_H
#define VELK_INTF_FUTURE_H

#include <velk/interface/intf_any.h>
#include <velk/interface/intf_function.h>

namespace velk {

/**
 * @brief Read-side interface for a future value.
 *
 * Consumers poll or block on the future, and can attach continuations
 * that fire when the result becomes available.
 */
class IFuture : public Interface<IFuture>
{
public:
    /** @brief Returns true if the result has been set. Lock-free. */
    virtual bool is_ready() const = 0;
    /** @brief Blocks the calling thread until the result is available. */
    virtual void wait() const = 0;
    /** @brief Returns the result if ready, nullptr otherwise. */
    virtual const IAny* get_result() const = 0;
    /**
     * @brief Adds a continuation to be invoked when the result is set.
     *
     * If the future is already ready the continuation fires immediately (for Immediate type)
     * or is queued (for Deferred type). The continuation receives the result as a single FnArgs element.
     *
     * @param fn The continuation function.
     * @param type Immediate fires synchronously; Deferred queues via instance().queue_deferred_tasks().
     *             Auto is resolved when the continuation fires: Immediate if that happens on the
     *             thread that created the future, Deferred otherwise.
     */
    virtual void add_continuation(const IFunction::ConstPtr& fn, InvokeType type = Auto) = 0;

    /**
     * @brief Adds a continuation and returns a chained future that resolves with its result.
     *
     * The continuation's IAny::Ptr return value becomes the chained future's result.
     * nullptr results resolve as void.
     *
     * @param fn The continuation function.
     * @param type Immediate fires synchronously; Deferred queues via instance().queue_deferred_tasks().
     *             Auto is resolved when the continuation fires, as in add_continuation().
     * @return A new IFuture that resolves when the continuation completes.
     */
    virtual IFuture::Ptr then(const IFunction::ConstPtr& fn, InvokeType type = Auto) = 0;
};

/**
 * @brief Write-side interface for resolving a future with a value.
 */
class IFutureInternal : public Interface<IFutureInternal, IFuture>
{
public:
    /**
     * @brief Resolves the future with the given result.
     *
     * The value is cloned internally. Pass nullptr for void futures.
     * Can only be called once; subsequent calls return NothingToDo.
     *
     * @param result The value to resolve with (cloned), or nullptr for void.
     * @return Success on first call, NothingToDo if already resolved.
     */
    virtual ReturnValue set_result(const IAny* result) = 0;
};

} // namespace velk

#endif // VELK_INTF_FUTURE_H
