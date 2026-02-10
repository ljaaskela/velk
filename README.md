# Strata

A C++17 component object model with interface-based polymorphism, typed properties with change notifications, events, and compile-time metadata with runtime introspection.

The name *Strata* (plural of *stratum*, meaning layers) reflects the library's layered architecture: abstract interfaces at the bottom, CRTP helpers and template implementations in the middle, and user-facing typed wrappers on top.

## Features

- **Interface-based architecture** -- define abstract interfaces with properties, events, and functions
- **Typed properties** -- `PropertyT<T>` wrappers with automatic change notification events
- **Events** -- observable multi-handler events
- **Type-erased values** -- `AnyT<T>` wrappers over a generic `IAny` container
- **Compile-time metadata** -- declare members with `STRATA_INTERFACE`, query them at compile time or runtime
- **Central type system** -- register types, create instances by UID, query class info without instantiation
- **Custom type support** -- extend with user-defined `IAny` implementations for external or shared data

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
        (PROP, float, Width),
        (PROP, float, Height),
        (EVT, OnClicked),
        (FN, Reset)
    )
};
```

### Implement with Object

`Object` automatically collects metadata from all listed interfaces and provides the `IMetadata` implementation. No boilerplate needed.

```cpp
#include <ext/object.h>

class MyWidget : public Object<MyWidget, IMyWidget>
{};
```

Multiple interfaces are supported:

```cpp
class ISerializable : public Interface<ISerializable>
{
public:
    STRATA_INTERFACE(
        (PROP, std::string, Name),
        (FN, Serialize)
    )
};

class MyWidget : public Object<MyWidget, IMyWidget, ISerializable>
{};
```

### Register and create

```cpp
auto& s = Strata();
s.RegisterType<MyWidget>();

auto widget = s.Create<IObject>(MyWidget::GetClassUid());
```

### Use typed accessors

```cpp
if (auto* iw = interface_cast<IMyWidget>(widget)) {
    iw->Width().Set(42.f);
    float w = iw->Width().Get();  // 42.f

    IEvent::Ptr clicked = iw->OnClicked();
    IFunction::Ptr reset = iw->Reset();
}
```

### Query metadata

Static metadata is available from Strata without creating an instance:

```cpp
if (auto* info = s.GetClassInfo(MyWidget::GetClassUid())) {
    for (auto& m : info->members) {
        // m.name, m.kind, m.typeUid, m.interfaceInfo
    }
}
```

Runtime metadata is available through `IMetadata` on any instance:

```cpp
if (auto* meta = interface_cast<IMetadata>(widget)) {
    auto prop  = meta->GetProperty("Width");   // IProperty::Ptr
    auto event = meta->GetEvent("OnClicked");  // IEvent::Ptr
    auto func  = meta->GetFunction("Reset");   // IFunction::Ptr
}
```

### Properties with change notifications

```cpp
auto prop = PropertyT<float>();
prop.Set(5.f);

Function onChange([](const IAny* any) -> ReturnValue {
    if (auto v = AnyT<const float>(*any)) {
        std::cout << "new value: " << v.Get() << std::endl;
    }
    return ReturnValue::SUCCESS;
});
prop.AddOnChanged(onChange);

prop.Set(10.f);  // triggers onChange
```

### Custom Any types

Implement `SingleTypeAny` to back a property with external or shared data:

```cpp
class MyDataAny final : public SingleTypeAny<MyDataAny, Data, IExternalAny>
{
public:
    Data& Get() const override { return globalData_; }
    ReturnValue Set(const Data& value) override {
        globalData_ = value;
        InvokeEvent(OnDataChanged(), this);
        return ReturnValue::SUCCESS;
    }
    IEvent::Ptr OnDataChanged() const override { return onChanged_; }
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
    common.h             Uid, TypeUid<T>(), GetName<T>()
    array_view.h         Lightweight constexpr span-like view
  src/                   Internal runtime implementations (compiled into DLL)
```

### interface/ -- Contracts

| Header | Description |
|---|---|
| `intf_interface.h` | `IInterface` root with UID-based `GetInterface()` and ref-counting; `Interface<T>` CRTP with auto UID |
| `intf_object.h` | `IObject` base, `ISharedFromObject` for self-pointer |
| `intf_metadata.h` | `MemberDesc`, `IMetadata`, `IMetadataContainer`, `STRATA_INTERFACE` macro |
| `intf_property.h` | `IProperty` with type-erased get/set and OnChanged |
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
| `core_object.h` | `RefCountedDispatch<Interfaces...>` dispatch + ref-counting; `CoreObject<T, Interfaces...>` with factory |
| `object.h` | `Object<T, Interfaces...>` adds `IMetadata` support with collected metadata |
| `metadata.h` | `collected_metadata<Interfaces...>` constexpr array concatenation |
| `any.h` | `BaseAny`, `SingleTypeAny<T>`, `SimpleAny<T>` |
| `event.h` | `LazyEvent` helper for deferred event creation |

### api/ -- User wrappers

| Header | Description |
|---|---|
| `strata.h` | `Strata()` singleton access |
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
| `strata.cpp` | DLL entry point, exports `Strata()` |

## Key types

| Type | Role |
|---|---|
| `Uid` | 64-bit FNV-1a hash identifying types and interfaces |
| `Interface<T>` | CRTP base for interfaces; provides `UID`, `INFO`, smart pointer aliases |
| `CoreObject<T, Interfaces...>` | CRTP base for concrete objects (without metadata); auto UID/name, factory, ref-counting |
| `Object<T, Interfaces...>` | Extends `CoreObject` with metadata from all interfaces |
| `PropertyT<T>` | Typed property with `Get()`/`Set()` and change events |
| `AnyT<T>` | Typed view over `IAny` |
| `Function` | Wraps `ReturnValue(const IAny*)` callbacks |
| `MemberDesc` | Describes a property, event, or function member |
| `ClassInfo` | UID, name, and `array_view<MemberDesc>` for a registered class |

## STRATA_INTERFACE reference

```cpp
STRATA_INTERFACE(
    (PROP, Type, Name),   // generates PropertyT<Type> Name() const
    (EVT, Name),          // generates IEvent::Ptr Name() const
    (FN, Name)            // generates IFunction::Ptr Name() const
)
```

Each entry produces a `MemberDesc` in a `static constexpr std::array metadata` and a typed accessor method. Up to 32 members per interface. Members track which interface declared them via `InterfaceInfo`.

## Project structure

```
strata/
  CMakeLists.txt
  README.md
  strata/
    CMakeLists.txt
    include/              Public headers
      common.h            Uid, TypeUid<T>(), GetName<T>()
      array_view.h        Lightweight constexpr span-like view
      interface/          Abstract interfaces
      ext/                CRTP helpers
      api/                User-facing wrappers
    src/                  Internal implementations
  test/
    CMakeLists.txt
    main.cpp              Feature demonstration
```
