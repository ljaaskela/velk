#ifndef INTF_METADATA_H
#define INTF_METADATA_H

#include <api/property.h>
#include <array_view.h>
#include <common.h>
#include <interface/intf_event.h>
#include <interface/intf_interface.h>
#include <interface/intf_property.h>

#include <cstdint>
#include <string_view>

namespace strata {

/** @brief Discriminator for the kind of member described by a MemberDesc. */
enum class MemberKind : uint8_t { Property, Event, Function };

/** @brief Describes a single member (property, event, or function) declared by an object class. */
struct MemberDesc {
    std::string_view name;
    MemberKind kind;
    Uid typeUid;                        // only meaningful for Property
    const InterfaceInfo* interfaceInfo; // interface where this member was declared
};

/** @brief Creates a Property MemberDesc for type T. */
template<class T>
constexpr MemberDesc PropertyDesc(std::string_view name, const InterfaceInfo* info = nullptr)
{
    return {name, MemberKind::Property, TypeUid<T>(), info};
}

/** @brief Creates an Event MemberDesc. */
constexpr MemberDesc EventDesc(std::string_view name, const InterfaceInfo* info = nullptr)
{
    return {name, MemberKind::Event, 0, info};
}

/** @brief Creates a Function MemberDesc. */
constexpr MemberDesc FunctionDesc(std::string_view name, const InterfaceInfo* info = nullptr)
{
    return {name, MemberKind::Function, 0, info};
}

/** @brief Interface for querying object metadata: static member descriptors and runtime instances. */
class IMetadata : public Interface<IMetadata>
{
public:
    /** @brief Returns the static metadata descriptors for this object's class. */
    virtual array_view<MemberDesc> GetStaticMetadata() const = 0;

    /** @brief Returns the runtime property instance for the named member, or nullptr. */
    virtual IProperty::Ptr GetProperty(std::string_view name) const = 0;
    /** @brief Returns the runtime event instance for the named member, or nullptr. */
    virtual IEvent::Ptr GetEvent(std::string_view name) const = 0;
    /** @brief Returns the runtime function instance for the named member, or nullptr. */
    virtual IFunction::Ptr GetFunction(std::string_view name) const = 0;
};

/**
 * @brief Abstract interface for runtime metadata storage.
 */
class IMetadataContainer : public Interface<IMetadataContainer>
{
public:
    /**
     * @brief Set metadata container. Called internally by the library.
     */
    virtual void SetMetadataContainer(IMetadata *metadata) = 0;
};

} // namespace strata

// --- Preprocessor FOR_EACH machinery ---

#define _STRATA_EXPAND(...) __VA_ARGS__
#define _STRATA_CAT(a, b) _STRATA_CAT_(a, b)
#define _STRATA_CAT_(a, b) a##b

// Argument counting (supports 1..32)
#define _STRATA_NARG(...) \
    _STRATA_EXPAND(_STRATA_NARG_IMPL(__VA_ARGS__, \
    32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17, \
    16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))
#define _STRATA_NARG_IMPL( \
    _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16, \
    _17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,N,...) N

// Apply dispatch macro M to parenthesized args: _STRATA_APPLY(M, (a,b,c)) -> M(a,b,c)
#define _STRATA_APPLY(M, args) _STRATA_EXPAND(M args)

// FOR_EACH unrolling (1..32)
#define _STRATA_FE_1(M, x)       _STRATA_APPLY(M, x)
#define _STRATA_FE_2(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_1(M, __VA_ARGS__))
#define _STRATA_FE_3(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_2(M, __VA_ARGS__))
#define _STRATA_FE_4(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_3(M, __VA_ARGS__))
#define _STRATA_FE_5(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_4(M, __VA_ARGS__))
#define _STRATA_FE_6(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_5(M, __VA_ARGS__))
#define _STRATA_FE_7(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_6(M, __VA_ARGS__))
#define _STRATA_FE_8(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_7(M, __VA_ARGS__))
#define _STRATA_FE_9(M, x, ...)  _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_8(M, __VA_ARGS__))
#define _STRATA_FE_10(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_9(M, __VA_ARGS__))
#define _STRATA_FE_11(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_10(M, __VA_ARGS__))
#define _STRATA_FE_12(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_11(M, __VA_ARGS__))
#define _STRATA_FE_13(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_12(M, __VA_ARGS__))
#define _STRATA_FE_14(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_13(M, __VA_ARGS__))
#define _STRATA_FE_15(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_14(M, __VA_ARGS__))
#define _STRATA_FE_16(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_15(M, __VA_ARGS__))
#define _STRATA_FE_17(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_16(M, __VA_ARGS__))
#define _STRATA_FE_18(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_17(M, __VA_ARGS__))
#define _STRATA_FE_19(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_18(M, __VA_ARGS__))
#define _STRATA_FE_20(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_19(M, __VA_ARGS__))
#define _STRATA_FE_21(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_20(M, __VA_ARGS__))
#define _STRATA_FE_22(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_21(M, __VA_ARGS__))
#define _STRATA_FE_23(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_22(M, __VA_ARGS__))
#define _STRATA_FE_24(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_23(M, __VA_ARGS__))
#define _STRATA_FE_25(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_24(M, __VA_ARGS__))
#define _STRATA_FE_26(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_25(M, __VA_ARGS__))
#define _STRATA_FE_27(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_26(M, __VA_ARGS__))
#define _STRATA_FE_28(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_27(M, __VA_ARGS__))
#define _STRATA_FE_29(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_28(M, __VA_ARGS__))
#define _STRATA_FE_30(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_29(M, __VA_ARGS__))
#define _STRATA_FE_31(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_30(M, __VA_ARGS__))
#define _STRATA_FE_32(M, x, ...) _STRATA_APPLY(M, x) _STRATA_EXPAND(_STRATA_FE_31(M, __VA_ARGS__))

#define _STRATA_FOR_EACH(M, ...) \
    _STRATA_EXPAND(_STRATA_CAT(_STRATA_FE_, _STRATA_NARG(__VA_ARGS__))(M, __VA_ARGS__))

// --- Metadata dispatch: tag -> MemberDesc initializer ---

#define _STRATA_META_PROP(Type, Name) ::strata::PropertyDesc<Type>(#Name, &INFO),
#define _STRATA_META_EVT(Name)        ::strata::EventDesc(#Name, &INFO),
#define _STRATA_META_FN(Name)         ::strata::FunctionDesc(#Name, &INFO),
#define _STRATA_META(Tag, ...)        _STRATA_EXPAND(_STRATA_CAT(_STRATA_META_, Tag)(__VA_ARGS__))

/** @brief Generates a static constexpr metadata array from STRATA_P/STRATA_E/STRATA_F entries. */
#define STRATA_METADATA(...) \
    static constexpr std::array metadata = { _STRATA_FOR_EACH(_STRATA_META, __VA_ARGS__) };

// --- Accessor dispatch: tag -> typed non-virtual accessor method ---

#define _STRATA_ACC_PROP(Type, Name) \
    ::strata::PropertyT<Type> Name() const { \
        auto* meta_ = this->template GetInterface<::strata::IMetadata>(); \
        return ::strata::PropertyT<Type>(meta_ ? meta_->GetProperty(#Name) : nullptr); \
    }
#define _STRATA_ACC_EVT(Name) \
    ::strata::IEvent::Ptr Name() const { \
        auto* meta_ = this->template GetInterface<::strata::IMetadata>(); \
        return meta_ ? meta_->GetEvent(#Name) : nullptr; \
    }
#define _STRATA_ACC_FN(Name) \
    ::strata::IFunction::Ptr Name() const { \
        auto* meta_ = this->template GetInterface<::strata::IMetadata>(); \
        return meta_ ? meta_->GetFunction(#Name) : nullptr; \
    }
#define _STRATA_ACC(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_ACC_, Tag)(__VA_ARGS__))

/**
 * @brief Declares interface members: generates both a static constexpr metadata
 *        array and typed accessor methods from a list of member descriptors.
 *
 * Place this macro in the @c public section of an interface class that inherits
 * from @c Interface<T>. Each argument is a parenthesized tuple describing one
 * member. Three member kinds are supported:
 *
 * | Kind     | Syntax                    | Description                        |
 * |----------|---------------------------|------------------------------------|
 * | Property | @c (PROP, Type, Name)     | A typed property with Get/Set      |
 * | Event    | @c (EVT, Name)            | An observable event                |
 * | Function | @c (FN, Name)             | A callable function slot           |
 *
 * @par What the macro generates
 * For each member entry the macro produces:
 * -# A @c MemberDesc initializer in a @c static @c constexpr @c std::array
 *    named @c metadata, used for compile-time and runtime introspection.
 * -# A non-virtual @c const accessor method on the interface:
 *    - @c PROP &rarr; <tt>PropertyT\<Type\> Name() const</tt>
 *    - @c EVT  &rarr; <tt>IEvent::Ptr Name() const</tt>
 *    - @c FN   &rarr; <tt>IFunction::Ptr Name() const</tt>
 *
 *    Each accessor obtains the runtime instance by querying the object's
 *    @c IMetadata interface, so it works on any @c Object that implements
 *    this interface.
 *
 * @par Example: defining an interface
 * @code
 * class IMyWidget : public Interface<IMyWidget>
 * {
 * public:
 *     STRATA_INTERFACE(
 *         (PROP, float, Width),
 *         (PROP, float, Height),
 *         (EVT, OnClicked),
 *         (FN, Reset)
 *     )
 * };
 * @endcode
 *
 * @par Example: implementing the interface
 * Concrete classes inherit from @c Object, which automatically collects
 * metadata from all listed interfaces and provides the @c IMetadata
 * implementation. No additional code is needed in the concrete class.
 * @code
 * class MyWidget : public Object<MyWidget, IMyWidget> {};
 * @endcode
 *
 * @par Example: using accessors on an instance
 * @code
 * auto widget = Strata().Create<IObject>(MyWidget::GetClassUid());
 * if (auto* iw = interface_cast<IMyWidget>(widget)) {
 *     iw->Width().Set(42.f);
 *     float w = iw->Width().Get();   // 42.f
 *     IEvent::Ptr clicked = iw->OnClicked();
 *     IFunction::Ptr reset = iw->Reset();
 * }
 * @endcode
 *
 * @par Example: querying static metadata without an instance
 * @code
 * if (auto* info = Strata().GetClassInfo(MyWidget::GetClassUid())) {
 *     for (auto& m : info->members) {
 *         // m.name, m.kind, m.typeUid
 *     }
 * }
 * @endcode
 *
 * @note Up to 32 members are supported per interface.
 *
 * @see STRATA_METADATA For generating only the metadata array without accessors.
 * @see Object       For the CRTP base that collects metadata from interfaces.
 * @see IMetadata     For the runtime metadata query interface.
 */
#define STRATA_INTERFACE(...) \
    STRATA_METADATA(__VA_ARGS__) \
    _STRATA_FOR_EACH(_STRATA_ACC, __VA_ARGS__)

#endif // INTF_METADATA_H
