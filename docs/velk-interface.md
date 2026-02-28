# VELK_INTERFACE reference

This document covers VELK_INTERFACE macro and its usage, as well as manual option for metadata definition.

## Contents

- [VELK_INTERFACE](#velk_interface)
- [Array property members](#array-property-members)
- [Function member variants](#function-member-variants)
- [Argument metadata](#argument-metadata)
- [Practical example](#practical-example)
- [Manual metadata and accessors](#manual-metadata-and-accessors)

## VELK_INTERFACE

```cpp
VELK_INTERFACE(
    (PROP, Type, Name, Default),              // Property<Type> Name() const
    (RPROP, Type, Name, Default),             // ConstProperty<Type> Name() const (read-only)
    (ARR, Type, Name),                        // ArrayProperty<Type> Name() const
    (ARR, Type, Name, v1, v2, v3),            // ArrayProperty<Type> Name() const (default {v1,v2,v3})
    (RARR, Type, Name, v1, v2),              // ConstArrayProperty<Type> Name() const (read-only)
    (EVT, Name),                              // Event Name() const
    (FN, RetType, Name),                      // virtual RetType fn_Name()          (zero-arg)
    (FN, RetType, Name, (T1, a1), (T2, a2)),  // virtual RetType fn_Name(T1 a1, T2 a2) (typed)
    (FN_RAW, Name)                            // virtual fn_Name(FnArgs)   (raw untyped)
)
```

## Array property members

`ARR` and `RARR` declare array properties backed by `velk::vector<T>` in the State struct. They provide element-level access (get, set, push, erase) without copying the full vector.

| Syntax | Accessor return type | Mutability |
|--------|---------------------|------------|
| `(ARR, float, items)` | `ArrayProperty<float>` | Read-write |
| `(ARR, float, items, 1.f, 2.f)` | `ArrayProperty<float>` | Read-write, default `{1.f, 2.f}` |
| `(RARR, int, ids)` | `ConstArrayProperty<int>` | Read-only |
| `(RARR, int, ids, 10, 20)` | `ConstArrayProperty<int>` | Read-only, default `{10, 20}` |

Default values are variadic: any arguments after the name become the initializer list for the vector.

### API wrappers

`ConstArrayProperty<T>` (returned by `RARR`) provides read-only access:

```cpp
size_t size() const;
bool empty() const;
T at(size_t index) const;           // single element, no full vector copy
vector<T> get_value() const;        // full copy when needed
```

`ArrayProperty<T>` (returned by `ARR`) adds mutation:

```cpp
ReturnValue set_at(size_t index, const T& value);
ReturnValue push_back(const T& value);
ReturnValue erase_at(size_t index);
void clear();
ReturnValue set_value(const vector<T>& value, InvokeType type = Immediate);
```

### ArrayAny<T>

`ArrayAny<T>` (in `api/any.h`) is a typed wrapper for `IArrayAny`, similar to how `Any<T>` wraps `IAny`. It can be value-constructed (owning) or wrap an existing `IAny::Ptr`/`IAny::ConstPtr`. Use `const T` for read-only access:

```cpp
// Owning: default-constructed empty array
ArrayAny<float> empty;

// Owning: from initializer list
ArrayAny<float> arr({3.14f, 2.71f});
float val = arr.at(0);

// Owning: from array_view
float data[] = {1.f, 2.f, 3.f};
ArrayAny<float> fromView(array_view<float>(data, 3));

// Wrapping an existing IAny::Ptr
ArrayAny<float> wrapped(some_any_ptr);
wrapped.push_back(42.f);

// Read-only: constructed from IAny::ConstPtr, mutation methods are disabled
ArrayAny<const float> readonly_arr(some_const_any_ptr);
float v = readonly_arr.at(0);         // OK
// readonly_arr.push_back(1.f);       // compile error
```

### Architecture

Array properties use `IArrayAny` for element-level operations. When the macro generates `ArrBind<State, &State::member>`, it produces a `PropertyKind` whose `createRef` returns an `ArrayAnyRef<T>` (in `ext/any.h`). This ref implements both `IAny` (whole-vector get/set) and `IArrayAny` (element ops). `ArrayPropertyImpl` delegates element operations to `interface_cast<IArrayAny>(data_)` on its backing Any, and wraps them in `on_changed` notifications.

## Function member variants

`FN` and `FN_RAW` are the two tags for function members. `FN` supports zero-arg and typed-arg forms; `FN_RAW` preserves the untyped `FnArgs` signature.

| Syntax | Virtual generated | Arg metadata | Use case |
|--------|------------------|--------------|----------|
| `(FN, void, reset)` | `void fn_reset()` | none | Zero-arg void functions |
| `(FN, int, add, (int, x), (float, y))` | `int fn_add(int x, float y)` | `FnArgDesc` per param | Typed args with return value |
| `(FN_RAW, process)` | `IAny::Ptr fn_process(FnArgs)` | none | Raw untyped args |

All three variants generate:
1. A pure virtual method (signature depends on variant)
2. A `static constexpr FunctionKind` with a trampoline (via `detail::FnBind` or `detail::FnRawBind`) that routes `IFunction::invoke()` to the virtual
3. A `MemberDesc` with the trampoline pointer in the metadata array
4. An accessor `Function Name() const`

For `FN` members, `RetType` specifies the native C++ return type of the virtual method. The trampoline wraps the result into `IAny::Ptr` automatically: `void` returns `nullptr`, other types are wrapped via `Any<R>::clone()`. `FN_RAW` always returns `IAny::Ptr`.

For typed-arg functions, the trampoline automatically extracts each argument from `FnArgs` using `IAny::get_data()` with type deduction from the member function pointer. If fewer arguments are provided than expected, the trampoline returns `nullptr`. Extra arguments are ignored.

## Argument metadata

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
if (auto* info = instance().type_registry().get_class_info(MyWidget::class_id())) {
    for (auto& m : info->members) {
        if (auto* fk = m.functionKind(); fk && !fk->args.empty()) {
            for (auto& arg : fk->args) {
                // arg.name, arg.typeUid
            }
        }
    }
}
```

## Practical example

```cpp
class IMyWidget : public Interface<IMyWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 0.f),
        (ARR, float, weights, 1.f, 2.f, 3.f),     // array with defaults
        (RARR, int32_t, tags),                      // read-only array, empty default
        (EVT, on_clicked),
        (FN, void, reset),                          // zero-arg, void return
        (FN, float, add, (int, x), (float, y))     // typed args, typed return
    )
};

class ISerializable : public Interface<ISerializable>
{
public:
    VELK_INTERFACE(
        (PROP, std::string, name, ""),
        (FN_RAW, serialize)                 // raw FnArgs
    )
};

class MyWidget : public ext::Object<MyWidget, IMyWidget, ISerializable>
{
    void fn_reset() override {
        // void return, trampoline returns nullptr to IFunction::invoke()
    }

    float fn_add(int x, float y) override {
        // x and y are extracted from FnArgs automatically
        // return value is wrapped into IAny::Ptr by the trampoline
        return x + y;
    }

    IAny::Ptr fn_serialize(FnArgs args) override {
        // manual arg unpacking via FunctionContext or Any<const T>
        return nullptr;
    }
};
```

Invocation works the same for all variants, callers always go through `IFunction::invoke()`:

```cpp
auto widget = instance().create<IObject>(MyWidget::class_id());
if (auto* iw = interface_cast<IMyWidget>(widget)) {
    // Scalar property
    iw->width().set_value(100.f);

    // Array property: element-level access
    iw->weights().push_back(4.f);
    float w = iw->weights().at(0);      // 1.f
    iw->weights().set_at(0, 10.f);
    iw->weights().erase_at(2);

    // Read-only array property
    size_t n = iw->tags().size();
    // iw->tags().push_back(42);         // won't compile: ConstArrayProperty

    // Functions
    invoke_function(iw->reset());                            // zero-arg
    invoke_function(iw, "add", Any<int>(10), Any<float>(3.14f)); // typed
}
if (auto* is = interface_cast<ISerializable>(widget)) {
    invoke_function(is, "serialize");                         // FN_RAW
}
```

Each entry produces a `MemberDesc` in a `static constexpr std::array metadata` and a typed accessor method. Up to 32 members per interface. Up to 8 typed parameters per function. Members track which interface declared them via `InterfaceInfo`. Array properties use `MemberKind::ArrayProperty` and are backed by `ArrayPropertyImpl` at runtime.

## Manual metadata and accessors

This is **not** recommended, but if you prefer not to use the `VELK_INTERFACE` macro (e.g. for IDE autocompletion, debugging, or fine-grained control), you can write everything by hand. The macro generates five things:

1. A `State` struct containing one field per `PROP`/`RPROP`/`ARR`/`RARR` member, initialized with its default value. Scalar properties use `T`, array properties use `vector<T>`.
2. Per-property statics: a `static constexpr PropertyKind` generated via `detail::PropBind<State, &State::member>` (for `PROP`/`RPROP`), or a `static constexpr ArrayPropertyKind` generated via `detail::ArrBind<State, &State::member>` (for `ARR`/`RARR`). `PropBind` provides `typeUid`, `getDefault`, `createRef`, and `flags`. `ArrBind` provides the same plus `elementUid`, and its `createRef` returns an `ArrayAnyRef<T>` that implements both `IAny` and `IArrayAny`.
3. A `static constexpr std::array metadata` containing `MemberDesc` entries (with `PropertyKind` pointers for `PROP`/`RPROP`, `ArrayPropertyKind` pointers for `ARR`/`RARR`, and `FunctionKind` pointers for `FN`/`FN_RAW` members).
4. Non-virtual `const` accessor methods that query `IMetadata` at runtime. `PROP` returns `Property<T>`, `RPROP` returns `ConstProperty<T>`, `ARR` returns `ArrayProperty<T>`, `RARR` returns `ConstArrayProperty<T>`, `EVT` returns `Event`, `FN`/`FN_RAW` return `Function`.
5. For function members: a pure virtual method and a `static constexpr FunctionKind` generated via `detail::FnBind<&Intf::fn_Name>` (for `FN`) or `detail::FnRawBind<&Intf::fn_Name>` (for `FN_RAW`). Typed-arg `FN` additionally stores a `FnArgDesc` array in their `FunctionKind`.

Here is a manual equivalent showing all three function variants:

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
