#include <velk/api/any.h>
#include <velk/api/callback.h>
#include <velk/api/event.h>
#include <velk/api/function.h>
#include <velk/api/property.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/api/hive/iterate.h>
#include <velk/interface/hive/intf_hive_store.h>
#include <velk/interface/intf_metadata.h>

#include <benchmark/benchmark.h>
#include <memory>
#include <mutex>
#include <vector>

using namespace velk;

// ---------------------------------------------------------------------------
// Benchmark interface + implementation
// ---------------------------------------------------------------------------

class IBenchWidget : public Interface<IBenchWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, value, 0.f),
        (EVT, on_changed),
        (FN, void, do_nothing),
        (FN, void, add, (int, x), (float, y)),
        (FN_RAW, raw_fn)
    )
};

class BenchWidget : public ext::Object<BenchWidget, IBenchWidget>
{
    void fn_do_nothing() override {}
    void fn_add(int, float) override {}
    IAny::Ptr fn_raw_fn(FnArgs) override { return nullptr; }
};

// ---------------------------------------------------------------------------
// One-time setup
// ---------------------------------------------------------------------------

static void ensureRegistered()
{
    static bool done = false;
    if (!done) {
        instance().type_registry().register_type<BenchWidget>();
        done = true;
    }
}

// ---------------------------------------------------------------------------
// Property get/set
// ---------------------------------------------------------------------------

static void BM_PropertyGetValue(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
    auto* iw = interface_cast<IBenchWidget>(obj);
    Event evt = iw->on_changed();
    evt.add_handler([](FnArgs) -> ReturnValue { return ReturnValue::Success; }, Immediate);
    for (auto _ : state) {
        evt.invoke(Immediate);
    }
}
BENCHMARK(BM_EventDispatchImmediate);

static void BM_EventDispatchDeferred(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::class_id());
    auto* iw = interface_cast<IBenchWidget>(obj);
    Event evt = iw->on_changed();
    evt.add_handler([](FnArgs) -> ReturnValue { return ReturnValue::Success; }, Deferred);
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
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
        auto obj = instance().create<IObject>(BenchWidget::class_id());
        auto* meta = interface_cast<IMetadata>(obj);
        state.ResumeTiming();
        benchmark::DoNotOptimize(meta->get_property("value"));
    }
}
BENCHMARK(BM_MetadataLookupCold);

static void BM_MetadataLookupCached(benchmark::State& state)
{
    ensureRegistered();
    auto obj = instance().create<IObject>(BenchWidget::class_id());
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
    auto uid = BenchWidget::class_id();
    for (auto _ : state) {
        auto obj = instance().create<IObject>(uid);
        benchmark::DoNotOptimize(obj.get());
    }
}
BENCHMARK(BM_ObjectCreate);

// ---------------------------------------------------------------------------
// Control block allocation: pooled vs raw new/delete
// ---------------------------------------------------------------------------

static void BM_ControlBlockPooled(benchmark::State& state)
{
    for (auto _ : state) {
        auto* b = detail::alloc_control_block();
        benchmark::DoNotOptimize(b);
        detail::dealloc_control_block(b);
    }
}
BENCHMARK(BM_ControlBlockPooled);

static void BM_ControlBlockNewDelete(benchmark::State& state)
{
    for (auto _ : state) {
        auto* b = new control_block;
        b->strong.store(1, std::memory_order_relaxed);
        benchmark::DoNotOptimize(b);
        delete b;
    }
}
BENCHMARK(BM_ControlBlockNewDelete);

// ---------------------------------------------------------------------------
// Hive vs vector-of-structs comparison
// ---------------------------------------------------------------------------

class PlainData
{
public:
    PlainData() : f0(0.f), f1(1.f), f2(2.f), f3(3.f), f4(4.f), i0(0), i1(1), i2(2), i3(3), i4(4) {}

    float f0, f1, f2, f3, f4;
    int i0, i1, i2, i3, i4;
};

class HeapBase
{
public:
    virtual ~HeapBase() = default;
    virtual float sum_fields() const = 0;
};

class HeapObject : public HeapBase
{
public:
    HeapObject() : f0(0.f), f1(1.f), f2(2.f), f3(3.f), f4(4.f), i0(0), i1(1), i2(2), i3(3), i4(4) {}

    float sum_fields() const override
    {
        return f0 + f1 + f2 + f3 + f4 + static_cast<float>(i0 + i1 + i2 + i3 + i4);
    }

    float f0, f1, f2, f3, f4;
    int i0, i1, i2, i3, i4;
};

class IHiveData : public Interface<IHiveData>
{
public:
    VELK_INTERFACE(
        (PROP, float, f0, 0.f),
        (PROP, float, f1, 1.f),
        (PROP, float, f2, 2.f),
        (PROP, float, f3, 3.f),
        (PROP, float, f4, 4.f),
        (PROP, int, i0, 0),
        (PROP, int, i1, 1),
        (PROP, int, i2, 2),
        (PROP, int, i3, 3),
        (PROP, int, i4, 4)
    )
};

class HiveData : public ext::Object<HiveData, IHiveData>
{};

static constexpr size_t kHiveCount = 512;

static void ensureHiveRegistered()
{
    static bool done = false;
    if (!done) {
        register_type<HiveData>(instance());
        done = true;
    }
}

// --- Memory reporting ---

static void BM_MemoryPlainVector(benchmark::State& state)
{
    for (auto _ : state) {
        std::vector<PlainData> vec(kHiveCount);
        benchmark::DoNotOptimize(vec.data());
    }
    state.counters["bytes"] = static_cast<double>(kHiveCount * sizeof(PlainData));
}
BENCHMARK(BM_MemoryPlainVector);

static void BM_MemoryHive(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);

    for (auto _ : state) {
        auto hive = registry->get_hive<HiveData>();
        std::vector<IObject::Ptr> refs;
        refs.reserve(kHiveCount);
        for (size_t i = 0; i < kHiveCount; ++i) {
            refs.push_back(hive->add());
        }
        benchmark::DoNotOptimize(hive.get());
        // Teardown: remove all objects.
        state.PauseTiming();
        for (auto& r : refs) {
            hive->remove(*r);
            r.reset();
        }
        state.ResumeTiming();
    }
    state.counters["obj_size"] = static_cast<double>(sizeof(HiveData));
}
BENCHMARK(BM_MemoryHive);

// --- Creation speed ---

static void BM_CreatePlainVector(benchmark::State& state)
{
    for (auto _ : state) {
        std::vector<PlainData> vec(kHiveCount);
        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_CreatePlainVector);

static void BM_CreateHeapVector(benchmark::State& state)
{
    for (auto _ : state) {
        std::vector<std::unique_ptr<HeapBase>> vec;
        vec.reserve(kHiveCount);
        for (size_t i = 0; i < kHiveCount; ++i) {
            vec.push_back(std::make_unique<HeapObject>());
        }
        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_CreateHeapVector);

static void BM_CreateVelkVector(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto uid = HiveData::class_id();

    for (auto _ : state) {
        std::vector<IObject::Ptr> vec;
        vec.reserve(kHiveCount);
        for (size_t i = 0; i < kHiveCount; ++i) {
            vec.push_back(instance().create<IObject>(uid));
        }
        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_CreateVelkVector);

static void BM_CreateHive(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);
    auto hive = registry->get_hive<HiveData>();

    std::vector<IObject::Ptr> teardown_refs;
    teardown_refs.reserve(kHiveCount);
    for (auto _ : state) {
        for (size_t i = 0; i < kHiveCount; ++i) {
            auto obj = hive->add();
            benchmark::DoNotOptimize(obj.get());
            teardown_refs.push_back(std::move(obj));
        }
        // Teardown
        state.PauseTiming();
        for (auto& r : teardown_refs) {
            hive->remove(*r);
            r.reset();
        }
        teardown_refs.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(BM_CreateHive);

// --- Iteration speed: read all 10 fields, accumulate ---

static void BM_IteratePlainVector(benchmark::State& state)
{
    std::vector<PlainData> vec(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        vec[i].f0 = static_cast<float>(i);
        vec[i].i0 = static_cast<int>(i);
    }

    for (auto _ : state) {
        float sum = 0.f;
        for (auto& d : vec) {
            sum += d.f0 + d.f1 + d.f2 + d.f3 + d.f4;
            sum += static_cast<float>(d.i0 + d.i1 + d.i2 + d.i3 + d.i4);
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_IteratePlainVector);

static void BM_IterateHeapVector(benchmark::State& state)
{
    std::vector<std::unique_ptr<HeapBase>> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        auto obj = std::make_unique<HeapObject>();
        obj->f0 = static_cast<float>(i);
        obj->i0 = static_cast<int>(i);
        vec.push_back(std::move(obj));
    }

    for (auto _ : state) {
        float sum = 0.f;
        for (auto& obj : vec) {
            auto* h = static_cast<HeapObject*>(obj.get());
            sum += h->f0 + h->f1 + h->f2 + h->f3 + h->f4;
            sum += static_cast<float>(h->i0 + h->i1 + h->i2 + h->i3 + h->i4);
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_IterateHeapVector);

static void BM_IterateVelkVector(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();

    std::vector<IObject::Ptr> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        auto obj = instance().create<IObject>(HiveData::class_id());
        auto* ps = interface_cast<IPropertyState>(obj);
        auto* s = ps->get_property_state<IHiveData>();
        s->f0 = static_cast<float>(i);
        s->i0 = static_cast<int>(i);
        vec.push_back(std::move(obj));
    }

    for (auto _ : state) {
        float sum = 0.f;
        for (auto& obj : vec) {
            auto* ps = interface_cast<IPropertyState>(obj);
            auto* s = ps->get_property_state<IHiveData>();
            sum += s->f0 + s->f1 + s->f2 + s->f3 + s->f4;
            sum += static_cast<float>(s->i0 + s->i1 + s->i2 + s->i3 + s->i4);
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_IterateVelkVector);

static void BM_IterateHive(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);
    auto hive = registry->get_hive<HiveData>();

    std::vector<IObject::Ptr> refs;
    refs.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        auto obj = hive->add();
        auto* ps = interface_cast<IPropertyState>(obj);
        auto* s = ps->get_property_state<IHiveData>();
        s->f0 = static_cast<float>(i);
        s->i0 = static_cast<int>(i);
        refs.push_back(std::move(obj));
    }

    for (auto _ : state) {
        float sum = 0.f;
        hive->for_each(&sum, [](void* ctx, IObject& obj) -> bool {
            auto* ps = interface_cast<IPropertyState>(&obj);
            auto* s = ps->get_property_state<IHiveData>();
            float& sum = *static_cast<float*>(ctx);
            sum += s->f0 + s->f1 + s->f2 + s->f3 + s->f4;
            sum += static_cast<float>(s->i0 + s->i1 + s->i2 + s->i3 + s->i4);
            return true;
        });
        benchmark::DoNotOptimize(sum);
    }

    refs.clear();
}
BENCHMARK(BM_IterateHive);

static void BM_IterateHiveState(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);
    auto hive = registry->get_hive<HiveData>();

    std::vector<IObject::Ptr> refs;
    refs.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        auto obj = hive->add();
        auto* ps = interface_cast<IPropertyState>(obj);
        auto* s = ps->get_property_state<IHiveData>();
        s->f0 = static_cast<float>(i);
        s->i0 = static_cast<int>(i);
        refs.push_back(std::move(obj));
    }

    for (auto _ : state) {
        float sum = 0.f;
        for_each_hive<IHiveData>(*hive, [&](IObject&, IHiveData::State& s) {
            sum += s.f0 + s.f1 + s.f2 + s.f3 + s.f4;
            sum += static_cast<float>(s.i0 + s.i1 + s.i2 + s.i3 + s.i4);
            return true;
        });
        benchmark::DoNotOptimize(sum);
    }

    refs.clear();
}
BENCHMARK(BM_IterateHiveState);

// --- Iteration speed: write all 10 fields ---

static void BM_IterateWritePlainVector(benchmark::State& state)
{
    std::vector<PlainData> vec(kHiveCount);

    for (auto _ : state) {
        float v = 0.f;
        for (auto& d : vec) {
            d.f0 = v;
            d.f1 = v;
            d.f2 = v;
            d.f3 = v;
            d.f4 = v;
            int iv = static_cast<int>(v);
            d.i0 = iv;
            d.i1 = iv;
            d.i2 = iv;
            d.i3 = iv;
            d.i4 = iv;
            v += 1.f;
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_IterateWritePlainVector);

static void BM_IterateWriteHeapVector(benchmark::State& state)
{
    std::vector<std::unique_ptr<HeapBase>> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        vec.push_back(std::make_unique<HeapObject>());
    }

    float counter = 0.f;
    for (auto _ : state) {
        float v = counter;
        for (auto& obj : vec) {
            auto* h = static_cast<HeapObject*>(obj.get());
            h->f0 = v;
            h->f1 = v;
            h->f2 = v;
            h->f3 = v;
            h->f4 = v;
            int iv = static_cast<int>(v);
            h->i0 = iv;
            h->i1 = iv;
            h->i2 = iv;
            h->i3 = iv;
            h->i4 = iv;
            v += 1.f;
        }
        benchmark::ClobberMemory();
        counter = v;
    }
}
BENCHMARK(BM_IterateWriteHeapVector);

static void BM_IterateWriteVelkVector(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();

    std::vector<IObject::Ptr> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        vec.push_back(instance().create<IObject>(HiveData::class_id()));
    }

    float counter = 0.f;
    for (auto _ : state) {
        float v = counter;
        for (auto& obj : vec) {
            auto* ps = interface_cast<IPropertyState>(obj);
            auto* s = ps->get_property_state<IHiveData>();
            s->f0 = v;
            s->f1 = v;
            s->f2 = v;
            s->f3 = v;
            s->f4 = v;
            int iv = static_cast<int>(v);
            s->i0 = iv;
            s->i1 = iv;
            s->i2 = iv;
            s->i3 = iv;
            s->i4 = iv;
            v += 1.f;
        }
        benchmark::ClobberMemory();
        counter = v;
    }
}
BENCHMARK(BM_IterateWriteVelkVector);

static void BM_IterateWriteHive(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);
    auto hive = registry->get_hive<HiveData>();

    std::vector<IObject::Ptr> refs;
    refs.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        refs.push_back(hive->add());
    }

    float counter = 0.f;
    for (auto _ : state) {
        hive->for_each(&counter, [](void* ctx, IObject& obj) -> bool {
            float& v = *static_cast<float*>(ctx);
            auto* ps = interface_cast<IPropertyState>(&obj);
            auto* s = ps->get_property_state<IHiveData>();
            s->f0 = v;
            s->f1 = v;
            s->f2 = v;
            s->f3 = v;
            s->f4 = v;
            int iv = static_cast<int>(v);
            s->i0 = iv;
            s->i1 = iv;
            s->i2 = iv;
            s->i3 = iv;
            s->i4 = iv;
            v += 1.f;
            return true;
        });
        benchmark::ClobberMemory();
    }

    refs.clear();
}
BENCHMARK(BM_IterateWriteHive);

static void BM_IterateWriteHiveState(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);
    auto hive = registry->get_hive<HiveData>();

    std::vector<IObject::Ptr> refs;
    refs.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        refs.push_back(hive->add());
    }

    float counter = 0.f;
    for (auto _ : state) {
        float v = counter;
        for_each_hive<IHiveData>(*hive, [&](IObject&, IHiveData::State& s) {
            s.f0 = v;
            s.f1 = v;
            s.f2 = v;
            s.f3 = v;
            s.f4 = v;
            int iv = static_cast<int>(v);
            s.i0 = iv;
            s.i1 = iv;
            s.i2 = iv;
            s.i3 = iv;
            s.i4 = iv;
            return true;
        });
        benchmark::ClobberMemory();
        counter += kHiveCount;
    }

    refs.clear();
}
BENCHMARK(BM_IterateWriteHiveState);

// --- Churn: erase every 4th element, then repopulate back to 512 ---

static void BM_ChurnPlainVector(benchmark::State& state)
{
    std::vector<PlainData> vec(kHiveCount);

    for (auto _ : state) {
        // Erase every 4th element (iterate backwards to keep indices stable).
        for (size_t i = vec.size(); i > 0; --i) {
            if ((i - 1) % 4 == 0) {
                vec.erase(vec.begin() + static_cast<ptrdiff_t>(i - 1));
            }
        }

        // Repopulate back to 512.
        while (vec.size() < kHiveCount) {
            vec.emplace_back();
        }

        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_ChurnPlainVector);

static void BM_ChurnHeapVector(benchmark::State& state)
{
    std::vector<std::unique_ptr<HeapBase>> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        vec.push_back(std::make_unique<HeapObject>());
    }

    for (auto _ : state) {
        // Erase every 4th element (iterate backwards to keep indices stable).
        for (size_t i = vec.size(); i > 0; --i) {
            if ((i - 1) % 4 == 0) {
                vec.erase(vec.begin() + static_cast<ptrdiff_t>(i - 1));
            }
        }

        // Repopulate back to 512.
        while (vec.size() < kHiveCount) {
            vec.push_back(std::make_unique<HeapObject>());
        }

        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_ChurnHeapVector);

static void BM_ChurnVelkVector(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto uid = HiveData::class_id();

    std::vector<IObject::Ptr> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        vec.push_back(instance().create<IObject>(uid));
    }

    for (auto _ : state) {
        // Erase every 4th element (iterate backwards to keep indices stable).
        for (size_t i = vec.size(); i > 0; --i) {
            if ((i - 1) % 4 == 0) {
                vec.erase(vec.begin() + static_cast<ptrdiff_t>(i - 1));
            }
        }

        // Repopulate back to 512.
        while (vec.size() < kHiveCount) {
            vec.push_back(instance().create<IObject>(uid));
        }

        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_ChurnVelkVector);

static void BM_ChurnPlainVectorLocked(benchmark::State& state)
{
    std::vector<PlainData> vec(kHiveCount);
    std::mutex mtx;

    for (auto _ : state) {
        for (size_t i = vec.size(); i > 0; --i) {
            if ((i - 1) % 4 == 0) {
                std::lock_guard lock(mtx);
                vec.erase(vec.begin() + static_cast<ptrdiff_t>(i - 1));
            }
        }

        while (vec.size() < kHiveCount) {
            std::lock_guard lock(mtx);
            vec.emplace_back();
        }

        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_ChurnPlainVectorLocked);

static void BM_ChurnHeapVectorLocked(benchmark::State& state)
{
    std::vector<std::unique_ptr<HeapBase>> vec;
    vec.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        vec.push_back(std::make_unique<HeapObject>());
    }
    std::mutex mtx;

    for (auto _ : state) {
        for (size_t i = vec.size(); i > 0; --i) {
            if ((i - 1) % 4 == 0) {
                std::lock_guard lock(mtx);
                vec.erase(vec.begin() + static_cast<ptrdiff_t>(i - 1));
            }
        }

        while (vec.size() < kHiveCount) {
            std::lock_guard lock(mtx);
            vec.push_back(std::make_unique<HeapObject>());
        }

        benchmark::DoNotOptimize(vec.data());
    }
}
BENCHMARK(BM_ChurnHeapVectorLocked);

static void BM_ChurnHive(benchmark::State& state)
{
    ensureRegistered();
    ensureHiveRegistered();
    auto registry = instance().create<IHiveStore>(ClassId::HiveStore);
    auto hive = registry->get_hive<HiveData>();

    // Pre-populate.
    std::vector<IObject::Ptr> refs;
    refs.reserve(kHiveCount);
    for (size_t i = 0; i < kHiveCount; ++i) {
        refs.push_back(hive->add());
    }

    for (auto _ : state) {
        // Remove every 4th element.
        for (size_t i = 0; i < refs.size(); i += 4) {
            hive->remove(*refs[i]);
            refs[i].reset();
        }

        // Repopulate the removed slots.
        for (size_t i = 0; i < refs.size(); i += 4) {
            refs[i] = hive->add();
        }

        benchmark::DoNotOptimize(hive.get());
    }

    // Teardown.
    for (auto& r : refs) {
        if (r) {
            hive->remove(*r);
            r.reset();
        }
    }
}
BENCHMARK(BM_ChurnHive);
