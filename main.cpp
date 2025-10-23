#include <api/any.h>
#include <api/function.h>
#include <api/property.h>
#include <ext/any.h>
#include <ext/event.h>
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

    IEvent::Ptr onChanged;
};

Data globalData_;

DECLARE_CLASS(MyDataAny)

// Custom any type which accesses global data
class MyDataAny final : public SingleTypeAny<MyDataAny, Data, IExternalAny>
{
    IMPLEMENT_CLASS(ClassId::MyDataAny)
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
        if (!globalData_.onChanged) {
            globalData_.onChanged = GetRegistry().Create<IEvent>(ClassId::Event);
        }
        return globalData_.onChanged;
    }

private:
};

int main()
{
    auto &r = GetRegistry();

    r.RegisterType<MyDataAny>();

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

    return 0;
}
