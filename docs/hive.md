# Hive

A hive is a dense, typed container that stores objects of a single class contiguously in memory. Instead of allocating each object individually on the heap, a hive groups objects into cache-friendly pages and constructs them in place. This is useful for systems that manage large numbers of homogeneous objects (entities, particles, nodes) where iteration performance matters.

Objects in a hive are full Velk objects with reference counting, metadata, and interface support. They can be passed around as `IObject::Ptr` just like any other object. The only difference is where they live in memory.

## Contents

- [Getting started](#getting-started)
- [Hive registry](#hive-registry)
- [Adding objects](#adding-objects)
- [Removing objects](#removing-objects)
- [Iterating objects](#iterating-objects)
- [Checking membership](#checking-membership)
- [Object flags](#object-flags)
- [Lifetime and zombies](#lifetime-and-zombies)
- [Memory layout](#memory-layout)
- [Performance](#performance)

## Getting started

The hive system is built into Velk as an internal plugin. No setup is required. Create a hive registry and start adding objects:

```cpp
#include <velk/interface/hive/intf_hive_registry.h>

auto& velk = instance();
auto registry = velk.create<IHiveRegistry>(ClassId::HiveRegistry);

// Get (or create) a hive for MyWidget objects
auto hive = registry->get_hive<MyWidget>();

// Add objects
auto obj = hive->add();
```

## Hive registry

`IHiveRegistry` manages hives, one per class UID. It provides lazy creation and lookup:

```cpp
auto registry = instance().create<IHiveRegistry>(ClassId::HiveRegistry);

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

Multiple hive registries can coexist independently. Each registry maintains its own set of hives.

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

When a hive is destroyed (e.g. its registry is released), all active objects are released. Any objects that still have external references become orphans. The underlying page memory is kept alive until the last orphan is destroyed, then freed automatically.

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

The table below compares a plain `std::vector<PlainData>` (a class with 5 floats and 5 ints, constructor-initialized, 40 bytes) against a hive of 512 Velk objects declaring the same 10 members via `VELK_INTERFACE`. The hive benchmarks use `get_property_state<T>()` for direct state access, the recommended path for performance-critical bulk operations. No metadata containers are allocated (lazy init is never triggered).

Measured on an x64 desktop (16 cores, 16 KiB L3). MSVC Release build, Google Benchmark. The benchmark source is in `benchmark/main.cpp` (search for `BM_Churn`, `BM_Create`, `BM_Iterate`, `BM_Memory`).

### Memory

| | Per-element | 512 items |
|---|---|---|
| Plain struct | 40 bytes | 20,480 bytes |
| Hive object | 128 bytes | ~65,536 bytes + control blocks |

The per-object overhead (88 bytes) covers the MI base layout (vtable pointers), ObjectData (flags + control block pointer), the lazy metadata pointer, and state tuple alignment padding.

### Creation (512 items)

| | Time | Ratio |
|---|---|---|
| `vector<PlainData>(512)` | ~750 ns | 1x |
| Hive `add()` x 512 | ~22,000 ns | ~30x |

The hive cost is dominated by per-object control block allocation, placement-new with vtable initialization, and free list management. This is a one-time cost.

### Iteration: read all 10 fields (512 items)

| | Time | Ratio |
|---|---|---|
| Plain vector | ~800 ns | 1x |
| Hive + `get_property_state` | ~3,700 ns | ~4.5x |

### Iteration: write all 10 fields (512 items)

| | Time | Ratio |
|---|---|---|
| Plain vector | ~570 ns | 1x |
| Hive + `get_property_state` | ~3,700 ns | ~6.5x |

The iteration overhead comes from `for_each` function-pointer dispatch, `interface_cast<IPropertyState>` per element, and per-slot state checks. The actual state struct read/write is a direct memory access with no indirection.

### Churn: erase every 4th element + repopulate (512 items, pre-populated)

| | Time | Ratio |
|---|---|---|
| Plain vector (erase + emplace_back) | ~9,800 ns | 1x |
| Hive (remove + add) | ~9,100 ns | **0.93x** |

For churn workloads the hive is slightly faster than vector. Vector erase shifts all subsequent elements on each removal (O(n) per erase). The hive flips a state byte and pushes the slot onto the freelist (O(1) per remove), then repopulation reuses freed slots via LIFO with no reallocation or element shifting.

### Summary

For the iteration and creation overhead you get: reference-counted lifetime with `shared_ptr`/`weak_ptr` support, runtime interface dispatch, type-erased metadata available on demand, O(1) removal with slot reuse, zombie/orphan safety, and `ObjectFlags::HiveManaged` tagging. For workloads with frequent add/remove churn, the hive matches or beats `std::vector`.
