# Velk

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

[![Linux](https://img.shields.io/github/actions/workflow/status/ljaaskela/velk/linux.yml?label=Linux)](https://github.com/ljaaskela/velk/actions/workflows/linux.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/ljaaskela/velk/windows.yml?label=Windows)](https://github.com/ljaaskela/velk/actions/workflows/windows.yml)
[![macOS](https://img.shields.io/github/actions/workflow/status/ljaaskela/velk/macos.yml?label=macOS)](https://github.com/ljaaskela/velk/actions/workflows/macos.yml)
[![CodeQL](https://github.com/ljaaskela/velk/actions/workflows/codeql.yml/badge.svg)](https://github.com/ljaaskela/velk/actions/workflows/codeql.yml)

---

Velk is a C++17 component object model library with interface-based polymorphism, typed properties with change notifications, events, compile-time metadata with runtime introspection, and a plugin system for modular extension via shared libraries. 

Velk includes dense, cache-friendly object storage via hives, promise/future pairs with `.then()` chaining, and deferred execution for batching property writes and function calls.

Velk is designed to be built as a shared library (DLL on Windows, .so on Linux). All runtime implementations live inside the shared library, while consumers only depend on public headers containing abstract interfaces and header-only templates. This means the internal implementation can evolve without recompiling consumer code, multiple modules can share a single type registry and object factory, and ABI compatibility is maintained through stable virtual interfaces. 

```cpp
// Define an interface
class IWidget : public Interface<IWidget> {
public:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),
        (EVT, on_clicked),
        (FN, void, reset)
    )
};

// Implement, register, create, use
class Widget : public ext::Object<Widget, IWidget> {
    void fn_reset() override { width().set_value(0.f); }
};

// Register type
auto& instance = velk::instance();
instance.type_registry().register_type<Widget>();

// Instantiate and use
auto w = instance.create<IWidget>(Widget::class_id());
w->width().set_value(42.f);
w->width().on_changed().add_handler([](){ /*...*/ });
w->on_clicked().add_handler([]() { /*...*/ });
velk::invoke_function(w->reset());
```
---
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
| **ABI stable** | Interface-based contracts with properties, events, and functions. Consumers depend only on public headers; internals can change without recompilation |
| **Zero dependencies** | Pure C++17 with platform headers only |
| **Compile-time metadata** | <p>Declare [properties](docs/guide.md#properties), [events, and functions](docs/guide.md#functions-and-events) with [`VELK_INTERFACE`](docs/guide.md#declaring-interfaces), introspect at compile time or runtime<p>Typed properties with change notifications, multicast events, virtual functions with typed parameters and return values, [promise/future](docs/guide.md#futures-and-promises) chaining, and [deferred execution](docs/guide.md#deferred-invocation) |
| **Extensible** | <p>[Plugin registry](docs/plugins.md) for inline or DLL-based plugins with declarative dependencies and multi-plugin bundles<p>[Type registry](#query-metadata-without-instance) for UID-based creation and runtime introspection |
| **[Hive](docs/hive.md) storage** | Dense, cache-friendly containers with slot reuse, zombie lifecycle, and automatic page management |
| **Performance-focused** | <p>Inline state structs, lazy instantiation, single-indirect-call dispatch, and direct state access with zero overhead<p>No RTTI or exceptions, builds with `/GR- /EHs-c-` (MSVC) or `-fno-rtti -fno-exceptions` (GCC/Clang) |

## Documentation

| Document | Description |
|---|---|
| [Quick start](#quick-start) | Define an interface, implement it, create objects, and use typed accessors |
| [Guide](docs/guide.md) | How to declare interfaces, work with properties, functions, events, and futures |
| [Architecture](docs/architecture.md) | How the four layers fit together, type hierarchy, and ABI stability |
| [Hive](docs/hive.md) | Storing objects in dense, cache-friendly containers |
| [Plugins](docs/plugins.md) | Extending Velk with inline or DLL-based plugins |
| [Performance](docs/performance.md) | Benchmark numbers, memory layout, and object sizes |
| [Advanced](docs/advanced.md) | Writing metadata by hand, shared_ptr internals, control block pooling |

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
    hive.md               Dense object storage and hive registry
    performance.md        Performance and memory usage
    plugins.md            Plugin system: writing, loading, dependencies, bundles
    advanced.md           Manual metadata and accessors
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

Use `VELK_INTERFACE` to declare properties, events, and functions. This generates both a static constexpr metadata array and typed accessor methods.

```cpp
#include <interface/intf_metadata.h>

class IMyWidget : public Interface<IMyWidget>
{
public:
    // Static interface metadata:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),                // Read-write float property, default value 100.f
        (PROP, float, height, 100.f),
        (RPROP, int, id, 0),                        // Read-only int property, default value 0
        (ARR, uint32_t, layers, 1u, 2u, 3u),        // Array uint32_t property (array of values), default value {1u, 2u, 3u}
        (EVT, on_clicked),                          // Event
        (FN, void, reset),                          // Function without arguments
        (FN, void, resize, (float, w), (float, h))  // Function with 2 arguments
    )

    // Regular pure virtual function, part of the interface but not metadata.
    virtual void paint() = 0; 
};
```

### Implement with Object

`Object` automatically collects metadata from all listed interfaces and provides the `IMetadata` implementation. Override `fn_<Name>` to provide function logic.

```cpp
#include <velk/ext/object.h>

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
        (PROP, velk::string, name, ""),
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

Properties are backed by inline `State` structs generated by `VELK_INTERFACE`. Use `read_state` and `write_state` for direct struct access with automatic change notifications. Both return a handle that converts to `false` if the interface is not implemented.

```cpp
auto widget = s.create<IObject>(MyWidget::class_id());
auto* iw = interface_cast<IMyWidget>(widget);

// Read state directly
if (auto reader = read_state<IMyWidget>(iw)) {
    float w = reader->width;                                    // 100.f (default)
}

// Write state, fires on_changed for instantiated properties when writer goes out of scope
if (auto writer = write_state<IMyWidget>(iw)) {
    // struct IMyWidget::State {                                 // generated by VELK_INTERFACE
    //   float width { 100.f };
    //   float height { 100.f };
    //   int id { 0 };
    //   velk::vector<uint32_t> layers { 1u, 2u, 3u };
    // }
    writer->width = 200.f;
    writer->height = 50.f;
    writer->layers.push_back(4u);
}                                                               // ~StateWriter notifies listeners

iw->width().get_value();                                        // 200.f
```
