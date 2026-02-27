#include <velk/api/hive/hive.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/hive/intf_hive_store.h>
#include <velk/interface/intf_metadata.h>

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace velk;

// --- Test interface and object for hive storage ---

class IObjectHiveWidget : public Interface<IObjectHiveWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, x, 0.f),
        (PROP, float, y, 0.f)
    )
};

class HiveWidget : public ext::Object<HiveWidget, IObjectHiveWidget>
{};

// A second type so that tests needing a fresh hive can use it.
class IObjectHiveGadget : public Interface<IObjectHiveGadget>
{
public:
    VELK_INTERFACE(
        (PROP, int, id, 0)
    )
};

class HiveGadget : public ext::Object<HiveGadget, IObjectHiveGadget>
{};

// --- Test fixture ---

class HiveTest : public ::testing::Test
{
protected:
    IVelk& velk_ = instance();

    void SetUp() override
    {
        register_type<HiveWidget>(velk_);
        register_type<HiveGadget>(velk_);
        registry_ = velk_.create<IHiveStore>(ClassId::HiveStore);
        ASSERT_TRUE(registry_);
    }

    void TearDown() override
    {
        registry_.reset();
        unregister_type<HiveGadget>(velk_);
        unregister_type<HiveWidget>(velk_);
    }

    /** @brief Returns a fresh hive for HiveGadget (never used by other tests). */
    IObjectHive::Ptr fresh_hive() { return registry_->get_hive(HiveGadget::class_id()); }

    IHiveStore::Ptr registry_;
};

// --- IHiveStore tests ---

TEST_F(HiveTest, GetHiveCreatesHive)
{
    auto initial = registry_->hive_count();
    auto hive = registry_->get_hive(HiveWidget::class_id());
    ASSERT_TRUE(hive);
    EXPECT_GE(registry_->hive_count(), initial);
}

TEST_F(HiveTest, GetHiveReturnsSameInstance)
{
    auto hive1 = registry_->get_hive(HiveWidget::class_id());
    auto hive2 = registry_->get_hive(HiveWidget::class_id());
    EXPECT_EQ(hive1.get(), hive2.get());
}

TEST_F(HiveTest, FindHiveReturnsNullForUnregistered)
{
    Uid bogus{"ffffffff-ffff-ffff-ffff-ffffffffffff"};
    EXPECT_FALSE(registry_->find_hive(bogus));
}

TEST_F(HiveTest, FindHiveReturnsExisting)
{
    auto hive = registry_->get_hive(HiveWidget::class_id());
    auto found = registry_->find_hive(HiveWidget::class_id());
    ASSERT_TRUE(found);
    EXPECT_EQ(hive.get(), found.get());
}

TEST_F(HiveTest, GetHiveUnregisteredTypeReturnsNull)
{
    Uid bogus{"ffffffff-ffff-ffff-ffff-ffffffffffff"};
    EXPECT_FALSE(registry_->get_hive(bogus));
}

TEST_F(HiveTest, GetHiveTyped)
{
    auto hive = registry_->get_hive<HiveWidget>();
    ASSERT_TRUE(hive);
    EXPECT_EQ(HiveWidget::class_id(), hive->get_element_uid());
}

TEST_F(HiveTest, ForEachHive)
{
    // Ensure at least one hive exists
    registry_->get_hive(HiveWidget::class_id());

    int count = 0;
    registry_->for_each_hive(&count, [](void* ctx, IHive&) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_GE(count, 1);
}

// --- IObjectHive tests (use fresh_hive() for isolation) ---

TEST_F(HiveTest, NewHiveIsEmpty)
{
    auto hive = fresh_hive();
    EXPECT_TRUE(hive->empty());
    EXPECT_EQ(0u, hive->size());
}

TEST_F(HiveTest, ElementClassUid)
{
    auto hive = fresh_hive();
    EXPECT_EQ(HiveGadget::class_id(), hive->get_element_uid());
}

TEST_F(HiveTest, AddCreatesObject)
{
    auto hive = fresh_hive();
    auto obj = hive->add();
    ASSERT_TRUE(obj);
    EXPECT_EQ(1u, hive->size());
    EXPECT_FALSE(hive->empty());
}

TEST_F(HiveTest, AddedObjectHasCorrectType)
{
    auto hive = fresh_hive();
    auto obj = hive->add();
    EXPECT_EQ(HiveGadget::class_id(), obj->get_class_uid());

    auto* gadget = interface_cast<IObjectHiveGadget>(obj);
    ASSERT_NE(nullptr, gadget);
}

TEST_F(HiveTest, AddMultiple)
{
    auto hive = fresh_hive();
    auto before = hive->size();
    auto obj1 = hive->add();
    auto obj2 = hive->add();
    auto obj3 = hive->add();
    EXPECT_EQ(before + 3, hive->size());
    EXPECT_NE(obj1.get(), obj2.get());
    EXPECT_NE(obj2.get(), obj3.get());
}

TEST_F(HiveTest, ContainsFindsAddedObject)
{
    auto hive = fresh_hive();
    auto obj = hive->add();
    EXPECT_TRUE(hive->contains(*obj));
}

TEST_F(HiveTest, ContainsReturnsFalseForUnknownObject)
{
    auto hive = fresh_hive();
    auto standalone = velk_.create<IObject>(HiveGadget::class_id());
    EXPECT_FALSE(hive->contains(*standalone));
}

TEST_F(HiveTest, RemoveSucceeds)
{
    auto hive = fresh_hive();
    auto before = hive->size();
    auto obj = hive->add();
    EXPECT_EQ(ReturnValue::Success, hive->remove(*obj));
    EXPECT_EQ(before, hive->size());
}

TEST_F(HiveTest, RemoveNotFoundFails)
{
    auto hive = fresh_hive();
    auto standalone = velk_.create<IObject>(HiveGadget::class_id());
    EXPECT_EQ(ReturnValue::Fail, hive->remove(*standalone));
}

TEST_F(HiveTest, ContainsReturnsFalseAfterRemove)
{
    auto hive = fresh_hive();
    auto obj = hive->add();
    hive->remove(*obj);
    EXPECT_FALSE(hive->contains(*obj));
}

TEST_F(HiveTest, ObjectSurvivesRemoveWhileReferenced)
{
    auto hive = fresh_hive();
    auto obj = hive->add();
    auto held = obj; // extra strong reference

    hive->remove(*obj);

    // Object should still be alive because we hold a reference
    EXPECT_NE(nullptr, held.get());
    EXPECT_EQ(HiveGadget::class_id(), held->get_class_uid());
}

TEST_F(HiveTest, SlotReuse)
{
    auto hive = fresh_hive();
    auto before = hive->size();
    hive->add();
    auto obj2 = hive->add();
    hive->add();
    EXPECT_EQ(before + 3, hive->size());

    // Remove the middle one
    hive->remove(*obj2);
    EXPECT_EQ(before + 2, hive->size());

    // Add a new one, should reuse the freed slot
    auto obj4 = hive->add();
    EXPECT_EQ(before + 3, hive->size());
    EXPECT_TRUE(hive->contains(*obj4));
}

TEST_F(HiveTest, ForEachVisitsAllLiveObjects)
{
    auto hive = fresh_hive();
    auto before = hive->size();
    hive->add();
    hive->add();
    hive->add();

    int count = 0;
    hive->for_each(&count, [](void* ctx, IObject&) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_EQ(static_cast<int>(before) + 3, count);
}

TEST_F(HiveTest, ForEachSkipsRemovedSlots)
{
    auto hive = fresh_hive();
    auto before = hive->size();
    hive->add();
    auto obj2 = hive->add();
    hive->add();

    hive->remove(*obj2);

    int count = 0;
    hive->for_each(&count, [](void* ctx, IObject&) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_EQ(static_cast<int>(before) + 2, count);
}

TEST_F(HiveTest, ForEachStopsOnFalse)
{
    auto hive = fresh_hive();
    hive->add();
    hive->add();
    hive->add();

    int count = 0;
    hive->for_each(&count, [](void* ctx, IObject&) -> bool {
        ++(*static_cast<int*>(ctx));
        return false; // stop after first
    });
    EXPECT_EQ(1, count);
}

TEST_F(HiveTest, ForEachOnEmptyHive)
{
    auto hive = fresh_hive();

    // Remove all objects that may have been added by previous tests
    size_t initial = hive->size();
    int count = 0;
    hive->for_each(&count, [](void* ctx, IObject&) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_EQ(static_cast<int>(initial), count);
}

TEST_F(HiveTest, HiveIsVelkObject)
{
    auto hive = fresh_hive();

    // IObjectHive inherits IObject, so we can query class info
    auto* obj = interface_cast<IObject>(hive);
    ASSERT_NE(nullptr, obj);
    EXPECT_FALSE(obj->get_class_name().empty());
}

TEST_F(HiveTest, CreateHiveStoreViaClassId)
{
    // Verify that a second HiveStore can be created independently.
    auto reg2 = velk_.create<IHiveStore>(ClassId::HiveStore);
    ASSERT_TRUE(reg2);

    auto hive = reg2->get_hive(HiveWidget::class_id());
    EXPECT_TRUE(hive);

    // The two registries are independent instances.
    EXPECT_NE(registry_.get(), reg2.get());
}

// --- Placement storage tests ---

TEST_F(HiveTest, PageGrowth)
{
    // Add enough objects to trigger multiple pages (first page = 16 slots).
    auto hive = fresh_hive();
    std::vector<IObject::Ptr> objs;
    for (int i = 0; i < 100; ++i) {
        auto obj = hive->add();
        ASSERT_TRUE(obj);
        objs.push_back(obj);
    }
    EXPECT_EQ(100u, hive->size());

    // All objects should be accessible via for_each.
    int count = 0;
    hive->for_each(&count, [](void* ctx, IObject&) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_EQ(100, count);
}

TEST_F(HiveTest, ZombieToFreeTransition)
{
    // Remove an object while an external ref exists. Verify the slot is
    // reused after the external ref drops.
    auto hive = fresh_hive();
    auto obj = hive->add();
    auto held = obj; // external ref

    hive->remove(*obj);
    obj.reset(); // drop our copy, held still alive
    EXPECT_EQ(0u, hive->size());

    // Object still alive via held.
    EXPECT_NE(nullptr, held.get());

    // Drop the last external ref. The destroy callback should free the slot.
    held.reset();

    // Add a new object. The freed slot should be reused.
    auto obj2 = hive->add();
    ASSERT_TRUE(obj2);
    EXPECT_EQ(1u, hive->size());
}

TEST_F(HiveTest, FillAndEmptyPage)
{
    // Fill the first page (16 slots), then remove all objects.
    auto hive = fresh_hive();
    std::vector<IObject::Ptr> objs;
    for (int i = 0; i < 16; ++i) {
        objs.push_back(hive->add());
    }
    EXPECT_EQ(16u, hive->size());

    // Remove all.
    for (auto& o : objs) {
        hive->remove(*o);
    }
    EXPECT_EQ(0u, hive->size());

    // Drop all external refs.
    objs.clear();

    // Verify the hive is empty and functional.
    EXPECT_TRUE(hive->empty());
    auto fresh = hive->add();
    ASSERT_TRUE(fresh);
    EXPECT_EQ(1u, hive->size());
}

TEST_F(HiveTest, GetSelfWorksForHiveObjects)
{
    auto hive = fresh_hive();
    auto obj = hive->add();

    auto self = obj->get_self();
    ASSERT_TRUE(self);
    EXPECT_EQ(obj.get(), self.get());
}

TEST_F(HiveTest, MetadataAvailableOnHiveObjects)
{
    auto hive = registry_->get_hive(HiveWidget::class_id());
    auto obj = hive->add();

    auto* widget = interface_cast<IObjectHiveWidget>(obj);
    ASSERT_NE(nullptr, widget);

    // Properties should be accessible via metadata.
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(nullptr, meta);
    auto prop = meta->get_property("x");
    EXPECT_TRUE(prop);
}

// --- for_each_state and for_each_hive tests ---

TEST_F(HiveTest, ForEachStateVisitsAllWithCorrectValues)
{
    auto hive = registry_->get_hive(HiveWidget::class_id());
    std::vector<IObject::Ptr> refs;
    for (int i = 0; i < 5; ++i) {
        auto obj = hive->add();
        auto* ps = interface_cast<IPropertyState>(obj);
        auto* s = ps->get_property_state<IObjectHiveWidget>();
        s->x = static_cast<float>(i * 10);
        s->y = static_cast<float>(i * 20);
        refs.push_back(std::move(obj));
    }

    ptrdiff_t offset = detail::compute_state_offset(*hive, IObjectHiveWidget::UID);
    ASSERT_GE(offset, 0);

    float sum_x = 0.f;
    hive->for_each_state(offset, &sum_x, [](void* ctx, IObject&, void* state) -> bool {
        auto& s = *static_cast<IObjectHiveWidget::State*>(state);
        *static_cast<float*>(ctx) += s.x;
        return true;
    });
    // sum_x should be 0 + 10 + 20 + 30 + 40 = 100
    EXPECT_FLOAT_EQ(100.f, sum_x);

    // Clean up
    for (auto& r : refs) {
        hive->remove(*r);
    }
}

TEST_F(HiveTest, ForEachStateEarlyTermination)
{
    auto hive = fresh_hive();
    std::vector<IObject::Ptr> refs;
    for (int i = 0; i < 5; ++i) {
        refs.push_back(hive->add());
    }

    ptrdiff_t offset = detail::compute_state_offset(*hive, IObjectHiveGadget::UID);
    ASSERT_GE(offset, 0);

    int count = 0;
    hive->for_each_state(offset, &count, [](void* ctx, IObject&, void*) -> bool {
        ++(*static_cast<int*>(ctx));
        return false; // stop after first
    });
    EXPECT_EQ(1, count);
}

TEST_F(HiveTest, ForEachStateEmptyHive)
{
    auto hive = fresh_hive();

    // Empty hive: for_each_state with any offset should visit nothing.
    int count = 0;
    hive->for_each_state(0, &count, [](void* ctx, IObject&, void*) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_EQ(0, count);
}

TEST_F(HiveTest, ForEachHiveTypedAccess)
{
    auto hive = registry_->get_hive(HiveWidget::class_id());
    std::vector<IObject::Ptr> refs;
    for (int i = 0; i < 3; ++i) {
        auto obj = hive->add();
        auto* ps = interface_cast<IPropertyState>(obj);
        auto* s = ps->get_property_state<IObjectHiveWidget>();
        s->x = static_cast<float>(i + 1);
        refs.push_back(std::move(obj));
    }

    float sum = 0.f;
    int count = 0;
    ObjectHive(hive).for_each<IObjectHiveWidget>([&](IObject&, IObjectHiveWidget::State& s) {
        sum += s.x;
        ++count;
        return true;
    });
    // x values: 1, 2, 3
    EXPECT_FLOAT_EQ(6.f, sum);
    EXPECT_EQ(3, count);

    for (auto& r : refs) {
        hive->remove(*r);
    }
}

TEST_F(HiveTest, ForEachHiveOnEmptyHive)
{
    auto hive = fresh_hive();
    int count = 0;
    ObjectHive(hive).for_each<IObjectHiveGadget>([&](IObject&, IObjectHiveGadget::State&) {
        ++count;
        return true;
    });
    EXPECT_EQ(0, count);
}

// --- IRawHive tests ---

struct RawPoint
{
    float x, y, z;
    RawPoint() : x(0.f), y(0.f), z(0.f) {}
    RawPoint(float x, float y, float z) : x(x), y(y), z(z) {}
};

TEST_F(HiveTest, RawHiveAllocateDeallocate)
{
    auto hive = registry_->get_raw_hive<RawPoint>();
    ASSERT_TRUE(hive);
    EXPECT_TRUE(hive->empty());
    EXPECT_EQ(0u, hive->size());

    void* slot = hive->allocate();
    ASSERT_NE(nullptr, slot);
    auto* pt = new (slot) RawPoint(1.f, 2.f, 3.f);
    EXPECT_EQ(1u, hive->size());
    EXPECT_FALSE(hive->empty());

    EXPECT_FLOAT_EQ(1.f, pt->x);
    EXPECT_FLOAT_EQ(2.f, pt->y);
    EXPECT_FLOAT_EQ(3.f, pt->z);

    pt->~RawPoint();
    hive->deallocate(slot);
    EXPECT_EQ(0u, hive->size());
    EXPECT_TRUE(hive->empty());
}

TEST_F(HiveTest, RawHiveContains)
{
    auto hive = registry_->get_raw_hive<RawPoint>();
    void* slot = hive->allocate();
    auto* pt = new (slot) RawPoint();

    EXPECT_TRUE(hive->contains(pt));

    RawPoint stack_pt;
    EXPECT_FALSE(hive->contains(&stack_pt));

    pt->~RawPoint();
    hive->deallocate(slot);
    EXPECT_FALSE(hive->contains(slot));
}

TEST_F(HiveTest, RawHiveForEach)
{
    auto hive = registry_->get_raw_hive<RawPoint>();
    std::vector<RawPoint*> ptrs;
    for (int i = 0; i < 5; ++i) {
        void* slot = hive->allocate();
        ptrs.push_back(new (slot) RawPoint(static_cast<float>(i), 0.f, 0.f));
    }

    float sum = 0.f;
    hive->for_each(&sum, [](void* ctx, void* elem) -> bool {
        auto& pt = *static_cast<RawPoint*>(elem);
        *static_cast<float*>(ctx) += pt.x;
        return true;
    });
    EXPECT_FLOAT_EQ(10.f, sum); // 0+1+2+3+4

    for (auto* p : ptrs) {
        p->~RawPoint();
        hive->deallocate(p);
    }
}

TEST_F(HiveTest, RawHiveForEachStopsOnFalse)
{
    auto hive = registry_->get_raw_hive<RawPoint>();
    std::vector<RawPoint*> ptrs;
    for (int i = 0; i < 3; ++i) {
        void* slot = hive->allocate();
        ptrs.push_back(new (slot) RawPoint());
    }

    int count = 0;
    hive->for_each(&count, [](void* ctx, void*) -> bool {
        ++(*static_cast<int*>(ctx));
        return false; // stop after first
    });
    EXPECT_EQ(1, count);

    for (auto* p : ptrs) {
        p->~RawPoint();
        hive->deallocate(p);
    }
}

TEST_F(HiveTest, RawHiveElementUid)
{
    auto hive = registry_->get_raw_hive<RawPoint>();
    EXPECT_EQ(type_uid<RawPoint>(), hive->get_element_uid());
}

TEST_F(HiveTest, RawHiveSlotReuse)
{
    auto hive = registry_->get_raw_hive<RawPoint>();
    void* slot1 = hive->allocate();
    new (slot1) RawPoint();
    void* slot2 = hive->allocate();
    new (slot2) RawPoint();

    // Deallocate first slot
    static_cast<RawPoint*>(slot1)->~RawPoint();
    hive->deallocate(slot1);
    EXPECT_EQ(1u, hive->size());

    // Allocate again, should reuse the freed slot
    void* slot3 = hive->allocate();
    new (slot3) RawPoint();
    EXPECT_EQ(2u, hive->size());

    // Clean up
    static_cast<RawPoint*>(slot2)->~RawPoint();
    hive->deallocate(slot2);
    static_cast<RawPoint*>(slot3)->~RawPoint();
    hive->deallocate(slot3);
}

// --- RawHive<T> tests ---

TEST_F(HiveTest, ExtRawHiveEmplace)
{
    auto raw = registry_->get_raw_hive<RawPoint>();
    RawHive<RawPoint> hive(raw);

    auto* pt = hive.emplace(1.f, 2.f, 3.f);
    ASSERT_NE(nullptr, pt);
    EXPECT_FLOAT_EQ(1.f, pt->x);
    EXPECT_FLOAT_EQ(2.f, pt->y);
    EXPECT_FLOAT_EQ(3.f, pt->z);
    EXPECT_EQ(1u, hive.size());
    EXPECT_TRUE(hive.contains(pt));

    hive.deallocate(pt);
    EXPECT_EQ(0u, hive.size());
}

TEST_F(HiveTest, ExtRawHiveForEach)
{
    auto raw = registry_->get_raw_hive<RawPoint>();
    RawHive<RawPoint> hive(raw);

    std::vector<RawPoint*> ptrs;
    for (int i = 0; i < 4; ++i) {
        ptrs.push_back(hive.emplace(static_cast<float>(i), 0.f, 0.f));
    }

    float sum = 0.f;
    hive.for_each([&](RawPoint& pt) { sum += pt.x; });
    EXPECT_FLOAT_EQ(6.f, sum); // 0+1+2+3

    // Test bool-returning variant
    int count = 0;
    hive.for_each([&](RawPoint&) -> bool {
        ++count;
        return false;
    });
    EXPECT_EQ(1, count);

    for (auto* p : ptrs) {
        hive.deallocate(p);
    }
}

struct NonTrivial
{
    std::string name;
    int value;
    NonTrivial(std::string n, int v) : name(static_cast<std::string&&>(n)), value(v) {}
};

TEST_F(HiveTest, ExtRawHiveNonTrivialType)
{
    auto raw = registry_->get_raw_hive<NonTrivial>();
    RawHive<NonTrivial> hive(raw);

    auto* obj = hive.emplace("hello", 42);
    EXPECT_EQ("hello", obj->name);
    EXPECT_EQ(42, obj->value);

    hive.deallocate(obj); // destructor runs, string freed
    EXPECT_EQ(0u, hive.size());
}

// --- IHiveStore raw hive integration tests ---

TEST_F(HiveTest, ExtRawHiveClear)
{
    auto raw = registry_->get_raw_hive<NonTrivial>();
    RawHive<NonTrivial> hive(raw);

    hive.emplace("a", 1);
    hive.emplace("b", 2);
    hive.emplace("c", 3);
    EXPECT_EQ(3u, hive.size());

    hive.clear();
    EXPECT_EQ(0u, hive.size());
    EXPECT_TRUE(hive.empty());

    // Hive is still usable after clear.
    auto* p = hive.emplace("d", 4);
    EXPECT_EQ(1u, hive.size());
    EXPECT_EQ("d", p->name);
    hive.deallocate(p);
}

TEST_F(HiveTest, ExtRawHiveDestructorCleansUp)
{
    auto raw = registry_->get_raw_hive<NonTrivial>();
    {
        RawHive<NonTrivial> hive(raw);
        hive.emplace("x", 1);
        hive.emplace("y", 2);
        // hive goes out of scope, destructor should destroy all elements
    }
    // The underlying IRawHive should be empty after RawHive destroyed elements.
    EXPECT_EQ(0u, raw->size());
}

TEST_F(HiveTest, ExtRawHiveFromStore)
{
    RawHive<RawPoint> hive(*registry_);
    EXPECT_TRUE(hive);
    auto* p = hive.emplace(1.f, 2.f, 3.f);
    EXPECT_EQ(1u, hive.size());
    hive.deallocate(p);
}

TEST_F(HiveTest, HiveStoreGetRawHiveReturnsSameInstance)
{
    auto h1 = registry_->get_raw_hive<RawPoint>();
    auto h2 = registry_->get_raw_hive<RawPoint>();
    EXPECT_EQ(h1.get(), h2.get());
}

TEST_F(HiveTest, HiveStoreFindRawHive)
{
    EXPECT_FALSE(registry_->find_raw_hive<RawPoint>());

    registry_->get_raw_hive<RawPoint>();
    auto found = registry_->find_raw_hive<RawPoint>();
    ASSERT_TRUE(found);
}

TEST_F(HiveTest, ExtRawHiveNullSafety)
{
    RawHive<RawPoint> hive(nullptr);
    EXPECT_FALSE(hive);
    EXPECT_TRUE(hive.empty());
    EXPECT_EQ(0u, hive.size());
    EXPECT_EQ(nullptr, hive.emplace(1.f, 2.f, 3.f));
    EXPECT_FALSE(hive.contains(nullptr));

    int count = 0;
    hive.for_each([&](RawPoint&) { ++count; });
    EXPECT_EQ(0, count);
}

TEST_F(HiveTest, ForEachHiveVisitsBothTypes)
{
    registry_->get_hive(HiveWidget::class_id());
    registry_->get_raw_hive<RawPoint>();

    int count = 0;
    registry_->for_each_hive(&count, [](void* ctx, IHive&) -> bool {
        ++(*static_cast<int*>(ctx));
        return true;
    });
    EXPECT_GE(count, 2);
}
