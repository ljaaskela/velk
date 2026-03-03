#ifndef VELK_EXT_OBJECT_H
#define VELK_EXT_OBJECT_H

#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/ext/metadata.h>
#include <velk/interface/intf_object_storage.h>

#include <tuple>

namespace velk::detail {

/**
 * @brief Non-template base holding IObjectStorage pointer and delegation helpers.
 *
 * Avoids duplicating the storage delegation logic in every Object<> instantiation.
 * The ObjectStorage is created lazily on first runtime metadata/attachment access.
 * ClassInfo and owner are passed in from the Object template rather than stored.
 */
class ObjectStorageBase
{
protected:
    ~ObjectStorageBase()
    {
        if (storage_) {
            instance().destroy_metadata_container(storage_);
        }
    }

    void ensure_storage(const ClassInfo& info, IInterface* owner) const
    {
        if (!storage_) {
            storage_ = instance().create_metadata_container(info, owner);
        }
    }

    IProperty::Ptr storage_get_property(string_view name, Resolve mode = Resolve::Create) const
    {
        return storage_ ? storage_->get_property(name, mode) : nullptr;
    }
    IEvent::Ptr storage_get_event(string_view name, Resolve mode = Resolve::Create) const
    {
        return storage_ ? storage_->get_event(name, mode) : nullptr;
    }
    IFunction::Ptr storage_get_function(string_view name, Resolve mode = Resolve::Create) const
    {
        return storage_ ? storage_->get_function(name, mode) : nullptr;
    }
    void storage_notify(MemberKind kind, Uid interfaceUid, Notification notification) const
    {
        if (storage_) {
            storage_->notify(kind, interfaceUid, notification);
        }
    }

    ReturnValue storage_add_attachment(const IInterface::Ptr& attachment) const
    {
        return storage_ ? storage_->add_attachment(attachment) : ReturnValue::Fail;
    }
    ReturnValue storage_remove_attachment(const IInterface::Ptr& attachment) const
    {
        return storage_ ? storage_->remove_attachment(attachment) : ReturnValue::Fail;
    }
    size_t storage_attachment_count() const { return storage_ ? storage_->attachment_count() : 0; }
    IInterface::Ptr storage_get_attachment(size_t index) const
    {
        return storage_ ? storage_->get_attachment(index) : nullptr;
    }
    IInterface::Ptr storage_find_attachment(const AttachmentQuery& query, Resolve mode)
    {
        return storage_ ? storage_->find_attachment(query, mode) : nullptr;
    }

    mutable IObjectStorage* storage_{};
};

} // namespace velk::detail

namespace velk::ext {

/**
 * @brief CRTP base for Velk objects with metadata and object storage.
 *
 * Extends ObjectCore with IObjectStorage support. Metadata is automatically collected
 * from all Interfaces that declare metadata through VELK_INTERFACE. Attachments can be
 * added/removed at runtime via the IObjectStorage interface.
 * The ObjectStorage is created lazily on first runtime metadata/attachment access.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template <class FinalClass, class... Interfaces>
class Object : public ObjectCore<FinalClass, IObjectStorage, Interfaces...>,
               protected detail::ObjectStorageBase
{
public:
    /** @brief Compile-time collected metadata from all Interfaces. */
    static constexpr auto metadata = CollectedMetadata<Interfaces...>::value;
    static constexpr array_view<MemberDesc> class_metadata{metadata.data(), metadata.size()};

    Object() = default;
    ~Object() override = default;

private:
    void ensure_stor() const
    {
        ensure_storage(
            FinalClass::get_factory().get_class_info(),
            static_cast<IInterface*>(const_cast<IObjectStorage*>(static_cast<const IObjectStorage*>(this))));
    }

public: // IMetadata overrides
    array_view<MemberDesc> get_static_metadata() const override { return class_metadata; }
    IProperty::Ptr get_property(string_view name, Resolve mode = Resolve::Create) const override
    {
        if (mode == Resolve::Existing && !storage_) {
            return {};
        }
        ensure_stor();
        return storage_get_property(name, mode);
    }
    IEvent::Ptr get_event(string_view name, Resolve mode = Resolve::Create) const override
    {
        if (mode == Resolve::Existing && !storage_) {
            return {};
        }
        ensure_stor();
        return storage_get_event(name, mode);
    }
    IFunction::Ptr get_function(string_view name, Resolve mode = Resolve::Create) const override
    {
        if (mode == Resolve::Existing && !storage_) {
            return {};
        }
        ensure_stor();
        return storage_get_function(name, mode);
    }
    void notify(MemberKind kind, Uid interfaceUid, Notification notification) const override
    {
        // No need to ensure storage. If container has not been initialized there won't be anything
        // to notify either.
        storage_notify(kind, interfaceUid, notification);
    }

public: // IObjectStorage overrides
    ReturnValue add_attachment(const IInterface::Ptr& attachment) override
    {
        ensure_stor();
        return storage_add_attachment(attachment);
    }
    ReturnValue remove_attachment(const IInterface::Ptr& attachment) override
    {
        ensure_stor();
        return storage_remove_attachment(attachment);
    }
    size_t attachment_count() const override { return storage_attachment_count(); }
    IInterface::Ptr get_attachment(size_t index) const override { return storage_get_attachment(index); }
    IInterface::Ptr find_attachment(const AttachmentQuery& query, Resolve mode) override
    {
        if (mode == Resolve::Existing && !storage_) {
            return {};
        }
        ensure_stor();
        return storage_find_attachment(query, mode);
    }

public: // IPropertyState override
    /** @brief Returns a pointer to the State struct for the given interface UID. */
    void* get_property_state(Uid uid) override { return find_state<0>(uid); }

public:
    /** @brief Returns the singleton factory for creating instances of FinalClass (with metadata). */
    static const IObjectFactory& get_factory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo& get_class_info() const override
        {
            static constexpr ClassInfo info{FinalClass::class_id(),
                                            FinalClass::class_name(),
                                            FinalClass::class_interfaces,
                                            FinalClass::class_metadata};
            return info;
        }
    };

    // Heterogeneous storage for each interface's State struct (e.g. IMyWidget::State,
    // ISerializable::State). Requires std::tuple because each State is a different type
    // with its own size, alignment, and constructor/destructor. This is safe across the
    // DLL boundary: the tuple lives inline in the consumer-compiled Object<T> template
    // and is never passed to or interpreted by the DLL.
    std::tuple<typename InterfaceState<Interfaces>::type...> states_;

    template <size_t I>
    void* find_state(Uid uid)
    {
        if constexpr (I < sizeof...(Interfaces)) {
            using Intf = std::tuple_element_t<I, std::tuple<Interfaces...>>;
            if (Intf::UID == uid) {
                return &std::get<I>(states_);
            }
            return find_state<I + 1>(uid);
        } else {
            return nullptr;
        }
    }
};

} // namespace velk::ext

#endif // VELK_EXT_OBJECT_H
