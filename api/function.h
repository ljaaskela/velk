#ifndef API_FUNCTION_H
#define API_FUNCTION_H

#include <common.h>
#include <ext/object.h>
#include <interface/intf_function.h>

class Function : public Object<Function>
{
    IMPLEMENT_CLASS(ClassId::Function)
public:
    using CallbackFn = IFunction::CallableFn;

    Function() = delete;
    Function(CallbackFn *cb)
    {
        fn_ = GetRegistry().Create<IFunction>(ClassId::Function);
        if (auto internal = interface_pointer_cast<IFunctionInternal>(fn_); internal && cb) {
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

#endif // API_FUNCTION_H
