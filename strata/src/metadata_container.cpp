#include "metadata_container.h"
#include "function.h"
#include "property.h"

#include <api/strata.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

namespace {

template<class T>
IObject::Ptr make_shared_impl()
{
    IObject::Ptr obj(new T, [](IObject* p) { p->unref(); });
    if (auto* s = obj->template get_interface<ISharedFromObject>()) {
        s->set_self(obj);
    }
    return obj;
}

} // namespace

MetadataContainer::MetadataContainer(array_view<MemberDesc> members, IInterface* owner)
    : members_(members), owner_(owner)
{}

array_view<MemberDesc> MetadataContainer::get_static_metadata() const
{
    return members_;
}

IInterface::Ptr MetadataContainer::create(MemberDesc desc) const
{
    // instantiate directly to avoid factory lookups
    IInterface::Ptr created;
    switch (desc.kind) {
    case MemberKind::Property: {
        created = make_shared_impl<PropertyImpl>();
        if (auto *pi = created->get_interface<IPropertyInternal>()) {
            if (auto any = instance().create_any(desc.typeUid)) {
                pi->set_any(any);
            }
        }
        break;
    }
    case MemberKind::Event:
        [[fallthrough]];
    case MemberKind::Function:
        created = make_shared_impl<FunctionImpl>();
        break;
    }
    return created;
}

IInterface::Ptr MetadataContainer::find_or_create(std::string_view name, MemberKind kind) const
{
    for (size_t i = 0; i < members_.size(); ++i) {
        auto &member = members_[i];
        if (member.kind != kind || member.name != name) {
            // Not a match
            continue;
        }
        // Check cache
        for (auto& [idx, ptr] : instances_) {
            if (idx == i) return ptr;
        }
        // Create and cache
        if (auto created = create(member)) {
            // Wire virtual dispatch: if the interface declared a fn_Name()
            // virtual (via STRATA_INTERFACE), bind the FunctionImpl so that
            // invoke() routes through the static trampoline to the virtual
            // method on the owning object's interface subobject.
            if ((kind == MemberKind::Function || kind == MemberKind::Event) && member.fnTrampoline && owner_) {
                if (auto* fi = interface_cast<IFunctionInternal>(created)) {
                    // Resolve the interface pointer that declared this function.
                    // This is the 'self' the trampoline will static_cast back to
                    // the concrete interface type (e.g. IMyWidget*).
                    void* intf_ptr = owner_->get_interface(member.interfaceInfo->uid);
                    if (intf_ptr) {
                        fi->bind(intf_ptr, member.fnTrampoline);
                    }
                }
            }
            // Add to metadata
            instances_.emplace_back(i, created);
            return created;
        }
    }
    return {};
}

IProperty::Ptr MetadataContainer::get_property(std::string_view name) const
{
    return interface_pointer_cast<IProperty>(find_or_create(name, MemberKind::Property));
}

IEvent::Ptr MetadataContainer::get_event(std::string_view name) const
{
    return interface_pointer_cast<IEvent>(find_or_create(name, MemberKind::Event));
}

IFunction::Ptr MetadataContainer::get_function(std::string_view name) const
{
    return interface_pointer_cast<IFunction>(find_or_create(name, MemberKind::Function));
}

} // namespace strata
