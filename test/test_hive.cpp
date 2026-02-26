#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/api/hive/iterate.h>
#include <velk/interface/hive/intf_hive_store.h>
#include <velk/interface/intf_metadata.h>

#include <gtest/gtest.h>
#include <vector>

using namespace velk;

// --- Test interface and object for hive storage ---

class IHiveWidget : public Interface<IHiveWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, x, 0.f),
        (PROP, float, y, 0.f)
    )
};

class HiveWidget : public ext::Object<HiveWidget, IHiveWidget>
{};

// A second type so that tests needing a fresh hive can use it.
class IHiveGadget : public Interface<IHiveGadget>
{
public:
    VELK_INTERFACE(
        (PROP, int, id, 0)
    )
};

class HiveGadget : public ext::Object<HiveGadget, IHiveGadget>
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
        velk_.plugin_registry().load_plugin(ClassId::HivePlugin);
        registry_ = velk_.create<IHiveStore>(ClassId::HiveStore);
        ASSERT_TRUE(registry_);
    }

    void TearDown() override
    {
        registry_.reset();
        velk_.plugin_registry().unload_plugin(ClassId::HivePlugin);
        unregister_type<HiveGadget>(velk_);
        unregister_type<HiveWidget>(velk_);
    }

    /** @brief Returns a fresh hive for HiveGadget (never used by other tests). */
    IHive::Ptr fresh_hive() { return registry_->get_hive(HiveGadget::class_id()); }

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
    EXPECT_EQ(HiveWidget::class_id(), hive->get_element_class_uid());
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

// --- IHive tests (use fresh_hive() for isolation) ---

TEST_F(HiveTest, NewHiveIsEmpty)
{
    auto hive = fresh_hive();
    EXPECT_TRUE(hive->empty());
    EXPECT_EQ(0u, hive->size());
}

TEST_F(HiveTest, ElementClassUid)
{
    auto hive = fresh_hive();
    EXPECT_EQ(HiveGadget::class_id(), hive->get_element_class_uid());
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

    auto* gadget = interface_cast<IHiveGadget>(obj);
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

    // IHive inherits IObject, so we can query class info
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

    auto* widget = interface_cast<IHiveWidget>(obj);
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
        auto* s = ps->get_property_state<IHiveWidget>();
        s->x = static_cast<float>(i * 10);
        s->y = static_cast<float>(i * 20);
        refs.push_back(std::move(obj));
    }

    ptrdiff_t offset = detail::compute_state_offset(*hive, IHiveWidget::UID);
    ASSERT_GE(offset, 0);

    float sum_x = 0.f;
    hive->for_each_state(offset, &sum_x, [](void* ctx, IObject&, void* state) -> bool {
        auto& s = *static_cast<IHiveWidget::State*>(state);
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

    ptrdiff_t offset = detail::compute_state_offset(*hive, IHiveGadget::UID);
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
        auto* s = ps->get_property_state<IHiveWidget>();
        s->x = static_cast<float>(i + 1);
        refs.push_back(std::move(obj));
    }

    float sum = 0.f;
    int count = 0;
    for_each_hive<IHiveWidget>(*hive, [&](IObject&, IHiveWidget::State& s) {
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
    for_each_hive<IHiveGadget>(*hive, [&](IObject&, IHiveGadget::State&) {
        ++count;
        return true;
    });
    EXPECT_EQ(0, count);
}
