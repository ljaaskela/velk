#ifndef METADATA_CONTAINER_H
#define METADATA_CONTAINER_H

#include <ext/refcounted_dispatch.h>
#include <interface/intf_metadata.h>
#include <interface/intf_strata.h>
#include <vector>

namespace strata {

class MetadataContainer final : public RefCountedDispatch<IMetadata>
{
public:
    explicit MetadataContainer(array_view<MemberDesc> members, const IStrata &instance);

public: // IMetadata
    array_view<MemberDesc> GetStaticMetadata() const override;
    IProperty::Ptr GetProperty(std::string_view name) const override;
    IEvent::Ptr GetEvent(std::string_view name) const override;
    IFunction::Ptr GetFunction(std::string_view name) const override;

private:
    array_view<MemberDesc> members_;
    const IStrata &instance_;
    mutable std::vector<std::pair<std::string_view, IProperty::Ptr>> properties_;
    mutable std::vector<std::pair<std::string_view, IEvent::Ptr>> events_;
    mutable std::vector<std::pair<std::string_view, IFunction::Ptr>> functions_;
};

} // namespace strata

#endif // METADATA_CONTAINER_H
