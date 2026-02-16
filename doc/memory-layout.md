# Object memory layout

An `ext::Object<T, Interfaces...>` instance carries minimal per-object data. The metadata container is heap-allocated once per object and lazily creates member instances on first access.

## ext::ObjectCore

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

## MetadataContainer

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

## ext::Object

Full object with ext::ObjectCore features, runtime metadata, and per-interface property state storage.

| Layer | Member | Size (x64) |
|---|---|---|
| ext::ObjectCore | [ext::ObjectCore](#extobjectcore) | varies by interface count |
| ext::Object | `meta_` (`unique_ptr<IMetadata>`) | 8 |
| ext::Object | `states_` (tuple of interface `State` structs) | varies by interfaces |
| MetadataContainer | [MetadataContainer](#metadatacontainer), heap-allocated | 64 |

The `states_` tuple contains one `State` struct per interface that declares properties via `STRATA_INTERFACE`. Each `State` struct holds one field per `PROP` member, initialized with its declared default value. Properties backed by state storage use `ext::AnyRef<T>` to read/write directly into these fields.

## Example: MyWidget with 6 members

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

## Base types

### Any

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

### Function

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

### Property

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
