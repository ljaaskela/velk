#include "metadata_container.h"

#include <interface/types.h>

namespace strata {

MetadataContainer::MetadataContainer(array_view<MemberDesc> members, const IStrata &instance)
    : members_(members), instance_(instance)
{}

array_view<MemberDesc> MetadataContainer::get_static_metadata() const
{
    return members_;
}

IInterface::Ptr MetadataContainer::find_or_create(std::string_view name, MemberKind kind) const
{
    for (size_t i = 0; i < members_.size(); ++i) {
        auto &member = members_[i];
        if (member.kind != kind || member.name != name) {
            continue;
        }

        // Check cache
        for (auto& [idx, ptr] : instances_) {
            if (idx == i) return ptr;
        }

        // Create and cache
        IInterface::Ptr created;
        switch (kind) {
        case MemberKind::Property:
            created = instance_.create_property(member.typeUid, {});
            break;
        case MemberKind::Event:
            created = instance_.create<IEvent>(ClassId::Event);
            break;
        case MemberKind::Function:
            created = instance_.create<IFunction>(ClassId::Function);
            break;
        }
        if (created) {
            instances_.emplace_back(i, created);
        }
        return created;
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
