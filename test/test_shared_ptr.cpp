#include <gtest/gtest.h>
#include <api/velk.h>
#include <ext/any.h>
#include <ext/core_object.h>
#include <interface/intf_object.h>
#include <interface/types.h>
#include <memory.h>

using namespace velk;

// shared_ptr<IInterface> tests (intrusive)

TEST(SharedPtr, DefaultIsNull)
{
    shared_ptr<IInterface> sp;
    EXPECT_FALSE(sp);
    EXPECT_EQ(sp.get(), nullptr);
}

TEST(SharedPtr, NullptrConstruction)
{
    shared_ptr<IInterface> sp(nullptr);
    EXPECT_FALSE(sp);
}

TEST(SharedPtr, CreateViaFactory)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    EXPECT_TRUE(obj);
    EXPECT_NE(obj.get(), nullptr);
}

TEST(SharedPtr, CopySharesOwnership)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    EXPECT_TRUE(obj);
    auto copy = obj;
    EXPECT_TRUE(copy);
    EXPECT_EQ(obj.get(), copy.get());
}

TEST(SharedPtr, MoveTransfersOwnership)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    auto* raw = obj.get();
    auto moved = std::move(obj);
    EXPECT_FALSE(obj);
    EXPECT_TRUE(moved);
    EXPECT_EQ(moved.get(), raw);
}

TEST(SharedPtr, ResetReleasesOwnership)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    EXPECT_TRUE(obj);
    obj.reset();
    EXPECT_FALSE(obj);
}

TEST(SharedPtr, InterfacePointerCast)
{
    auto obj = instance().create(ClassId::Property);
    EXPECT_TRUE(obj);
    auto prop = interface_pointer_cast<IProperty>(obj);
    EXPECT_TRUE(prop);
}

TEST(SharedPtr, Equality)
{
    auto a = instance().create<IObject>(ClassId::Property);
    auto b = a;
    auto c = instance().create<IObject>(ClassId::Property);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, nullptr);

    shared_ptr<IObject> null;
    EXPECT_EQ(null, nullptr);
}

// shared_ptr<non-IInterface> tests (external control block)

struct PlainData
{
    int value = 0;
    static int destroyed_count;
    ~PlainData() { ++destroyed_count; }
};
int PlainData::destroyed_count = 0;

TEST(SharedPtrPlain, ConstructAndAccess)
{
    PlainData::destroyed_count = 0;
    {
        auto sp = make_shared<PlainData>();
        sp->value = 42;
        EXPECT_EQ(sp->value, 42);
        EXPECT_TRUE(sp);
    }
    EXPECT_EQ(PlainData::destroyed_count, 1);
}

TEST(SharedPtrPlain, CopySharesOwnership)
{
    PlainData::destroyed_count = 0;
    {
        auto sp = make_shared<PlainData>();
        sp->value = 10;
        {
            auto copy = sp;
            EXPECT_EQ(copy->value, 10);
            EXPECT_EQ(sp.get(), copy.get());
        }
        // copy destroyed, but object should still be alive
        EXPECT_EQ(PlainData::destroyed_count, 0);
        EXPECT_EQ(sp->value, 10);
    }
    EXPECT_EQ(PlainData::destroyed_count, 1);
}

TEST(SharedPtrPlain, MoveTransfers)
{
    PlainData::destroyed_count = 0;
    {
        auto sp = make_shared<PlainData>();
        sp->value = 99;
        auto moved = std::move(sp);
        EXPECT_FALSE(sp);
        EXPECT_TRUE(moved);
        EXPECT_EQ(moved->value, 99);
        EXPECT_EQ(PlainData::destroyed_count, 0);
    }
    EXPECT_EQ(PlainData::destroyed_count, 1);
}

// weak_ptr tests

TEST(WeakPtr, DefaultIsExpired)
{
    weak_ptr<IObject> wp;
    EXPECT_TRUE(wp.expired());
    EXPECT_FALSE(wp.lock());
}

TEST(WeakPtr, LockReturnsSharedWhileAlive)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    EXPECT_TRUE(obj);
    EXPECT_NE(obj.block(), nullptr);

    weak_ptr<IObject> wp(obj);
    EXPECT_FALSE(wp.expired());

    auto locked = wp.lock();
    EXPECT_TRUE(locked);
    EXPECT_EQ(locked.get(), obj.get());
}

TEST(WeakPtr, ExpiredAfterSharedRelease)
{
    weak_ptr<IObject> wp;
    {
        auto obj = instance().create<IObject>(ClassId::Property);
        wp = obj;
        EXPECT_FALSE(wp.expired());
    }
    EXPECT_TRUE(wp.expired());
    EXPECT_FALSE(wp.lock());
}

TEST(WeakPtr, CopySemantic)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    weak_ptr<IObject> wp1(obj);
    weak_ptr<IObject> wp2(wp1);
    EXPECT_FALSE(wp2.expired());
    EXPECT_EQ(wp2.lock().get(), obj.get());
}

TEST(WeakPtr, MoveSemantic)
{
    auto obj = instance().create<IObject>(ClassId::Property);
    weak_ptr<IObject> wp1(obj);
    weak_ptr<IObject> wp2(std::move(wp1));
    EXPECT_TRUE(wp1.expired());
    EXPECT_FALSE(wp2.expired());
    EXPECT_EQ(wp2.lock().get(), obj.get());
}

// weak_ptr with non-IInterface

TEST(WeakPtrPlain, LockWhileAlive)
{
    PlainData::destroyed_count = 0;
    auto sp = make_shared<PlainData>();
    sp->value = 55;
    weak_ptr<PlainData> wp(sp);
    EXPECT_FALSE(wp.expired());

    auto locked = wp.lock();
    EXPECT_TRUE(locked);
    EXPECT_EQ(locked->value, 55);
}

TEST(WeakPtrPlain, ExpiredAfterRelease)
{
    PlainData::destroyed_count = 0;
    weak_ptr<PlainData> wp;
    {
        auto sp = make_shared<PlainData>();
        wp = sp;
        EXPECT_FALSE(wp.expired());
    }
    EXPECT_TRUE(wp.expired());
    EXPECT_FALSE(wp.lock());
    EXPECT_EQ(PlainData::destroyed_count, 1);
}

// make_shared tests

TEST(MakeShared, InterfaceType)
{
    // AnyValue is IInterface-derived
    auto any = instance().create_any(type_uid<float>());
    EXPECT_TRUE(any);
}

TEST(MakeShared, NonInterfaceType)
{
    auto sp = make_shared<PlainData>();
    EXPECT_TRUE(sp);
    sp->value = 123;
    EXPECT_EQ(sp->value, 123);
}

// shared_ptr / self-pointer interop

TEST(SharedPtrSelf, GetSelfReturnsValidPointer)
{
    auto obj = instance().create<IObject>(ClassId::Function);
    EXPECT_TRUE(obj);
    if (auto* shared = obj->get_interface<ISharedFromObject>()) {
        auto self = shared->get_self();
        EXPECT_TRUE(self);
        EXPECT_EQ(self.get(), obj.get());
    }
}
