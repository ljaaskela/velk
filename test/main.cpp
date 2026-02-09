#include <api/ltk.h>

#include <api/any.h>
#include <api/function.h>
#include <api/property.h>
#include <ext/any.h>
#include <ext/event.h>
#include <ext/meta_object.h>
#include <interface/intf_external_any.h>
#include <interface/intf_property.h>
#include <interface/intf_registry.h>
#include <interface/types.h>
#include <iostream>

using namespace std;

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
class MyDataAny final : public SingleTypeAny<MyDataAny, Data, IExternalAny>
{
public:
    Data &Get() const override { return globalData_; }
    ReturnValue Set(const Data &value) override
    {
        if (value != globalData_) {
            globalData_.value = value.value;
            globalData_.name = value.name;
            InvokeEvent(OnDataChanged(), this);
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::NOTHING_TO_DO;
    }

    IEvent::Ptr OnDataChanged() const override
    {
        return globalData_.onChanged;
    }

private:
};

class IMyWidget : public Interface<IMyWidget>
{
public:
    LTK_INTERFACE(
        (PROP, float, Width),
        (PROP, float, Height),
        (EVT, OnClicked),
        (FN, Reset)
    )
};

class ISerializable : public Interface<ISerializable>
{
public:
    LTK_INTERFACE(
        (PROP, std::string, Name),
        (FN, Serialize)
    )
};

class MyWidget : public MetaObject<MyWidget, IMyWidget, ISerializable>
{
};

int main()
{
    auto &r = GetRegistry();

    r.RegisterType<MyDataAny>();
    r.RegisterType<MyWidget>();

    auto prop = PropertyT<float>();
    prop.Set(5.f);
    auto prop2 = prop;
    auto prop3 = PropertyT<float>(prop);

    auto value = prop.Get();
    std::cout << "Property<float> prop1 value is " << value << std::endl;
    std::cout << "Property<float> prop2 value is " << prop2.Get() << std::endl;
    std::cout << "Property<float> prop3 value is " << prop3.Get() << std::endl;

    AnyT<Data> data;                 // One view to globalData
    auto myprop = PropertyT<Data>(); // Property to global data
    myprop.Set({10.f, "Hello"});

    std::cout << "Property<Data> value is " << myprop.Get().value << ":" << myprop.Get().name
              << std::endl;

    Function valueChanged([](const IAny *any) -> ReturnValue {
        std::cout << "Property value changed, ";
        if (!any) {
            return ReturnValue::INVALID_ARGUMENT;
        }
        if (auto v = AnyT<const float>(*any)) {
            std::cout << "new value: " << v.Get();
        } else if (auto v = AnyT<const Data>(*any)) {
            auto value = v.Get();
            std::cout << "new value: " << value.name << ", " << value.value;
        } else {
            std::cout << "property not convertible to float or Data";
        }
        std::cout << std::endl;
        return ReturnValue::SUCCESS;
    });
    prop.AddOnChanged(valueChanged);
    myprop.AddOnChanged(valueChanged);

    prop.Set(10.f);
    data.Set({20.f, "Hello2"});   // Set global data directly
    myprop.Set({30.f, "Hello3"}); // Set global data through property

    std::cout << "sizeof(float)            " << sizeof(float) << std::endl;
    std::cout << "sizeof(IObject::WeakPtr) " << sizeof(IObject::WeakPtr) << std::endl;
    std::cout << "sizeof(AnyT<float>)      " << sizeof(AnyT<float>) << std::endl;
    std::cout << "sizeof(SimpleAny<float>) " << sizeof(SimpleAny<float>) << std::endl;
    std::cout << "sizeof(PropertyT<float>) " << sizeof(PropertyT<float>) << std::endl;
    std::cout << "sizeof(MyDataAny)        " << sizeof(MyDataAny) << std::endl;

    // --- Static metadata via registry (no instance needed) ---
    std::cout << "\n--- MyWidget static metadata ---" << std::endl;
    if (auto* info = r.GetClassInfo(MyWidget::GetClassUid())) {
        std::cout << "Class: " << info->name << " (" << info->members.size() << " members)" << std::endl;
        for (auto& m : info->members) {
            const char* kind = m.kind == MemberKind::Property ? "Property"
                             : m.kind == MemberKind::Event    ? "Event"
                                                              : "Function";
            std::cout << "  " << kind << " \"" << m.name << "\"";
            if (m.interfaceInfo) {
                std::cout << " (from " << m.interfaceInfo->name << ")";
            }
            std::cout << std::endl;
        }
    }

    // --- Runtime metadata via IMetadata ---
    std::cout << "\n--- MyWidget instance metadata ---" << std::endl;
    auto widget = r.Create<IObject>(MyWidget::GetClassUid());
    if (auto* meta = interface_cast<IMetadata>(widget)) {
        for (auto &m : meta->GetStaticMetadata()) {
            std::cout << "  member: " << m.name << std::endl;
        }
        if (auto p = meta->GetProperty("Width")) {
            std::cout << "  GetProperty(\"Width\") ok" << std::endl;
        }
        if (auto e = meta->GetEvent("OnClicked")) {
            std::cout << "  GetEvent(\"OnClicked\") ok" << std::endl;
        }
        if (auto f = meta->GetFunction("Reset")) {
            std::cout << "  GetFunction(\"Reset\") ok" << std::endl;
        }
        if (!meta->GetProperty("Bogus")) {
            std::cout << "  GetProperty(\"Bogus\") correctly returned null" << std::endl;
        }
    }

    // --- Typed access via IMyWidget interface ---
    std::cout << "\n--- MyWidget via IMyWidget interface ---" << std::endl;
    if (auto* iw = interface_cast<IMyWidget>(widget)) {
        auto width = iw->Width();
        width.Set(42.f);
        std::cout << "  Width().Set(42) -> Width().Get() = " << iw->Width().Get() << std::endl;

        std::cout << "  Height() ok: " << (iw->Height() ? "yes" : "no") << std::endl;
        std::cout << "  OnClicked() ok: " << (iw->OnClicked() ? "yes" : "no") << std::endl;
        std::cout << "  Reset() ok: " << (iw->Reset() ? "yes" : "no") << std::endl;
    }

    // --- Typed access via ISerializable interface ---
    std::cout << "\n--- MyWidget via ISerializable interface ---" << std::endl;
    if (auto* is = interface_cast<ISerializable>(widget)) {
        auto name = is->Name();
        name.Set(std::string("MyWidget1"));
        std::cout << "  Name().Set(\"MyWidget1\") -> Name().Get() = " << is->Name().Get() << std::endl;
        std::cout << "  Serialize() ok: " << (is->Serialize() ? "yes" : "no") << std::endl;
    }

    return 0;
}
