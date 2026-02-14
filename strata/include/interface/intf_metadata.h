#ifndef INTF_METADATA_H
#define INTF_METADATA_H

#include <api/property.h>
#include <array_view.h>
#include <common.h>
#include <ext/any.h>
#include <interface/intf_event.h>
#include <interface/intf_interface.h>
#include <interface/intf_property.h>

#include <cstdint>
#include <string_view>

namespace strata {

/** @brief Discriminator for the kind of member described by a MemberDesc. */
enum class MemberKind : uint8_t { Property, Event, Function };

/** @brief Function pointer type for trampoline callbacks that route to virtual methods. */
using FnTrampoline = ReturnValue(*)(void* self, const IAny* args);

/** @brief Kind-specific data for Property members. */
struct PropertyKind {
    const IAny*(*getDefault)() = nullptr;
    IAny::Ptr(*createRef)(void* stateBase) = nullptr;
};

/** @brief Kind-specific data for Function/Event members. */
struct FunctionKind {
    FnTrampoline trampoline = nullptr;
};

/** @brief Describes a single member (property, event, or function) declared by an object class. */
struct MemberDesc {
    std::string_view name;
    MemberKind kind;
    Uid typeUid;                        // only meaningful for Property
    const InterfaceInfo* interfaceInfo; // interface where this member was declared
    const void* ext = nullptr;          // points to PropertyKind or FunctionKind based on kind

    /// Typed ext member getter for MemberDesc.kind = MemberKind::Property
    constexpr const PropertyKind *propertyKind() const
    {
        return kind == MemberKind::Property ? static_cast<const PropertyKind*>(ext) : nullptr;
    }
    /// Typed ext member getter for MemberDesc.kind = MemberKind::Function or MemberDesc.kind = MemberKind::Event
    constexpr const FunctionKind *functionKind() const
    {
        return (kind == MemberKind::Function || kind == MemberKind::Event)
            ? static_cast<const FunctionKind*>(ext) : nullptr;
    }
};

/** @brief Creates a Property MemberDesc for type T. */
template<class T>
constexpr MemberDesc PropertyDesc(std::string_view name, const InterfaceInfo* info = nullptr,
    const PropertyKind* pk = nullptr)
{
    return {name, MemberKind::Property, type_uid<T>(), info, pk};
}

/** @brief Retrieves the typed default value from a MemberDesc. */
template<class T>
T get_default_value(const MemberDesc& desc) {
    T value{};
    const auto *pk = desc.propertyKind();
    if (pk && pk->getDefault) {
        if (const auto *any = pk->getDefault()) {
            any->get_data(&value, sizeof(T), type_uid<T>());
        }
    }
    return value;
}

/** @brief Creates an Event MemberDesc. */
constexpr MemberDesc EventDesc(std::string_view name, const InterfaceInfo* info = nullptr)
{
    return {name, MemberKind::Event, {}, info};
}

/** @brief Creates a Function MemberDesc. */
constexpr MemberDesc FunctionDesc(std::string_view name, const InterfaceInfo* info = nullptr,
    const FunctionKind* fk = nullptr)
{
    return {name, MemberKind::Function, {}, info, fk};
}

/**
 * @brief Interface for accessing per-interface property state structs.
 *
 * Provides contiguous, state-struct-backed property storage. MetadataContainer
 * uses this to create AnyRef instances that read/write directly into the state struct.
 */
class IPropertyState : public Interface<IPropertyState>
{
public:
    /** @brief Returns a pointer to the State struct for the given interface UID, or nullptr. */
    virtual void *get_property_state(Uid interfaceUid) = 0;

    /**
     * @brief Type-safe state access. Returns a typed pointer to T::State.
     * @tparam T The interface type whose State struct to retrieve.
     */
    template<class T>
    typename T::State *get_property_state()
    {
        return static_cast<typename T::State *>(get_property_state(T::UID));
    }
};

/** @brief Interface for querying object metadata: static member descriptors and runtime instances. */
class IMetadata : public Interface<IMetadata, IPropertyState>
{
public:
    /** @brief Returns the static metadata descriptors for this object's class. */
    virtual array_view<MemberDesc> get_static_metadata() const = 0;

    /** @brief Returns the runtime property instance for the named member, or nullptr. */
    virtual IProperty::Ptr get_property(std::string_view name) const = 0;
    /** @brief Returns the runtime event instance for the named member, or nullptr. */
    virtual IEvent::Ptr get_event(std::string_view name) const = 0;
    /** @brief Returns the runtime function instance for the named member, or nullptr. */
    virtual IFunction::Ptr get_function(std::string_view name) const = 0;
};

/** @brief Null-safe property lookup on an IMetadata pointer. */
inline IProperty::Ptr get_property(const IMetadata* meta, std::string_view name)
{
    return meta ? meta->get_property(name) : nullptr;
}

/** @brief Null-safe event lookup on an IMetadata pointer. */
inline IEvent::Ptr get_event(const IMetadata* meta, std::string_view name)
{
    return meta ? meta->get_event(name) : nullptr;
}

/** @brief Null-safe function lookup on an IMetadata pointer. */
inline IFunction::Ptr get_function(const IMetadata* meta, std::string_view name)
{
    return meta ? meta->get_function(name) : nullptr;
}

/**
 * @brief Abstract interface for runtime metadata storage.
 */
class IMetadataContainer : public Interface<IMetadataContainer>
{
public:
    /**
     * @brief Set metadata container. Called internally by the library.
     */
    virtual void set_metadata_container(IMetadata *metadata) = 0;
};

/**
 * @brief Invoke a function from target object metadata.
 * @param o The object to query for the function.
 * @param name Name of the function to query.
 * @param args Function arguments.
 */
[[maybe_unused]] static ReturnValue invoke_function(const IInterface *o,
                                                    std::string_view name,
                                                    const IAny *args = nullptr)
{
    auto meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), args) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IInterface::Ptr &o,
                                                    std::string_view name,
                                                    const IAny *args = nullptr)
{
    return invoke_function(o.get(), name, args);
}

/** @copydoc invoke_function */
[[maybe_unused]] static ReturnValue invoke_function(const IInterface::ConstPtr &o,
                                                    std::string_view name,
                                                    const IAny *args = nullptr)
{
    return invoke_function(o.get(), name, args);
}

/**
 * @brief Invoke a event from target object metadata.
 * @param o The object to query for the event.
 * @param name Name of the event to query.
 * @param args Event arguments.
 */
[[maybe_unused]] static ReturnValue invoke_event(const IInterface::Ptr &o,
                                                 std::string_view name,
                                                 const IAny *args = nullptr)
{
    auto meta = interface_cast<IMetadata>(o);
    return meta ? invoke_event(meta->get_event(name), args) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_event */
[[maybe_unused]] static ReturnValue invoke_event(const IInterface::ConstPtr &o,
                                                 std::string_view name,
                                                 const IAny *args = nullptr)
{
    auto meta = interface_cast<IMetadata>(o);
    return meta ? invoke_event(meta->get_event(name), args) : ReturnValue::INVALID_ARGUMENT;
}

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

// --- State pass: generates State struct fields for PROP members ---

#define _STRATA_STATE_PROP(Type, Name, Default)  Type Name = Default;
#define _STRATA_STATE_EVT(Name)
#define _STRATA_STATE_FN(Name)
#define _STRATA_STATE(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_STATE_, Tag)(__VA_ARGS__))

// --- Defaults pass: generates kind-specific static data for each member ---

#define _STRATA_DEFAULTS_PROP(Type, Name, Default) \
    static const ::strata::IAny* _strata_getdefault_##Name() { \
        static ::strata::AnyValue<Type> a; \
        static const bool _init_ = (a.set_value(Default), true); \
        (void)_init_; \
        return &a; \
    } \
    static ::strata::IAny::Ptr _strata_createref_##Name(void* base) { \
        return ::strata::create_any_ref<Type>(&static_cast<State*>(base)->Name); \
    } \
    static constexpr ::strata::PropertyKind _strata_propkind_##Name { \
        &_strata_getdefault_##Name, &_strata_createref_##Name };
#define _STRATA_DEFAULTS_EVT(...)
#define _STRATA_DEFAULTS_FN(Name) \
    static constexpr ::strata::FunctionKind _strata_fnkind_##Name { &_strata_trampoline_##Name };
#define _STRATA_DEFAULTS(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_DEFAULTS_, Tag)(__VA_ARGS__))

// --- Metadata dispatch: tag -> MemberDesc initializer ---

#define _STRATA_META_PROP(Type, Name, ...) \
    ::strata::PropertyDesc<Type>(#Name, &INFO, &_strata_propkind_##Name),
#define _STRATA_META_EVT(Name) \
    ::strata::EventDesc(#Name, &INFO),
#define _STRATA_META_FN(Name) \
    ::strata::FunctionDesc(#Name, &INFO, &_strata_fnkind_##Name),
#define _STRATA_META(Tag, ...)        _STRATA_EXPAND(_STRATA_CAT(_STRATA_META_, Tag)(__VA_ARGS__))

// --- Trampoline dispatch: tag -> virtual method + static trampoline for FN, no-op for PROP/EVT ---

#define _STRATA_TRAMPOLINE_PROP(...)
#define _STRATA_TRAMPOLINE_EVT(Name)
#define _STRATA_TRAMPOLINE_FN(Name) \
    virtual ::strata::ReturnValue fn_##Name(const ::strata::IAny*) = 0; \
    static ::strata::ReturnValue _strata_trampoline_##Name(void* self, const ::strata::IAny* args) { \
        return static_cast<_strata_intf_type*>(self)->fn_##Name(args); \
    }
#define _STRATA_TRAMPOLINE(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_TRAMPOLINE_, Tag)(__VA_ARGS__))

/** @brief Generates default-value functions and a static constexpr metadata array. */
#define STRATA_METADATA(...) \
    struct State { _STRATA_FOR_EACH(_STRATA_STATE, __VA_ARGS__) }; \
    _STRATA_FOR_EACH(_STRATA_DEFAULTS, __VA_ARGS__) \
    static constexpr std::array metadata = { _STRATA_FOR_EACH(_STRATA_META, __VA_ARGS__) };

// --- Accessor dispatch: tag -> typed non-virtual accessor method ---

#define _STRATA_ACC_PROP(Type, Name, ...) \
    ::strata::PropertyT<Type> Name() const { \
        return ::strata::PropertyT<Type>(::strata::get_property( \
            this->template get_interface<::strata::IMetadata>(), #Name)); \
    }
#define _STRATA_ACC_EVT(Name) \
    ::strata::IEvent::Ptr Name() const { \
        return ::strata::get_event( \
            this->template get_interface<::strata::IMetadata>(), #Name); \
    }
#define _STRATA_ACC_FN(Name) \
    ::strata::IFunction::Ptr Name() const { \
        return ::strata::get_function( \
            this->template get_interface<::strata::IMetadata>(), #Name); \
    }
#define _STRATA_ACC(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_ACC_, Tag)(__VA_ARGS__))

/**
 * @brief Declares interface members: generates virtual methods for functions,
 *        a static constexpr metadata array, and typed accessor methods.
 *
 * Place this macro in the @c public section of an interface class that inherits
 * from @c Interface<T>. Each argument is a parenthesized tuple describing one
 * member. Three member kinds are supported:
 *
 * | Kind     | Syntax                          | Description                        |
 * |----------|---------------------------------|------------------------------------|
 * | Property | @c (PROP, Type, Name, Default)  | A typed property with default value  |
 * | Event    | @c (EVT, Name)                  | An observable event                |
 * | Function | @c (FN, Name)                   | A callable function slot           |
 *
 * @par What the macro generates
 * For each member entry the macro produces:
 * -# For @c FN members only: a @c virtual method <tt>fn_Name(const IAny*)</tt>
 *    with a default implementation returning @c NOTHING_TO_DO, plus a static
 *    trampoline function that routes @c IFunction::invoke() calls to the
 *    virtual method. Implementing classes can @c override the virtual to
 *    provide function logic.
 * -# A @c MemberDesc initializer in a @c static @c constexpr @c std::array
 *    named @c metadata, used for compile-time and runtime introspection.
 *    For @c FN members the descriptor includes a pointer to the trampoline.
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
 *         (PROP, float, width, 0.f),
 *         (PROP, float, height, 0.f),
 *         (EVT, on_clicked),
 *         (FN, reset)          // generates: virtual fn_reset(const IAny*)
 *     )
 * };
 * @endcode
 *
 * @par Example: implementing the interface with a function override
 * Concrete classes inherit from @c Object and can override @c fn_Name
 * to provide function logic. The override is automatically wired so that
 * calling @c IFunction::invoke() on the metadata function routes to the
 * virtual method.
 * @code
 * class MyWidget : public Object<MyWidget, IMyWidget>
 * {
 *     ReturnValue fn_reset(const IAny* args) override {
 *         // implementation with access to 'this'
 *         return ReturnValue::SUCCESS;
 *     }
 * };
 * @endcode
 *
 * @par Example: invoking a function
 * @code
 * auto widget = instance().create<IObject>(MyWidget::get_class_uid());
 * if (auto* iw = interface_cast<IMyWidget>(widget)) {
 *     invoke_function(iw->reset());  // calls MyWidget::fn_reset
 * }
 * @endcode
 *
 * @par Example: using accessors on an instance
 * @code
 * auto widget = instance().create<IObject>(MyWidget::get_class_uid());
 * if (auto* iw = interface_cast<IMyWidget>(widget)) {
 *     iw->width().set_value(42.f);
 *     float w = iw->width().get_value();   // 42.f
 *     IEvent::Ptr clicked = iw->on_clicked();
 *     IFunction::Ptr reset = iw->reset();
 * }
 * @endcode
 *
 * @par Example: querying static metadata without an instance
 * @code
 * if (auto* info = instance().get_class_info(MyWidget::get_class_uid())) {
 *     for (auto& m : info->members) {
 *         // m.name, m.kind, m.typeUid
 *     }
 * }
 * @endcode
 *
 * @par Invoke priority
 * When @c IFunction::invoke() is called, the runtime checks in order:
 * -# An explicit callback set via @c set_invoke_callback() (highest priority).
 * -# A bound trampoline from a @c fn_Name override (automatic wiring).
 * -# Returns @c NOTHING_TO_DO if neither is set.
 *
 * @note Up to 32 members are supported per interface.
 *
 * @see STRATA_METADATA For generating only the metadata array without accessors or virtuals.
 * @see Object       For the CRTP base that collects metadata from interfaces.
 * @see IMetadata     For the runtime metadata query interface.
 */
#define STRATA_INTERFACE(...) \
    _STRATA_FOR_EACH(_STRATA_TRAMPOLINE, __VA_ARGS__) \
    STRATA_METADATA(__VA_ARGS__) \
    _STRATA_FOR_EACH(_STRATA_ACC, __VA_ARGS__)

#endif // INTF_METADATA_H
