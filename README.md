# Strata

[![CI](https://github.com/ljaaskela/strata/actions/workflows/ci.yml/badge.svg)](https://github.com/ljaaskela/strata/actions/workflows/ci.yml)

Strata is a C++17 component object model library with interface-based polymorphism, typed properties with change notifications, events, and compile-time metadata with runtime introspection.

Strata is designed to be built as a shared library (DLL on Windows, .so on Linux). All runtime implementations live inside the shared library, while consumers only depend on public headers containing abstract interfaces and header-only templates. This means the internal implementation can evolve without recompiling consumer code, multiple modules can share a single type registry and object factory, and ABI compatibility is maintained through stable virtual interfaces.

The name *Strata* (plural of *stratum*, meaning layers) reflects the library's layered architecture: abstract interfaces at the bottom, CRTP helpers and template implementations in the middle, and user-facing typed wrappers on top.

## Table of contents

- [Features](#features)
- [Documentation](#documentation)
- [Project structure](#project-structure)
- [Building](#building)
- [Testing](#testing)
- [Quick start](#quick-start)
  - [Define an interface](#define-an-interface)
  - [Implement with Object](#implement-with-object)
  - [Register and create](#register-and-create)
  - [Use typed accessors](#use-typed-accessors)
  - [Query metadata](#query-metadata)
  - [Direct state access](#direct-state-access)

## Features

| Feature | Description |
|---|---|
| **Interfaces** | Define abstract contracts with properties, events, and functions |
| **Type registry** | Register types, create instances by UID, query class info, directly from [type registry](#query-metadata-without-instance) or from [an object](#query-metadata-from-object).|
| **Compile-time metadata** | Declare members with `STRATA_INTERFACE`, introspect at compile time or runtime |
| **Type-erased values** | `Any<T>` wrappers over a generic `IAny` container |
| **Typed properties** | `Property<T>` with get/set and automatic change notifications |
| **Direct state access** | Read/write property data with zero overhead, `memcpy`-able object state for trivially-copyable types |
| **Events** | Observable events with multiple handlers, immediate or deferred |
| **Functions** | Overridable virtual functions with optional typed parameters |
| **Deferred invocation** | Queue function calls and event handlers for batch execution during `instance().update()` |
| **Extensible** | Implement custom `IAny` types for used-defined types, external or shared data storage |
| **High performance** | Inline state structs, lazy member instantiation, single-indirect-call function dispatch, and cache-friendly metadata lookups |

## Documentation

| Document | Contents |
|---|---|
| [Quick start](#quick-start) | Getting started |
| [Architecture](docs/architecture.md) | Four-layer design, header reference tables, type hierarchy, key types |
| [Guide](docs/guide.md) | Virtual function dispatch, typed lambdas, change notifications, custom Any types, direct state access, deferred invocation |
| [Performance](docs/performance.md) | Operation costs, memory layout, object sizes |
| [STRATA_INTERFACE](docs/strata-interface.md) | Macro reference, function variants, argument metadata, manual metadata |

## Project structure

```
strata/
  CMakeLists.txt
  README.md               This file
  benchmark/              Benchmarks (Google Benchmark)
  demo/                   Feature demonstration
  docs/                   Documentation
    architecture.md       Layers, headers, type hierarchy, key types
    guide.md              Extended usage guide
    performance.md        Performance and memory usage
    strata-interface.md   STRATA_INTERFACE macro reference
  strata/
    include/              Public API (consumers depend only on these headers)
      interface/          Abstract interfaces (ABI contracts)
      ext/                CRTP helpers and template implementations
      api/                User-facing typed wrappers
    src/                  DLL internals (compiled into strata.dll)
  test/                   Unit tests (GoogleTest)
```

## Building

Requires CMake 3.14+ and a C++17 compiler. Tested with MSVC 2019.

```bash
cmake -B build
cmake --build build --config Release
```

Output: 
* `build/bin/Release/strata.dll` (shared library)
* `build/bin/Release/demo.exe` (demo)
* `build/bin/Release/tests.exe` (unit tests)
* `build/bin/Release/benchmarks.exe` (benchmarks)

## Testing

Unit tests use [GoogleTest 1.14.0](https://github.com/google/googletest) (vendored in `test/third_party/`).
Benchmarks use [Google Benchmark 1.9.1](https://github.com/google/benchmark) (vendored in `benchmark/third_party/`).

Both are extracted into the build directory automatically during CMake configuration.

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
        (FN, reset),
        (FN, resize, (float, w), (float, h))
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
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_resize(float w, float h) override {
        width().set_value(w);   // width property defined in STRATA_INTERFACE
        height().set_value(h);
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
    ReturnValue fn_reset() override { /* ... */ return ReturnValue::SUCCESS; }                  // zero-arg
    ReturnValue fn_resize(float w, float h) override { /* ... */ return ReturnValue::SUCCESS; } // typed args
    ReturnValue fn_serialize() override { /* ... */ return ReturnValue::SUCCESS; }              // from ISerializable
};
```

### Register and create

```cpp
auto& s = instance();                                           // global type registry
s.register_type<MyWidget>();                                    // register factory

auto widget = s.create<IObject>(MyWidget::get_class_uid());     // create by UID
```

### Use typed accessors

```cpp
auto widget = s.create<IMyWidget>(MyWidget::get_class_uid());
if (widget) {             // query interface
    iw->width().set_value(42.f);                                // set property
    float w = iw->width().get_value();                          // read property

    Event clicked = iw->on_clicked();                           // get event handle
    Function reset = iw->reset();                               // get function handle
}
```

### Query metadata without instance

Static metadata is available from Strata without creating an instance:

```cpp
if (auto* info = instance().get_class_info(MyWidget::get_class_uid())) {  // lookup by UID
    for (auto& m : info->members) {
        // m.name, m.kind, m.interfaceInfo
    }
}
```

### Query metadata from object

Runtime metadata is available through `IMetadata` on any instance:

```cpp
auto widget = s.create(MyWidget::get_class_uid());
if (auto* meta = interface_cast<IMetadata>(widget)) {           // runtime introspection
    auto prop  = meta->get_property("width");                   // lookup by name
    auto event = meta->get_event("on_clicked");                 // lookup by name
    auto func  = meta->get_function("reset");                   // lookup by name
}
```

### Direct state access

Properties are backed by inline `State` structs. You can bypass the property layer and access them directly through `IPropertyState`:

This is useful for bulk operations like serialization or snapshotting (`memcpy` for trivially-copyable state).

```cpp
auto widget = s.create(MyWidget::get_class_uid());
if (auto* ps = interface_cast<IPropertyState>(widget)) {
    if (auto* state = ps->get_property_state<IMyWidget>()) {    // IMyWidget::State*
        state->width = 200.f;                                   // write directly, no notifications
        float w = state->width;                                 // read with zero overhead
        iw->width().get_value();                                // reads the same field (200.f) with
    }
}
```
