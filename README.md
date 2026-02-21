# Velk

[![Linux](https://img.shields.io/github/actions/workflow/status/ljaaskela/velk/linux.yml?label=Linux)](https://github.com/ljaaskela/velk/actions/workflows/linux.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/ljaaskela/velk/windows.yml?label=Windows)](https://github.com/ljaaskela/velk/actions/workflows/windows.yml)
[![macOS](https://img.shields.io/github/actions/workflow/status/ljaaskela/velk/macos.yml?label=macOS)](https://github.com/ljaaskela/velk/actions/workflows/macos.yml)
[![CodeQL](https://github.com/ljaaskela/velk/actions/workflows/codeql.yml/badge.svg)](https://github.com/ljaaskela/velk/actions/workflows/codeql.yml)

Velk is a C++17 component object model library with interface-based polymorphism, typed properties with change notifications, events, and compile-time metadata with runtime introspection.

Velk is designed to be built as a shared library (DLL on Windows, .so on Linux). All runtime implementations live inside the shared library, while consumers only depend on public headers containing abstract interfaces and header-only templates. This means the internal implementation can evolve without recompiling consumer code, multiple modules can share a single type registry and object factory, and ABI compatibility is maintained through stable virtual interfaces.

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

---

## Features

| Feature | Description |
|---|---|
| **ABI stable** | Functionality through interfaces defining abstract contracts with properties, events, and functions |
| **Extensible** | <p>Register inline or DLL-based plugins, declarative dependencies and multi-plugin bundles through [plugin registry](docs/plugins.md).<p>Register types, create instances by UID, query class info, directly from [type registry](#query-metadata-without-instance) or from [an object](#query-metadata-from-object). |
| **Compile-time metadata** | Declare members with `VELK_INTERFACE`, introspect at compile time or runtime |
| **Properties** | <p>`Property<T>` with get/set and automatic change notifications<p>Type-erased values through `Any<T>` wrappers over a generic `IAny` container<p>Direct statae access to Read/write property data with zero overhead, `memcpy`-able object state for trivially-copyable types |
| **Events** | Observable events with multiple handlers, immediate or deferred |
| **Functions** | <p>Overridable virtual functions with optional typed parameters and native return types, automatically wrapped into `IAny::Ptr`<p>Promise/Future pairs with typed results, `.then()` chaining, type transforms, and thread-safe resolution<p>Deferred function calls and event handlers for batch execution during `instance().update()` |
| **Performance-focused** | <p>Inline state structs, lazy member instantiation, single-indirect-call function dispatch, and cache-friendly metadata lookups<p>No RTTI or exceptions, builds with `/GR- /EHs-c-` (MSVC) or `-fno-rtti -fno-exceptions` (GCC/Clang) |

## Documentation

| Document | Contents |
|---|---|
| [Quick start](#quick-start) | Getting started |
| [Architecture](docs/architecture.md) | Four-layer design, header reference tables, type hierarchy, key types |
| [Guide](docs/guide.md) | Virtual function dispatch, typed lambdas, change notifications, custom Any types, direct state access, deferred invocation, futures and promises |
| [Performance](docs/performance.md) | Operation costs, memory layout, object sizes |
| [Plugins](docs/plugins.md) | Writing plugins, DLL loading, dependencies, multi-plugin bundles |
| [VELK_INTERFACE](docs/velk-interface.md) | Macro reference, function variants, argument metadata, manual metadata |

## Project structure

```
velk/
  CMakeLists.txt
  README.md               This file
  benchmark/              Benchmarks (Google Benchmark)
  demo/                   Feature demonstration
  docs/                   Documentation
    architecture.md       Layers, headers, type hierarchy, key types
    guide.md              Extended usage guide
    performance.md        Performance and memory usage
    velk-interface.md     VELK_INTERFACE macro reference
  velk/
    include/              Public API (consumers depend only on these headers)
      interface/          Abstract interfaces (ABI contracts)
      ext/                CRTP helpers and template implementations
      api/                User-facing typed wrappers
    src/                  DLL internals (compiled into velk.dll)
  test/                   Unit tests (GoogleTest)
```

## Building

Requires CMake 3.14+ and a C++17 compiler. Tested with MSVC 2019.

```bash
cmake -B build
cmake --build build --config Release
```

Output: 
* `build/bin/Release/velk.dll` (shared library)
* `build/bin/Release/demo.exe` (demo)
* `build/bin/Release/tests.exe` (unit tests)
* `build/bin/Release/benchmarks.exe` (benchmarks)

## Testing

Unit tests use [GoogleTest 1.14.0](https://github.com/google/googletest) (vendored in `test/third_party/`).
Benchmarks use [Google Benchmark 1.9.1](https://github.com/google/benchmark) (vendored in `benchmark/third_party/`).

Both are extracted into the build directory automatically during CMake configuration.

## Quick start

### Define an interface

Use `VELK_INTERFACE` to declare properties, events, and functions. This generates both a static constexpr metadata array and typed accessor methods. Use `RPROP` for read-only properties that can be observed but not written through the public API.

```cpp
#include <interface/intf_metadata.h>

class IMyWidget : public Interface<IMyWidget>
{
public:
    // Static interface metadata: 2 writable properties, 1 read-only property, 1 event and 2 functions
    VELK_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, float, height, 100.f),
        (RPROP, int, id, 0),
        (EVT, on_clicked),
        (FN, void, reset),
        (FN, void, resize, (float, w), (float, h))
    )

    // Regular pure virtual function, part of the interface but not metadata.
    virtual void paint() = 0; 
};
```

### Implement with Object

`Object` automatically collects metadata from all listed interfaces and provides the `IMetadata` implementation. Override `fn_<Name>` to provide function logic.

```cpp
#include <ext/object.h>

class MyWidget : public ext::Object<MyWidget, IMyWidget>
{
    // Implementations of the IMyWidget static metadata-defined functions "reset" and "resize"
    void fn_reset() override {
    }
    void fn_resize(float w, float h) override {
        width().set_value(w);   // width property as defined in VELK_INTERFACE
        height().set_value(h);
    }

    // Implementation of the pure virtual non-metadata function defined in IMyWidget
    void paint() override {}
};
```

Multiple interfaces are supported:

```cpp
class ISerializable : public Interface<ISerializable>
{
public:
    VELK_INTERFACE(
        (PROP, std::string, name, ""),
        (FN, void, serialize)
    )
};

class MyWidget : public ext::Object<MyWidget, IMyWidget, ISerializable>
{
    // Implementations for the functions defined in IMyWidget and ISerializable static metadata
    void fn_reset() override { /* ... */ }                  // zero-arg
    void fn_resize(float w, float h) override { /* ... */ } // typed args
    void fn_serialize() override { /* ... */ }              // from ISerializable

    // Non-metadata paint() from IMyWidget
    void paint() override {}
};
```

### Register and create

```cpp
auto& s = instance();                                           // global IVelk
s.type_registry().register_type<MyWidget>();                    // register factory

auto widget = s.create<IObject>(MyWidget::class_id());     // create by UID
```

### Use typed accessors

```cpp
auto widget = s.create<IMyWidget>(MyWidget::class_id());
if (widget) {
    auto wp = iw->width();
    wp.set_value(42.f);                                         // set property
    float w = wp.get_value();                                   // read property

    Event clicked = iw->on_clicked();                           // get event handle
    clicked.add_handler([]() { std::cout << "clicked"; });      // add event handler
    Function reset = iw->reset().invoke();                      // get function handle
}
```

### Query metadata without instance

Static metadata is available from Velk without creating an instance:

```cpp
if (auto* info = instance().type_registry().get_class_info(MyWidget::class_id())) {  // lookup by UID
    for (auto& i : info->interfaces) {                             // enumerate interfaces
        // i.uid, i.name
    }
    for (auto& m : info->members) {                                // enumerate members
        // m.name, m.kind, m.interfaceInfo
    }
}
```

### Query metadata from object

Runtime metadata is available through `IMetadata` on any instance:

```cpp
auto widget = s.create(MyWidget::class_id());
if (auto* meta = interface_cast<IMetadata>(widget)) {           // query interface for runtime introspection
    auto prop  = meta->get_property("width");                   // lookup by name
    auto event = meta->get_event("on_clicked");                 // lookup by name
    auto func  = meta->get_function("reset");                   // lookup by name
}
```

### Direct state access

Properties are backed by inline `State` structs. You can bypass the property layer and access them directly through `IPropertyState`:

This is useful for bulk operations like serialization or snapshotting (`memcpy` for trivially-copyable state).

```cpp
auto widget = s.create(MyWidget::class_id());
if (auto* state = get_property_state<IMyWidget>(widget.get())) {// IMyWidget::State*
    // State struct generated through VELK_INTERFACE in IMyWidget declaration
    // struct IWidget::State { 
    //   float width { 100.f };
    //   float height { 100.f };
    //   int id { 0 };
    // }
    state->width = 200.f;                                       // write directly, no notifications
    float w = state->width;                                     // read with zero overhead
    iw->width().get_value();                                    // reads the same field (200.f) through property accessor
}
```
