#ifndef EXT_OBJECT_H
#define EXT_OBJECT_H

#include <api/property.h>
#include <ext/metadata.h>
#include <ext/core_object.h>

#include <tuple>

namespace strata::ext {

/**
 * @brief CRTP base for Strata objects with metadata.
 *
 * Extends ObjectCore with IMetadata support. Metadata is automatically collected
 * from all Interfaces that declare metadata through STRATA_INTERFACE.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template<class FinalClass, class... Interfaces>
class Object : public ObjectCore<FinalClass, IMetadata, IMetadataContainer, Interfaces...>
{
public:
    /** @brief Compile-time collected metadata from all Interfaces. */
    static constexpr auto metadata = CollectedMetadata<Interfaces...>::value;

    Object() = default;
    ~Object() override = default;

public: // IMetadata overrides
    /** @brief Returns the static metadata descriptors, or an empty view if none. */
    array_view<MemberDesc> get_static_metadata() const override
    {
        return meta_ ? meta_->get_static_metadata() : array_view<MemberDesc>{};
    }
    /** @brief Looks up a property by name, or returns nullptr. */
    IProperty::Ptr get_property(std::string_view name) const override
    {
        return meta_ ? meta_->get_property(name) : nullptr;
    }
    /** @brief Looks up an event by name, or returns nullptr. */
    IEvent::Ptr get_event(std::string_view name) const override
    {
        return meta_ ? meta_->get_event(name) : nullptr;
    }
    /** @brief Looks up a function by name, or returns nullptr. */
    IFunction::Ptr get_function(std::string_view name) const override
    {
        return meta_ ? meta_->get_function(name) : nullptr;
    }

public: // IMetadataContainer override
    /** @brief Accepts the runtime metadata container (called once by Strata at construction). */
    void set_metadata_container(IMetadata *metadata) override
    {
        // Allow one set (called by Strata at construction)
        if (!meta_) {
            meta_.reset(metadata);
        }
    }

public: // IPropertyState override
    /** @brief Returns a pointer to the State struct for the given interface UID. */
    void *get_property_state(Uid uid) override { return find_state<0>(uid); }

public:
    /** @brief Returns the singleton factory for creating instances of FinalClass (with metadata). */
    static const IObjectFactory &get_factory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo &get_class_info() const override
        {
            static constexpr ClassInfo info{
                FinalClass::get_class_uid(),
                FinalClass::get_class_name(),
                {FinalClass::metadata.data(), FinalClass::metadata.size()}
            };
            return info;
        }
    };

    std::unique_ptr<IMetadata> meta_;
    std::tuple<typename InterfaceState<Interfaces>::type...> states_;

    template<size_t I>
    void *find_state(Uid uid)
    {
        if constexpr (I < sizeof...(Interfaces)) {
            using Intf = std::tuple_element_t<I, std::tuple<Interfaces...>>;
            if (Intf::UID == uid) return &std::get<I>(states_);
            return find_state<I + 1>(uid);
        } else {
            return nullptr;
        }
    }
};

} // namespace strata::ext

#endif // EXT_OBJECT_H
