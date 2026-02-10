#ifndef EXT_OBJECT_H
#define EXT_OBJECT_H

#include <api/property.h>
#include <ext/metadata.h>
#include <ext/core_object.h>

namespace strata {

/**
 * @brief CRTP base for Strata objects with metadata.
 *
 * Extends CoreObject with IMetadata support. Metadata is automatically collected
 * from all Interfaces that declare metadata through STRATA_INTERFACE.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template<class FinalClass, class... Interfaces>
class Object : public CoreObject<FinalClass, IMetadata, IMetadataContainer, Interfaces...>
{
public:
    /** @brief Collected metadata from all Interfaces. */
    static constexpr auto metadata = collected_metadata<Interfaces...>::value;

    Object() = default;
    ~Object() override = default;

public: // IMetadata
    array_view<MemberDesc> GetStaticMetadata() const override
    {
        return meta_ ? meta_->GetStaticMetadata() : array_view<MemberDesc>{};
    }
    IProperty::Ptr GetProperty(std::string_view name) const override
    {
        return meta_ ? meta_->GetProperty(name) : nullptr;
    }
    IEvent::Ptr GetEvent(std::string_view name) const override
    {
        return meta_ ? meta_->GetEvent(name) : nullptr;
    }
    IFunction::Ptr GetFunction(std::string_view name) const override
    {
        return meta_ ? meta_->GetFunction(name) : nullptr;
    }

public: // IMetadataContainer
    void SetMetadataContainer(IMetadata *metadata) override
    {
        // Allow one set (called by Strata at construction)
        if (!meta_) {
            meta_.reset(metadata);
        }
    }

public:
    static const IObjectFactory &GetFactory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo &GetClassInfo() const override
        {
            static constexpr ClassInfo info{
                FinalClass::GetClassUid(),
                FinalClass::GetClassName(),
                {FinalClass::metadata.data(), FinalClass::metadata.size()}
            };
            return info;
        }
    };

    refcnt_ptr<IMetadata> meta_;
};

} // namespace strata

#endif // EXT_OBJECT_H
