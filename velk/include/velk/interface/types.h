#ifndef VELK_TYPES_H
#define VELK_TYPES_H

#include <velk/array_view.h>
#include <velk/common.h>
#include <velk/interface/intf_interface.h>

namespace velk {

struct MemberDesc; // Forward declaration

/** @brief Describes a registered class with its UID, name, and static metadata. */
struct ClassInfo
{
    const Uid uid;                              ///< Unique identifier for this class.
    const string_view name;                     ///< Human-readable class name.
    const array_view<InterfaceInfo> interfaces; ///< Interfaces implemented by this class.
    const array_view<MemberDesc> members;       ///< Static metadata members (empty when no metadata).
};

/** @brief Compile-time class identifiers for built-in object types. */
namespace ClassId {
/** @brief Typed value container for object members. Declared via `(PROP, T, name, default)` in
 * VELK_INTERFACE.
 *  @see velk::Property (api/property.h) */
inline constexpr Uid Property{"a66badbf-c750-4580-b035-b5446806d67e"};
/** @brief Callable object that can be invoked with typed arguments. Declared via `(FN, name)` in
 * VELK_INTERFACE.
 *  @see velk::Function (api/function.h) */
inline constexpr Uid Function{"d3c150cc-0b2b-4237-93c5-5a16e9619be8"};
/** @brief Observable multicast delegate. Declared via `(EVT, name)` in VELK_INTERFACE.
 *  @see velk::Event (api/event.h) */
inline constexpr Uid Event{"e4a7b2c1-3d5f-48e9-a1c6-7b8d9e0f2a34"};
/** @brief Promise/future pair for asynchronous results. Created via ITypeRegistry::create_future().
 *  @see velk::Future (api/future.h) */
inline constexpr Uid Future{"371dfa91-1cf7-441e-b688-20d7e0114745"};
/** @brief Fixed-type, variable-length array property. Declared via `(ARR, T, name)` in VELK_INTERFACE.
 *  @see velk::ArrayProperty (api/property.h) */
inline constexpr Uid ArrayProperty{"f8e2a3b1-7c4d-49e6-8f1a-2b3c4d5e6f70"};
/** @brief Single-root tree that manages parent/child relationships between objects.
 *  @see velk::Hierarchy (api/hierarchy.h) */
inline constexpr Uid Hierarchy{"b7d3e1a2-5f48-4c96-9e0a-1d2b3c4e5f67"};
/** @brief Reactive link that propagates value changes from a source property to one or more targets.
 *  @see velk::Binding (api/binding.h) */
inline constexpr Uid Binding{"c4e8f2a1-6b39-47d5-8e1c-3a9d5f7b2e04"};
/** @brief Standalone property for runtime-created and dynamic object properties.
 *  @see velk::StandaloneProperty (src/standalone_property.h) */
inline constexpr Uid StandaloneProperty{"c1f97544-35af-42b6-8748-f24e86ba8a07"};
/** @brief Dynamically-typed value container. Declared via `(PROP, Variant, name, {})` in VELK_INTERFACE.
 *  @see velk::Variant (api/variant.h) */
inline constexpr Uid Variant{"e5f2a7b3-8c14-4d69-9e2f-3a1b5c6d7e80"};
/** @brief Reference to another object, with optional owning semantics and interface constraints.
 *  Declared via `(PROP, ObjectRef, name, {})` in VELK_INTERFACE.
 *  @see velk::ObjectRef (api/object_ref.h) */
inline constexpr Uid ObjectRef{"f3a1b5c6-d7e8-4f09-a2b3-c4d5e6f70189"};
/** @brief Opt-in shared reader/writer lock for concurrent property access.
 *  @see velk::ThreadContext (api/thread_context.h) */
inline constexpr Uid ThreadContext{"a1b2c3d4-e5f6-4a7b-8c9d-0e1f2a3b4c5d"};
/** @brief Flat collection of objects with id-based lookup.
 *  @see velk::Store (api/store.h) */
inline constexpr Uid Store{"b4dde845-c159-4737-90ed-f63d4bce6c42"};
/** @brief Local filesystem protocol handler. Supports configurable scheme and base path.
 *  @see velk::IResourceProtocol (interface/resource/intf_resource_protocol.h) */
inline constexpr Uid FileProtocol{"0f781b05-29d9-47f5-82c0-ac3cac32319c"};
/** @brief In-memory protocol handler. Serves registered byte buffers under `memory://`.
 *  @see velk::IMemoryProtocol (interface/resource/intf_resource_protocol.h) */
inline constexpr Uid MemoryProtocol{"98a7c593-747f-4340-b105-a8285bb22786"};
/** @brief Bare object with metadata support for holding dynamic properties.
 *  @see velk::create_object (api/object.h) */
inline constexpr Uid Object{"a51b001b-d5ad-4e03-9bb4-9b14c500e591"};
} // namespace ClassId

/** @brief Standard return codes for Velk operations. Non-negative values indicate success. */
enum ReturnValue : int16_t
{
    Success = 0,          ///< Operation succeeded.
    NothingToDo = 1,      ///< Operation succeeded but had no effect (e.g. value unchanged).
    Fail = -1,            ///< Operation failed.
    InvalidArgument = -2, ///< One or more arguments were invalid.
    ReadOnly = -3,        ///< Write rejected: target is read-only.
    Refused = -4,         ///< Operation refused by the target object.
};

/** @brief General-purpose object flags. Checked by runtime implementations. */
namespace ObjectFlags {
inline constexpr uint32_t None = 0;
inline constexpr uint32_t ReadOnly = 1 << 0;    ///< Property rejects writes via set_value/set_data.
inline constexpr uint32_t HiveManaged = 1 << 1; ///< Object is managed by a Hive.

// Internal flags, set and cleared by velk itself, allocated from the top bit down.

/// A bound property's value in the object's state is out of date; reading the state refreshes it.
inline constexpr uint32_t StaleBindings = 1u << 31;
} // namespace ObjectFlags

/** @brief Controls whether a lookup should create a new instances on miss.
           Used by e.g. IMetadata and IObjectStorage lookup functions. */
enum class Resolve : uint8_t
{
    Existing = 0, ///< Return only an already-created instance; never allocate.
    Create        ///< Create the instance on first access (default behavior).
};

/** @brief Returns true if the return value indicates success (non-negative). */
inline constexpr bool succeeded(ReturnValue ret)
{
    return ret >= 0;
}

/** @brief Returns true if the return value indicates failure (negative). */
inline constexpr bool failed(ReturnValue ret)
{
    return ret < 0;
}

/** @brief Combines two return values. If the left side already failed, keeps it; otherwise takes the right
 * side. */
inline constexpr ReturnValue operator&(ReturnValue a, ReturnValue b)
{
    return failed(a) ? a : b;
}

/** @brief Compound assignment: preserves the first failure across a chain of operations. */
inline constexpr ReturnValue& operator&=(ReturnValue& a, ReturnValue b)
{
    if (!failed(a)) {
        a = b;
    }
    return a;
}

} // namespace velk

#endif
