#include <gtest/gtest.h>

#include <api/any.h>
#include <api/callback.h>
#include <api/event.h>
#include <api/function.h>
#include <api/function_context.h>
#include <api/strata.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

using namespace strata;

// --- FnArgs ---

TEST(FnArgs, DefaultIsEmpty)
{
    FnArgs args;
    EXPECT_EQ(args.count, 0u);
    EXPECT_TRUE(args.empty());
    EXPECT_EQ(args.begin(), args.end());
}

TEST(FnArgs, IndexOutOfBoundsReturnsNullptr)
{
    FnArgs args;
    EXPECT_EQ(args[0], nullptr);
    EXPECT_EQ(args[100], nullptr);
}

TEST(FnArgs, IndexInBoundsReturnsPointer)
{
    Any<int> a(42);
    const IAny* ptrs[] = {a};
    FnArgs args{ptrs, 1};
    EXPECT_NE(args[0], nullptr);
    EXPECT_EQ(args[1], nullptr);
}

TEST(FnArgs, Iteration)
{
    Any<int> a(1);
    Any<float> b(2.f);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};

    int count = 0;
    for (auto* arg : args) {
        EXPECT_NE(arg, nullptr);
        count++;
    }
    EXPECT_EQ(count, 2);
}

// --- Function ---

TEST(Callback,LambdaCallbackInvoked)
{
    bool called = false;

    Callback fn([&](FnArgs) -> ReturnValue {
        called = true;
        return ReturnValue::SUCCESS;
    });

    auto result = fn.invoke();
    EXPECT_TRUE(called);
    EXPECT_EQ(result, nullptr);
}

TEST(Callback,InvokeWithArgs)
{
    float received = 0.f;

    Callback fn([&](FnArgs args) -> ReturnValue {
        if (auto v = Any<const float>(args[0])) {
            received = v.get_value();
        }
        return ReturnValue::SUCCESS;
    });

    Any<float> arg(3.14f);
    const IAny* ptrs[] = {arg};
    fn.invoke(FnArgs{ptrs, 1});
    EXPECT_FLOAT_EQ(received, 3.14f);
}

// --- invoke_function variadic with values ---

TEST(InvokeFunction, VariadicWithValues)
{
    float varA = 0.f;
    int varB = 0;

    Callback fn([&](FnArgs args) -> ReturnValue {
        if (auto a = Any<const float>(args[0])) varA = a.get_value();
        if (auto b = Any<const int>(args[1])) varB = b.get_value();
        return ReturnValue::SUCCESS;
    });

    invoke_function(IFunction::Ptr(fn), 10.f, 20);
    EXPECT_FLOAT_EQ(varA, 10.f);
    EXPECT_EQ(varB, 20);
}

// --- invoke_function variadic with IAny* pointers ---

TEST(InvokeFunction, VariadicWithAnyPointers)
{
    float ptrA = 0.f;
    int ptrB = 0;

    Callback fn([&](FnArgs args) -> ReturnValue {
        if (auto a = Any<const float>(args[0])) ptrA = a.get_value();
        if (auto b = Any<const int>(args[1])) ptrB = b.get_value();
        return ReturnValue::SUCCESS;
    });

    Any<float> arg0(5.f);
    Any<int> arg1(7);
    invoke_function(IFunction::Ptr(fn), arg0, arg1);
    EXPECT_FLOAT_EQ(ptrA, 5.f);
    EXPECT_EQ(ptrB, 7);
}

// --- FunctionContext ---

TEST(FunctionContext, DefaultIsEmpty)
{
    FunctionContext ctx;
    EXPECT_FALSE(ctx);
    EXPECT_EQ(ctx.size(), 0u);
}

TEST(FunctionContext, MatchingCountAccepts)
{
    Any<int> a(1);
    Any<int> b(2);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};

    FunctionContext ctx(args, 2);
    EXPECT_TRUE(ctx);
    EXPECT_EQ(ctx.size(), 2u);
}

TEST(FunctionContext, MismatchedCountRejects)
{
    Any<int> a(1);
    const IAny* ptrs[] = {a};
    FnArgs args{ptrs, 1};

    FunctionContext ctx(args, 2);
    EXPECT_FALSE(ctx);
    EXPECT_EQ(ctx.size(), 0u);
}

TEST(FunctionContext, ArgTypedAccess)
{
    Any<float> a(3.14f);
    Any<int> b(42);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};

    FunctionContext ctx(args, 2);
    ASSERT_TRUE(ctx);

    auto fa = ctx.arg<float>(0);
    ASSERT_TRUE(fa);
    EXPECT_FLOAT_EQ(fa.get_value(), 3.14f);

    auto ib = ctx.arg<int>(1);
    ASSERT_TRUE(ib);
    EXPECT_EQ(ib.get_value(), 42);
}

TEST(FunctionContext, ArgOutOfRangeReturnsNull)
{
    Any<int> a(1);
    const IAny* ptrs[] = {a};
    FnArgs args{ptrs, 1};

    FunctionContext ctx(args, 1);
    EXPECT_EQ(ctx.arg(5), nullptr);
}

// --- Typed lambda Function ---

TEST(Callback,TypedLambdaTwoParams)
{
    float received_a = 0.f;
    int received_b = 0;

    Callback fn([&](const float& a, const int& b) -> ReturnValue {
        received_a = a;
        received_b = b;
        return ReturnValue::SUCCESS;
    });

    Any<float> a(3.14f);
    Any<int> b(42);
    const IAny* ptrs[] = {a, b};
    auto result = fn.invoke(FnArgs{ptrs, 2});
    EXPECT_EQ(result, nullptr);
    EXPECT_FLOAT_EQ(received_a, 3.14f);
    EXPECT_EQ(received_b, 42);
}

TEST(Callback,TypedLambdaVoidReturn)
{
    float received = 0.f;

    Callback fn([&](float a) {
        received = a;
    });

    Any<float> a(2.5f);
    const IAny* ptrs[] = {a};
    auto result = fn.invoke(FnArgs{ptrs, 1});
    EXPECT_EQ(result, nullptr);
    EXPECT_FLOAT_EQ(received, 2.5f);
}

TEST(Callback,TypedLambdaInsufficientArgs)
{
    Callback fn([](const float&, const int&) -> ReturnValue {
        return ReturnValue::SUCCESS;
    });

    Any<float> a(1.f);
    const IAny* ptrs[] = {a};
    auto result = fn.invoke(FnArgs{ptrs, 1});
    EXPECT_EQ(result, nullptr);
}

TEST(Callback,TypedLambdaZeroArity)
{
    bool called = false;

    Callback fn([&]() {
        called = true;
    });

    auto result = fn.invoke();
    EXPECT_EQ(result, nullptr);
    EXPECT_TRUE(called);
}

TEST(Callback,TypedLambdaExtraArgsIgnored)
{
    float received = 0.f;

    Callback fn([&](float a) {
        received = a;
    });

    Any<float> a(1.5f);
    Any<int> b(99);
    const IAny* ptrs[] = {a, b};
    auto result = fn.invoke(FnArgs{ptrs, 2});
    EXPECT_EQ(result, nullptr);
    EXPECT_FLOAT_EQ(received, 1.5f);
}

TEST(Callback,TypedLambdaTypeMismatchGetsDefault)
{
    int received = -1;

    Callback fn([&](int a) {
        received = a;
    });

    Any<float> a(3.14f); // float, not int
    const IAny* ptrs[] = {a};
    auto result = fn.invoke(FnArgs{ptrs, 1});
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(received, 0); // default int
}

TEST(Callback,TypedLambdaWithInvokeFunction)
{
    float received_a = 0.f;
    int received_b = 0;

    Callback fn([&](const float& a, const int& b) -> ReturnValue {
        received_a = a;
        received_b = b;
        return ReturnValue::SUCCESS;
    });

    invoke_function(IFunction::Ptr(fn), 10.f, 20);
    EXPECT_FLOAT_EQ(received_a, 10.f);
    EXPECT_EQ(received_b, 20);
}

TEST(Callback,TypedLambdaMutable)
{
    int count = 0;

    Callback fn([count]() mutable {
        count++;
    });

    auto result = fn.invoke();
    EXPECT_EQ(result, nullptr);
}

TEST(Callback,FnArgsLambdaStillWorks)
{
    bool called = false;

    Callback fn([&](FnArgs) -> ReturnValue {
        called = true;
        return ReturnValue::SUCCESS;
    });

    auto result = fn.invoke();
    EXPECT_TRUE(called);
    EXPECT_EQ(result, nullptr);
}

// --- Event handler add/remove ---

TEST(Event, HandlerAddRemove)
{
    int callCount = 0;

    auto event = instance().create<IEvent>(ClassId::Event);
    ASSERT_TRUE(event);

    Callback handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::SUCCESS;
    });

    event->add_handler(handler);
    EXPECT_TRUE(event->has_handlers());

    event->invoke({});
    EXPECT_EQ(callCount, 1);

    event->remove_handler(handler);
    EXPECT_FALSE(event->has_handlers());

    event->invoke({});
    EXPECT_EQ(callCount, 1); // no longer called
}

TEST(Event, AddHandlerWithLambdaHelper)
{
    int callCount = 0;

    Event event(instance().create<IEvent>(ClassId::Event));
    ASSERT_TRUE(event);

    // Lambda automatically wrapped in Callback
    event.add_handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::SUCCESS;
    });

    EXPECT_TRUE(event.has_handlers());
    event.invoke({});
    EXPECT_EQ(callCount, 1);
}

TEST(Event, AddHandlerWithTypedLambdaHelper)
{
    float received_a = 0.f;
    int received_b = 0;

    Event event(instance().create<IEvent>(ClassId::Event));
    ASSERT_TRUE(event);

    // Typed lambda automatically wrapped in Callback
    event.add_handler([&](const float& a, const int& b) -> ReturnValue {
        received_a = a;
        received_b = b;
        return ReturnValue::SUCCESS;
    });

    Any<float> a(3.14f);
    Any<int> b(42);
    const IAny* ptrs[] = {a, b};
    event.invoke(FnArgs{ptrs, 2});

    EXPECT_FLOAT_EQ(received_a, 3.14f);
    EXPECT_EQ(received_b, 42);
}

TEST(Event, AddHandlerWithZeroArityLambdaHelper)
{
    bool called = false;

    Event event(instance().create<IEvent>(ClassId::Event));
    ASSERT_TRUE(event);

    // Zero-arity lambda automatically wrapped in Callback
    event.add_handler([&]() {
        called = true;
    });

    event.invoke({});
    EXPECT_TRUE(called);
}

// --- Deferred invocation ---

TEST(Callback,DeferredInvocationQueuesAndExecutesOnUpdate)
{
    int callCount = 0;

    Callback fn([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::SUCCESS;
    });

    fn.invoke({}, Deferred);
    EXPECT_EQ(callCount, 0); // Not called yet

    instance().update();
    EXPECT_EQ(callCount, 1); // Now called
}
