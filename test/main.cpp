#include <api/strata.h>

#include <api/any.h>
#include <api/function.h>
#include <api/property.h>
#include <ext/any.h>
#include <ext/event.h>
#include <ext/object.h>
#include <interface/intf_external_any.h>
#include <interface/intf_property.h>
#include <interface/intf_strata.h>
#include <interface/types.h>
#include <iostream>

using namespace std;
using namespace strata;

struct Data
{
    Data() = default;
    Data(float v, const std::string &n) : value(v), name(n) {}
    float value;
    std::string name;
    constexpr bool operator!=(const Data &other) const noexcept
    {
        return value != other.value && name != other.name;
    }

    LazyEvent onChanged;
};

Data globalData_;

// Custom any type which accesses global data
class MyDataAny final : public AnyCore<MyDataAny, Data, IExternalAny>
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

class IMyWidget : public Interface<IMyWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, float, height, 50.f),
        (EVT, on_clicked),
        (FN, reset)
    )
};

class ISerializable : public Interface<ISerializable>
{
public:
    STRATA_INTERFACE(
        (PROP, std::string, name, ""),
        (FN, serialize)
    )
};

class MyWidget : public Object<MyWidget, IMyWidget, ISerializable>
{
    ReturnValue fn_reset(const IAny *args) override
    {
        AnyT<const int> value(args);
        if (value) {
            std::cout << "  MyWidget::fn_reset called with value " << value.get_value()
                      << std::endl;
        } else {
            std::cout << "  MyWidget::fn_reset called!" << std::endl;
        }
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_serialize(const IAny *) override
    {
        std::cout << "  MyWidget::fn_serialize called!" << std::endl;
        return ReturnValue::SUCCESS;
    }
};

int main()
{
    auto &r = instance();

    r.register_type<MyDataAny>();
    r.register_type<MyWidget>();

    auto prop = PropertyT<float>();
    prop.set_value(5.f);
    auto prop2 = prop;
    auto prop3 = PropertyT<float>(prop);

    auto value = prop.get_value();
    std::cout << "Property<float> prop1 value is " << value << std::endl;
    std::cout << "Property<float> prop2 value is " << prop2.get_value() << std::endl;
    std::cout << "Property<float> prop3 value is " << prop3.get_value() << std::endl;

    AnyT<Data> data;                 // One view to globalData
    auto myprop = PropertyT<Data>(); // Property to global data
    myprop.set_value({10.f, "Hello"});

    std::cout << "Property<Data> value is " << myprop.get_value().value << ":" << myprop.get_value().name
              << std::endl;

    Function valueChanged([](const IAny *any) -> ReturnValue {
        std::cout << "Property value changed, ";
        if (!any) {
            return ReturnValue::INVALID_ARGUMENT;
        }
        if (auto v = AnyT<const float>(*any)) {
            std::cout << "new value: " << v.get_value();
        } else if (auto v = AnyT<const Data>(*any)) {
            auto value = v.get_value();
            std::cout << "new value: " << value.name << ", " << value.value;
        } else {
            std::cout << "property not convertible to float or Data";
        }
        std::cout << std::endl;
        return ReturnValue::SUCCESS;
    });
    prop.add_on_changed(valueChanged);
    myprop.add_on_changed(valueChanged);

    prop.set_value(10.f);
    data.set_value({20.f, "Hello2"});   // Set global data directly
    myprop.set_value({30.f, "Hello3"}); // Set global data through property

    std::cout << "sizeof(float)            " << sizeof(float) << std::endl;
    std::cout << "sizeof(IObject::WeakPtr) " << sizeof(IObject::WeakPtr) << std::endl;
    std::cout << "sizeof(AnyT<float>)      " << sizeof(AnyT<float>) << std::endl;
    std::cout << "sizeof(AnyValue<float>) " << sizeof(AnyValue<float>) << std::endl;
    std::cout << "sizeof(PropertyT<float>) " << sizeof(PropertyT<float>) << std::endl;
    std::cout << "sizeof(MyDataAny)        " << sizeof(MyDataAny) << std::endl;

    // --- Static metadata via Strata (no instance needed) ---
    std::cout << "\n--- MyWidget static metadata ---" << std::endl;
    if (auto* info = r.get_class_info(MyWidget::get_class_uid())) {
        std::cout << "Class: " << info->name << " (" << info->members.size() << " members)" << std::endl;
        for (auto& m : info->members) {
            const char* kind = m.kind == MemberKind::Property ? "Property"
                             : m.kind == MemberKind::Event    ? "Event"
                                                              : "Function";
            std::cout << "  " << kind << " \"" << m.name << "\"";
            if (m.interfaceInfo) {
                std::cout << " (from " << m.interfaceInfo->name << ")";
            }
            if (m.propertyKind()) {
                std::cout << " [has default]";
            }
            std::cout << std::endl;
        }
    }

    // --- Static defaults (no instance needed) ---
    std::cout << "\n--- Static metadata defaults ---" << std::endl;
    if (auto* info = r.get_class_info(MyWidget::get_class_uid())) {
        for (auto& m : info->members) {
            if (auto* pk = m.propertyKind()) {
                auto* def = pk->getDefault ? pk->getDefault() : nullptr;
                std::cout << "  " << m.name << " default IAny: " << (def ? "ok" : "null") << std::endl;
            }
        }
        // Typed access to specific defaults
        std::cout << "  width default = " << get_default_value<float>(info->members[0]) << std::endl;
        std::cout << "  height default = " << get_default_value<float>(info->members[1]) << std::endl;
        // ISerializable::name (no custom default, should be "")
        std::cout << "  name default = \"" << get_default_value<std::string>(info->members[4]) << "\"" << std::endl;
    }

    // --- Runtime metadata via IMetadata ---
    std::cout << "\n--- MyWidget instance metadata ---" << std::endl;
    auto widget = r.create<IObject>(MyWidget::get_class_uid());
    if (auto* meta = interface_cast<IMetadata>(widget)) {
        for (auto &m : meta->get_static_metadata()) {
            std::cout << "  member: " << m.name << std::endl;
        }
        if (auto p = meta->get_property("width")) {
            std::cout << "  get_property(\"width\") ok" << std::endl;
        }
        if (auto e = meta->get_event("on_clicked")) {
            std::cout << "  get_event(\"on_clicked\") ok" << std::endl;
        }
        if (auto f = meta->get_function("reset")) {
            std::cout << "  get_function(\"reset\") ok" << std::endl;
        }
        if (!meta->get_property("Bogus")) {
            std::cout << "  get_property(\"Bogus\") correctly returned null" << std::endl;
        }
    }

    // --- Typed access via IMyWidget interface ---
    std::cout << "\n--- MyWidget via IMyWidget interface ---" << std::endl;
    if (auto* iw = interface_cast<IMyWidget>(widget)) {
        // Verify runtime properties start with declared defaults
        std::cout << "  width() initial value = " << iw->width().get_value() << " (expected 100)" << std::endl;
        std::cout << "  height() initial value = " << iw->height().get_value() << " (expected 50)" << std::endl;

        auto w = iw->width();
        w.set_value(42.f);
        std::cout << "  width().set_value(42) -> width().get_value() = " << iw->width().get_value() << std::endl;

        std::cout << "  height() ok: " << (iw->height() ? "yes" : "no") << std::endl;
        std::cout << "  on_clicked() ok: " << (iw->on_clicked() ? "yes" : "no") << std::endl;
        std::cout << "  reset() ok: " << (iw->reset() ? "yes" : "no") << std::endl;

        std::cout << "  Invoking reset()..." << std::endl;

        invoke_function(iw->reset()); // Invoke by interface accessor
        invoke_function(iw, "reset"); // Invoke by name
        invoke_function(iw, "reset", AnyT<int>(42));
    }

    // --- Uid from string ---
    std::cout << "\n--- Uid from string ---" << std::endl;
    constexpr Uid a(0xcc262192d151941f, 0xd542d4c622b50b09);
    constexpr Uid b("cc262192-d151-941f-d542-d4c622b50b09");
    static_assert(a == b, "Uid string parsing mismatch");
    static_assert(is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b09"));
    static_assert(!is_valid_uid_format("not-a-uid"));
    static_assert(!is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b0")); // too short
    static_assert(!is_valid_uid_format("cc262192Xd151-941f-d542-d4c622b50b09")); // wrong dash
    static_assert(!is_valid_uid_format("gg262192-d151-941f-d542-d4c622b50b09")); // bad hex
    // constexpr Uid bad("not-a-uid"); // would fail to compile
    std::cout << "  from ints:   " << a << std::endl;
    std::cout << "  from string: " << b << std::endl;

    // Verify STRATA_UID macro on Interface template
    static_assert(ISerializable::UID == type_uid<ISerializable>(), "auto UID should match type_uid");
    constexpr Uid customUid("a0b1c2d3-e4f5-6789-abcd-ef0123456789");
    // A hypothetical interface with a fixed UID:
    // class IFixed : public Interface<IFixed, STRATA_UID("a0b1c2d3-e4f5-6789-abcd-ef0123456789")> {};
    // would have IFixed::UID == customUid
    std::cout << "  STRATA_UID:  " << customUid << std::endl;

    // --- Typed access via ISerializable interface ---
    std::cout << "\n--- MyWidget via ISerializable interface ---" << std::endl;
    if (auto* is = interface_cast<ISerializable>(widget)) {
        auto n = is->name();
        n.set_value(std::string("MyWidget1"));
        std::cout << "  name().set_value(\"MyWidget1\") -> name().get_value() = " << is->name().get_value() << std::endl;
        std::cout << "  serialize() ok: " << (is->serialize() ? "yes" : "no") << std::endl;
    }

    // --- AnyRef: external struct storage ---
    std::cout << "\n--- AnyRef<T> with struct storage ---" << std::endl;
    {
        struct WidgetState {
            float width = 100.f;
            float height = 50.f;
        } state;

        AnyRef<float> widthRef(&state.width);
        AnyRef<float> heightRef(&state.height);

        std::cout << "  state = {" << state.width << ", " << state.height << "}" << std::endl;
        std::cout << "  widthRef.get_value() = " << widthRef.get_value() << std::endl;

        // Write through the any, reads back from struct
        widthRef.set_value(42.f);
        std::cout << "  widthRef.set_value(42) -> state.width = " << state.width << std::endl;

        // Write directly to struct, reads back through the any
        state.height = 99.f;
        std::cout << "  state.height = 99 -> heightRef.get_value() = " << heightRef.get_value() << std::endl;

        // Snapshot via memcpy
        WidgetState snapshot;
        std::memcpy(&snapshot, &state, sizeof(WidgetState));
        state.width = 0.f;
        std::cout << "  memcpy snapshot, then state.width = 0" << std::endl;
        std::cout << "  snapshot.width = " << snapshot.width << " (preserved)" << std::endl;
        std::cout << "  widthRef.get_value() = " << widthRef.get_value() << " (follows live state)" << std::endl;

        // Clone produces an owned AnyValue copy
        auto cloned = widthRef.clone();
        state.width = 777.f;
        float clonedValue{};
        cloned->get_data(&clonedValue, sizeof(float), type_uid<float>());
        std::cout << "  clone() after state.width = 777 -> cloned value = " << clonedValue << " (independent)" << std::endl;

        std::cout << "  sizeof(AnyRef<float>) " << sizeof(AnyRef<float>) << std::endl;
    }

    // --- State-backed property storage ---
    std::cout << "\n--- State-backed property storage ---" << std::endl;
    {
        auto widget2 = r.create<IObject>(MyWidget::get_class_uid());
        auto* iw = interface_cast<IMyWidget>(widget2);
        auto* ps = interface_cast<IPropertyState>(widget2);
        if (iw && ps) {
            auto *state = ps->get_property_state<IMyWidget>();
            std::cout << "  IMyWidget::State* = " << (state ? "ok" : "null") << std::endl;
            if (state) {
                // Verify defaults in state struct
                std::cout << "  state->width = " << state->width << " (expected 100)" << std::endl;
                std::cout << "  state->height = " << state->height << " (expected 50)" << std::endl;

                // Write through property, verify state reflects it
                iw->width().set_value(200.f);
                std::cout << "  After width().set_value(200): state->width = " << state->width << std::endl;

                // Write to state directly, verify property reads it
                state->height = 75.f;
                std::cout << "  After state->height = 75: height().get_value() = " << iw->height().get_value() << std::endl;
            }

            // ISerializable state
            auto* is = interface_cast<ISerializable>(widget2);
            if (is) {
                auto* sstate = static_cast<ISerializable::State*>(ps->get_property_state(ISerializable::UID));
                std::cout << "  ISerializable::State* = " << (sstate ? "ok" : "null") << std::endl;
                if (sstate) {
                    is->name().set_value(std::string("TestName"));
                    std::cout << "  After name().set_value(\"TestName\"): sstate->name = " << sstate->name << std::endl;
                }
            }
        }

        std::cout << "  sizeof(IMyWidget::State) = " << sizeof(IMyWidget::State) << std::endl;
        std::cout << "  sizeof(ISerializable::State) = " << sizeof(ISerializable::State) << std::endl;
    }

    return 0;
}
