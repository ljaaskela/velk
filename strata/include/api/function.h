#ifndef API_FUNCTION_H
#define API_FUNCTION_H

#include <api/strata.h>
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_function.h>

namespace strata {

/** @brief Convenience wrapper that creates and owns an IFunction with a callback. */
class Function : public ext::ObjectCore<Function>
{
public:
    /** @brief Type alias for the native callback signature. */
    using CallbackFn = IFunction::CallableFn;

    Function() = delete;
    /** @brief Creates a Function backed by the given callback. */
    Function(CallbackFn *cb)
    {
        fn_ = instance().create<IFunction>(ClassId::Function);
        if (auto internal = interface_cast<IFunctionInternal>(fn_); internal && cb) {
            internal->set_invoke_callback(cb);
        }
    }

    /** @brief Implicit conversion to IFunction::Ptr. */
    operator IFunction::Ptr() { return fn_; }
    operator const IFunction::ConstPtr() const { return fn_; }

    /** @brief Invokes the function with no arguments.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    ReturnValue invoke(InvokeType type = Immediate) const { return fn_->invoke(nullptr, type); }
    /** @brief Invokes the function with the given @p args.
     *  @param args Arguments for invocation.
     *  @param type Immediate executes now; Deferred queues for the next update() call. */
    ReturnValue invoke(const IAny *args, InvokeType type = Immediate) const { return fn_->invoke(args, type); }

private:
    IFunction::Ptr fn_;
};

} // namespace strata

#endif // API_FUNCTION_H
