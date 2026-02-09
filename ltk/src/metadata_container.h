#ifndef METADATA_CONTAINER_H
#define METADATA_CONTAINER_H

#include <ext/object.h>
#include <interface/intf_metadata.h>
#include <interface/intf_registry.h>
#include <vector>

class MetadataContainer final : public BaseObject<IMetadata>
{
public:
    explicit MetadataContainer(array_view<MemberDesc> members, const IRegistry &registry);

public: // IMetadata
    array_view<MemberDesc> GetStaticMetadata() const override;
    IProperty::Ptr GetProperty(std::string_view name) const override;
    IEvent::Ptr GetEvent(std::string_view name) const override;
    IFunction::Ptr GetFunction(std::string_view name) const override;

private:
    array_view<MemberDesc> members_;
    const IRegistry &registry_;
    mutable std::vector<std::pair<std::string_view, IProperty::Ptr>> properties_;
    mutable std::vector<std::pair<std::string_view, IEvent::Ptr>> events_;
    mutable std::vector<std::pair<std::string_view, IFunction::Ptr>> functions_;
};

#endif // METADATA_CONTAINER_H
