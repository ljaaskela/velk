#ifndef VELK_EXT_OBJECT_H
#define VELK_EXT_OBJECT_H

#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/ext/metadata.h>
#include <velk/interface/intf_object_storage.h>

#include <atomic>
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
private:
    template <class Fn>
    auto with_storage(Fn&& fn) const
    {
        auto* s = stor();
        using F = decltype(fn(*s));
        if constexpr (std::is_void_v<F>) {
            if (s) {
                fn(*s);
            }
        } else {
            return s ? fn(*s) : F{};
        }
    }

    template <class Fn, class R>
    auto with_storage(Fn&& fn, R error) const
    {
        auto* s = stor();
        return s ? fn(*s) : error;
    }

protected:
    ~ObjectStorageBase()
    {
        auto* s = storage_.load(std::memory_order_relaxed);
        if (s) {
            instance().destroy_metadata_container(s);
        }
    }

    void ensure_storage(const ClassInfo& info, IInterface* owner, IMetadataObserver* observer) const
    {
        if (storage_.load(std::memory_order_acquire)) {
            return;
        }
        auto* created = instance().create_metadata_container(info, owner);
        IObjectStorage* expected = nullptr;
        if (!storage_.compare_exchange_strong(expected, created, std::memory_order_release)) {
            // Another thread won the race; destroy our duplicate.
            instance().destroy_metadata_container(created);
        } else if (observer) {
            // This object owns the storage and lifetime of both is the same, no remove_observer needed
            created->add_observer(observer);
        }
    }
    IProperty::Ptr storage_get_property(string_view name, Uid interfaceUid, Resolve mode) const
    {
        return with_storage([&](auto& s) { return s.get_property(name, interfaceUid, mode); });
    }
    IEvent::Ptr storage_get_event(string_view name, Resolve mode = Resolve::Create) const
    {
        return with_storage([&](auto& s) { return s.get_event(name, mode); });
    }
    IFunction::Ptr storage_get_function(string_view name, Resolve mode = Resolve::Create) const
    {
        return with_storage([&](auto& s) { return s.get_function(name, mode); });
    }
    void storage_notify(MemberKind kind, Uid interfaceUid, Notification notification) const
    {
        return with_storage([&](auto& s) { return s.notify(kind, interfaceUid, notification); });
    }
    IProperty::Ptr storage_add_property(string_view name, Uid typeUid, const IAny* defaultValue) const
    {
        return with_storage([&](auto& s) { return s.add_property(name, typeUid, defaultValue); });
    }
    ReturnValue storage_remove_property(string_view name) const
    {
        return with_storage([&](auto& s) { return s.remove_property(name); }, ReturnValue::Fail);
    }
    vector<PropertyInfo> storage_get_properties() const
    {
        return with_storage([&](auto& s) { return s.get_properties(); }, vector<PropertyInfo>{});
    }
    ReturnValue storage_add_attachment(const IInterface::Ptr& attachment) const
    {
        return with_storage([&](auto& s) { return s.add_attachment(attachment); }, ReturnValue::Fail);
    }
    ReturnValue storage_remove_attachment(const IInterface::Ptr& attachment) const
    {
        return with_storage([&](auto& s) { return s.remove_attachment(attachment); }, ReturnValue::Fail);
    }
    size_t storage_attachment_count() const
    {
        return with_storage([&](auto& s) { return s.attachment_count(); }, 0);
    }
    IInterface::Ptr storage_get_attachment(size_t index) const
    {
        return with_storage([&](auto& s) { return s.get_attachment(index); });
    }
    IInterface::Ptr storage_find_attachment(const AttachmentQuery& query, Resolve mode)
    {
        return with_storage([&](auto& s) { return s.find_attachment(query, mode); });
    }
    vector<IInterface::Ptr> storage_find_attachments(const AttachmentQuery& query) const
    {
        return with_storage([&](auto& s) { return s.find_attachments(query); });
    }
    IEvent::Ptr storage_get_property_event(size_t storage_id, Resolve mode) const
    {
        return with_storage([&](auto& s) { return s.get_property_event(storage_id, mode); });
    }
    void storage_invoke_property_changed(size_t storage_id, IProperty* property) const
    {
        with_storage([&](auto& s) { s.invoke_property_changed(storage_id, property); });
    }
    void storage_mark_binding_stale(size_t storage_id) const
    {
        with_storage([&](auto& s) { s.mark_binding_stale(storage_id); });
    }
    bool storage_refresh_stale_bindings(Uid interfaceUid) const
    {
        return with_storage([&](auto& s) { return s.refresh_stale_bindings(interfaceUid); }, false);
    }
    void storage_add_observer(IMetadataObserver* observer)
    {
        with_storage([&](auto& s) { s.add_observer(observer); });
    }
    void storage_remove_observer(IMetadataObserver* observer)
    {
        with_storage([&](auto& s) { s.remove_observer(observer); });
    }

private:
    IObjectStorage* stor() const { return storage_.load(std::memory_order_acquire); }
    mutable std::atomic<IObjectStorage*> storage_{};
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
    void ensure_object_storage(Resolve mode = Resolve::Create) const
    {
        if (mode == Resolve::Existing) {
            return;
        }
        static const auto& ci = FinalClass::get_factory().get_class_info();
        auto* me =
            static_cast<IInterface*>(const_cast<IObjectStorage*>(static_cast<const IObjectStorage*>(this)));
        if constexpr (std::is_base_of_v<IMetadataObserver, FinalClass>) {
            IMetadataObserver* observer =
                const_cast<IMetadataObserver*>(static_cast<const IMetadataObserver*>(this));
            ensure_storage(ci, me, observer);
        } else {
            ensure_storage(ci, me, nullptr);
        }
    }

public: // IMetadata overrides
    array_view<MemberDesc> get_static_metadata() const override { return class_metadata; }
    IProperty::Ptr get_property(string_view name, Uid interfaceUid, Resolve mode) const override
    {
        ensure_object_storage(mode);
        return storage_get_property(name, interfaceUid, mode);
    }
    IEvent::Ptr get_event(string_view name, Resolve mode = Resolve::Create) const override
    {
        ensure_object_storage(mode);
        return storage_get_event(name, mode);
    }
    IFunction::Ptr get_function(string_view name, Resolve mode = Resolve::Create) const override
    {
        ensure_object_storage(mode);
        return storage_get_function(name, mode);
    }
    void notify(MemberKind kind, Uid interfaceUid, Notification notification) const override
    {
        if constexpr (std::is_base_of_v<IMetadataObserver, FinalClass>) {
            ensure_object_storage();
        }
        storage_notify(kind, interfaceUid, notification);
    }
    IProperty::Ptr add_property(string_view name, Uid typeUid, const IAny* defaultValue) const override
    {
        ensure_object_storage();
        return storage_add_property(name, typeUid, defaultValue);
    }
    ReturnValue remove_property(string_view name) const override { return storage_remove_property(name); }
    vector<PropertyInfo> get_properties() const override { return storage_get_properties(); }

public: // IObjectStorage overrides
    ReturnValue add_attachment(const IInterface::Ptr& attachment) override
    {
        ensure_object_storage();
        return storage_add_attachment(attachment);
    }
    ReturnValue remove_attachment(const IInterface::Ptr& attachment) override
    {
        // No need to ensure storage, if there's none there can't be an attachment to remove either
        return storage_remove_attachment(attachment);
    }
    size_t attachment_count() const override { return storage_attachment_count(); }
    IInterface::Ptr get_attachment(size_t index) const override { return storage_get_attachment(index); }
    IInterface::Ptr find_attachment(const AttachmentQuery& query, Resolve mode) override
    {
        ensure_object_storage(mode);
        return storage_find_attachment(query, mode);
    }
    vector<IInterface::Ptr> find_attachments(const AttachmentQuery& query) const override
    {
        return storage_find_attachments(query);
    }
    IEvent::Ptr get_property_event(size_t storage_id, Resolve mode) const override
    {
        return storage_get_property_event(storage_id, mode);
    }
    void invoke_property_changed(size_t storage_id, IProperty* property) const override
    {
        storage_invoke_property_changed(storage_id, property);
    }
    void mark_binding_stale(size_t storage_id) override
    {
        this->get_object_data().flags |= ObjectFlags::StaleBindings;
        storage_mark_binding_stale(storage_id);
    }
    bool refresh_stale_bindings(Uid interfaceUid) override
    {
        const bool remaining = storage_refresh_stale_bindings(interfaceUid);
        if (!remaining) {
            this->get_object_data().flags &= ~ObjectFlags::StaleBindings;
        }
        return remaining;
    }
    void add_observer(IMetadataObserver* observer) override
    {
        ensure_object_storage();
        storage_add_observer(observer);
    }
    void remove_observer(IMetadataObserver* observer) override { storage_remove_observer(observer); }

public: // IPropertyState override
    /** @brief Returns a pointer to the State struct for the given interface UID. */
    void* get_property_state(Uid uid) override
    {
        // A bound property whose source changed is copied into the state when it is read.
        if (this->get_object_data().flags & ObjectFlags::StaleBindings) {
            refresh_stale_bindings(uid);
        }
        return find_state<0>(uid);
    }

    /** @brief Type-safe state access. Returns a typed pointer to T::State for a VELK_INTERFACE. */
    template <class T>
    typename T::State* interface_state()
    {
        return static_cast<typename T::State*>(get_property_state(T::UID));
    }
    /** @brief Const type-safe state access. Safe because state is always mutable storage owned by the object.
     */
    template <class T>
    const typename T::State* interface_state() const
    {
        return const_cast<Object*>(this)->interface_state<T>();
    }

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
            static constexpr ClassInfo info{FinalClass::static_class_id(),
                                            FinalClass::static_class_name(),
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
