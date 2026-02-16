#include <benchmark/benchmark.h>

#include <api/any.h>
#include <api/callback.h>
#include <api/event.h>
#include <api/function.h>
#include <api/property.h>
#include <api/strata.h>
#include <ext/object.h>
#include <interface/intf_metadata.h>

using namespace strata;

// ---------------------------------------------------------------------------
// Benchmark interface + implementation
// ---------------------------------------------------------------------------

class IBenchWidget : public Interface<IBenchWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, value, 0.f),
        (EVT, on_changed),
        (FN, do_nothing),
        (FN, add, (int, x), (float, y)),
        (FN_RAW, raw_fn)
    )
};

class BenchWidget : public ext::Object<BenchWidget, IBenchWidget>
{
    ReturnValue fn_do_nothing() override { return ReturnValue::SUCCESS; }
    ReturnValue fn_add(int, float) override { return ReturnValue::SUCCESS; }
    ReturnValue fn_raw_fn(FnArgs) override { return ReturnValue::SUCCESS; }
};

// ---------------------------------------------------------------------------
// One-time setup
// ---------------------------------------------------------------------------

static void ensureRegistered()
{
    static bool done = false;
    if (!done) {
        instance().register_type<BenchWidget>();
        done = true;
    }
}

// ---------------------------------------------------------------------------
// Property get/set
// ---------------------------------------------------------------------------

static void BM_PropertyGetValue(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    auto prop = iw->value();
    for (auto _ : state) {
        benchmark::DoNotOptimize(prop.get_value());
    }
}
BENCHMARK(BM_PropertyGetValue);

static void BM_PropertySetValue(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    auto prop = iw->value();
    float v = 0.f;
    for (auto _ : state) {
        prop.set_value(v);
        v += 1.f;
    }
}
BENCHMARK(BM_PropertySetValue);

// ---------------------------------------------------------------------------
// Direct state access
// ---------------------------------------------------------------------------

static void BM_DirectStateRead(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* ps = interface_cast<IPropertyState>(obj);
    auto* s = ps->get_property_state<IBenchWidget>();
    s->value = 42.f;
    for (auto _ : state) {
        benchmark::DoNotOptimize(s->value);
    }
}
BENCHMARK(BM_DirectStateRead);

static void BM_DirectStateWrite(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* ps = interface_cast<IPropertyState>(obj);
    auto* s = ps->get_property_state<IBenchWidget>();
    float v = 0.f;
    for (auto _ : state) {
        s->value = v;
        benchmark::ClobberMemory();
        v += 1.f;
    }
}
BENCHMARK(BM_DirectStateWrite);

// ---------------------------------------------------------------------------
// Function invoke
// ---------------------------------------------------------------------------

static void BM_FunctionInvoke(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    auto fn = iw->do_nothing();
    for (auto _ : state) {
        invoke_function(fn);
    }
}
BENCHMARK(BM_FunctionInvoke);

static void BM_FunctionInvokeTypedArgs(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    auto fn = iw->add();
    Any<int> arg0(10);
    Any<float> arg1(3.14f);
    for (auto _ : state) {
        invoke_function(fn, arg0, arg1);
    }
}
BENCHMARK(BM_FunctionInvokeTypedArgs);

static void BM_FunctionInvokeRaw(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    auto fn = iw->raw_fn();
    for (auto _ : state) {
        invoke_function(fn);
    }
}
BENCHMARK(BM_FunctionInvokeRaw);

// ---------------------------------------------------------------------------
// Event dispatch
// ---------------------------------------------------------------------------

static void BM_EventDispatchImmediate(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    Event evt = iw->on_changed();
    evt.add_handler([](FnArgs) -> ReturnValue { return ReturnValue::SUCCESS; }, Immediate);
    for (auto _ : state) {
        evt.invoke(Immediate);
    }
}
BENCHMARK(BM_EventDispatchImmediate);

static void BM_EventDispatchDeferred(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* iw = interface_cast<IBenchWidget>(obj);
    Event evt = iw->on_changed();
    evt.add_handler([](FnArgs) -> ReturnValue { return ReturnValue::SUCCESS; }, Deferred);
    for (auto _ : state) {
        evt.invoke(Deferred);
    }
}
BENCHMARK(BM_EventDispatchDeferred);

// ---------------------------------------------------------------------------
// interface_cast
// ---------------------------------------------------------------------------

static void BM_InterfaceCast(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    for (auto _ : state) {
        benchmark::DoNotOptimize(interface_cast<IBenchWidget>(obj));
    }
}
BENCHMARK(BM_InterfaceCast);

// ---------------------------------------------------------------------------
// Metadata lookup
// ---------------------------------------------------------------------------

static void BM_MetadataLookupCold(benchmark::State& state)
{
    ensureRegistered();
    for (auto _ : state) {
        state.PauseTiming();
        auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
        auto* meta = interface_cast<IMetadata>(obj);
        state.ResumeTiming();
        benchmark::DoNotOptimize(meta->get_property("value"));
    }
}
BENCHMARK(BM_MetadataLookupCold);

static void BM_MetadataLookupCached(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    meta->get_property("value"); // prime the cache
    for (auto _ : state) {
        benchmark::DoNotOptimize(meta->get_property("value"));
    }
}
BENCHMARK(BM_MetadataLookupCached);

// ---------------------------------------------------------------------------
// Object creation
// ---------------------------------------------------------------------------

static void BM_ObjectCreate(benchmark::State& state)
{
    ensureRegistered();
    auto uid = BenchWidget::get_class_uid();
    for (auto _ : state) {
        auto obj = instance().create<IObject>(uid);
        benchmark::DoNotOptimize(obj.get());
    }
}
BENCHMARK(BM_ObjectCreate);
