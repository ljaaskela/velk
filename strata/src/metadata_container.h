#ifndef METADATA_CONTAINER_H
#define METADATA_CONTAINER_H

#include <ext/refcounted_dispatch.h>
#include <interface/intf_metadata.h>
#include <interface/intf_strata.h>
#include <memory>
#include <vector>

namespace strata {

class MetadataContainer final : public InterfaceDispatch<IMetadata>
{
public:
    explicit MetadataContainer(array_view<MemberDesc> members, const IStrata &instance,
        IInterface* owner = nullptr);

public: // IMetadata
    array_view<MemberDesc> get_static_metadata() const override;
    IProperty::Ptr get_property(std::string_view name) const override;
    IEvent::Ptr get_event(std::string_view name) const override;
    IFunction::Ptr get_function(std::string_view name) const override;

private:
    array_view<MemberDesc> members_;
    const IStrata &instance_;
    IInterface* owner_{};

    // Static members: lazily populated as accessed. Each entry maps
    // a metadata index to its runtime instance.
    mutable std::vector<std::pair<size_t, IInterface::Ptr>> instances_;

    // Future: dynamically added members
    struct DynamicMembers {
        std::vector<MemberDesc> descriptors;
        std::vector<IInterface::Ptr> instances;
    };
    mutable std::unique_ptr<DynamicMembers> dynamic_;

    // Helper: find static member by name+kind, lazily create if needed
    IInterface::Ptr find_or_create(std::string_view name, MemberKind kind) const;
};

} // namespace strata

#endif // METADATA_CONTAINER_H
