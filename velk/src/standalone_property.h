#ifndef STANDALONE_PROPERTY_H
#define STANDALONE_PROPERTY_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>
#include <velk/string.h>

namespace velk::impl {

/**
 * @brief Standalone IProperty implementation for runtime-created and dynamic properties.
 *
 * Unlike impl::Property (which is storage-owned and delegates slot data to ObjectStorage),
 * StandaloneProperty owns its IAny data and on_changed event directly. Used by
 * IVelk::create_property() for freestanding properties and by ObjectStorage::add_property()
 * for dynamically added object properties.
 *
 * When attached to an object as a dynamic property, a notify callback is set so that
 * ObjectStorage can forward change notifications to IMetadataObserver instances.
 */
class StandaloneProperty final
    : public ext::ObjectCore<StandaloneProperty, IStandaloneProperty>
{
public:
    VELK_CLASS_UID(ClassId::StandaloneProperty, "StandaloneProperty");

    StandaloneProperty() = default;
    ~StandaloneProperty() override;

public: // IStandaloneProperty
    void set_name(string_view name) override { name_ = name; }
    void set_notify(void* ctx, void (*fn)(void*, string_view, IProperty*)) override
    {
        notify_ctx_ = ctx;
        notify_fn_ = fn;
    }

public: // IObject
    string get_name() const override { return name_; }

public: // IStorageOwned (always standalone, no owner)
    IObjectStorage* get_owner() const override { return nullptr; }
    uint16_t get_member_index() const override { return IStorageOwned::InvalidIndex; }
    uint16_t get_storage_id() const override { return 0; }
    void set_owner(IObjectStorage*, uint16_t, uint16_t) override {}
    bool binding_stale() const override { return false; } // No owner, no state to refresh.
    void set_binding_stale(bool) override {}

public: // IProperty
    ReturnValue set_value(const IAny& from, InvokeType type = Auto) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override;

public: // IPropertyInternal
    bool set_any(const IAny::Ptr& value, IAny::Ptr* previous = nullptr) override;
    IAny::ConstPtr get_any() const override;
    ReturnValue set_data(const void* data, size_t size, Uid type,
                         InvokeType invokeType = Auto) override;
    ReturnValue set_value_silent(const IAny& from) override;
    bool install_extension(const IAnyExtension::Ptr& extension) override;
    bool remove_extension(const IAnyExtension::Ptr& extension) override;

private:
    IAny::Ptr data_;
    mutable IEvent::Ptr event_;
    string name_;
    bool external_{};
    void* notify_ctx_{};
    void (*notify_fn_)(void*, string_view, IProperty*){};

    void invoke_on_changed() const;
};

} // namespace velk::impl

#endif // STANDALONE_PROPERTY_H
