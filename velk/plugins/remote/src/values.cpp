#include "values.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_object_ref.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_variant.h>
#include <velk/string.h>

namespace velk::remote {

RemoteHandle HandleTable::handle_of(const IObject::Ptr& object)
{
    if (!object) {
        return 0;
    }
    auto it = by_object_.find(object.get());
    if (it != by_object_.end() && !it->second.object.expired()) {
        return it->second.handle;
    }
    // New, or a dead object's address reused by a new one.
    if (it != by_object_.end()) {
        by_handle_.erase(it->second.handle);
    }
    const RemoteHandle h = next_++;
    by_object_[object.get()] = {h, object};
    by_handle_[h] = object;
    return h;
}

RemoteHandle HandleTable::find(const IObject* object) const
{
    auto it = by_object_.find(object);
    return it != by_object_.end() && !it->second.object.expired() ? it->second.handle : 0;
}

IObject::Ptr HandleTable::object_of(RemoteHandle handle) const
{
    auto it = by_handle_.find(handle);
    return it != by_handle_.end() ? it->second.lock() : nullptr;
}

RemoteObject HandleTable::describe(const IObject::Ptr& object)
{
    RemoteObject o;
    o.handle = handle_of(object);
    if (object) {
        o.class_uid = object->get_class_uid();
    }
    return o;
}

void HandleTable::purge()
{
    if (by_handle_.size() < purge_at_) {
        return;
    }
    for (auto it = by_object_.begin(); it != by_object_.end();) {
        if (it->second.object.expired()) {
            by_handle_.erase(it->second.handle);
            it = by_object_.erase(it);
        } else {
            ++it;
        }
    }
    purge_at_ = by_handle_.size() * 2 > 1024 ? by_handle_.size() * 2 : 1024;
}

ReturnValue encode_value(const IAny& any, Uid type, Uid element_type, HandleTable& handles, RemoteValue& out)
{
    out = {};
    out.type = type;
    auto& mutable_any = const_cast<IAny&>(any);

    if (type == type_uid<string>()) {
        string s;
        if (failed(any.get_data(&s, sizeof(s), type))) {
            return ReturnValue::Fail;
        }
        out.form = RemoteValue::Form::String;
        out.data.resize(s.size());
        if (!s.empty()) {
            std::memcpy(out.data.data(), s.data(), s.size());
        }
        return ReturnValue::Success;
    }
    if (type == ClassId::ObjectRef) {
        auto* ref = interface_cast<IObjectRef>(&mutable_any);
        if (!ref) {
            return ReturnValue::Fail;
        }
        out.form = RemoteValue::Form::Object;
        out.object = handles.handle_of(ref->get_object());
        return ReturnValue::Success;
    }
    if (auto* array = interface_cast<IArrayAny>(&mutable_any)) {
        if (element_type == Uid{}) {
            return ReturnValue::Fail;
        }
        out.form = RemoteValue::Form::Array;
        out.element_type = element_type;
        const size_t n = array->array_size();
        out.elements.resize(n);
        for (size_t i = 0; i < n; ++i) {
            auto element = instance().create_any(element_type);
            if (!element || failed(array->get_at(i, *element))
                || failed(encode_value(*element, element_type, {}, handles, out.elements[i]))) {
                return ReturnValue::Fail;
            }
        }
        return ReturnValue::Success;
    }
    if (interface_cast<IVariant>(&mutable_any)) {
        return ReturnValue::Fail;
    }

    // Plain data: the value's memory.
    const size_t size = any.get_data_size(type);
    if (size == 0) {
        return ReturnValue::Fail;
    }
    out.form = RemoteValue::Form::Bytes;
    out.data.resize(size);
    return any.get_data(out.data.data(), size, type);
}

IAny::Ptr decode_value(const RemoteValue& value, HandleTable& handles)
{
    switch (value.form) {
    case RemoteValue::Form::String: {
        auto any = instance().create_any(type_uid<string>());
        string s(string_view(reinterpret_cast<const char*>(value.data.data()), value.data.size()));
        if (!any || failed(any->set_data(&s, sizeof(s), type_uid<string>()))) {
            return nullptr;
        }
        return any;
    }
    case RemoteValue::Form::Object: {
        auto ref = instance().create_object_ref();
        if (!ref) {
            return nullptr;
        }
        IObject::Ptr target;
        if (value.object) {
            target = handles.object_of(value.object);
            if (!target) {
                return nullptr;
            }
        }
        ref->set_object(target);
        return interface_pointer_cast<IAny>(ref);
    }
    case RemoteValue::Form::Array: {
        auto any = instance().create_any(value.type);
        auto* array = interface_cast<IArrayAny>(any);
        if (!array) {
            return nullptr;
        }
        array->clear_array();
        for (auto& e : value.elements) {
            auto element = decode_value(e, handles);
            if (!element || failed(array->push_back(*element))) {
                return nullptr;
            }
        }
        return any;
    }
    case RemoteValue::Form::Bytes: {
        auto any = instance().create_any(value.type);
        if (!any || any->get_data_size(value.type) != value.data.size()
            || failed(any->set_data(value.data.data(), value.data.size(), value.type))) {
            return nullptr;
        }
        return any;
    }
    default:
        return nullptr;
    }
}

ReturnValue encode_property(IMetadata& meta, string_view name, Uid interface_uid, HandleTable& handles,
                            RemoteValue& out)
{
    auto property = meta.get_property(name, interface_uid);
    auto value = property ? property->get_value() : nullptr;
    if (!value) {
        return ReturnValue::Fail;
    }
    // The declaration gives the type, and the element type for arrays.
    Uid type;
    Uid element_type;
    for (auto& member : meta.get_static_metadata()) {
        const auto* kind = member.propertyKind();
        if (!kind || member.name != name
            || (interface_uid != Uid{} && member.interfaceInfo && member.interfaceInfo->uid != interface_uid)) {
            continue;
        }
        type = kind->typeUid;
        if (const auto* array = member.arrayPropertyKind()) {
            element_type = array->elementUid;
        }
        break;
    }
    if (type == Uid{}) {
        for (auto& info : meta.get_properties()) {
            if (info.name == name) {
                type = info.typeUid;
                break;
            }
        }
    }
    if (type == Uid{}) {
        return ReturnValue::Fail;
    }
    return encode_value(*value, type, element_type, handles, out);
}

} // namespace velk::remote
