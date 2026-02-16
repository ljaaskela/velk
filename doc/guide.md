# Guide

This guide covers advanced topics beyond the basics shown in the [README](../README.md).

## Virtual function dispatch

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

### Function arguments

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

### Typed lambda parameters

`Callback` also accepts lambdas with typed parameters. Arguments are automatically extracted from `FnArgs` using `Any<const T>`, so there's no manual unpacking:

```cpp
Callback fn([&](const float& a, const int& b) -> ReturnValue {
    // a and b are extracted from FnArgs automatically
    return ReturnValue::SUCCESS;
});

Any<float> x(3.14f);
Any<int> y(42);
const IAny* ptrs[] = {x, y};
fn.invoke(FnArgs{ptrs, 2});
```

Void-returning lambdas are supported — `Callback` wraps them to return `ReturnValue::SUCCESS`:

```cpp
Callback fn([&](float value) {
    std::cout << "received: " << value << std::endl;
});
```

Zero-arity lambdas work too:

```cpp
Callback fn([&]() {
    std::cout << "called!" << std::endl;
});
fn.invoke();  // SUCCESS
```

The three constructor forms are mutually exclusive via SFINAE:

| Callable type | Constructor |
|---|---|
| `ReturnValue(*)(FnArgs)` (raw function pointer) | `Callback(CallbackFn*)` |
| Callable with `(FnArgs) -> ReturnValue` | Capturing lambda ctor |
| Callable with typed params (any return) | Typed lambda ctor |

When fewer arguments are provided than the lambda expects, `invoke()` returns `INVALID_ARGUMENT`. Extra arguments are ignored. If an argument's type doesn't match the lambda parameter type, the parameter receives a default-constructed value.

## Properties with change notifications

```cpp
auto prop = Property<float>();
prop.set_value(5.f);

Callback onChange([](FnArgs args) -> ReturnValue {
    if (auto v = Any<const float>(args[0])) {
        std::cout << "new value: " << v.get_value() << std::endl;
    }
    return ReturnValue::SUCCESS;
});
prop.add_on_changed(onChange);

prop.set_value(10.f);  // triggers onChange
```

## Custom Any types

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

## Direct state access

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

## Deferred invocation

Functions and event handlers support deferred execution via the `InvokeType` enum (`Immediate` or `Deferred`). Deferred work is queued and executed when `::strata::instance().update()` is called.

### Defer at the call site

Pass `Deferred` to `invoke()` to queue the entire invocation:

```cpp
auto fn = iw->reset();
invoke_function(fn, args);                                // executes now (default)
invoke_function(fn, args, InvokeType::Deferred);          // queued for update()
```

### Deferred event handlers

Register a handler as deferred so it is queued each time the event fires, while immediate handlers on the same event still execute synchronously:

```cpp
auto event = iw->on_clicked();
event->add_handler(immediateHandler);                        // fires synchronously
event->add_handler(deferredHandler, InvokeType::Deferred);   // queued for update()

invoke_event(event, args);  // immediateHandler runs now, deferredHandler is queued
instance().update();        // deferredHandler runs here
```

Arguments are cloned when a task is queued, so the original `IAny` does not need to outlive the call. Deferred tasks that themselves produce deferred work will re-queue, and will be handled when `update()` is called the next time.
