# Advanced topics

## Contents

- [Any types and property value chains](#any-types-and-property-value-chains)
  - [IAny and the ext/ hierarchy](#iany-and-the-ext-hierarchy)
  - [How properties use IAny](#how-properties-use-iany)
  - [IAnyExtension: stacking values on a property](#ianyextension-stacking-values-on-a-property)
  - [Writing an extension](#writing-an-extension)
  - [Installing and removing extensions](#installing-and-removing-extensions)
  - [Chaining multiple extensions](#chaining-multiple-extensions)
  - [Inspecting the chain](#inspecting-the-chain)
- [Manual metadata and accessors](#manual-metadata-and-accessors)
- [shared_ptr and control blocks](#shared_ptr-and-control-blocks)
  - [Control block pooling](#control-block-pooling)

## Any types and property value chains

### IAny and the ext/ hierarchy

`IAny` (in `intf_any.h`) defines the interface for type-erased value containers. It declares `get_data`, `set_data`, `copy_from`, `clone`, and type compatibility queries. Every property stores its data through an `IAny` pointer, and all value transfer across the DLL boundary goes through this interface.

The `ext/` layer provides a hierarchy of CRTP bases that implement `IAny`:

| Class | Purpose |
|---|---|
| `ext::AnyBase<Final, Intf...>` | Internal base; adds ref-counting, `clone()` via factory. Rarely used directly. |
| `ext::AnyMulti<Final, Types...>` | Multi-type compatible any (exposes multiple type UIDs). |
| `ext::AnyCore<Final, T, Intf...>` | Single-type with virtual `get_value()`/`set_value()`. Extend for custom storage. |
| `ext::AnyValue<T>` | Inline storage. The default choice for simple typed values. |
| `ext::AnyRef<T>` | Points to external storage (e.g. a field in a State struct). Does not own the data. |
| `ext::ArrayAnyRef<T>` | External `vector<T>` storage, also implements `IArrayAny` for element-level access. |
| `ext::ArrayAnyValue<T>` | Owned `vector<T>` storage with `IArrayAny`. |

`AnyValue<T>` and `AnyRef<T>` handle the most common cases. `AnyValue<T>` owns its data inline and is what `clone()` produces by default. `AnyRef<T>` points into a struct field, which is how `VELK_INTERFACE` properties work: the State struct lives in the object, and an `AnyRef<T>` provides the IAny interface over each field.

For types that need custom storage or side effects (e.g. data that lives in a GPU buffer, or a value that fires a notification on write), extend `AnyCore<Final, T>` and override `get_value()`/`set_value()`. Adding `IExternalAny` to the interface list lets the property know that the data can change externally, so it wires up automatic `on_changed` notifications.

### How properties use IAny

A property (`PropertyImpl`) holds a single `IAny::Ptr` called `data_`. All reads and writes go through it:

```
Property::set_value(from)  -->  data_->copy_from(from)  -->  on_changed fires
Property::get_value()      -->  returns data_
```

When a property is created by `MetadataContainer` for a `VELK_INTERFACE` member, `data_` is set to an `AnyRef<T>` pointing into the object's State struct. The property doesn't know or care what concrete IAny type backs it. This indirection is what makes extensions possible.

### IAnyExtension: stacking values on a property

`IAnyExtension` (in `intf_any_extension.h`) is an interface that inherits `IAny` and adds three methods for wrapping another `IAny`:

```cpp
class IAnyExtension : public Interface<IAnyExtension, IAny>
{
public:
    virtual IAny::ConstPtr get_inner() const = 0;
    virtual IAny::Ptr take_inner() = 0;
    virtual void set_inner(IAny::Ptr inner) = 0;
};
```

Because `IAnyExtension` inherits `IAny`, an extension can be placed directly into a property's `data_` slot. The extension wraps the previous `data_` as its inner, intercepting reads and writes while delegating to the original value when appropriate.

Extensions form a chain. Each extension's inner points to the next, which may be another extension or the original plain `IAny`:

```
PropertyImpl::data_
  --> ExtensionA (IAnyExtension)
        inner_ --> ExtensionB (IAnyExtension)
                     inner_ --> AnyRef<float> (plain IAny, the original value)
```

When no extensions are installed, `data_` points directly to the plain `IAny` and there is zero overhead.

### Writing an extension

Derive from `ext::AnyExtension<Final, Interfaces...>` (in `ext/any_extension.h`). This CRTP base provides:

1. Default `IAnyExtension` implementation (`get_inner`, `take_inner`, `set_inner`) backed by a protected `inner_` member.
2. Default passthrough implementations for all `IAny` methods that delegate to `inner_`.

Override only the methods you want to intercept:

```cpp
class IValueClamp : public Interface<IValueClamp, IAnyExtension>
{
public:
    virtual void set_bounds(float lo, float hi) = 0;
};

class ValueClamp final : public ext::AnyExtension<ValueClamp, IValueClamp>
{
public:
    VELK_CLASS_UID("...");

    void set_bounds(float lo, float hi) override { lo_ = lo; hi_ = hi; }

    // Intercept writes to clamp the value
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override
    {
        if (type == type_uid<float>() && fromSize == sizeof(float)) {
            float v = *reinterpret_cast<const float*>(from);
            v = std::clamp(v, lo_, hi_);
            return inner_ ? inner_->set_data(&v, sizeof(v), type) : ReturnValue::Fail;
        }
        return inner_ ? inner_->set_data(from, fromSize, type) : ReturnValue::Fail;
    }

private:
    float lo_ = 0.f;
    float hi_ = 1.f;
};
```

Methods you don't override pass through to `inner_` automatically: `get_data`, `get_compatible_types`, `get_data_size`, `copy_from`, `clone`. This means the extension is transparent for operations it doesn't care about.

### Installing and removing extensions

`IPropertyInternal` provides two methods for managing the extension chain:

```cpp
bool install_extension(const IAnyExtension::Ptr& extension);
bool remove_extension(const IAnyExtension::Ptr& extension);
```

`install_extension` sets the extension's inner to the property's current `data_`, then replaces `data_` with the extension. `remove_extension` finds the extension in the chain and relinks around it.

```cpp
auto* pi = interface_cast<IPropertyInternal>(property);

// Install
auto clamp = instance().create<IValueClamp>(ValueClamp::class_id());
clamp->set_bounds(0.f, 100.f);
pi->install_extension(interface_pointer_cast<IAnyExtension>(clamp));

// The property now clamps writes to [0, 100]
property->set_value(Any<float>(200.f));
// get_value() returns 100.f

// Remove
pi->remove_extension(interface_pointer_cast<IAnyExtension>(clamp));
// The property is back to normal
```

The property holds a strong reference to the installed extension (it is part of the `data_` chain), so the extension stays alive as long as the property exists. If you drop your external handle you lose the ability to call `remove_extension` directly, but you can recover it later via `get_any_chain()`.

### Chaining multiple extensions

Multiple extensions can be stacked. Each `install_extension` call pushes the new extension to the head of the chain:

```cpp
pi->install_extension(clampExt);      // chain: clamp -> original
pi->install_extension(animationExt);  // chain: animation -> clamp -> original
```

The property calls into the head of the chain. Each extension decides whether to delegate to its inner or handle the call itself. For example, a clamp extension might modify the value and delegate `set_data` to the next node, while an animation extension might capture the value as a target and return without delegating at all.

`remove_extension` can remove any extension from any position in the chain, relinking the predecessor's inner to the removed extension's inner.

### Inspecting the chain

`get_any_chain` (in `intf_property.h`) walks the chain and returns all nodes:

```cpp
auto chain = get_any_chain(*property);
// chain[0] = head (outermost extension or plain IAny)
// chain[N] = tail (original plain IAny)
```

Use `interface_cast<IAnyExtension>` on individual nodes to distinguish extensions from the terminal plain `IAny`.

## Manual metadata and accessors

This is **not** recommended, but if you prefer not to use the `VELK_INTERFACE` macro (e.g. for IDE autocompletion, debugging, or fine-grained control), you can write everything by hand. The macro generates five things:

1. A `State` struct containing one field per `PROP`/`RPROP`/`ARR`/`RARR` member, initialized with its default value. Scalar properties use `T`, array properties use `vector<T>`.
2. Per-property statics: a `static constexpr PropertyKind` generated via `detail::PropBind<State, &State::member>` (for `PROP`/`RPROP`), or a `static constexpr ArrayPropertyKind` generated via `detail::ArrBind<State, &State::member>` (for `ARR`/`RARR`). `PropBind` provides `typeUid`, `getDefault`, `createRef`, and `flags`. `ArrBind` provides the same plus `elementUid`, and its `createRef` returns an `ext::ArrayAnyRef<T>` that implements both `IAny` and `IArrayAny`.
3. A `static constexpr std::array metadata` containing `MemberDesc` entries (with `PropertyKind` pointers for `PROP`/`RPROP`, `ArrayPropertyKind` pointers for `ARR`/`RARR`, and `FunctionKind` pointers for `FN`/`FN_RAW` members).
4. Non-virtual `const` accessor methods that query `IMetadata` at runtime. `PROP` returns `Property<T>`, `RPROP` returns `ConstProperty<T>`, `ARR` returns `ArrayProperty<T>`, `RARR` returns `ConstArrayProperty<T>`, `EVT` returns `Event`, `FN`/`FN_RAW` return `Function`.
5. For function members: a pure virtual method and a `static constexpr FunctionKind` generated via `detail::FnBind<&Intf::fn_Name>` (for `FN`) or `detail::FnRawBind<&Intf::fn_Name>` (for `FN_RAW`). Typed-arg `FN` additionally stores a `FnArgDesc` array in their `FunctionKind`.

Minimal example with a single float property:

```cpp
class IToggle : public Interface<IToggle>
{
public:
    struct State { float opacity = 1.f; };

    static constexpr PropertyKind _velk_propkind_opacity =
        detail::PropBind<State, &State::opacity>::kind;

    static constexpr std::array metadata = {
        PropertyDesc("opacity", &INFO, &_velk_propkind_opacity),
    };

    Property<float> opacity() const {
        return Property<float>(::velk::get_property(
            this->template get_interface<IMetadata>(), "opacity"));
    }
};
```

Here is a fuller manual equivalent showing all three function variants:

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    // 1. State struct
    //    One field per PROP/ARR, initialized with its declared default.
    //    ext::Object<T, IMyWidget> stores a copy of this struct, and properties
    //    read/write directly into it via ext::AnyRef<T> or ext::ArrayAnyRef<T>.
    struct State {
        float width = 0.f;
        vector<float> weights = {1.f, 2.f, 3.f};
    };

    // 2. Property kind statics via PropBind / ArrBind
    //    detail::PropBind<State, &State::member> provides typeUid, getDefault,
    //    and createRef automatically. typeUid is type_uid<T>() for the property's
    //    value type. getDefault returns a pointer to a static ext::AnyRef<T>
    //    pointing into a shared default State singleton. createRef creates an
    //    ext::AnyRef<T> pointing into the State struct at the given base address,
    //    used by MetadataContainer to back properties with contiguous state storage.
    //
    //    detail::ArrBind<State, &State::member> works the same way but its
    //    createRef returns an ext::ArrayAnyRef<T> that implements both IAny
    //    (whole-vector) and IArrayAny (element-level access).
    static constexpr PropertyKind _velk_propkind_width =
        detail::PropBind<State, &State::width>::kind;
    static constexpr ArrayPropertyKind _velk_propkind_weights =
        detail::ArrBind<State, &State::weights>::kind;

    // 5a. Zero-arg FN: virtual with native return type + FnBind trampoline
    virtual void fn_reset() = 0;
    static constexpr FunctionKind _velk_fnkind_reset =
        detail::FnBind<&_velk_intf_type::fn_reset>::kind;

    // 5b. Typed-arg FN: virtual with typed params and return + FnBind trampoline + FnArgDesc array
    virtual float fn_add(int x, float y) = 0;
    static constexpr FnArgDesc _velk_fnargs_add[] = {
        {"x", type_uid<int>()}, {"y", type_uid<float>()}
    };
    static constexpr FunctionKind _velk_fnkind_add {
        &detail::FnBind<&_velk_intf_type::fn_add>::trampoline,
        {_velk_fnargs_add, 2} };

    // 5c. FN_RAW: virtual with FnArgs + FnRawBind trampoline (no type extraction)
    virtual IAny::Ptr fn_process(FnArgs) = 0;
    static constexpr FunctionKind _velk_fnkind_process =
        detail::FnRawBind<&_velk_intf_type::fn_process>::kind;

    // 3. Static metadata array
    //    Each entry uses a helper: PropertyDesc(), EventDesc(), or FunctionDesc().
    //    Pass &INFO so each member knows which interface declared it.
    //    INFO is provided by Interface<IMyWidget> automatically.
    //    PropertyDesc accepts an optional PropertyKind pointer as the third argument.
    //    The PropertyKind carries the type UID, so PropertyDesc doesn't need a template argument.
    //    FunctionDesc accepts an optional FunctionKind pointer as the third argument.
    static constexpr std::array metadata = {
        PropertyDesc("width", &INFO, &_velk_propkind_width),
        ArrayPropertyDesc("weights", &INFO, &_velk_propkind_weights),
        EventDesc("on_clicked", &INFO),
        FunctionDesc("reset", &INFO, &_velk_fnkind_reset),
        FunctionDesc("add", &INFO, &_velk_fnkind_add),
        FunctionDesc("process", &INFO, &_velk_fnkind_process),
    };

    // 4. Typed accessor methods
    //    Each accessor queries IMetadata on the concrete object (via get_interface)
    //    and returns the runtime property/event/function by name.
    //    The free functions get_property(), get_event(), get_function() handle
    //    the null check on the IMetadata pointer.

    Property<float> width() const {
        return Property<float>(::velk::get_property(
            this->template get_interface<IMetadata>(), "width"));
    }

    ArrayProperty<float> weights() const {
        return ArrayProperty<float>(::velk::get_property(
            this->template get_interface<IMetadata>(), "weights"));
    }

    Event on_clicked() const {
        return Event(::velk::get_event(
            this->template get_interface<IMetadata>(), "on_clicked"));
    }

    Function reset() const {
        return Function(::velk::get_function(
            this->template get_interface<IMetadata>(), "reset"));
    }

    Function add() const {
        return Function(::velk::get_function(
            this->template get_interface<IMetadata>(), "add"));
    }

    Function process() const {
        return Function(::velk::get_function(
            this->template get_interface<IMetadata>(), "process"));
    }
};
```

The key differences between variants:
- **Zero-arg FN** and **typed-arg FN** both use `detail::FnBind<&Intf::fn_Name>`, which generates a trampoline that deduces parameter types from the member function pointer and extracts values from `FnArgs` automatically. Typed-arg functions additionally store a `FnArgDesc` array in their `FunctionKind`.
- **FN_RAW** uses `detail::FnRawBind<&Intf::fn_Name>`, which generates a trampoline that passes `FnArgs` through to the virtual unchanged without type extraction.

The string names passed to `PropertyDesc` / `get_property` (etc.) are used for runtime lookup, so they must match exactly. `&INFO` is a pointer to the `static constexpr InterfaceInfo` provided by `Interface<T>`, which records the interface UID and name for each member.

You can also use `VELK_METADATA(...)` alone to generate the `State` struct, property kind statics, and metadata array without the accessor methods or virtual methods, then write them yourself.

## shared_ptr and control blocks

Velk provides its own `shared_ptr<T>` and `weak_ptr<T>` (in `memory.h`) rather than using `std::shared_ptr`. This gives ABI stability across compiler versions and allows two modes of operation selected at compile time:

**Intrusive mode** (IInterface-derived types): The object itself owns the strong reference count via `ref()`/`unref()`. The control block only tracks the weak count and stores the `IObject*` self-pointer. Multiple independent `shared_ptr` instances can be created from raw pointers to the same object without a separate "shared from this" mechanism.

> **Warning:** Intrusive mode requires that the object implements `ref()`/`unref()` with actual reference counting (i.e. derives from `RefCountedDispatch` or provides its own implementation). `InterfaceDispatch` has no-op `ref()`/`unref()` stubs, so wrapping a bare `InterfaceDispatch` object in `shared_ptr` will silently leak: `unref()` does nothing, the strong count never reaches zero, and the object is never deleted.

**External mode** (non-IInterface types): An `external_control_block` is allocated with a type-erased destructor, similar to `std::shared_ptr`. The control block owns both strong and weak counts.

Both modes use the same 16-byte `control_block` layout (two `atomic<int32_t>` counts + one `void*` pointer). External mode extends this to 24 bytes with a `destroy` function pointer.

Every `RefCountedDispatch`-derived object needs a control block. Rather than calling `new`/`delete` for each one, freed blocks are recycled via a per-thread free-list (see [Control block pooling](#control-block-pooling) below). The pooling functions `alloc_control_block` and `dealloc_control_block` are exported from the DLL so that allocation and deallocation always go through the same CRT heap, regardless of which module triggers the operation.

### Control block pooling

The per-thread pool is a singly-linked free-list that reuses the block's own `ptr` field as a next-pointer. Each thread's pool holds up to 256 blocks (4 KB at 16 bytes/block).

**Performance impact** (AMD Ryzen 7 5800X, MSVC, Release):

| Operation | new/delete | Pooled |
|---|---|---|
| Control block alloc + dealloc | ~26 ns | ~6 ns |
| Object creation (end-to-end) | ~115 ns | ~55 ns |

Pooling is enabled by default and can be controlled via the `VELK_ENABLE_BLOCK_POOL` CMake option.

**Implementation layers:**

The pool uses three layers that work together:

| Layer | Purpose | Hot-path cost |
|---|---|---|
| **thread_local cache** | Trivially-destructible `thread_local block_pool*`. Gives near-zero-cost access on the hot path. No destructor is registered, so DLL unload cannot crash. | ~0 ns (compiler TLS access) |
| **Platform TLS** | Owns the pool lifetime and provides cleanup callbacks (see table below). Populated on first access per thread; the thread_local cache is set from this layer and cleared by the cleanup callback. | ~1-2 ns (API call, cold path only) |
| **new/delete fallback** | When the pool infrastructure has been torn down (static destruction, DLL unload), `get_pool_ptr()` returns `nullptr` and alloc/dealloc fall through to plain `new`/`delete`. | ~25 ns (heap allocation) |

**Platform TLS details:**

| | Windows | POSIX (Linux, macOS, iOS) | Android |
|---|---|---|---|
| **API** | FLS (`FlsAlloc`/`FlsFree`) | `pthread_key_create`/`pthread_key_delete` | `pthread_key_create`/`pthread_key_delete` |
| **Thread exit cleanup** | FLS callback fires for the exiting thread | pthread destructor fires for the exiting thread | pthread destructor fires for the exiting thread |
| **Library unload cleanup** | `FlsFree` invokes the callback for *every* thread, draining all pools before the DLL is unmapped | `pthread_key_delete` does *not* invoke callbacks, but glibc/Apple keep the DSO mapped while TLS references exist | `pthread_key_delete` does *not* invoke callbacks; Bionic does not refcount DSOs, but the pthread_key approach avoids the crash because the callback code is not tied to the DSO |
| **Post-shutdown access** | After `FlsFree`, the FLS index is invalidated; `get_pool_ptr()` returns `nullptr` | After `pthread_key_delete`, `g_key_valid` is set to `false`; `get_pool_ptr()` returns `nullptr` | Same as POSIX |
| **Granularity** | Fiber-local (each fiber gets its own pool) | Thread-local | Thread-local |

**Why not plain `thread_local`?**

C++ `thread_local` is not used for the pool itself because of two lifetime issues:

- **DLL unload** (Windows, Android): `thread_local` destructors point into the DLL's code. If the DLL is unloaded while other threads are alive, those destructors crash on thread exit. FLS avoids this because `FlsFree` drains all pools before unmapping. On Android, Bionic lacks DSO refcounting for TLS, so the same crash occurs.
- **Post-destruction access** (MSVC): Accessing a destroyed `thread_local` silently re-constructs it, but the destructor won't fire again, leaking the pool. With FLS/pthread_key, the pool is simply unavailable after cleanup and the fallback path handles it.

These issues do not affect Linux, macOS, or iOS (glibc/Apple runtimes keep the DSO mapped while TLS references exist), but the platform TLS approach is used uniformly for simplicity.

The free list requires no synchronization since each thread has its own pool. Blocks freed on a different thread than they were allocated on join that thread's pool; this is correct but suboptimal in producer/consumer patterns.

**Free-list structure:**

```
pool->head -> [block A] -> [block B] -> [block C] -> nullptr
               ptr=B        ptr=C        ptr=nullptr
```

When a block is in the free list, its `strong`, `weak`, and `ptr` fields are meaningless (no one holds a reference to it). The `ptr` field, which normally stores the `IObject*` self-pointer, is repurposed as the next-link.

**Dealloc** pushes to the front:

```
block->ptr = head;   // new block points to current head
head = block;        // new block becomes the head

Before:  head -> [B] -> [C] -> nullptr
After:   head -> [block] -> [B] -> [C] -> nullptr
```

**Alloc** pops from the front, reinitializing the block for use:

```
b = head;                                    // grab the head
head = static_cast<control_block*>(b->ptr);  // advance head to next
b->strong = 1; b->weak = 1; b->ptr = nullptr;
return b;

Before:  head -> [A] -> [B] -> [C] -> nullptr
After:   head -> [B] -> [C] -> nullptr          (A returned to caller)
```

If the free list is empty, `alloc_control_block()` falls back to `new control_block{1, 1, nullptr}`.
