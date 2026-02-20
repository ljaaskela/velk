# VELK_INTERFACE reference

This document covers VELK_INTERFACE macro and its usage, as well as manual option for metadata definition.

## Contents

- [VELK_INTERFACE](#velk_interface)
- [Function member variants](#function-member-variants)
- [Argument metadata](#argument-metadata)
- [Practical example](#practical-example)
- [Manual metadata and accessors](#manual-metadata-and-accessors)

## VELK_INTERFACE

```cpp
VELK_INTERFACE(
    (PROP, Type, Name, Default),              // Property<Type> Name() const
    (RPROP, Type, Name, Default),             // ConstProperty<Type> Name() const (read-only)
    (EVT, Name),                              // Event Name() const
    (FN, RetType, Name),                      // virtual RetType fn_Name()          (zero-arg)
    (FN, RetType, Name, (T1, a1), (T2, a2)),  // virtual RetType fn_Name(T1 a1, T2 a2) (typed)
    (FN_RAW, Name)                            // virtual fn_Name(FnArgs)   (raw untyped)
)
```

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
if (auto* info = instance().get_class_info(MyWidget::class_id())) {
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
        (EVT, on_clicked),
        (FN, void, reset),                        // zero-arg, void return
        (FN, float, add, (int, x), (float, y))    // typed args, typed return
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
    invoke_function(iw->reset());                            // zero-arg
    invoke_function(iw, "add", Any<int>(10), Any<float>(3.14f)); // typed
}
if (auto* is = interface_cast<ISerializable>(widget)) {
    invoke_function(is, "serialize");                         // FN_RAW
}
```

Each entry produces a `MemberDesc` in a `static constexpr std::array metadata` and a typed accessor method. Up to 32 members per interface. Up to 8 typed parameters per function. Members track which interface declared them via `InterfaceInfo`.

## Manual metadata and accessors

This is **not** recommended, but if you prefer not to use the `VELK_INTERFACE` macro (e.g. for IDE autocompletion, debugging, or fine-grained control), you can write everything by hand. The macro generates five things:

1. A `State` struct containing one field per `PROP`/`RPROP` member, initialized with its default value.
2. Per-property statics: a `static constexpr PropertyKind` generated via `detail::PropBind<State, &State::member>` (or `detail::PropBind<State, &State::member, ObjectFlags::ReadOnly>` for `RPROP`), which provides `typeUid`, `getDefault`, `createRef`, and `flags` automatically.
3. A `static constexpr std::array metadata` containing `MemberDesc` entries (with `PropertyKind` pointers for `PROP`/`RPROP` members and `FunctionKind` pointers for `FN`/`FN_RAW` members).
4. Non-virtual `const` accessor methods that query `IMetadata` at runtime. `PROP` returns `Property<T>`, `RPROP` returns `ConstProperty<T>`, `EVT` returns `Event`, `FN`/`FN_RAW` return `Function`.
5. For function members: a pure virtual method and a `static constexpr FunctionKind` generated via `detail::FnBind<&Intf::fn_Name>` (for `FN`) or `detail::FnRawBind<&Intf::fn_Name>` (for `FN_RAW`). Typed-arg `FN` additionally stores a `FnArgDesc` array in their `FunctionKind`.

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
    static constexpr PropertyKind _velk_propkind_width =
        detail::PropBind<State, &State::width>::kind;

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
