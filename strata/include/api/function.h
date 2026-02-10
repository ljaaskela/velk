#ifndef API_FUNCTION_H
#define API_FUNCTION_H

#include <api/strata.h>
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_function.h>

namespace strata {

/** @brief Convenience wrapper that creates and owns an IFunction with a callback. */
class Function : public CoreObject<Function>
{
public:
    using CallbackFn = IFunction::CallableFn;

    Function() = delete;
    Function(CallbackFn *cb)
    {
        fn_ = Strata().Create<IFunction>(ClassId::Function);
        if (auto internal = interface_cast<IFunctionInternal>(fn_); internal && cb) {
            internal->SetInvokeCallback(cb);
        }
    }

    operator IFunction::Ptr() { return fn_; }
    operator const IFunction::ConstPtr() const { return fn_; }

    ReturnValue Invoke() const { return Invoke({}); }
    ReturnValue Invoke(const IAny *args) const { return fn_->Invoke(args); }

private:
    IFunction::Ptr fn_;
};

} // namespace strata

#endif // API_FUNCTION_H
