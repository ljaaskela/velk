#ifndef METADATA_CONTAINER_H
#define METADATA_CONTAINER_H

#include <velk/ext/refcounted_dispatch.h>
#include <velk/interface/intf_metadata.h>
#include <memory>
#include <vector>

namespace velk {

/**
 * @brief Runtime metadata container that lazily creates property/event/function instances.
 *
 * Holds a static array_view<MemberDesc> (from VELK_INTERFACE) and creates runtime
 * PropertyImpl/FunctionImpl instances on first access via get_property()/get_event()/
 * get_function(). Created instances are cached for subsequent lookups.
 *
 * Does not inherit RefCountedDispatch; lifetime is managed by the owning Object
 * (allocated by VelkImpl at construction, deleted in Object's destructor).
 */
class MetadataContainer final : public ext::InterfaceDispatch<IMetadata>
{
public:
    /**
     * @brief Constructs a metadata container from static member descriptors.
     * @param members The static metadata array (from VELK_INTERFACE).
     * @param owner The owning object, used to bind function trampolines and resolve property state.
     */
    explicit MetadataContainer(array_view<MemberDesc> members, IInterface* owner = nullptr);

public: // IPropertyState (inherited via IMetadata; state lives in the Object, not here)
    void *get_property_state(Uid) override { return nullptr; }

public: // IMetadata
    array_view<MemberDesc> get_static_metadata() const override;
    IProperty::Ptr get_property(string_view name) const override;
    IEvent::Ptr get_event(string_view name) const override;
    IFunction::Ptr get_function(string_view name) const override;

private:
    array_view<MemberDesc> members_;    ///< Static metadata descriptors from VELK_INTERFACE.
    IInterface* owner_{};               ///< Owning object for trampoline binding and state access.

    /// Lazily populated cache mapping metadata index to runtime instance.
    mutable std::vector<std::pair<size_t, IInterface::Ptr>> instances_;

    /** @brief Storage for dynamically added members (future extension). */
    struct DynamicMembers {
        std::vector<MemberDesc> descriptors;    ///< Dynamic member descriptors.
        std::vector<IInterface::Ptr> instances;  ///< Runtime instances for dynamic members.
    };
    mutable std::unique_ptr<DynamicMembers> dynamic_;

    /** @brief Finds a static member by name and kind, creating its runtime instance if needed. */
    IInterface::Ptr find_or_create(string_view name, MemberKind kind) const;
    /** @brief Creates a runtime instance (PropertyImpl or FunctionImpl) from a member descriptor. */
    IInterface::Ptr create(MemberDesc desc) const;
    /** @brief Binds a function instance to the owner's virtual trampoline. */
    void bind(const MemberDesc &m, const IInterface::Ptr &fn) const;
};

} // namespace velk

#endif // METADATA_CONTAINER_H
