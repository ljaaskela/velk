#ifndef VELK_EXT_OBJECT_H
#define VELK_EXT_OBJECT_H

#include <velk/api/property.h>
#include <velk/ext/core_object.h>
#include <velk/ext/metadata.h>

#include <tuple>

namespace velk::detail {

/**
 * @brief Non-template base holding IMetadata pointer and delegation helpers.
 *
 * Avoids duplicating the metadata delegation logic in every Object<> instantiation.
 */
class ObjectMetadataBase
{
protected:
    ~ObjectMetadataBase() { delete meta_; }

    array_view<MemberDesc> meta_get_static_metadata() const
    {
        return meta_ ? meta_->get_static_metadata() : array_view<MemberDesc>{};
    }
    IProperty::Ptr meta_get_property(string_view name) const
    {
        return meta_ ? meta_->get_property(name) : nullptr;
    }
    IEvent::Ptr meta_get_event(string_view name) const { return meta_ ? meta_->get_event(name) : nullptr; }
    IFunction::Ptr meta_get_function(string_view name) const
    {
        return meta_ ? meta_->get_function(name) : nullptr;
    }
    void meta_notify(MemberKind kind, Uid interfaceUid, Notification notification) const
    {
        if (meta_) {
            meta_->notify(kind, interfaceUid, notification);
        }
    }
    void meta_set_container(IMetadata* metadata)
    {
        if (!meta_) {
            meta_ = metadata;
        }
    }

    IMetadata* meta_{};
};

} // namespace velk::detail

namespace velk::ext {

/**
 * @brief CRTP base for Velk objects with metadata.
 *
 * Extends ObjectCore with IMetadata support. Metadata is automatically collected
 * from all Interfaces that declare metadata through VELK_INTERFACE.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template <class FinalClass, class... Interfaces>
class Object : public ObjectCore<FinalClass, IMetadataContainer, Interfaces...>,
               protected detail::ObjectMetadataBase
{
public:
    /** @brief Compile-time collected metadata from all Interfaces. */
    static constexpr auto metadata = CollectedMetadata<Interfaces...>::value;
    static constexpr array_view<MemberDesc> class_metadata{metadata.data(), metadata.size()};

    Object() = default;
    ~Object() override = default;

public: // IMetadata overrides
    array_view<MemberDesc> get_static_metadata() const override { return meta_get_static_metadata(); }
    IProperty::Ptr get_property(string_view name) const override { return meta_get_property(name); }
    IEvent::Ptr get_event(string_view name) const override { return meta_get_event(name); }
    IFunction::Ptr get_function(string_view name) const override { return meta_get_function(name); }
    void notify(MemberKind kind, Uid interfaceUid, Notification notification) const override
    {
        meta_notify(kind, interfaceUid, notification);
    }

public: // IMetadataContainer override
    void set_metadata_container(IMetadata* metadata) override { meta_set_container(metadata); }

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
