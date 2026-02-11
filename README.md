# Strata

Strata is a C++17 component object model library with interface-based polymorphism, typed properties with change notifications, events, and compile-time metadata with runtime introspection.

Strata is designed to be built as a shared library (DLL on Windows, .so on Linux). All runtime implementations live inside the shared library, while consumers only depend on public headers containing abstract interfaces and header-only templates. This means the internal implementation can evolve without recompiling consumer code, multiple modules can share a single type registry and object factory, and ABI compatibility is maintained through stable virtual interfaces.

The name *Strata* (plural of *stratum*, meaning layers) reflects the library's layered architecture: abstract interfaces at the bottom, CRTP helpers and template implementations in the middle, and user-facing typed wrappers on top.

## Table of contents

- [Features](#features)
- [Building](#building)
- [Quick start](#quick-start)
  - [Define an interface](#define-an-interface)
  - [Implement with Object](#implement-with-object)
  - [Register and create](#register-and-create)
  - [Use typed accessors](#use-typed-accessors)
  - [Query metadata](#query-metadata)
  - [Virtual function dispatch](#virtual-function-dispatch)
  - [Properties with change notifications](#properties-with-change-notifications)
  - [Custom Any types](#custom-any-types)
- [Architecture](#architecture)
  - [interface/ -- Contracts](#interface----contracts)
  - [ext/ -- CRTP helpers](#ext----crtp-helpers)
  - [api/ -- User wrappers](#api----user-wrappers)
  - [src/ -- Internal implementations](#src----internal-implementations)
- [Key types](#key-types)
- [Object memory layout](#object-memory-layout)
  - [Per-object data (CoreObject / Object)](#per-object-data-coreobject--object)
  - [Per-object data (BaseAny)](#per-object-data-baseany)
  - [MetadataContainer](#metadatacontainer-heap-allocated-one-per-object)
  - [Example: MyWidget with 6 members](#example-mywidget-with-6-members)
- [STRATA_INTERFACE reference](#strata_interface-reference)
  - [Manual metadata and accessors](#manual-metadata-and-accessors)
- [Project structure](#project-structure)

## Features

- **Interface-based architecture** -- define abstract interfaces with properties, events, and functions
- **Typed properties** -- `PropertyT<T>` wrappers with automatic change notification events
- **Virtual function dispatch** -- `(FN, Name)` generates overridable `fn_Name()` virtuals, automatically wired to `IFunction::invoke()`
- **Events** -- observable multi-handler events
- **Type-erased values** -- `AnyT<T>` wrappers over a generic `IAny` container
- **Compile-time metadata** -- declare members with `STRATA_INTERFACE`, query them at compile time or runtime
- **Central type system** -- register types, create instances by UID, query class info without instantiation
- **Custom type support** -- extend with user-defined `IAny` implementations for external or shared data

## Project structure

```
strata/
  CMakeLists.txt
  README.md
  strata/
    CMakeLists.txt
    include/              Public headers
      common.h            Uid, type_uid<T>(), get_name<T>()
      array_view.h        Lightweight constexpr span-like view
      interface/          Abstract interfaces
      ext/                CRTP helpers
      api/                User-facing wrappers
    src/                  Internal implementations
  test/
    CMakeLists.txt
    main.cpp              Feature demonstration
```

## Building

Requires CMake 3.5+ and a C++17 compiler. Tested with MSVC 2022.

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

Output: `build/bin/strata.dll` (shared library) and `build/bin/test.exe` (demo).

## Quick start

### Define an interface

Use `STRATA_INTERFACE` to declare properties, events, and functions. This generates both a static constexpr metadata array and typed accessor methods.

```cpp
#include <interface/intf_metadata.h>

class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width),
        (PROP, float, height),
        (EVT, on_clicked),
        (FN, reset)
    )
};
```

### Implement with Object

`Object` automatically collects metadata from all listed interfaces and provides the `IMetadata` implementation. Override `fn_<Name>` to provide function logic.

```cpp
#include <ext/object.h>

class MyWidget : public Object<MyWidget, IMyWidget>
{
    ReturnValue fn_reset(const IAny*) override {
        // implementation with access to 'this'
        return ReturnValue::SUCCESS;
    }
};
```

Multiple interfaces are supported:

```cpp
class ISerializable : public Interface<ISerializable>
{
public:
    STRATA_INTERFACE(
        (PROP, std::string, name),
        (FN, serialize)
    )
};

class MyWidget : public Object<MyWidget, IMyWidget, ISerializable>
{
    ReturnValue fn_reset(const IAny*) override { /* ... */ return ReturnValue::SUCCESS; }
    ReturnValue fn_serialize(const IAny*) override { /* ... */ return ReturnValue::SUCCESS; }
};
```

### Register and create

```cpp
auto& s = instance();
s.register_type<MyWidget>();

auto widget = s.create<IObject>(MyWidget::get_class_uid());
```

### Use typed accessors

```cpp
if (auto* iw = interface_cast<IMyWidget>(widget)) {
    iw->width().set_value(42.f);
    float w = iw->width().get_value();  // 42.f

    IEvent::Ptr clicked = iw->on_clicked();
    IFunction::Ptr reset = iw->reset();
}
```

### Virtual function dispatch

`(FN, Name)` in `STRATA_INTERFACE` generates a virtual method `fn_Name(const IAny*)` on the interface. Implementing classes override this virtual to provide logic. The override is automatically wired so that calling `invoke()` on the runtime `IFunction` routes to the virtual method.

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width),
        (FN, reset)          // generates: virtual fn_reset(const IAny*)
    )
};

class MyWidget : public Object<MyWidget, IMyWidget>
{
    ReturnValue fn_reset(const IAny*) override {
        std::cout << "reset!" << std::endl;
        return ReturnValue::SUCCESS;
    }
};

// Calling invoke() routes to fn_reset()
auto widget = instance().create<IObject>(MyWidget::get_class_uid());
if (auto* iw = interface_cast<IMyWidget>(widget)) {
    invoke_function(iw->reset());  // prints "reset!"
}
```

If a class doesn't override `fn_Name`, the default returns `NOTHING_TO_DO`. An explicit `set_invoke_callback()` takes priority over the virtual.

### Query metadata

Static metadata is available from Strata without creating an instance:

```cpp
if (auto* info = s.get_class_info(MyWidget::get_class_uid())) {
    for (auto& m : info->members) {
        // m.name, m.kind, m.typeUid, m.interfaceInfo
    }
}
```

Runtime metadata is available through `IMetadata` on any instance:

```cpp
if (auto* meta = interface_cast<IMetadata>(widget)) {
    auto prop  = meta->get_property("width");       // IProperty::Ptr
    auto event = meta->get_event("on_clicked");     // IEvent::Ptr
    auto func  = meta->get_function("reset");       // IFunction::Ptr
}
```

### Properties with change notifications

```cpp
auto prop = PropertyT<float>();
prop.set_value(5.f);

Function onChange([](const IAny* any) -> ReturnValue {
    if (auto v = AnyT<const float>(*any)) {
        std::cout << "new value: " << v.get_value() << std::endl;
    }
    return ReturnValue::SUCCESS;
});
prop.add_on_changed(onChange);

prop.set_value(10.f);  // triggers onChange
```

### Custom Any types

Implement `SingleTypeAny` to back a property with external or shared data:

```cpp
class MyDataAny final : public SingleTypeAny<MyDataAny, Data, IExternalAny>
{
public:
    Data& get_value() const override { return globalData_; }
    ReturnValue set_value(const Data& value) override {
        globalData_ = value;
        invoke_event(on_data_changed(), this);
        return ReturnValue::SUCCESS;
    }
    IEvent::Ptr on_data_changed() const override { return onChanged_; }
};
```

## Architecture

The library is organized in four layers:

```
strata/
  include/               Public headers (available to DLL consumers)
    interface/           Abstract interfaces (pure virtual)
    ext/                 CRTP helpers and template implementations
    api/                 User-facing typed wrappers
    common.h             Uid, type_uid<T>(), get_name<T>()
    array_view.h         Lightweight constexpr span-like view
  src/                   Internal runtime implementations (compiled into DLL)
```

### interface/ -- Contracts

| Header | Description |
|---|---|
| `intf_interface.h` | `IInterface` root with UID-based `get_interface()` and ref-counting; `Interface<T>` CRTP with auto UID |
| `intf_object.h` | `IObject` base, `ISharedFromObject` for self-pointer |
| `intf_metadata.h` | `MemberDesc`, `IMetadata`, `IMetadataContainer`, `STRATA_INTERFACE` macro |
| `intf_property.h` | `IProperty` with type-erased get/set and on_changed |
| `intf_event.h` | `IEvent` with add/remove handler |
| `intf_function.h` | `IFunction` invocable callback |
| `intf_any.h` | `IAny` type-erased value container |
| `intf_external_any.h` | `IExternalAny` for externally-managed data |
| `intf_strata.h` | `IStrata` for type registration and object creation |
| `intf_object_factory.h` | `IObjectFactory` for instance creation |
| `types.h` | `ClassInfo`, `ReturnValue`, `interface_cast`, `interface_pointer_cast` |

### ext/ -- CRTP helpers

| Header | Description |
|---|---|
| `interface_dispatch.h` | `InterfaceDispatch<Interfaces...>` generic `get_interface` dispatching across a pack of interfaces |
| `refcounted_dispatch.h` | `RefCountedDispatch<Interfaces...>` extends `InterfaceDispatch` with intrusive ref-counting |
| `core_object.h` | `ObjectFactory<T>` singleton factory; `CoreObject<T, Interfaces...>` CRTP with factory, self-pointer |
| `object.h` | `Object<T, Interfaces...>` adds `IMetadata` support with collected metadata |
| `metadata.h` | `collected_metadata<Interfaces...>` constexpr array concatenation |
| `any.h` | `BaseAny`, `SingleTypeAny<T>`, `SimpleAny<T>` |
| `event.h` | `LazyEvent` helper for deferred event creation |

### api/ -- User wrappers

| Header | Description |
|---|---|
| `strata.h` | `instance()` singleton access |
| `property.h` | `PropertyT<T>` typed property wrapper |
| `any.h` | `AnyT<T>` typed any wrapper |
| `function.h` | `Function` wrapper with lambda support |

### src/ -- Internal implementations

| File | Description |
|---|---|
| `strata_impl.cpp/h` | `StrataImpl` implementing `IStrata` |
| `metadata_container.cpp/h` | `MetadataContainer` implementing `IMetadata` with lazy member creation |
| `property.cpp/h` | `PropertyImpl` |
| `event.cpp/h` | `EventImpl` |
| `function.cpp/h` | `FunctionImpl` |
| `strata.cpp` | DLL entry point, exports `instance()` |

## Key types

| Type | Role |
|---|---|
| `Uid` | 64-bit FNV-1a hash identifying types and interfaces |
| `Interface<T>` | CRTP base for interfaces; provides `UID`, `INFO`, smart pointer aliases |
| `InterfaceDispatch<Interfaces...>` | Implements `get_interface` dispatching across a pack of interfaces |
| `RefCountedDispatch<Interfaces...>` | Extends `InterfaceDispatch` with atomic ref-counting (`ref`/`unref`) |
| `CoreObject<T, Interfaces...>` | CRTP base for concrete objects (without metadata); auto UID/name, factory, self-pointer |
| `Object<T, Interfaces...>` | Extends `CoreObject` with metadata from all interfaces |
| `PropertyT<T>` | Typed property with `get_value()`/`set_value()` and change events |
| `AnyT<T>` | Typed view over `IAny` |
| `Function` | Wraps `ReturnValue(const IAny*)` callbacks |
| `MemberDesc` | Describes a property, event, or function member |
| `ClassInfo` | UID, name, and `array_view<MemberDesc>` for a registered class |

## Object memory layout

An `Object<T, Interfaces...>` instance carries minimal per-object data. The metadata container is heap-allocated once per object and lazily creates member instances on first access.

### Per-object data (CoreObject / Object)

| Layer | Member | Size (x64) |
|---|---|---|
| InterfaceDispatch | vptr | 8 |
| RefCountedDispatch | refCount (`atomic<int32_t>`) | 4 |
| RefCountedDispatch | flags (`int32_t`) | 4 |
| CoreObject | `self_` (`weak_ptr`) | 16 |
| Object | `meta_` (`unique_ptr<IMetadata>`) | 8 |
| **Total** | | **40 bytes** |

### Per-object data (BaseAny)

`BaseAny` types inherit `RefCountedDispatch<IAny>` directly — `IAny` inherits `IObject` (for factory compatibility) but skips `ISharedFromObject` and the `self_` weak pointer. The single inheritance chain (`IInterface` → `IObject` → `IAny`) means only one vptr, saving 24 bytes total vs. CoreObject.

| Layer | Member | Size (x64) |
|---|---|---|
| InterfaceDispatch | vptr | 8 |
| RefCountedDispatch | refCount (`atomic<int32_t>`) | 4 |
| RefCountedDispatch | flags (`int32_t`) | 4 |
| **BaseAny total** | | **16 bytes** |

Measured sizes (MSVC x64):

| Type | Size |
|---|---|
| `SimpleAny<float>` | 32 bytes |
| `MyDataAny` (with `IExternalAny`) | 48 bytes |

### MetadataContainer (heap-allocated, one per object)

| Member | Size |
|---|---|
| vptr | 8 |
| `members_` (`array_view`) | 16 |
| `owner_` (pointer to owning object) | 8 |
| `instances_` (vector, initially empty) | 24 |
| `dynamic_` (`unique_ptr`, initially null) | 8 |
| **Total (baseline)** | **64 bytes** |

Each accessed member adds a **24-byte** entry to the `instances_` vector:
* an 8-byte metadata index (`size_t`) plus 
* a 16-byte `shared_ptr<IInterface>`. 

Members are created lazily, i.e. only when first accessed via `get_property()`, `get_event()`, or `get_function()`.

Static metadata arrays (`MemberDesc`, `InterfaceInfo`) are `constexpr` data shared across all instances at zero per-object cost.

### Example: MyWidget with 6 members

| Scenario | Object | MetadataContainer | Cached members | Total |
|---|---|---|---|---|
| No members accessed | 40 | 64 | 0 | **104 bytes** |
| 3 members accessed | 40 | 64 | 3 × 24 = 72 | **176 bytes** |
| All 6 members accessed | 40 | 64 | 6 × 24 = 144 | **248 bytes** |

## STRATA_INTERFACE reference

```cpp
STRATA_INTERFACE(
    (PROP, Type, Name),   // generates PropertyT<Type> Name() const
    (EVT, Name),          // generates IEvent::Ptr Name() const
    (FN, Name)            // generates virtual fn_Name(const IAny*),
                          //          IFunction::Ptr Name() const
)
```

For each `(FN, Name)` entry the macro generates:
1. A `virtual ReturnValue fn_Name(const IAny*)` method (default returns `NOTHING_TO_DO`)
2. A static trampoline that routes `IFunction::invoke()` to `fn_Name()`
3. A `MemberDesc` with the trampoline pointer in the metadata array
4. An accessor `IFunction::Ptr Name() const`

A practical example:

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width),
        (EVT, on_clicked),
        (FN, reset)         // virtual fn_reset + accessor reset()
    )
};
```

Each entry produces a `MemberDesc` in a `static constexpr std::array metadata` and a typed accessor method. Up to 32 members per interface. Members track which interface declared them via `InterfaceInfo`.

### Manual metadata and accessors

This is **not** recommended, but if you prefer not to use the `STRATA_INTERFACE` macro (e.g. for IDE autocompletion, debugging, or fine-grained control), you can write the virtual methods, metadata array, and accessor methods by hand. The macro generates three things:

1. For `FN` members: a virtual `fn_Name()` method and a static trampoline function.
2. A `static constexpr std::array metadata` containing `MemberDesc` entries (with trampoline pointers for `FN` members).
3. Non-virtual `const` accessor methods that query `IMetadata` at runtime.

Here is a manual equivalent to the STRATA_INTERFACE-using IMyWidget interface above:

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    // 1. Virtual method and trampoline for FN members
    //    The virtual provides the override point for implementing classes.
    //    The static trampoline casts the void* context back to the interface
    //    type and calls the virtual. _strata_intf_type is a protected alias
    //    for T provided by Interface<T>.
    virtual ReturnValue fn_reset(const IAny*) { return ReturnValue::NOTHING_TO_DO; }
    static ReturnValue _strata_trampoline_reset(void* self, const IAny* args) {
        return static_cast<_strata_intf_type*>(self)->fn_reset(args);
    }

    // 2. Static metadata array
    //    Each entry uses a helper: PropertyDesc<T>(), EventDesc(), or FunctionDesc().
    //    Pass &INFO so each member knows which interface declared it.
    //    INFO is provided by Interface<IMyWidget> automatically.
    //    FunctionDesc accepts an optional trampoline pointer as the third argument.
    static constexpr std::array metadata = {
        PropertyDesc<float>("width", &INFO),
        EventDesc("on_clicked", &INFO),
        FunctionDesc("reset", &INFO, &_strata_trampoline_reset),
    };

    // 3. Typed accessor methods
    //    Each accessor queries IMetadata on the concrete object (via get_interface)
    //    and returns the runtime property/event/function by name.
    //    The free functions get_property(), get_event(), get_function() handle
    //    the null check on the IMetadata pointer.

    PropertyT<float> width() const {
        return PropertyT<float>(::strata::get_property(
            this->template get_interface<IMetadata>(), "width"));
    }

    IEvent::Ptr on_clicked() const {
        return ::strata::get_event(
            this->template get_interface<IMetadata>(), "on_clicked");
    }

    IFunction::Ptr reset() const {
        return ::strata::get_function(
            this->template get_interface<IMetadata>(), "reset");
    }
};
```

The string names passed to `PropertyDesc` / `get_property` (etc.) are used for runtime lookup, so they must match exactly. `&INFO` is a pointer to the `static constexpr InterfaceInfo` provided by `Interface<T>`, which records the interface UID and name for each member.

You can also use `STRATA_METADATA(...)` alone to generate only the metadata array without the accessor methods or virtual methods, then write them yourself.

