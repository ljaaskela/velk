#include "metadata_container.h"
#include "function.h"
#include "property.h"

#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/types.h>

namespace velk {

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
        created = ext::make_object<PropertyImpl>();
        if (auto *pi = created->get_interface<IPropertyInternal>()) {
            if (auto* pk = desc.propertyKind()) {
                // Try state-backed ref first
                if (pk->createRef && owner_) {
                    if (auto* ps = interface_cast<IPropertyState>(owner_)) {
                        if (void* base = ps->get_property_state(desc.interfaceInfo->uid)) {
                            if (auto ref = pk->createRef(base)) {
                                pi->set_any(ref);
                            }
                        }
                    }
                }
                // Fallback to cloning default
                if (!pi->get_any() && pk->getDefault) {
                    if (auto* def = pk->getDefault()) {
                        if (auto any = def->clone()) {
                            pi->set_any(any);
                        }
                    }
                }
                // Apply flags (e.g. ReadOnly) to the PropertyImpl
                if (pk->flags) {
                    pi->set_flags(pk->flags);
                }
            }
        }
        break;
    }
    case MemberKind::Event:
        [[fallthrough]];
    case MemberKind::Function:
        created = ext::make_object<FunctionImpl>();
        break;
    }
    return created;
}

void MetadataContainer::bind(const MemberDesc &m, const IInterface::Ptr &fn) const
{
    // Wire virtual dispatch: if the interface declared a fn_Name()
    // virtual (via VELK_INTERFACE), bind the FunctionImpl so that
    // invoke() routes through the static trampoline to the virtual
    // method on the owning object's interface subobject.

    auto *fk = m.functionKind(); // Only valid for functions
    if (fk && fk->trampoline && owner_) {
        if (auto *fi = interface_cast<IFunctionInternal>(fn)) {
            // Resolve the interface pointer that declared this function.
            // This is the 'self' the trampoline will static_cast back to
            // the concrete interface type (e.g. IMyWidget*).
            void *intf_ptr = owner_->get_interface(m.interfaceInfo->uid);
            if (intf_ptr) {
                fi->bind(intf_ptr, fk->trampoline);
            }
        }
    }
}

IInterface::Ptr MetadataContainer::find_or_create(string_view name, MemberKind kind) const
{
    auto matches = [&](const MemberDesc &m) { return m.kind == kind && m.name == name; };

    // Check cache first
    for (auto &[idx, ptr] : instances_) {
        if (matches(members_[idx])) {
            return ptr;
        }
    }
    IInterface::Ptr created;
    // Find in static metadata and create if found
    for (size_t i = 0; i < members_.size(); ++i) {
        auto &m = members_[i];
        if (matches(m)) {
            // Create
            created = create(m);
            // Bind (if function)
            bind(m, created);
            // Add to metadata
            instances_.emplace_back(i, created);
        }
    }
    return created;
}

IProperty::Ptr MetadataContainer::get_property(string_view name) const
{
    return interface_pointer_cast<IProperty>(find_or_create(name, MemberKind::Property));
}

IEvent::Ptr MetadataContainer::get_event(string_view name) const
{
    return interface_pointer_cast<IEvent>(find_or_create(name, MemberKind::Event));
}

IFunction::Ptr MetadataContainer::get_function(string_view name) const
{
    return interface_pointer_cast<IFunction>(find_or_create(name, MemberKind::Function));
}

} // namespace velk
