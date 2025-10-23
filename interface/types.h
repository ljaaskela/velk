#ifndef TYPES_H
#define TYPES_H

#include <common.h>
#include <interface/intf_interface.h>

struct ClassInfo
{
    const Uid uid;
    const std::string_view name;
};

#define DECLARE_CLASS(Name) \
    namespace ClassId { \
    [[maybe_unused]] static constexpr ClassInfo Name{MakeHash(#Name), #Name}; \
    }

// Built-in classes
DECLARE_CLASS(Property)
DECLARE_CLASS(Event)
DECLARE_CLASS(Function)

// interface cast helper
template <class T>
typename T::Ptr interface_pointer_cast(const IInterface::Ptr& obj)
{
    if (obj && obj->GetInterface(T::UID)) {
        return std::dynamic_pointer_cast<T>(obj);
    }
    return nullptr;
}

enum ReturnValue : int16_t {
    SUCCESS = 0,
    NOTHING_TO_DO = 1,
    FAIL = -1,
    INVALID_ARGUMENT = -2,
};

[[maybe_unused]] static constexpr bool Succeeded(ReturnValue ret)
{
    return ret >= 0;
}

[[maybe_unused]] static constexpr bool Failed(ReturnValue ret)
{
    return ret < 0;
}

#endif
