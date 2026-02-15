# Strata

[![CI](https://github.com/ljaaskela/strata/actions/workflows/ci.yml/badge.svg)](https://github.com/ljaaskela/strata/actions/workflows/ci.yml)

Strata is a C++17 component object model library with interface-based polymorphism, typed properties with change notifications, events, and compile-time metadata with runtime introspection.

Strata is designed to be built as a shared library (DLL on Windows, .so on Linux). All runtime implementations live inside the shared library, while consumers only depend on public headers containing abstract interfaces and header-only templates. This means the internal implementation can evolve without recompiling consumer code, multiple modules can share a single type registry and object factory, and ABI compatibility is maintained through stable virtual interfaces.

The name *Strata* (plural of *stratum*, meaning layers) reflects the library's layered architecture: abstract interfaces at the bottom, CRTP helpers and template implementations in the middle, and user-facing typed wrappers on top.

## Table of contents

- [Features](#features)
- [Project structure](#project-structure)
- [Building](#building)
- [Testing](#testing)
- [Quick start](#quick-start)
  - [Define an interface](#define-an-interface)
  - [Implement with Object](#implement-with-object)
  - [Register and create](#register-and-create)
  - [Use typed accessors](#use-typed-accessors)
  - [Query metadata](#query-metadata)
  - [Virtual function dispatch](#virtual-function-dispatch)
  - [Typed lambda parameters](#typed-lambda-parameters)
  - [Properties with change notifications](#properties-with-change-notifications)
  - [Custom Any types](#custom-any-types)
  - [Direct state access](#direct-state-access)
  - [Deferred invocation](#deferred-invocation)
- [Architecture](#architecture)
  - [interface/](#interface)
  - [ext/](#ext)
  - [api/](#api)
  - [src/](#src)
  - [Type hierarchy across layers](#type-hierarchy-across-layers)
- [Key types](#key-types)
- [Object memory layout](#object-memory-layout)
  - [ext::ObjectCore](#extobjectcore)
  - [MetadataContainer](#metadatacontainer)
  - [ext::Object](#extobject)
  - [Example: MyWidget with 6 members](#example-mywidget-with-6-members)
  - [Base types](#base-types)
- [STRATA_INTERFACE reference](#strata_interface-reference)
  - [Function member variants](#function-member-variants)
  - [Argument metadata](#argument-metadata)
  - [Practical example](#practical-example)
  - [Manual metadata and accessors](#manual-metadata-and-accessors)

## Features

- **Interface-based architecture:** define abstract interfaces with properties, events, and functions
- **Central type system:** register types, create instances by UID, query class info without instantiation
- **Compile-time metadata:** declare members with `STRATA_INTERFACE`, query them at compile time or runtime
- **Type-erased values:** `Any<T>` wrappers over a generic `IAny` container
- **Typed properties:** `Property<T>` wrappers with automatic change notification events
- **Events:** observable multi-handler events (inheriting IFunction) with immediate or deferred dispatch
- **Virtual function dispatch:** `(FN, Name)` generates overridable `fn_Name()` virtuals with optional typed parameters, automatically wired to `IFunction::invoke()`. `(FN_RAW, Name)` provides raw `FnArgs` access
- **Deferred invocation:** functions and event handlers can be queued for execution during `update()`
- **Custom type support:** extend with user-defined `IAny` implementations for external or shared data

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
  demo/
    CMakeLists.txt
    main.cpp              Feature demonstration
  test/
    CMakeLists.txt
    third_party/
      googletest-1.14.0.tar.gz  Vendored GoogleTest
    test_uid.cpp          Uid construction, parsing, validation
    test_any.cpp          IAny / AnyValue / AnyRef type erasure
    test_property.cpp     Property<T> wrapper and change events
    test_function.cpp     Function, FnArgs, FunctionContext, events
    test_object.cpp       Object system, interfaces, metadata
```

## Building

Requires CMake 3.14+ and a C++17 compiler. Tested with MSVC 2019.

```bash
cmake -B build
cmake --build build --config Release
```

Output: `build/bin/Release/strata.dll` (shared library), `build/bin/Release/demo.exe` (demo), and `build/bin/Release/tests.exe` (unit tests).

## Testing

Unit tests use [GoogleTest 1.14.0](https://github.com/google/googletest), vendored as a tarball in `test/third_party/`. It is extracted into the build directory automatically during CMake configuration.

```bash
build/bin/Release/tests.exe
```

The test suite covers Uid construction/validation, Any/AnyValue/AnyRef type erasure, Property get/set/change events, Function/FnArgs/FunctionContext invocation, event handler add/remove, deferred execution, and the full object system (registration, interface casting, metadata lookup, property defaults, state-backed storage).

## Quick start

### Define an interface

Use `STRATA_INTERFACE` to declare properties, events, and functions. This generates both a static constexpr metadata array and typed accessor methods. Use `RPROP` for read-only properties that can be observed but not written through the public API.

```cpp
#include <interface/intf_metadata.h>

class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width, 0.f),
        (PROP, float, height, 0.f),
        (RPROP, int, id, 0),
        (EVT, on_clicked),
        (FN, reset)
    )
};
```

### Implement with Object

`Object` automatically collects metadata from all listed interfaces and provides the `IMetadata` implementation. Override `fn_<Name>` to provide function logic.

```cpp
#include <ext/object.h>

class MyWidget : public ext::Object<MyWidget, IMyWidget>
{
    ReturnValue fn_reset() override {
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
        (PROP, std::string, name, ""),
        (FN, serialize)
    )
};

class MyWidget : public ext::Object<MyWidget, IMyWidget, ISerializable>
{
    ReturnValue fn_reset() override { /* ... */ return ReturnValue::SUCCESS; }
    ReturnValue fn_serialize() override { /* ... */ return ReturnValue::SUCCESS; }
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

`STRATA_INTERFACE` supports three function forms. `(FN, Name)` generates a zero-arg virtual `fn_Name()`. `(FN, Name, (T1, a1), ...)` generates a typed virtual `fn_Name(T1 a1, ...)` with automatic argument extraction from `FnArgs`. `(FN_RAW, Name)` generates `fn_Name(FnArgs)` for manual argument handling.

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width, 0.f),
        (FN, reset),                       // virtual fn_reset()
        (FN, add, (int, x), (float, y)),   // virtual fn_add(int x, float y)
        (FN_RAW, process)                  // virtual fn_process(FnArgs)
    )
};

class MyWidget : public ext::Object<MyWidget, IMyWidget>
{
    ReturnValue fn_reset() override {
        std::cout << "reset!" << std::endl;
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_add(int x, float y) override {
        std::cout << x + y << std::endl;
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_process(FnArgs args) override {
        // manual unpacking via FunctionContext or Any<const T>
        return ReturnValue::SUCCESS;
    }
};

// All forms are invoked through IFunction::invoke()
auto widget = instance().create<IObject>(MyWidget::get_class_uid());
if (auto* iw = interface_cast<IMyWidget>(widget)) {
    invoke_function(iw->reset());                               // zero-arg
    invoke_function(iw, "add", Any<int>(10), Any<float>(3.f));  // typed
    invoke_function(iw->process());                             // raw
}
```

Each `fn_Name` is pure virtual, so implementing classes must override it. An explicit `set_invoke_callback()` takes priority over the virtual.

#### Function arguments

For **typed-arg** functions (`(FN, Name, (T1, a1), ...)`), the trampoline extracts typed values from `FnArgs` automatically — the override receives native C++ parameters. If fewer arguments are provided than expected, the trampoline returns `INVALID_ARGUMENT`.

For **FN_RAW** functions, arguments arrive as `FnArgs` — a lightweight non-owning view of `{const IAny* const* data, size_t count}`. Access individual arguments with bounds-checked indexing (`args[i]` returns nullptr if out of range) and check the count with `args.count`. Use `FunctionContext` to validate the expected argument count:

```cpp
ReturnValue fn_process(FnArgs args) override {
    if (auto ctx = FunctionContext(args, 2)) {
        auto a = ctx.arg<float>(0);
        auto b = ctx.arg<int>(1);
        // ...
    }
    return ReturnValue::SUCCESS;
}
```

Callers use variadic `invoke_function` overloads — values are automatically wrapped in `Any<T>`:

```cpp
invoke_function(iw->reset());                                   // zero-arg
invoke_function(iw, "add", Any<int>(10), Any<float>(3.14f));    // typed args
invoke_function(iw->process(), Any<int>(42));                   // single IAny arg
invoke_function(widget.get(), "process", 1.f, 2u);             // multi-value (auto-wrapped)
```

#### Typed lambda parameters

`Function` also accepts lambdas with typed parameters. Arguments are automatically extracted from `FnArgs` using `Any<const T>`, so there's no manual unpacking:

```cpp
Function fn([&](const float& a, const int& b) -> ReturnValue {
    // a and b are extracted from FnArgs automatically
    return ReturnValue::SUCCESS;
});

Any<float> x(3.14f);
Any<int> y(42);
const IAny* ptrs[] = {x, y};
fn.invoke(FnArgs{ptrs, 2});
```

Void-returning lambdas are supported — `Function` wraps them to return `ReturnValue::SUCCESS`:

```cpp
Function fn([&](float value) {
    std::cout << "received: " << value << std::endl;
});
```

Zero-arity lambdas work too:

```cpp
Function fn([&]() {
    std::cout << "called!" << std::endl;
});
fn.invoke();  // SUCCESS
```

The three constructor forms are mutually exclusive via SFINAE:

| Callable type | Constructor |
|---|---|
| `ReturnValue(*)(FnArgs)` (raw function pointer) | `Function(CallbackFn*)` |
| Callable with `(FnArgs) -> ReturnValue` | Capturing lambda ctor |
| Callable with typed params (any return) | Typed lambda ctor |

When fewer arguments are provided than the lambda expects, `invoke()` returns `INVALID_ARGUMENT`. Extra arguments are ignored. If an argument's type doesn't match the lambda parameter type, the parameter receives a default-constructed value.

### Query metadata

Static metadata is available from Strata without creating an instance:

```cpp
if (auto* info = s.get_class_info(MyWidget::get_class_uid())) {
    for (auto& m : info->members) {
        // m.name, m.kind, m.interfaceInfo
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
auto prop = Property<float>();
prop.set_value(5.f);

Function onChange([](FnArgs args) -> ReturnValue {
    if (auto v = Any<const float>(args[0])) {
        std::cout << "new value: " << v.get_value() << std::endl;
    }
    return ReturnValue::SUCCESS;
});
prop.add_on_changed(onChange);

prop.set_value(10.f);  // triggers onChange
```

### Custom Any types

Implement `ext::AnyCore` to back a property with external or shared data:

```cpp
class MyDataAny final : public ext::AnyCore<MyDataAny, Data, IExternalAny>
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

### Direct state access

Each interface that declares `PROP` members gets a `State` struct with one field per property, initialized with its declared default. `ext::Object` stores these structs inline, and properties read/write directly into them via `ext::AnyRef<T>`. You can also access the state struct directly through `IPropertyState`, bypassing the property layer entirely.

```cpp
auto widget = instance().create<IObject>(MyWidget::get_class_uid());
auto* iw = interface_cast<IMyWidget>(widget);
auto* ps = interface_cast<IPropertyState>(widget);

// Typed access: returns IMyWidget::State*
auto* state = ps->get_property_state<IMyWidget>();

// State fields are initialized with STRATA_INTERFACE defaults
state->width;   // 100.f
state->height;  // 50.f

// Write through property API, state reflects it
iw->width().set_value(200.f);
state->width;   // 200.f

// Write to state directly, property reads it back
state->height = 75.f;
iw->height().get_value();  // 75.f
```

This is useful for bulk operations like serialization, snapshotting (via `memcpy` for trivially-copyable state), or cases where you want to process raw data without going through the property layer.

Each interface's state is independent:

```cpp
auto* ws = ps->get_property_state<IMyWidget>();       // IMyWidget::State*
auto* ss = ps->get_property_state<ISerializable>();   // ISerializable::State*
```

### Deferred invocation

Functions and event handlers support deferred execution via the `InvokeType` enum (`Immediate` or `Deferred`). Deferred work is queued and executed when `::strata::instance().update()` is called.

#### Defer at the call site

Pass `Deferred` to `invoke()` to queue the entire invocation:

```cpp
auto fn = iw->reset();
invoke_function(fn, args);                                // executes now (default)
invoke_function(fn, args, InvokeType::Deferred);          // queued for update()
```

#### Deferred event handlers

Register a handler as deferred so it is queued each time the event fires, while immediate handlers on the same event still execute synchronously:

```cpp
auto event = iw->on_clicked();
event->add_handler(immediateHandler);                        // fires synchronously
event->add_handler(deferredHandler, InvokeType::Deferred);   // queued for update()

invoke_event(event, args);  // immediateHandler runs now, deferredHandler is queued
instance().update();        // deferredHandler runs here
```

Arguments are cloned when a task is queued, so the original `IAny` does not need to outlive the call. Deferred tasks that themselves produce deferred work will re-queue, and will be handled when `update()` is called the next time.

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

### interface/

Abstract interfaces (pure virtual). These define the ABI contracts.

| Header | Description |
|---|---|
| `intf_interface.h` | `IInterface` root with UID-based `get_interface()` and ref-counting; `Interface<T>` CRTP with auto UID |
| `intf_object.h` | `IObject` base, `ISharedFromObject` for self-pointer |
| `intf_metadata.h` | `MemberDesc`, `IMetadata`, `IMetadataContainer`, `STRATA_INTERFACE` macro |
| `intf_property.h` | `IProperty` with type-erased get/set and on_changed |
| `intf_event.h` | `IEvent` (inherits `IFunction`) with add/remove handler (immediate or deferred) |
| `intf_function.h` | `FnArgs` argument view, `IFunction` invocable callback with `InvokeType` support |
| `intf_any.h` | `IAny` type-erased value container |
| `intf_external_any.h` | `IExternalAny` for externally-managed data |
| `intf_strata.h` | `IStrata` for type registration and object creation |
| `intf_object_factory.h` | `IObjectFactory` for instance creation |
| `types.h` | `ClassInfo`, `ReturnValue`, `interface_cast`, `interface_pointer_cast` |

### ext/

CRTP helpers and template implementations.

| Header | Description |
|---|---|
| `interface_dispatch.h` | `ext::InterfaceDispatch<Interfaces...>` generic `get_interface` dispatching across a pack of interfaces (walks parent interface chain) |
| `refcounted_dispatch.h` | `ext::RefCountedDispatch<Interfaces...>` extends `InterfaceDispatch` with intrusive ref-counting |
| `core_object.h` | `ext::ObjectFactory<T>` singleton factory; `ext::ObjectCore<T, Interfaces...>` CRTP with factory, self-pointer |
| `object.h` | `ext::Object<T, Interfaces...>` adds `IMetadata` support with collected metadata |
| `metadata.h` | `ext::TypeMetadata<T>`, `ext::CollectedMetadata<Interfaces...>` constexpr metadata collection |
| `any.h` | `ext::AnyBase`, `ext::AnyMulti<Types...>`, `ext::AnyCore<T>`, `ext::AnyValue<T>` |
| `event.h` | `ext::LazyEvent` helper for deferred event creation |

### api/

User-facing typed wrappers.

| Header | Description |
|---|---|
| `strata.h` | `instance()` singleton access |
| `property.h` | `ConstProperty<T>` read-only and `Property<T>` typed property wrappers |
| `any.h` | `Any<T>` typed any wrapper |
| `function.h` | `Function` wrapper with lambda support, variadic `invoke_function` overloads |
| `function_context.h` | `FunctionContext` view for multi-arg access with count validation |

### src/

Internal runtime implementations (compiled into the DLL).

| File | Description |
|---|---|
| `strata_impl.cpp/h` | `StrataImpl` implementing `IStrata` |
| `metadata_container.cpp/h` | `MetadataContainer` implementing `IMetadata` with lazy member creation |
| `property.cpp/h` | `PropertyImpl` |
| `function.cpp/h` | `FunctionImpl` (implements `IEvent`, which inherits `IFunction`) |
| `strata.cpp` | DLL entry point, exports `instance()` |

### Type hierarchy across layers

Each concept in Strata has types at up to three layers. The naming follows a consistent pattern:

- **`I` prefix** — pure virtual interface (ABI contract)
- **`Core` suffix** — minimal CRTP base (extend for custom behavior)
- **`Value` / `Simple` / no suffix** — ready-to-use concrete or full-featured base
- **`T` suffix** — typed api wrapper that users hold by value

| Concept | interface/ | ext/ | api/ |
|---------|-----------|------|------|
| **Any** | `IAny` | `ext::AnyBase` | `Any<T>` |
| | | `ext::AnyMulti<Types...>` | |
| | | `ext::AnyCore<Final, T>` | |
| | | `ext::AnyValue<T>` | |
| **Object** | `IObject` | `ext::ObjectCore<Final, Intf...>` | — |
| | | `ext::Object<Final, Intf...>` | |
| **Property** | `IProperty` | — | `ConstProperty<T>`, `Property<T>` |
| **Function** | `IFunction` | — | `Function` |
| **Event** | `IEvent` | `ext::LazyEvent` | — |

**Any hierarchy** (ext/) — three levels for different extension points:

| Class | Role | When to use |
|-------|------|-------------|
| `ext::AnyBase<Final, Intf...>` | Internal base with ref-counting, clone, factory | Rarely used directly |
| `ext::AnyMulti<Final, Types...>` | Multi-type compatible any | When an any must expose multiple type UIDs |
| `ext::AnyCore<Final, T, Intf...>` | Single-type with virtual get/set | Extend for custom storage (external data, shared state) |
| `ext::AnyValue<T>` | Inline storage, ready to use | Default choice for simple typed values |

**Object hierarchy** (ext/) — two levels:

| Class | Role | When to use |
|-------|------|-------------|
| `ext::ObjectCore<Final, Intf...>` | Minimal base (no metadata) | Internal implementations (`PropertyImpl`, `FunctionImpl`, `StrataImpl`) |
| `ext::Object<Final, Intf...>` | Full base with metadata collection | User-defined types with `STRATA_INTERFACE` |

## Key types

| Type | Role |
|---|---|
| `Uid` | 128-bit identifier for types and interfaces; constexpr FNV-1a from type names or user-specified |
| `array_view<T>` | Lightweight constexpr span-like view over contiguous const data |
| `Interface<T, Base>` | CRTP base for interfaces; provides `UID`, `INFO`, smart pointer aliases, `ParentInterface` typedef for dispatch chain walking |
| `ext::InterfaceDispatch<Interfaces...>` | Implements `get_interface` dispatching across a pack of interfaces and their parent interface chains |
| `ext::RefCountedDispatch<Interfaces...>` | Extends `InterfaceDispatch` with atomic ref-counting (`ref`/`unref`) |
| `ext::ObjectCore<T, Interfaces...>` | Minimal CRTP base for objects (without metadata); auto UID/name, factory, self-pointer |
| `ext::Object<T, Interfaces...>` | Full CRTP base; extends `ObjectCore` with metadata from all interfaces |
| `InvokeType` | Enum (`Immediate`, `Deferred`) controlling execution timing |
| `FnArgs` | Non-owning view of function arguments (`{const IAny* const* data, size_t count}`) with bounds-checked `operator[]` |
| `FunctionContext` | Lightweight view over `FnArgs` with count validation and typed `arg<T>(i)` access |
| `DeferredTask` | Nested struct in `IStrata` pairing an `IFunction::ConstPtr` with a cloned `std::vector<IAny::Ptr>` of args |
| `ConstProperty<T>` | Read-only typed property with `get_value()` and change events (returned by `RPROP` accessors) |
| `Property<T>` | Typed property with `get_value()`/`set_value()` and change events |
| `Any<T>` | Typed view over `IAny`; `IAny::clone()` creates a deep copy via the type's factory |
| `Function` | Wraps `ReturnValue(FnArgs)` callbacks |
| `ext::LazyEvent` | Helper that lazily creates an `IEvent` on first access via implicit conversion |
| `MemberDesc` | Describes a property, event, or function member |
| `ClassInfo` | UID, name, and `array_view<MemberDesc>` for a registered class |

## Object memory layout

An `ext::Object<T, Interfaces...>` instance carries minimal per-object data. The metadata container is heap-allocated once per object and lazily creates member instances on first access.

### ext::ObjectCore

Interface infrastructure, reference counting and `ISharedFromObject` semantics. The size depends on how many interfaces the object implements, since multiple inheritance adds a vtable pointer per interface (MSVC x64).

| Layer | Member | Size (x64) |
|---|---|---|
| ext::InterfaceDispatch | vptr per interface | grows with N |
| ext::RefCountedDispatch | refCount (`atomic<int32_t>`) | 4 |
| ext::RefCountedDispatch | flags (`int32_t`) | 4 |
| ext::ObjectCore | `self_` (`weak_ptr`) | 16 |

Measured sizes (MSVC x64):

| Configuration | Interfaces | Size |
|---|---|---|
| `ext::ObjectCore<X>` (minimal) | 1 (ISharedFromObject) | **40 bytes** |
| `ext::ObjectCore<X, IMetadataContainer, IMyWidget, ISerializable>` | 4 | **112 bytes** |

### MetadataContainer

Storage for per-object metadata.

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

### ext::Object

Full object with ext::ObjectCore features, runtime metadata, and per-interface property state storage.

| Layer | Member | Size (x64) |
|---|---|---|
| ext::ObjectCore | [ext::ObjectCore](#extobjectcore) | varies by interface count |
| ext::Object | `meta_` (`unique_ptr<IMetadata>`) | 8 |
| ext::Object | `states_` (tuple of interface `State` structs) | varies by interfaces |
| MetadataContainer | [MetadataContainer](#metadatacontainer), heap-allocated | 64 |

The `states_` tuple contains one `State` struct per interface that declares properties via `STRATA_INTERFACE`. Each `State` struct holds one field per `PROP` member, initialized with its declared default value. Properties backed by state storage use `ext::AnyRef<T>` to read/write directly into these fields.

### Example: MyWidget with 6 members

MyWidget implements IMyWidget (2 PROP + 1 EVT + 1 FN) and ISerializable (1 PROP + 1 FN). `ext::Object` adds IMetadataContainer, totaling 4 interfaces in the dispatch pack (ISharedFromObject subsumes IObject, IMetadataContainer subsumes IMetadata and IPropertyState).

| Component | Size (x64) |
|---|---|
| ext::ObjectCore (4 interfaces) | 112 |
| `meta_` (`unique_ptr<IMetadata>`) | 8 |
| `states_` (`IMyWidget::State` (8) + `ISerializable::State` (32)) | 40 |
| **sizeof(MyWidget)** | **160 bytes** |

MetadataContainer (64 bytes) is heap-allocated separately. Member instances are created lazily as accessed.

| Scenario | MyWidget | MetadataContainer | Cached members | Total |
|---|---|---|---|---|
| No members accessed | 160 | 64 | 0 | **224 bytes** |
| 3 members accessed | 160 | 64 | 3 × 24 = 72 | **296 bytes** |
| All 6 members accessed | 160 | 64 | 6 × 24 = 144 | **368 bytes** |

### Base types

#### Any

`ext::AnyBase` types inherit `ext::RefCountedDispatch<IAny>` directly
* `IAny` inherits `IObject` (for factory compatibility) but skips `ISharedFromObject` and the `self_` weak pointer. 
* The single inheritance chain (`IInterface` → `IObject` → `IAny`) means only one vptr, saving 24 bytes total vs. `ext::ObjectCore`.

| Layer | Member | Size (x64) |
|---|---|---|
| ext::InterfaceDispatch | vptr | 8 |
| ext::RefCountedDispatch | refCount (`atomic<int32_t>`) | 4 |
| ext::RefCountedDispatch | flags (`int32_t`) | 4 |
| **ext::AnyBase total** | | **16 bytes** |

`ext::AnyValue<T>` adds the stored value on top of the `ext::AnyBase` base. Measured sizes (MSVC x64):

| Type | Size |
|---|---|
| `ext::AnyValue<float>` | 32 bytes |

An example of a custom any with external data storage `MyDataAny` can be found from the demo application. 

| Type | Size |
|---|---|
| `MyDataAny` (with `IExternalAny`) | 48 bytes |

#### Function 

`ClassId::Function` and `ClassId::Event` are implemented by the same class.

`ClassId::Function` implements `IFunctionInternal` and `IEvent` (which inherits `IFunction`). 
* The primary invoke target uses a unified context/function-pointer pair.
* Plain callbacks go through a static trampoline.
* Event handlers are stored in a single partitioned vector.

| Layer | Member | Size (x64) |
|---|---|---|
| ext::InterfaceDispatch | vptr | 8 |
| ext::RefCountedDispatch | refCount (`atomic<int32_t>`) | 4 |
| ext::RefCountedDispatch | flags (`int32_t`) | 4 |
| ext::ObjectCore | `self_` (`weak_ptr`) | 16 |
| FunctionImpl | `target_context_` (`void*`) | 8 |
| FunctionImpl | `target_fn_` (`BoundFn*`) | 8 |
| FunctionImpl | `handlers_` (`vector<ConstPtr>`) | 24 |
| FunctionImpl | `deferred_begin_` (`uint32_t`) + padding | 8 |
| **Total** | | **80 bytes** |

The `handlers_` vector is partitioned: 
* `[0, deferred_begin_)` holds immediate handlers
* `[deferred_begin_, size())` holds deferred handlers.
* When no handlers are registered the vector is empty (zero heap allocation).

#### Property

`ClassId::Property` implements `IProperty` and `IPropertyInternal`. It holds a shared pointer to its backing `IAny` storage and a `LazyEvent` for change notifications.

| Layer | Member | Size (x64) |
|---|---|---|
| ext::InterfaceDispatch | vptr | 8 |
| ext::RefCountedDispatch | refCount (`atomic<int32_t>`) | 4 |
| ext::RefCountedDispatch | flags (`int32_t`) | 4 |
| ext::ObjectCore | `self_` (`weak_ptr`) | 16 |
| PropertyImpl | `data_` (`shared_ptr<IAny>`) | 16 |
| PropertyImpl | `onChanged_` (`ext::LazyEvent`) | 16 |
| PropertyImpl | `external_` (`bool`) + padding | 8 |
| **Total** | | **72 bytes** |

`ext::LazyEvent` contains a single `shared_ptr<IEvent>` (16 bytes) that is null until first access, deferring the cost of creating the underlying `FunctionImpl` until a handler is actually registered or the event is invoked.

## STRATA_INTERFACE reference

```cpp
STRATA_INTERFACE(
    (PROP, Type, Name, Default),          // Property<Type> Name() const
    (RPROP, Type, Name, Default),         // ConstProperty<Type> Name() const (read-only)
    (EVT, Name),                          // IEvent::Ptr Name() const
    (FN, Name),                           // virtual fn_Name()          (zero-arg)
    (FN, Name, (T1, a1), (T2, a2)),       // virtual fn_Name(T1 a1, T2 a2) (typed)
    (FN_RAW, Name)                        // virtual fn_Name(FnArgs)   (raw untyped)
)
```

### Function member variants

`FN` and `FN_RAW` are the two tags for function members. `FN` supports zero-arg and typed-arg forms; `FN_RAW` preserves the untyped `FnArgs` signature.

| Syntax | Virtual generated | Arg metadata | Use case |
|--------|------------------|--------------|----------|
| `(FN, reset)` | `fn_reset()` | none | Zero-arg functions |
| `(FN, add, (int, x), (float, y))` | `fn_add(int x, float y)` | `FnArgDesc` per param | Typed args with reflection |
| `(FN_RAW, process)` | `fn_process(FnArgs)` | none | Raw untyped args |

All three variants generate:
1. A pure virtual method (signature depends on variant)
2. A static trampoline that routes `IFunction::invoke()` to the virtual
3. A `MemberDesc` with the trampoline pointer in the metadata array
4. An accessor `IFunction::Ptr Name() const`

For typed-arg functions, the trampoline automatically extracts each argument from `FnArgs` using `IAny::get_data()` with type deduction from the member function pointer. If fewer arguments are provided than expected, the trampoline returns `INVALID_ARGUMENT`. Extra arguments are ignored.

### Argument metadata

Typed-arg functions store a `static constexpr FnArgDesc[]` array alongside the trampoline in `FunctionKind`:

```cpp
struct FnArgDesc {
    std::string_view name;   // parameter name (e.g. "x")
    Uid typeUid;             // type_uid<T>() for the parameter type
};

struct FunctionKind {
    FnTrampoline trampoline;
    array_view<FnArgDesc> args;  // empty for zero-arg and FN_RAW
};
```

Access via `MemberDesc::functionKind()->args`:

```cpp
if (auto* info = instance().get_class_info(MyWidget::get_class_uid())) {
    for (auto& m : info->members) {
        if (auto* fk = m.functionKind(); fk && !fk->args.empty()) {
            for (auto& arg : fk->args) {
                // arg.name, arg.typeUid
            }
        }
    }
}
```

### Practical example

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width, 0.f),
        (EVT, on_clicked),
        (FN, reset),                        // zero-arg
        (FN, add, (int, x), (float, y))     // typed args
    )
};

class ISerializable : public Interface<ISerializable>
{
public:
    STRATA_INTERFACE(
        (PROP, std::string, name, ""),
        (FN_RAW, serialize)                 // raw FnArgs
    )
};

class MyWidget : public ext::Object<MyWidget, IMyWidget, ISerializable>
{
    ReturnValue fn_reset() override {
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_add(int x, float y) override {
        // x and y are extracted from FnArgs automatically
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_serialize(FnArgs args) override {
        // manual arg unpacking via FunctionContext or Any<const T>
        return ReturnValue::SUCCESS;
    }
};
```

Invocation works the same for all variants — callers always go through `IFunction::invoke()`:

```cpp
auto widget = instance().create<IObject>(MyWidget::get_class_uid());
if (auto* iw = interface_cast<IMyWidget>(widget)) {
    invoke_function(iw->reset());                            // zero-arg
    invoke_function(iw, "add", Any<int>(10), Any<float>(3.14f)); // typed
}
if (auto* is = interface_cast<ISerializable>(widget)) {
    invoke_function(is, "serialize");                         // FN_RAW
}
```

Each entry produces a `MemberDesc` in a `static constexpr std::array metadata` and a typed accessor method. Up to 32 members per interface. Up to 8 typed parameters per function. Members track which interface declared them via `InterfaceInfo`.

### Manual metadata and accessors

This is **not** recommended, but if you prefer not to use the `STRATA_INTERFACE` macro (e.g. for IDE autocompletion, debugging, or fine-grained control), you can write everything by hand. The macro generates five things:

1. A `State` struct containing one field per `PROP`/`RPROP` member, initialized with its default value.
2. Per-property statics: a `static constexpr PropertyKind` generated via `detail::PropBind<State, &State::member>` (or `detail::PropBind<State, &State::member, ObjectFlags::ReadOnly>` for `RPROP`), which provides `typeUid`, `getDefault`, `createRef`, and `flags` automatically.
3. A `static constexpr std::array metadata` containing `MemberDesc` entries (with `PropertyKind` pointers for `PROP`/`RPROP` members and `FunctionKind` pointers for `FN`/`FN_RAW` members).
4. Non-virtual `const` accessor methods that query `IMetadata` at runtime. `PROP` returns `Property<T>`, `RPROP` returns `ConstProperty<T>`.
5. For function members: a virtual method, a static trampoline, and (for typed-arg `FN`) a `static constexpr FnArgDesc[]` array.

Here is a manual equivalent showing all three function variants:

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    // 1. State struct
    //    One field per PROP, initialized with its declared default.
    //    ext::Object<T, IMyWidget> stores a copy of this struct, and properties
    //    read/write directly into it via ext::AnyRef<T>.
    struct State {
        float width = 0.f;
    };

    // 2. Property kind statics via PropBind
    //    detail::PropBind<State, &State::member> provides typeUid, getDefault,
    //    and createRef automatically. typeUid is type_uid<T>() for the property's
    //    value type. getDefault returns a pointer to a static ext::AnyRef<T>
    //    pointing into a shared default State singleton. createRef creates an
    //    ext::AnyRef<T> pointing into the State struct at the given base address,
    //    used by MetadataContainer to back properties with contiguous state storage.
    static constexpr PropertyKind _strata_propkind_width =
        detail::PropBind<State, &State::width>::kind;

    // 5a. Zero-arg FN: virtual + trampoline (uses detail::interface_trampoline)
    virtual ReturnValue fn_reset() = 0;
    static ReturnValue _strata_trampoline_reset(void* self, FnArgs args) {
        return detail::interface_trampoline(self, args, &_strata_intf_type::fn_reset);
    }
    static constexpr FunctionKind _strata_fnkind_reset {
        &_strata_trampoline_reset };

    // 5b. Typed-arg FN: virtual with typed params + trampoline + FnArgDesc array
    virtual ReturnValue fn_add(int x, float y) = 0;
    static ReturnValue _strata_trampoline_add(void* self, FnArgs args) {
        return detail::interface_trampoline(self, args, &_strata_intf_type::fn_add);
    }
    static constexpr FnArgDesc _strata_fnargs_add[] = {
        {"x", type_uid<int>()}, {"y", type_uid<float>()}
    };
    static constexpr FunctionKind _strata_fnkind_add {
        &_strata_trampoline_add, {_strata_fnargs_add, 2} };

    // 5c. FN_RAW: virtual with FnArgs + direct trampoline (no type extraction)
    virtual ReturnValue fn_process(FnArgs) = 0;
    static ReturnValue _strata_trampoline_process(void* self, FnArgs args) {
        return static_cast<_strata_intf_type*>(self)->fn_process(args);
    }
    static constexpr FunctionKind _strata_fnkind_process {
        &_strata_trampoline_process };

    // 3. Static metadata array
    //    Each entry uses a helper: PropertyDesc(), EventDesc(), or FunctionDesc().
    //    Pass &INFO so each member knows which interface declared it.
    //    INFO is provided by Interface<IMyWidget> automatically.
    //    PropertyDesc accepts an optional PropertyKind pointer as the third argument.
    //    The PropertyKind carries the type UID, so PropertyDesc doesn't need a template argument.
    //    FunctionDesc accepts an optional FunctionKind pointer as the third argument.
    static constexpr std::array metadata = {
        PropertyDesc("width", &INFO, &_strata_propkind_width),
        EventDesc("on_clicked", &INFO),
        FunctionDesc("reset", &INFO, &_strata_fnkind_reset),
        FunctionDesc("add", &INFO, &_strata_fnkind_add),
        FunctionDesc("process", &INFO, &_strata_fnkind_process),
    };

    // 4. Typed accessor methods
    //    Each accessor queries IMetadata on the concrete object (via get_interface)
    //    and returns the runtime property/event/function by name.
    //    The free functions get_property(), get_event(), get_function() handle
    //    the null check on the IMetadata pointer.

    Property<float> width() const {
        return Property<float>(::strata::get_property(
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

    IFunction::Ptr add() const {
        return ::strata::get_function(
            this->template get_interface<IMetadata>(), "add");
    }

    IFunction::Ptr process() const {
        return ::strata::get_function(
            this->template get_interface<IMetadata>(), "process");
    }
};
```

The key differences between variants:
- **Zero-arg FN** and **typed-arg FN** both use `detail::interface_trampoline`, which deduces parameter types from the member function pointer and extracts values from `FnArgs` automatically. Typed-arg functions additionally store a `FnArgDesc` array in their `FunctionKind`.
- **FN_RAW** uses a direct trampoline that passes `FnArgs` through to the virtual unchanged, matching the pre-typed-args behavior.

The string names passed to `PropertyDesc` / `get_property` (etc.) are used for runtime lookup, so they must match exactly. `&INFO` is a pointer to the `static constexpr InterfaceInfo` provided by `Interface<T>`, which records the interface UID and name for each member.

You can also use `STRATA_METADATA(...)` alone to generate the `State` struct, property kind statics, and metadata array without the accessor methods or virtual methods, then write them yourself.

