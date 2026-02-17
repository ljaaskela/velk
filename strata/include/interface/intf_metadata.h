#ifndef INTF_METADATA_H
#define INTF_METADATA_H

#include <api/any.h>
#include <api/event.h>
#include <api/function.h>
#include <api/property.h>
#include <api/traits.h>
#include <array_view.h>
#include <common.h>
#include <ext/any.h>
#include <interface/intf_event.h>
#include <interface/intf_interface.h>
#include <interface/intf_property.h>

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace strata {

/** @brief Discriminator for the kind of member described by a MemberDesc. */
enum class MemberKind : uint8_t { Property, Event, Function };

/** @brief Function pointer type for trampoline callbacks that route to virtual methods. */
using FnTrampoline = IAny::Ptr(*)(void* self, FnArgs args);

/** @brief Kind-specific data for Property members. */
struct PropertyKind {
    Uid typeUid;                                ///< type_uid<T>() for the property's value type.
    /** @brief Returns a pointer to a static IAny holding the property's default value. */
    const IAny*(*getDefault)() = nullptr;
    /** @brief Creates an AnyRef pointing into the State struct at @p stateBase. */
    IAny::Ptr(*createRef)(void* stateBase) = nullptr;
    int32_t flags{ObjectFlags::None}; ///< ObjectFlags to apply to the created PropertyImpl.
};

/** @brief Describes a single argument of a typed function. */
struct FnArgDesc {
    std::string_view name;  ///< Parameter name (e.g. "x").
    Uid typeUid;            ///< type_uid<T>() for the parameter type.
};

/** @brief Kind-specific data for Function/Event members. */
struct FunctionKind {
    FnTrampoline trampoline = nullptr;  ///< Static trampoline that routes invoke() to the virtual method.
    array_view<FnArgDesc> args;         ///< Typed argument descriptors; empty for zero-arg and FN_RAW.
};

/** @brief Describes a single member (property, event, or function) declared by an object class. */
struct MemberDesc {
    std::string_view name;              ///< Member name used for runtime lookup.
    MemberKind kind;                    ///< Discriminator (Property, Event, or Function).
    const InterfaceInfo* interfaceInfo; ///< Interface that declared this member.
    const void* ext = nullptr;          ///< Points to PropertyKind or FunctionKind based on @c kind.

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

/**
 * @brief Creates a Property MemberDesc.
 * @param name Member name for runtime lookup.
 * @param info Interface that declares this member (may be nullptr).
 * @param pk PropertyKind with typeUid, getDefault, and createRef (may be nullptr).
 */
constexpr MemberDesc PropertyDesc(std::string_view name, const InterfaceInfo* info = nullptr,
    const PropertyKind* pk = nullptr)
{
    return {name, MemberKind::Property, info, pk};
}

/**
 * @brief Retrieves the typed default value from a MemberDesc.
 * @tparam T The expected value type.
 * @param desc Member descriptor whose PropertyKind::getDefault to query.
 * @return The default value, or a value-initialized @p T if unavailable.
 */
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

/**
 * @brief Creates an Event MemberDesc.
 * @param name Member name for runtime lookup.
 * @param info Interface that declares this member (may be nullptr).
 */
constexpr MemberDesc EventDesc(std::string_view name, const InterfaceInfo* info = nullptr)
{
    return {name, MemberKind::Event, info};
}

/**
 * @brief Creates a Function MemberDesc.
 * @param name Member name for runtime lookup.
 * @param info Interface that declares this member (may be nullptr).
 * @param fk FunctionKind with trampoline and argument descriptors (may be nullptr).
 */
constexpr MemberDesc FunctionDesc(std::string_view name, const InterfaceInfo* info = nullptr,
    const FunctionKind* fk = nullptr)
{
    return {name, MemberKind::Function, info, fk};
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

/**
 * @brief Type-safe property state access. Returns a typed pointer to T::State.
 * @tparam T The interface type whose State struct to retrieve.
 * @param object The object whose property state to return.
 */
template<class T, class U>
typename T::State *get_property_state(U *object)
{
    auto state = interface_cast<IPropertyState>(object);
    return state ? state->template get_property_state<T>() : nullptr;
}

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

/**
 * @brief Null-safe property lookup on an IMetadata pointer.
 * @param meta Metadata interface pointer (may be nullptr).
 * @param name Property name to look up.
 * @return The runtime property instance, or nullptr if @p meta is null or the name is not found.
 */
inline IProperty::Ptr get_property(const IMetadata* meta, std::string_view name)
{
    return meta ? meta->get_property(name) : nullptr;
}

/**
 * @brief Null-safe event lookup on an IMetadata pointer.
 * @param meta Metadata interface pointer (may be nullptr).
 * @param name Event name to look up.
 * @return The runtime event instance, or nullptr if @p meta is null or the name is not found.
 */
inline IEvent::Ptr get_event(const IMetadata* meta, std::string_view name)
{
    return meta ? meta->get_event(name) : nullptr;
}

/**
 * @brief Null-safe function lookup on an IMetadata pointer.
 * @param meta Metadata interface pointer (may be nullptr).
 * @param name Function name to look up.
 * @return The runtime function instance, or nullptr if @p meta is null or the name is not found.
 */
inline IFunction::Ptr get_function(const IMetadata* meta, std::string_view name)
{
    return meta ? meta->get_function(name) : nullptr;
}

/**
 * @brief Abstract interface for runtime metadata storage.
 */
class IMetadataContainer : public Interface<IMetadataContainer, IMetadata>
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
inline IAny::Ptr invoke_function(const IInterface *o,
                                   std::string_view name,
                                   FnArgs args = {})
{
    auto meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), args) : nullptr;
}

/**
 * @brief Invoke a named function with a single IAny argument.
 * @param o Object to query for the function.
 * @param name Function name to look up.
 * @param arg Single argument to pass.
 */
inline IAny::Ptr invoke_function(const IInterface *o,
                                   std::string_view name,
                                   const IAny *arg)
{
    FnArgs args{&arg, 1};
    return invoke_function(o, name, args);
}

/**
 * @brief Invoke an event from target object metadata.
 * @param o The object to query for the event.
 * @param name Name of the event to query.
 * @param args Event arguments.
 */
inline ReturnValue invoke_event(const IInterface *o,
                                std::string_view name,
                                FnArgs args = {})
{
    auto meta = interface_cast<IMetadata>(o);
    return meta ? invoke_event(meta->get_event(name), args) : ReturnValue::INVALID_ARGUMENT;
}

/**
 * @brief Invoke a named event with a single IAny argument.
 * @param o Object to query for the event.
 * @param name Event name to look up.
 * @param arg Single argument to pass.
 */
inline ReturnValue invoke_event(const IInterface *o,
                                std::string_view name,
                                const IAny *arg)
{
    FnArgs args{&arg, 1};
    return invoke_event(o, name, args);
}

// Variadic invoke_function: IAny-convertible args (name-based)

/**
 * @brief Invokes a named function with multiple IAny-convertible arguments.
 * @param o The object to query for the function.
 * @param name Name of the function to query.
 * @param args Two or more IAny-convertible arguments.
 */
template<class... Args, detail::require_any_args<Args...> = 0>
IAny::Ptr invoke_function(const IInterface* o, std::string_view name, const Args&... args)
{
    const IAny* ptrs[] = {static_cast<const IAny*>(args)...};
    auto* meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), FnArgs{ptrs, sizeof...(Args)}) : nullptr;
}

// Variadic invoke_function: value args (name-based)

namespace detail {

template<class Tuple, size_t... Is>
FnArgs make_fn_args(Tuple& tup, const IAny** ptrs, std::index_sequence<Is...>)
{
    ((ptrs[Is] = static_cast<const IAny*>(std::get<Is>(tup))), ...);
    return {ptrs, sizeof...(Is)};
}

} // namespace detail

/** @brief Invokes a named function with multiple value arguments. */
template<class... Args, detail::require_value_args<Args...> = 0>
IAny::Ptr invoke_function(const IInterface* o, std::string_view name, const Args&... args)
{
    auto tup = std::make_tuple(Any<std::decay_t<Args>>(args)...);
    const IAny* ptrs[sizeof...(Args)];
    auto fnArgs = detail::make_fn_args(tup, ptrs, std::index_sequence_for<Args...>{});
    auto* meta = interface_cast<IMetadata>(o);
    return meta ? invoke_function(meta->get_function(name), fnArgs) : nullptr;
}

/** @brief Internal helpers for STRATA_INTERFACE macro expansion. */
namespace detail {

/**
 * @brief Extracts a typed value from an IAny pointer.
 * @tparam T The target value type.
 * @param any Source IAny (may be nullptr).
 * @return The extracted value, or a value-initialized @p T if @p any is null or type mismatches.
 */
template<class T>
T extract_arg(const IAny* any) {
    T value{};
    if (any) any->get_data(&value, sizeof(T), type_uid<T>());
    return value;
}

/**
 * @brief Implementation of interface_trampoline; unpacks FnArgs into typed parameters.
 * @tparam Intf The interface type containing the virtual method.
 * @tparam Args Parameter types deduced from the member function pointer.
 * @tparam Is Index sequence for argument extraction.
 * @param self Pointer to the interface instance (cast to @p Intf*).
 * @param args Runtime function arguments.
 * @param fn Pointer-to-member for the virtual method to call.
 */
template<class Intf, class R, class... Args, size_t... Is>
IAny::Ptr interface_trampoline_impl(void* self, FnArgs args,
    R(Intf::*fn)(Args...), std::index_sequence<Is...>)
{
    if constexpr (std::is_void_v<R>) {
        (static_cast<Intf*>(self)->*fn)(extract_arg<std::decay_t<Args>>(args[Is])...);
        return nullptr;
    } else if constexpr (std::is_same_v<R, IAny::Ptr>) {
        return (static_cast<Intf*>(self)->*fn)(extract_arg<std::decay_t<Args>>(args[Is])...);
    } else {
        return Any<R>((static_cast<Intf*>(self)->*fn)(extract_arg<std::decay_t<Args>>(args[Is])...)).clone();
    }
}

/**
 * @brief Routes an IFunction::invoke() call to a typed virtual method.
 *
 * Validates argument count (for non-zero-arg functions), then delegates to
 * interface_trampoline_impl to extract typed values from FnArgs.
 *
 * @tparam Intf The interface type containing the virtual method.
 * @tparam Args Parameter types deduced from the member function pointer.
 * @param self Pointer to the interface instance.
 * @param args Runtime function arguments.
 * @param fn Pointer-to-member for the virtual method to call.
 * @return The virtual method's return value, or INVALID_ARGUMENT if too few args.
 */
template<class Intf, class R, class... Args>
IAny::Ptr interface_trampoline(void* self, FnArgs args,
    R(Intf::*fn)(Args...))
{
    if constexpr (sizeof...(Args) > 0) {
        if (args.count < sizeof...(Args)) return nullptr;
    }
    return interface_trampoline_impl(self, args, fn, std::index_sequence_for<Args...>{});
}

/**
 * @brief Deduces the member type from a pointer-to-member.
 * @tparam S The struct/class type.
 * @tparam T The member type.
 * @note Declaration only; used with @c decltype, never called.
 */
template<class S, class T>
T member_type_helper(T S::*);

/**
 * @brief Returns a lazily-constructed default-initialized State singleton.
 * @tparam State The state struct type (e.g. IMyWidget::State).
 * @return Reference to the shared static default instance.
 */
template<class State>
State& default_state()
{
    static State s{};
    return s;
}

/**
 * @brief Binds a pointer-to-member to PropertyKind function pointers.
 *
 * Given a State struct type and a pointer-to-member, PropBind generates a
 * @c static @c constexpr PropertyKind with @c getDefault and @c createRef
 * functions. This replaces per-property boilerplate that the STRATA_INTERFACE
 * macro previously generated inline.
 *
 * @tparam State The state struct type (e.g. IMyWidget::State).
 * @tparam Mem Pointer-to-member (C++17 auto NTTP), e.g. @c &State::width.
 *
 * @par Usage
 * @code
 * static constexpr PropertyKind pk = detail::PropBind<State, &State::width>::kind;
 * @endcode
 */
template<class State, auto Mem, int32_t Flags = 0>
struct PropBind
{
    using value_type = decltype(member_type_helper(Mem)); ///< The member's value type.

    /**
     * @brief Returns a pointer to a static AnyRef holding the default value.
     *
     * The AnyRef points into the shared default_state<State>() singleton,
     * so the returned value reflects the State struct's default initialization.
     */
    static const IAny* getDefault()
    {
        static ext::AnyRef<value_type> ref(&(default_state<State>().*Mem));
        return &ref;
    }

    /**
     * @brief Creates an AnyRef pointing into a live State struct.
     * @param base Pointer to the State struct instance (cast from void*).
     * @return Owning pointer to an AnyRef<value_type> targeting the member.
     */
    static IAny::Ptr createRef(void* base)
    {
        return ext::create_any_ref<value_type>(
            &(static_cast<State*>(base)->*Mem));
    }

    static constexpr PropertyKind kind { type_uid<value_type>(), &getDefault, &createRef, Flags }; ///< Pre-built PropertyKind.
};

/**
 * @brief Binds a typed member function pointer to a FunctionKind trampoline.
 *
 * Given a pointer-to-member for a virtual method declared by @c FN (zero-arg
 * or typed-arg), FnBind generates a static trampoline function and a
 * @c static @c constexpr FunctionKind. This replaces per-function trampoline
 * boilerplate that the STRATA_INTERFACE macro previously generated inline.
 *
 * Works for both zero-arg and typed-arg FN because @c interface_trampoline
 * deduces @c Args... from the member function pointer.
 *
 * @tparam Fn Pointer-to-member (C++17 auto NTTP), e.g. @c &IMyWidget::fn_reset.
 *
 * @par Usage
 * @code
 * static constexpr FunctionKind fk = detail::FnBind<&IMyWidget::fn_reset>::kind;
 * @endcode
 *
 * @see FnRawBind For @c FN_RAW members that receive @c FnArgs directly.
 * @see PropBind  For the analogous property binding template.
 */
template<auto Fn>
struct FnBind
{
    static IAny::Ptr trampoline(void* self, FnArgs args) {
        return interface_trampoline(self, args, Fn);
    }
    static constexpr FunctionKind kind{&trampoline, {}};
};

/**
 * @brief Binds a raw member function pointer to a FunctionKind trampoline.
 *
 * For @c FN_RAW members whose virtual method receives @c FnArgs directly,
 * FnRawBind generates a trampoline that passes @c FnArgs through without
 * type extraction. This is needed because @c interface_trampoline would
 * try to extract @c FnArgs as a typed argument via @c IAny, which doesn't work.
 *
 * @tparam Fn Pointer-to-member (C++17 auto NTTP), e.g. @c &IMyWidget::fn_process.
 *
 * @par Usage
 * @code
 * static constexpr FunctionKind fk = detail::FnRawBind<&IMyWidget::fn_process>::kind;
 * @endcode
 *
 * @see FnBind  For typed @c FN members.
 * @see PropBind For the analogous property binding template.
 */
template<auto Fn>
struct FnRawBind
{
    template<class Intf, class R>
    static IAny::Ptr call(void* self, FnArgs args, R(Intf::*fn)(FnArgs)) {
        if constexpr (std::is_void_v<R>) {
            (static_cast<Intf*>(self)->*fn)(args);
            return nullptr;
        } else if constexpr (std::is_same_v<R, IAny::Ptr>) {
            return (static_cast<Intf*>(self)->*fn)(args);
        } else {
            return Any<R>((static_cast<Intf*>(self)->*fn)(args)).clone();
        }
    }
    static IAny::Ptr trampoline(void* self, FnArgs args) {
        return call(self, args, Fn);
    }
    static constexpr FunctionKind kind{&trampoline, {}};
};

} // namespace detail

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

// --- Param/ArgDesc expansion: (Type, Name) pairs -> typed param list / FnArgDesc array ---

#define _STRATA_PARAM_PAIR(Type, Name) Type Name
#define _STRATA_PARAMS_1(a)          _STRATA_EXPAND(_STRATA_PARAM_PAIR a)
#define _STRATA_PARAMS_2(a, b)       _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAM_PAIR b)
#define _STRATA_PARAMS_3(a, ...)     _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAMS_2(__VA_ARGS__))
#define _STRATA_PARAMS_4(a, ...)     _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAMS_3(__VA_ARGS__))
#define _STRATA_PARAMS_5(a, ...)     _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAMS_4(__VA_ARGS__))
#define _STRATA_PARAMS_6(a, ...)     _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAMS_5(__VA_ARGS__))
#define _STRATA_PARAMS_7(a, ...)     _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAMS_6(__VA_ARGS__))
#define _STRATA_PARAMS_8(a, ...)     _STRATA_PARAMS_1(a), _STRATA_EXPAND(_STRATA_PARAMS_7(__VA_ARGS__))
#define _STRATA_PARAMS(...) \
    _STRATA_EXPAND(_STRATA_CAT(_STRATA_PARAMS_, _STRATA_NARG(__VA_ARGS__))(__VA_ARGS__))

#define _STRATA_ARGDESC_PAIR(Type, Name) ::strata::FnArgDesc{#Name, ::strata::type_uid<Type>()}
#define _STRATA_ARGDESCS_1(a)        _STRATA_EXPAND(_STRATA_ARGDESC_PAIR a)
#define _STRATA_ARGDESCS_2(a, b)     _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESC_PAIR b)
#define _STRATA_ARGDESCS_3(a, ...)   _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESCS_2(__VA_ARGS__))
#define _STRATA_ARGDESCS_4(a, ...)   _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESCS_3(__VA_ARGS__))
#define _STRATA_ARGDESCS_5(a, ...)   _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESCS_4(__VA_ARGS__))
#define _STRATA_ARGDESCS_6(a, ...)   _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESCS_5(__VA_ARGS__))
#define _STRATA_ARGDESCS_7(a, ...)   _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESCS_6(__VA_ARGS__))
#define _STRATA_ARGDESCS_8(a, ...)   _STRATA_ARGDESCS_1(a), _STRATA_EXPAND(_STRATA_ARGDESCS_7(__VA_ARGS__))
#define _STRATA_ARGDESCS(...) \
    _STRATA_EXPAND(_STRATA_CAT(_STRATA_ARGDESCS_, _STRATA_NARG(__VA_ARGS__))(__VA_ARGS__))

// --- State pass: generates State struct fields for PROP members ---

#define _STRATA_STATE_PROP(Type, Name, Default)  Type Name = Default;
#define _STRATA_STATE_RPROP(Type, Name, Default) Type Name = Default;
#define _STRATA_STATE_EVT(Name)
#define _STRATA_STATE_FN(...)
#define _STRATA_STATE_FN_RAW(...)
#define _STRATA_STATE(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_STATE_, Tag)(__VA_ARGS__))

// --- Defaults pass: generates kind-specific static data for each member ---

#define _STRATA_DEFAULTS_PROP(Type, Name, Default) \
    static constexpr ::strata::PropertyKind _strata_propkind_##Name = \
        ::strata::detail::PropBind<State, &State::Name>::kind;
#define _STRATA_DEFAULTS_RPROP(Type, Name, Default) \
    static constexpr ::strata::PropertyKind _strata_propkind_##Name = \
        ::strata::detail::PropBind<State, &State::Name, ::strata::ObjectFlags::ReadOnly>::kind;
#define _STRATA_DEFAULTS_EVT(...)

#define _STRATA_DEFAULTS_FN_0(Name) \
    static constexpr ::strata::FunctionKind _strata_fnkind_##Name = \
        ::strata::detail::FnBind<&_strata_intf_type::fn_##Name>::kind;
#define _STRATA_DEFAULTS_FN_N(Name, ...) \
    static constexpr ::strata::FnArgDesc _strata_fnargs_##Name[] = { _STRATA_ARGDESCS(__VA_ARGS__) }; \
    static constexpr ::strata::FunctionKind _strata_fnkind_##Name { \
        &::strata::detail::FnBind<&_strata_intf_type::fn_##Name>::trampoline, \
        {_strata_fnargs_##Name, _STRATA_NARG(__VA_ARGS__)} };

#define _STRATA_DFN_2(RetType, Name)       _STRATA_DEFAULTS_FN_0(Name)
#define _STRATA_DFN_3(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_4(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_5(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_6(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_7(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_8(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_9(RetType, Name, ...)  _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DFN_10(RetType, Name, ...) _STRATA_DEFAULTS_FN_N(Name, __VA_ARGS__)
#define _STRATA_DEFAULTS_FN(...) \
    _STRATA_EXPAND(_STRATA_CAT(_STRATA_DFN_, _STRATA_NARG(__VA_ARGS__))(__VA_ARGS__))

#define _STRATA_DEFAULTS_FN_RAW(Name) \
    static constexpr ::strata::FunctionKind _strata_fnkind_##Name = \
        ::strata::detail::FnRawBind<&_strata_intf_type::fn_##Name>::kind;

#define _STRATA_DEFAULTS(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_DEFAULTS_, Tag)(__VA_ARGS__))

// --- Metadata dispatch: tag -> MemberDesc initializer ---

#define _STRATA_META_PROP(Type, Name, ...) \
    ::strata::PropertyDesc(#Name, &INFO, &_strata_propkind_##Name),
#define _STRATA_META_RPROP(Type, Name, ...) \
    ::strata::PropertyDesc(#Name, &INFO, &_strata_propkind_##Name),
#define _STRATA_META_EVT(Name) \
    ::strata::EventDesc(#Name, &INFO),
#define _STRATA_META_FN(RetType, Name, ...) \
    ::strata::FunctionDesc(#Name, &INFO, &_strata_fnkind_##Name),
#define _STRATA_META_FN_RAW(Name) \
    ::strata::FunctionDesc(#Name, &INFO, &_strata_fnkind_##Name),
#define _STRATA_META(Tag, ...)        _STRATA_EXPAND(_STRATA_CAT(_STRATA_META_, Tag)(__VA_ARGS__))

// --- Trampoline dispatch: tag -> virtual method + static trampoline for FN, no-op for PROP/EVT ---

#define _STRATA_TRAMPOLINE_PROP(...)
#define _STRATA_TRAMPOLINE_RPROP(...)
#define _STRATA_TRAMPOLINE_EVT(Name)

#define _STRATA_TRAMPOLINE_FN_0(RetType, Name) \
    virtual RetType fn_##Name() = 0;
#define _STRATA_TRAMPOLINE_FN_N(RetType, Name, ...) \
    virtual RetType fn_##Name(_STRATA_PARAMS(__VA_ARGS__)) = 0;

#define _STRATA_TFN_2(RetType, Name)       _STRATA_TRAMPOLINE_FN_0(RetType, Name)
#define _STRATA_TFN_3(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_4(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_5(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_6(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_7(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_8(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_9(RetType, Name, ...)  _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TFN_10(RetType, Name, ...) _STRATA_TRAMPOLINE_FN_N(RetType, Name, __VA_ARGS__)
#define _STRATA_TRAMPOLINE_FN(...) \
    _STRATA_EXPAND(_STRATA_CAT(_STRATA_TFN_, _STRATA_NARG(__VA_ARGS__))(__VA_ARGS__))

#define _STRATA_TRAMPOLINE_FN_RAW(Name) \
    virtual ::strata::IAny::Ptr fn_##Name(::strata::FnArgs) = 0;

#define _STRATA_TRAMPOLINE(Tag, ...) _STRATA_EXPAND(_STRATA_CAT(_STRATA_TRAMPOLINE_, Tag)(__VA_ARGS__))

/**
 * @brief Generates a State struct, per-property PropertyKind statics, and a
 *        @c static @c constexpr metadata array from member declarations.
 *
 * This is the data-only subset of STRATA_INTERFACE: it produces the State
 * struct and metadata array but does @e not generate virtual methods, trampolines,
 * or accessor methods. Use it when you want to write those by hand.
 *
 * @see STRATA_INTERFACE For the full macro that also generates accessors and virtuals.
 */
#define STRATA_METADATA(...) \
    struct State { _STRATA_FOR_EACH(_STRATA_STATE, __VA_ARGS__) }; \
    _STRATA_FOR_EACH(_STRATA_DEFAULTS, __VA_ARGS__) \
    static constexpr std::array metadata = { _STRATA_FOR_EACH(_STRATA_META, __VA_ARGS__) };

// --- Accessor dispatch: tag -> typed non-virtual accessor method ---

#define _STRATA_ACC_PROP(Type, Name, ...) \
    ::strata::Property<Type> Name() const { \
        return ::strata::Property<Type>(::strata::get_property( \
            this->template get_interface<::strata::IMetadata>(), #Name)); \
    }
#define _STRATA_ACC_RPROP(Type, Name, ...) \
    ::strata::ConstProperty<Type> Name() const { \
        return ::strata::ConstProperty<Type>(::strata::get_property( \
            this->template get_interface<::strata::IMetadata>(), #Name)); \
    }
#define _STRATA_ACC_EVT(Name) \
    ::strata::Event Name() const { \
        return ::strata::Event(::strata::get_event( \
            this->template get_interface<::strata::IMetadata>(), #Name)); \
    }
#define _STRATA_ACC_FN(RetType, Name, ...) \
    ::strata::Function Name() const { \
        return ::strata::Function(::strata::get_function( \
            this->template get_interface<::strata::IMetadata>(), #Name)); \
    }
#define _STRATA_ACC_FN_RAW(Name) \
    ::strata::Function Name() const { \
        return ::strata::Function(::strata::get_function( \
            this->template get_interface<::strata::IMetadata>(), #Name)); \
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
 * | Kind     | Syntax                                   | Description                          |
 * |----------|------------------------------------------|--------------------------------------|
 * | Property | @c (PROP, Type, Name, Default)           | A typed property with default value  |
 * | Read-only| @c (RPROP, Type, Name, Default)          | A read-only property with default    |
 * | Event    | @c (EVT, Name)                           | An observable event                  |
 * | Function | @c (FN, RetType, Name)                            | Zero-arg function with return type   |
 * | Function | @c (FN, RetType, Name, (T1, a1), (T2, a2), ...)   | Typed-arg function with metadata     |
 * | Function | @c (FN_RAW, Name)                        | Raw untyped function (receives FnArgs) |
 *
 * @par What the macro generates
 * For each member entry the macro produces:
 * -# For @c FN members only: a pure @c virtual method (e.g. <tt>fn_Name()</tt>,
 *    <tt>fn_Name(T1, T2)</tt>, or <tt>fn_Name(FnArgs)</tt>). A static
 *    trampoline that routes @c IFunction::invoke() calls to the virtual method
 *    is generated via @c detail::FnBind (for @c FN) or @c detail::FnRawBind
 *    (for @c FN_RAW). Implementing classes @c override the virtual to provide
 *    function logic.
 * -# A @c MemberDesc initializer in a @c static @c constexpr @c std::array
 *    named @c metadata, used for compile-time and runtime introspection.
 *    For @c FN members the descriptor includes a pointer to the trampoline.
 * -# A non-virtual @c const accessor method on the interface:
 *    - @c PROP &rarr; <tt>Property\<Type\> Name() const</tt>
 *    - @c EVT  &rarr; <tt>Event Name() const</tt>
 *    - @c FN   &rarr; <tt>Function Name() const</tt>
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
 *         (FN, void, reset)    // generates: virtual void fn_reset()
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
 *     Event clicked = iw->on_clicked();
 *     Function reset = iw->reset();
 * }
 * @endcode
 *
 * @par Example: querying static metadata without an instance
 * @code
 * if (auto* info = instance().get_class_info(MyWidget::get_class_uid())) {
 *     for (auto& m : info->members) {
 *         // m.name, m.kind, m.interfaceInfo
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
