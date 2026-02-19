#include <api/velk.h>

#include <api/any.h>
#include <api/callback.h>
#include <api/function_context.h>
#include <api/property.h>
#include <ext/any.h>
#include <ext/event.h>
#include <ext/object.h>
#include <interface/intf_external_any.h>
#include <interface/intf_property.h>
#include <interface/intf_velk.h>
#include <interface/types.h>
#include <iostream>

using std::cout;
using std::endl;
using std::string;
using namespace velk;

// Custom data
struct Data
{
    Data() = default;
    Data(float v, const string &n) : value(v), name(n) {}
    float value;
    string name;
    constexpr bool operator!=(const Data &other) const noexcept
    {
        return value != other.value && name != other.name;
    }

    ext::LazyEvent onChanged;
};

Data globalData_;

// Custom any type which accesses global data (of type Data)
class MyDataAny final : public ext::AnyCore<MyDataAny, Data, IExternalAny>
{
public:
    Data &get_value() const override { return globalData_; }
    ReturnValue set_value(const Data &value) override
    {
        if (value != globalData_) {
            globalData_.value = value.value;
            globalData_.name = value.name;
            invoke_event(on_data_changed(), this);
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::NOTHING_TO_DO;
    }

    IEvent::Ptr on_data_changed() const override
    {
        return globalData_.onChanged;
    }

private:
};

// Application-specific interface definitions (IMyWidget and ISerializable)
class IMyWidget : public Interface<IMyWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, float, height, 50.f),
        (EVT, on_clicked),
        (FN, void, reset),
        (FN, void, add, (int, x), (float, y))
    )
};

class ISerializable : public Interface<ISerializable>
{
public:
    VELK_INTERFACE(
        (PROP, string, name, ""),
        (FN_RAW, serialize)
    )
};

// MyWidget concrete class implementation, implements IMyWidget and ISerializable
class MyWidget : public ext::Object<MyWidget, IMyWidget, ISerializable>
{
    void fn_reset() override
    {
        cout << "  MyWidget::fn_reset called!" << endl;
    }

    void fn_add(int x, float y) override
    {
        cout << "  MyWidget::fn_add(" << x << ", " << y << ") = " << (x + y) << endl;
    }

    IAny::Ptr fn_serialize(FnArgs) override
    {
        cout << "  MyWidget::fn_serialize called (raw)!" << endl;
        return nullptr;
    }
};

// Create a Property<float>, show copy semantics and get/set round-trip.
void demo_properties()
{
    cout << endl << "=== demo_properties ===" << endl;

    auto prop = Property<float>(instance().create_property<float>());
    prop.set_value(5.f);
    auto prop2 = prop;
    auto prop3 = Property<float>(prop);

    auto value = prop.get_value();
    cout << "Property<float> prop1 value is " << value << endl;
    cout << "Property<float> prop2 value is " << prop2.get_value() << endl;
    cout << "Property<float> prop3 value is " << prop3.get_value() << endl;

    cout << "=== demo_properties done ===" << endl;
}

// Use Any<Data> backed by MyDataAny to access global storage through a property.
void demo_custom_any()
{
    cout << endl << "=== demo_custom_any ===" << endl;

    Any<Data> data;                 // One view to globalData
    auto myprop = Property<Data>(instance().create_property<Data>()); // Property to global data
    myprop.set_value({10.f, "Hello"});

    cout << "Property<Data> value is " << myprop.get_value().value << ":"
              << myprop.get_value().name << endl;

    cout << "=== demo_custom_any done ===" << endl;
}

// Subscribe to on_changed events on both float and Data properties, then trigger changes.
void demo_change_notifications()
{
    cout << endl << "=== demo_change_notifications ===" << endl;

    auto prop = Property<float>(instance().create_property<float>());
    prop.set_value(5.f);

    Any<Data> data;
    auto myprop = Property<Data>(instance().create_property<Data>());
    myprop.set_value({10.f, "Hello"});

    Callback valueChanged([](FnArgs args) -> ReturnValue {
        cout << "Property value changed, ";
        if (!args[0]) {
            return ReturnValue::INVALID_ARGUMENT;
        }
        if (auto v = Any<const float>(*args[0])) {
            cout << "new value: " << v.get_value();
        } else if (auto v = Any<const Data>(*args[0])) {
            auto value = v.get_value();
            cout << "new value: " << value.name << ", " << value.value;
        } else {
            cout << "property not convertible to float or Data";
        }
        cout << endl;
        return ReturnValue::SUCCESS;
    });
    prop.add_on_changed(valueChanged);
    myprop.add_on_changed(valueChanged);

    prop.set_value(10.f);
    data.set_value({20.f, "Hello2"});   // Set global data directly
    myprop.set_value({30.f, "Hello3"}); // Set global data through property

    cout << "=== demo_change_notifications done ===" << endl;
}

// Query class info and member metadata from the registry without creating an instance.
void demo_static_metadata()
{
    cout << endl << "=== demo_static_metadata ===" << endl;

    auto& r = instance();

    // --- Static metadata via Velk (no instance needed) ---
    cout << "MyWidget static metadata:" << endl;
    if (auto* info = r.get_class_info(MyWidget::class_id())) {
        cout << "  Class: " << info->name << " (" << info->members.size() << " members)" << endl;
        for (auto& m : info->members) {
            const char* kind = m.kind == MemberKind::Property ? "Property"
                             : m.kind == MemberKind::Event    ? "Event"
                                                              : "Function";
            cout << "    " << kind << " \"" << m.name << "\"";
            if (m.interfaceInfo) {
                cout << " (from " << m.interfaceInfo->name << ")";
            }
            if (m.propertyKind()) {
                cout << " [has default]";
            }
            cout << endl;
        }
    }

    // --- Static defaults (no instance needed) ---
    cout << endl << "  Static metadata defaults:" << endl;
    if (auto* info = r.get_class_info(MyWidget::class_id())) {
        for (auto& m : info->members) {
            if (auto* pk = m.propertyKind()) {
                auto* def = pk->getDefault ? pk->getDefault() : nullptr;
                cout << "    " << m.name << " default IAny: " << (def ? "ok" : "null") << endl;
            }
        }
        // Typed access to specific defaults
        cout << "    width default = " << get_default_value<float>(info->members[0]) << endl;
        cout << "    height default = " << get_default_value<float>(info->members[1]) << endl;
        // ISerializable::name (no custom default, should be "")
        cout << "    name default = \"" << get_default_value<string>(info->members[4]) << "\""
             << endl;
    }

    cout << "=== demo_static_metadata done ===" << endl;
}

// Enumerate members and look up properties/events/functions by name on a live object.
void demo_runtime_metadata(IObject::Ptr& widget)
{
    cout << endl << "=== demo_runtime_metadata ===" << endl;

    // --- Runtime metadata via IMetadata ---
    cout << "MyWidget instance metadata:" << endl;
    if (auto* meta = interface_cast<IMetadata>(widget)) {
        for (auto &m : meta->get_static_metadata()) {
            cout << "    member: " << m.name << endl;
        }
        if (auto p = meta->get_property("width")) {
            cout << "    get_property(\"width\") ok" << endl;
        }
        if (auto e = meta->get_event("on_clicked")) {
            cout << "    get_event(\"on_clicked\") ok" << endl;
        }
        if (auto f = meta->get_function("reset")) {
            cout << "    get_function(\"reset\") ok" << endl;
        }
        if (!meta->get_property("Bogus")) {
            cout << "    get_property(\"Bogus\") correctly returned null" << endl;
        }
    }

    cout << "=== demo_runtime_metadata done ===" << endl;
}

// Access properties, events, and functions through IMyWidget typed interface accessors.
void demo_interfaces(IObject::Ptr& widget)
{
    cout << endl << "=== demo_interfaces ===" << endl;

    // --- Typed access via IMyWidget interface ---
    cout << "MyWidget via IMyWidget interface:" << endl;
    if (auto* iw = interface_cast<IMyWidget>(widget)) {
        // Verify runtime properties start with declared defaults
        cout << "  width() initial value = " << iw->width().get_value() << " (expected 100)" << endl;
        cout << "  height() initial value = " << iw->height().get_value() << " (expected 50)" << endl;

        auto w = iw->width();
        w.set_value(42.f);
        cout << "  width().set_value(42) -> width().get_value() = " << iw->width().get_value() << endl;

        cout << "  height() ok: " << (iw->height() ? "yes" : "no") << endl;
        cout << "  on_clicked() ok: " << (iw->on_clicked() ? "yes" : "no") << endl;
        cout << "  reset() ok: " << (iw->reset() ? "yes" : "no") << endl;

        cout << "  Invoking reset() (zero-arg)..." << endl;
        invoke_function(iw->reset()); // Invoke by interface accessor
        invoke_function(iw, "reset"); // Invoke by name

        cout << "  Invoking add() (typed args)..." << endl;
        invoke_function(iw, "add", Any<int>(10), Any<float>(3.14f));

        // Inspect typed arg metadata
        if (auto* info = instance().get_class_info(MyWidget::class_id())) {
            for (auto& m : info->members) {
                if (auto* fk = m.functionKind()) {
                    if (!fk->args.empty()) {
                        cout << "  Function \"" << m.name << "\" args:";
                        for (auto& arg : fk->args) {
                            cout << " " << arg.name;
                        }
                        cout << endl;
                    }
                }
            }
        }
    }

    cout << "=== demo_interfaces done ===" << endl;
}

// Construct Uid from hex ints and string, verify constexpr parsing and VELK_UID.
void demo_uid()
{
    cout << endl << "=== demo_uid ===" << endl;

    // --- Uid from string ---
    cout << "Uid from string:" << endl;
    constexpr Uid a(0xcc262192d151941f, 0xd542d4c622b50b09);
    constexpr Uid b("cc262192-d151-941f-d542-d4c622b50b09");
    static_assert(a == b, "Uid string parsing mismatch");
    static_assert(is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b09"));
    static_assert(!is_valid_uid_format("not-a-uid"));
    static_assert(!is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b0")); // too short
    static_assert(!is_valid_uid_format("cc262192Xd151-941f-d542-d4c622b50b09")); // wrong dash
    static_assert(!is_valid_uid_format("gg262192-d151-941f-d542-d4c622b50b09")); // bad hex
    // constexpr Uid bad("not-a-uid"); // would fail to compile
    cout << "  from ints:   " << a << endl;
    cout << "  from string: " << b << endl;

    // Verify VELK_UID macro on Interface template
    static_assert(ISerializable::UID == type_uid<ISerializable>(), "auto UID should match type_uid");
    constexpr Uid customUid("a0b1c2d3-e4f5-6789-abcd-ef0123456789");
    // A hypothetical interface with a fixed UID:
    // class IFixed : public Interface<IFixed, VELK_UID("a0b1c2d3-e4f5-6789-abcd-ef0123456789")> {};
    // would have IFixed::UID == customUid
    cout << "  VELK_UID:  " << customUid << endl;

    cout << "=== demo_uid done ===" << endl;
}

// Access the ISerializable interface on MyWidget: set name, invoke serialize.
void demo_serializable(IObject::Ptr& widget)
{
    cout << endl << "=== demo_serializable ===" << endl;

    // --- Typed access via ISerializable interface ---
    cout << "MyWidget via ISerializable interface:" << endl;
    if (auto* is = interface_cast<ISerializable>(widget)) {
        auto n = is->name();
        n.set_value(string("MyWidget1"));
        cout << "  name().set_value(\"MyWidget1\") -> name().get_value() = " << is->name().get_value() << endl;
        cout << "  serialize() ok: " << (is->serialize() ? "yes" : "no") << endl;
        cout << "  Invoking serialize() (FN_RAW)..." << endl;
        invoke_function(is, "serialize");
    }

    cout << "=== demo_serializable done ===" << endl;
}

// AnyRef<T> points at external memory: read/write through the ref, memcpy snapshot, clone.
void demo_any_ref()
{
    cout << endl << "=== demo_any_ref ===" << endl;

    // --- AnyRef: external struct storage ---
    cout << "AnyRef<T> with struct storage:" << endl;
    {
        struct WidgetState {
            float width = 100.f;
            float height = 50.f;
        } state;

        ext::AnyRef<float> widthRef(&state.width);
        ext::AnyRef<float> heightRef(&state.height);

        cout << "  state = {" << state.width << ", " << state.height << "}" << endl;
        cout << "  widthRef.get_value() = " << widthRef.get_value() << endl;

        // Write through the any, reads back from struct
        widthRef.set_value(42.f);
        cout << "  widthRef.set_value(42) -> state.width = " << state.width << endl;

        // Write directly to struct, reads back through the any
        state.height = 99.f;
        cout << "  state.height = 99 -> heightRef.get_value() = " << heightRef.get_value() << endl;

        // Snapshot via memcpy
        WidgetState snapshot;
        memcpy(&snapshot, &state, sizeof(WidgetState));
        state.width = 0.f;
        cout << "  memcpy snapshot, then state.width = 0" << endl;
        cout << "  snapshot.width = " << snapshot.width << " (preserved)" << endl;
        cout << "  widthRef.get_value() = " << widthRef.get_value() << " (follows live state)" << endl;

        // Clone produces an owned AnyValue copy
        auto cloned = widthRef.clone();
        state.width = 777.f;
        float clonedValue{};
        cloned->get_data(&clonedValue, sizeof(float), type_uid<float>());
        cout << "  clone() after state.width = 777 -> cloned value = " << clonedValue << " (independent)" << endl;

        cout << "  sizeof(AnyRef<float>) " << sizeof(ext::AnyRef<float>) << endl;
    }

    cout << "=== demo_any_ref done ===" << endl;
}

// Read/write properties through IPropertyState raw struct pointers for zero-overhead access.
void demo_direct_state()
{
    cout << endl << "=== demo_direct_state ===" << endl;

    auto& r = instance();

    // --- State-backed property storage ---
    cout << "State-backed property storage:" << endl;
    {
        auto widget2 = r.create<IObject>(MyWidget::class_id());
        auto* iw = interface_cast<IMyWidget>(widget2);
        auto* ps = interface_cast<IPropertyState>(widget2);
        if (iw && ps) {
            auto *state = ps->get_property_state<IMyWidget>();
            cout << "  IMyWidget::State* = " << (state ? "ok" : "null") << endl;
            if (state) {
                // Verify defaults in state struct
                cout << "  state->width = " << state->width << " (expected 100)" << endl;
                cout << "  state->height = " << state->height << " (expected 50)" << endl;

                // Write through property, verify state reflects it
                iw->width().set_value(200.f);
                cout << "  After width().set_value(200): state->width = " << state->width << endl;

                // Write to state directly, verify property reads it
                state->height = 75.f;
                cout << "  After state->height = 75: height().get_value() = " << iw->height().get_value() << endl;
            }

            // ISerializable state
            auto* is = interface_cast<ISerializable>(widget2);
            if (is) {
                auto* sstate = static_cast<ISerializable::State*>(ps->get_property_state(ISerializable::UID));
                cout << "  ISerializable::State* = " << (sstate ? "ok" : "null") << endl;
                if (sstate) {
                    is->name().set_value(string("TestName"));
                    cout << "  After name().set_value(\"TestName\"): sstate->name = " << sstate->name << endl;
                }
            }
        }
    }

    cout << "=== demo_direct_state done ===" << endl;
}

// Invoke functions with FunctionContext for multi-arg dispatch, variadic helpers, and raw FnArgs.
void demo_function_context(IObject::Ptr& widget)
{
    cout << endl << "=== demo_function_context ===" << endl;

    // --- FunctionContext: multi-arg function invocation ---
    cout << "FunctionContext multi-arg:" << endl;
    {
        Callback add([](FnArgs args) -> ReturnValue {
            auto ctx = FunctionContext(args, 2);
            if (!ctx) {
                cout << "  add: expected 2 args" << endl;
                return ReturnValue::INVALID_ARGUMENT;
            }
            auto a = ctx.arg<float>(0);
            auto b = Any<const int>(ctx.arg(1));
            if (a && b) {
                cout << "  add(" << a.get_value() << ", " << b.get_value()
                          << ") = " << (a.get_value() + b.get_value()) << endl;
            }
            return ReturnValue::SUCCESS;
        });

        // Variadic invoke with raw values — auto-wraps in Any<T> + FunctionContext
        invoke_function(IFunction::Ptr(add), 10.f, 20);

        // Same for named dispatch (typed args)
        invoke_function(widget.get(), "add", Any<int>(5), Any<float>(2.5f));

        // Explicit IAny* args
        Any<float> arg0(3.f);
        Any<int> arg1(7);
        const IAny* ptrs[] = {arg0, arg1};
        FnArgs fnArgs{ptrs, 2};
        FunctionContext ctx(fnArgs, 2);
        cout << "  arg_count = " << ctx.size() << endl;
        invoke_function(IFunction::Ptr(add), Any<float>(3.f), Any<int>(7));

        // Direct FnArgs invocation
        add.invoke(fnArgs);
    }

    cout << "=== demo_function_context done ===" << endl;
}

// Print sizeof for key types used in docs/performance.md memory layout section.
void demo_sizes()
{
    cout << endl << "=== demo_sizes ===" << endl;

    // Smart pointers
    cout << "Smart pointers:" << endl;
    cout << "  control_block            " << sizeof(control_block) << endl;
    cout << "  shared_ptr<IInterface>   " << sizeof(IInterface::Ptr) << endl;
    cout << "  weak_ptr<IInterface>     " << sizeof(IInterface::WeakPtr) << endl;

    // Base layers
    cout << "Base layers:" << endl;
    cout << "  IInterface               " << sizeof(IInterface) << endl;
    cout << "  RefCountedDispatch<1>    " << sizeof(ext::RefCountedDispatch<IMyWidget>) << endl;
    cout << "  ObjectCore<X, 1 intf>    " << sizeof(ext::ObjectCore<MyWidget, IMyWidget>) << endl;
    cout << "  ObjectCore<X, 3 intfs>   "
         << sizeof(ext::ObjectCore<MyWidget, IMetadataContainer, IMyWidget, ISerializable>) << endl;

    // Any types
    cout << "Any types:" << endl;
    cout << "  AnyValue<float>          " << sizeof(ext::AnyValue<float>) << endl;
    cout << "  MyDataAny                " << sizeof(MyDataAny) << endl;

    // MyWidget (Object with states)
    cout << "MyWidget:" << endl;
    cout << "  IMyWidget::State         " << sizeof(IMyWidget::State) << endl;
    cout << "  ISerializable::State     " << sizeof(ISerializable::State) << endl;
    cout << "  MyWidget                 " << sizeof(MyWidget) << endl;

    // Wrappers
    cout << "Wrappers:" << endl;
    cout << "  Any<float>               " << sizeof(Any<float>) << endl;
    cout << "  Property<float>          " << sizeof(Property<float>) << endl;
    cout << "  LazyEvent                " << sizeof(ext::LazyEvent) << endl;

    cout << "=== demo_sizes done ===" << endl;
}

int main()
{
    auto &r = instance();

    r.register_type<MyDataAny>();
    r.register_type<MyWidget>();

    demo_properties();
    demo_custom_any();
    demo_change_notifications();
    demo_static_metadata();

    auto widget = r.create<IObject>(MyWidget::class_id());
    demo_runtime_metadata(widget);
    demo_interfaces(widget);
    demo_uid();
    demo_serializable(widget);
    demo_any_ref();
    demo_direct_state();
    demo_function_context(widget);
    demo_sizes();

    return 0;
}
