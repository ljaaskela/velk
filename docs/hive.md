# Hive

A hive is a dense, typed container that stores objects of a single class contiguously in memory. Instead of allocating each object individually on the heap, a hive groups objects into cache-friendly pages and constructs them in place. This is useful for systems that manage large numbers of homogeneous objects (entities, particles, nodes) where iteration performance matters.

The design shares core ideas with C++26's `std::hive` (P0447): paged allocation, O(1) erase via a per-page freelist, stable references across insertion and removal, and no element shifting. Where `std::hive` is a general-purpose standard container, Velk's hive is specialized for Velk objects, providing reference counting, interface dispatch, metadata, and zombie/orphan lifetime management on top of the same underlying storage model.

Objects in a hive are full Velk objects with reference counting, metadata, and interface support. They can be passed around as `IObject::Ptr` just like any other object. The only difference is where they live in memory, and that removed objects can [outlive the hive itself](#lifetime-and-zombies) if external references still hold them.

## Contents

- [Class diagram](#class-diagram)
- [Getting started](#getting-started)
- [Hive store](#hive-store)
- [Adding objects](#adding-objects)
- [Removing objects](#removing-objects)
- [Iterating objects](#iterating-objects)
- [Checking membership](#checking-membership)
- [Object flags](#object-flags)
- [Lifetime and zombies](#lifetime-and-zombies)
- [Memory layout](#memory-layout)
- [Performance](#performance)

## Class diagram

```mermaid
classDiagram
    direction TB

    class IHiveStore {
        <<interface>>
        get_hive(classUid)
        find_hive(classUid)
        hive_count()
        for_each_hive()
    }

    class IHive {
        <<interface>>
        add()
        remove(object)
        contains(object)
        for_each(visitor)
        size() / empty()
    }

    class IObject {
        <<interface>>
        get_self()
    }

    class HiveStore {
        ClassId::HiveStore
    }

    class Hive {
        ClassId::Hive
    }

    IHiveStore <|.. HiveStore : implements
    IHive <|.. Hive : implements
    IObject <|-- IHive : extends
    IHiveStore --> "0..*" IHive : manages
    IHive --> "0..*" IObject : stores
```

## Getting started

The hive system is built into Velk as an internal plugin. No setup is required. Create a hive store and start adding objects:

```cpp
#include <velk/interface/hive/intf_hive_store.h>

auto& velk = instance();
auto registry = velk.create<IHiveStore>(ClassId::HiveStore);

// Get (or create) a hive for MyWidget objects
auto hive = registry->get_hive<MyWidget>();

// Add objects
auto obj = hive->add();
```

## Hive store

The hive store manages hives, one per class UID. Create one via `ClassId::HiveStore` and use the `IHiveStore` interface for lazy creation and lookup:

```cpp
auto registry = instance().create<IHiveStore>(ClassId::HiveStore);

// get_hive: returns the hive for the class, creating it if needed
auto hive = registry->get_hive(MyWidget::class_id());

// find_hive: returns the hive if it exists, nullptr otherwise
auto hive = registry->find_hive(MyWidget::class_id());

// Templated versions (use T::class_id() internally)
auto hive = registry->get_hive<MyWidget>();
auto hive = registry->find_hive<MyWidget>();
```

You can enumerate all active hives:

```cpp
registry->hive_count();  // number of active hives

registry->for_each_hive(nullptr, [](void*, IHive& hive) -> bool {
    // hive.get_element_class_uid(), hive.size(), ...
    return true;  // return false to stop early
});
```

Multiple hive stores can coexist independently. Each store maintains its own set of hives.

## Adding objects

`IHive::add()` constructs a new object in the hive and returns a shared pointer:

```cpp
auto hive = registry->get_hive<MyWidget>();

IObject::Ptr obj = hive->add();
auto widget = interface_pointer_cast<IMyWidget>(obj);
widget->width().set_value(200.f);
```

The returned pointer behaves identically to one from `instance().create()`. The object has metadata, supports `interface_cast`, and participates in reference counting.

## Removing objects

`IHive::remove()` removes an object from the hive:

```cpp
hive->remove(*obj);
```

After removal, the object's slot becomes available for reuse. If external references to the object still exist, the object stays alive until the last reference is dropped (see [Lifetime and zombies](#lifetime-and-zombies)).

## Iterating objects

`IHive::for_each()` iterates all live objects in the hive:

```cpp
hive->for_each(nullptr, [](void*, IObject& obj) -> bool {
    auto* widget = interface_cast<IMyWidget>(&obj);
    if (widget) {
        // process widget
    }
    return true;  // return false to stop early
});
```

Objects are stored contiguously within pages, so iteration has good cache locality compared to individually heap-allocated objects.

## Checking membership

```cpp
hive->contains(*obj);  // true if the object is in this hive
hive->size();           // number of live objects
hive->empty();          // true if no objects
```

## Object flags

Objects created by a hive have the `ObjectFlags::HiveManaged` flag set. This allows code to check whether an object is hive-managed without needing a reference to the hive:

```cpp
if (obj->get_object_flags() & ObjectFlags::HiveManaged) {
    // object was created by a hive
}
```

## Lifetime and zombies

A hive holds one strong reference to each of its objects. When you call `add()`, the returned `IObject::Ptr` is a second strong reference. The hive's internal reference keeps the object alive even if the caller drops their pointer.

When `remove()` is called, the hive releases its strong reference. If no external references remain, the object is destroyed immediately and the slot is recycled. If external references still exist, the object enters a **zombie** state: it is no longer visible to `for_each()` or counted by `size()`, but it remains alive in its slot until the last reference is dropped.

When the last reference to a zombie drops, the destructor runs in place and the slot is returned to the page's free list for reuse.

When a hive is destroyed (e.g. its store is released), all active objects are released. Any objects that still have external references become orphans. The underlying page memory is kept alive until the last orphan is destroyed, then freed automatically.

### Slot lifecycle

Each slot in a hive page transitions through three states:

```mermaid
stateDiagram-v2
    [*] --> Free
    Free --> Active : add()
    Active --> Free : remove(), no external refs
    Active --> Zombie : remove(), external refs held
    Zombie --> Free : last ref dropped
```

## Memory layout

Objects are stored in chunked pages. Each page is a contiguous, aligned allocation sized for a fixed number of slots. Pages grow as needed:

| Page | Slot count |
|------|-----------|
| 1st  | 16        |
| 2nd  | 64        |
| 3rd  | 256       |
| 4th+ | 1024      |

Free slots within a page are linked through an intrusive free list stored in the slot memory itself, so there is no per-slot overhead for free slots. Each page also maintains a per-slot state byte (`Active`, `Zombie`, or `Free`) and a pointer to the slot's control block.

Slot reuse is LIFO within a page: the most recently freed slot is the next one allocated. This keeps active objects as dense as possible within each page.

## Performance

The tables below compare three configurations using 512 objects with 10 members (5 floats + 5 ints):

1. **Plain struct** -- `std::vector<PlainData>` (a class with constructor-initialized members, 40 bytes).
2. **Velk vector** -- `std::vector<IObject::Ptr>` of individually heap-allocated Velk objects.
3. **Hive** -- Velk objects stored contiguously in a hive.

Both Velk benchmarks use `get_property_state<T>()` for direct state access, the recommended path for performance-critical bulk operations. No metadata containers are allocated (lazy init is never triggered).

Measured on an x64 desktop (16 cores, 16 KiB L3). MSVC Release build, Google Benchmark. The benchmark source is in `benchmark/main.cpp` (search for `BM_Churn`, `BM_Create`, `BM_Iterate`, `BM_Memory`).

### Memory

| | Per-element | 512 items |
|---|---|---|
| Plain struct | 40 bytes | 20,480 bytes |
| Velk object | 128 bytes | ~65,536 bytes + control blocks |

The per-object overhead (88 bytes) covers the MI base layout (vtable pointers), ObjectData (flags + control block pointer), the lazy metadata pointer, and state tuple alignment padding. Both Velk vector and hive objects have the same per-element size.

### Creation (512 items)

| | Time | Ratio |
|---|---|---|
| `vector<PlainData>(512)` | ~750 ns | 1x |
| Hive `add()` x 512 | ~22,000 ns | ~30x |
| Velk vector `create()` x 512 | ~45,000 ns | ~60x |

The hive is roughly 2x faster than individual heap allocation because it uses placement-new into pre-allocated pages and avoids per-object heap allocations for the object itself (control blocks are still heap-allocated individually).

### Iteration: read all 10 fields (512 items)

| | Time | Ratio |
|---|---|---|
| Plain vector | ~800 ns | 1x |
| Hive + `get_property_state` | ~3,700 ns | ~4.5x |
| Velk vector + `get_property_state` | ~6,400 ns | ~8x |

### Iteration: write all 10 fields (512 items)

| | Time | Ratio |
|---|---|---|
| Plain vector | ~570 ns | 1x |
| Hive + `get_property_state` | ~3,500 ns | ~6x |
| Velk vector + `get_property_state` | ~6,000 ns | ~10x |

The hive's contiguous page layout gives it nearly 2x better iteration performance than a vector of individually heap-allocated objects. The heap-allocated objects are scattered across memory, causing more cache misses on each pointer chase.

### Churn: erase every 4th element + repopulate (512 items, pre-populated)

| | Time | Ratio |
|---|---|---|
| Plain vector (erase + emplace_back) | ~9,800 ns | 1x |
| Hive (remove + add) | ~9,100 ns | **0.93x** |
| Velk vector (erase + create) | ~35,000 ns | ~3.6x |

For churn workloads the hive is slightly faster than a plain vector and nearly 4x faster than a vector of heap-allocated objects. Vector erase shifts all subsequent elements on each removal (O(n) per erase). The hive flips a state byte and pushes the slot onto the freelist (O(1) per remove), then repopulation reuses freed slots via LIFO with no reallocation or element shifting. The heap-allocated vector pays both the O(n) shift cost and the per-object heap allocation cost.

### Summary

For the iteration and creation overhead you get: reference-counted lifetime with `shared_ptr`/`weak_ptr` support, runtime interface dispatch, type-erased metadata available on demand, O(1) removal with slot reuse, zombie/orphan safety, and `ObjectFlags::HiveManaged` tagging. Compared to a vector of heap-allocated Velk objects, the hive is ~2x faster for creation, ~2x faster for iteration, and ~4x faster for churn. For workloads with frequent add/remove churn, the hive matches or beats even a plain `std::vector` of structs.
