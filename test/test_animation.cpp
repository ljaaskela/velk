#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/plugins/animator/animator.h>

#include <gtest/gtest.h>

using namespace velk;

namespace {
constexpr Duration sec(float s) { return Duration::from_seconds(s); }
UpdateInfo dt(float s) { return {{}, {}, Duration::from_seconds(s)}; }
void flush() { instance().update({}); }
} // namespace

// ============================================================================
// Easing tests
// ============================================================================

TEST(Easing, LinearEndpoints)
{
    EXPECT_FLOAT_EQ(0.f, easing::linear(0.f));
    EXPECT_FLOAT_EQ(0.5f, easing::linear(0.5f));
    EXPECT_FLOAT_EQ(1.f, easing::linear(1.f));
}

TEST(Easing, QuadEndpoints)
{
    EXPECT_FLOAT_EQ(0.f, easing::in_quad(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::in_quad(1.f));
    EXPECT_FLOAT_EQ(0.f, easing::out_quad(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::out_quad(1.f));
    EXPECT_FLOAT_EQ(0.f, easing::in_out_quad(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::in_out_quad(1.f));
}

TEST(Easing, CubicEndpoints)
{
    EXPECT_FLOAT_EQ(0.f, easing::in_cubic(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::in_cubic(1.f));
    EXPECT_FLOAT_EQ(0.f, easing::out_cubic(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::out_cubic(1.f));
    EXPECT_FLOAT_EQ(0.f, easing::in_out_cubic(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::in_out_cubic(1.f));
}

TEST(Easing, SineEndpoints)
{
    EXPECT_NEAR(0.f, easing::in_sine(0.f), 1e-6f);
    EXPECT_NEAR(1.f, easing::in_sine(1.f), 1e-6f);
    EXPECT_NEAR(0.f, easing::out_sine(0.f), 1e-6f);
    EXPECT_NEAR(1.f, easing::out_sine(1.f), 1e-6f);
    EXPECT_NEAR(0.f, easing::in_out_sine(0.f), 1e-6f);
    EXPECT_NEAR(1.f, easing::in_out_sine(1.f), 1e-6f);
}

TEST(Easing, ExpoEndpoints)
{
    EXPECT_FLOAT_EQ(0.f, easing::in_expo(0.f));
    EXPECT_NEAR(1.f, easing::in_expo(1.f), 1e-3f);
    EXPECT_FLOAT_EQ(1.f, easing::out_expo(1.f));
    EXPECT_NEAR(0.f, easing::out_expo(0.f), 1e-6f);
    EXPECT_FLOAT_EQ(0.f, easing::in_out_expo(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::in_out_expo(1.f));
}

TEST(Easing, ElasticEndpoints)
{
    EXPECT_FLOAT_EQ(0.f, easing::in_elastic(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::in_elastic(1.f));
    EXPECT_FLOAT_EQ(0.f, easing::out_elastic(0.f));
    EXPECT_FLOAT_EQ(1.f, easing::out_elastic(1.f));
}

TEST(Easing, BounceEndpoints)
{
    EXPECT_NEAR(0.f, easing::in_bounce(0.f), 1e-6f);
    EXPECT_NEAR(1.f, easing::in_bounce(1.f), 1e-6f);
    EXPECT_NEAR(0.f, easing::out_bounce(0.f), 1e-6f);
    EXPECT_NEAR(1.f, easing::out_bounce(1.f), 1e-6f);
}

TEST(Easing, QuadMidpoint)
{
    EXPECT_FLOAT_EQ(0.25f, easing::in_quad(0.5f));
    EXPECT_FLOAT_EQ(0.75f, easing::out_quad(0.5f));
    EXPECT_FLOAT_EQ(0.5f, easing::in_out_quad(0.5f));
}

// ============================================================================
// Interpolator trait tests
// ============================================================================

TEST(InterpolatorTrait, FloatDefault)
{
    EXPECT_FLOAT_EQ(0.f, interpolator_trait<float>::interpolate(0.f, 100.f, 0.f));
    EXPECT_FLOAT_EQ(50.f, interpolator_trait<float>::interpolate(0.f, 100.f, 0.5f));
    EXPECT_FLOAT_EQ(100.f, interpolator_trait<float>::interpolate(0.f, 100.f, 1.f));
}

TEST(InterpolatorTrait, IntDefault)
{
    EXPECT_EQ(0, interpolator_trait<int>::interpolate(0, 100, 0.f));
    EXPECT_EQ(50, interpolator_trait<int>::interpolate(0, 100, 0.5f));
    EXPECT_EQ(100, interpolator_trait<int>::interpolate(0, 100, 1.f));
}

TEST(InterpolatorTrait, DoubleDefault)
{
    EXPECT_DOUBLE_EQ(25.0, interpolator_trait<double>::interpolate(0.0, 100.0, 0.25f));
}

struct Vec2
{
    float x = 0.f;
    float y = 0.f;
};

namespace velk {
template <>
struct interpolator_trait<Vec2>
{
    static Vec2 interpolate(const Vec2& a, const Vec2& b, float t)
    {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }
};
} // namespace velk

TEST(InterpolatorTrait, CustomSpecialization)
{
    Vec2 a{0.f, 0.f};
    Vec2 b{10.f, 20.f};
    auto mid = interpolator_trait<Vec2>::interpolate(a, b, 0.5f);
    EXPECT_FLOAT_EQ(5.f, mid.x);
    EXPECT_FLOAT_EQ(10.f, mid.y);
}

// ============================================================================
// Plugin / Animator tests (require velk_animator.dll)
// ============================================================================

#ifdef TEST_ANIMATOR_DLL_PATH

class AnimatorPluginTest : public ::testing::Test
{
protected:
    void SetUp() override { instance().plugin_registry().load_plugin_from_path(TEST_ANIMATOR_DLL_PATH); }
};

TEST_F(AnimatorPluginTest, LoadAnimatorPlugin)
{
    auto* plugin = instance().plugin_registry().find_plugin(PluginId::AnimatorPlugin);
    EXPECT_NE(nullptr, plugin);

    // Double load returns NothingToDo
    EXPECT_EQ(ReturnValue::NothingToDo,
              instance().plugin_registry().load_plugin_from_path(TEST_ANIMATOR_DLL_PATH));
}

TEST_F(AnimatorPluginTest, AnimationTypeRegistered)
{
    auto obj = instance().create<IObject>(ClassId::Animation);
    EXPECT_TRUE(obj);

    auto* anim = interface_cast<IAnimation>(obj);
    EXPECT_NE(nullptr, anim);
}

TEST_F(AnimatorPluginTest, AnimatorTypeRegistered)
{
    auto obj = instance().create<IObject>(ClassId::Animator);
    EXPECT_TRUE(obj);

    auto* animator = interface_cast<IAnimator>(obj);
    EXPECT_NE(nullptr, animator);
}

TEST_F(AnimatorPluginTest, AnimationProperties)
{
    auto obj = instance().create<IObject>(ClassId::Animation);
    auto* anim = interface_cast<IAnimation>(obj);
    ASSERT_NE(nullptr, anim);

    // Default state via VELK_INTERFACE accessors
    EXPECT_EQ(0, anim->duration().get_value().us);
    EXPECT_EQ(0, anim->elapsed().get_value().us);
    EXPECT_FALSE(anim->finished().get_value());

    // Write duration via accessor
    anim->duration().set_value(sec(2.f));
    EXPECT_EQ(sec(2.f).us, anim->duration().get_value().us);
}

// ============================================================================
// Tween tests
// ============================================================================

class AnimatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        instance().plugin_registry().load_plugin_from_path(TEST_ANIMATOR_DLL_PATH);
        auto obj = instance().create<IObject>(ClassId::Animator);
        animator_ = interface_cast<IAnimator>(obj);
        animatorObj_ = obj;
    }

    Property<float> prop_ = create_property<float>(0.f);
    IAnimator* animator_ = nullptr;
    IObject::Ptr animatorObj_;
};

TEST_F(AnimatorTest, TweenAndTick)
{
    auto h = tween(*animator_, prop_, 0.f, 100.f, sec(1.f));
    EXPECT_EQ(1u, animator_->active_count());
    EXPECT_FALSE(h.is_finished());

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_NEAR(50.f, prop_.get_value(), 0.1f);

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_TRUE(h.is_finished());
    EXPECT_FLOAT_EQ(100.f, prop_.get_value());
}

TEST_F(AnimatorTest, RemovesFinishedAnimations)
{
    tween(*animator_, prop_, 0.f, 100.f, sec(1.f));
    EXPECT_EQ(1u, animator_->active_count());

    animator_->tick(dt(2.f));
    EXPECT_EQ(0u, animator_->active_count());
}

TEST_F(AnimatorTest, TweenTo)
{
    prop_.set_value(50.f);
    auto h = tween_to(*animator_, prop_, 100.f, sec(1.f));

    animator_->tick(dt(1.f));
    flush();
    EXPECT_FLOAT_EQ(100.f, prop_.get_value());
    EXPECT_TRUE(h.is_finished());
}

TEST_F(AnimatorTest, TweenWithEasing)
{
    auto h = tween(*animator_, prop_, 0.f, 100.f, sec(1.f), easing::in_quad);

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_NEAR(25.f, prop_.get_value(), 0.1f);
}

TEST_F(AnimatorTest, CancelAll)
{
    tween(*animator_, prop_, 0.f, 100.f, sec(1.f));
    tween(*animator_, prop_, 0.f, 200.f, sec(2.f));
    EXPECT_EQ(2u, animator_->active_count());

    animator_->cancel_all();
    EXPECT_EQ(0u, animator_->active_count());
}

TEST_F(AnimatorTest, CancelViaAnimation)
{
    auto h = tween(*animator_, prop_, 0.f, 100.f, sec(1.f));
    h.cancel();
    EXPECT_TRUE(h.is_finished());

    animator_->tick(dt(0.1f));
    EXPECT_EQ(0u, animator_->active_count());
}

TEST_F(AnimatorTest, MultipleAnimations)
{
    auto prop2 = create_property<float>(0.f);

    tween(*animator_, prop_, 0.f, 100.f, sec(1.f));
    tween(*animator_, prop2, 0.f, 50.f, sec(0.5f));
    EXPECT_EQ(2u, animator_->active_count());

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_EQ(1u, animator_->active_count());
    EXPECT_FLOAT_EQ(50.f, prop2.get_value());
    EXPECT_NEAR(50.f, prop_.get_value(), 0.1f);

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_EQ(0u, animator_->active_count());
    EXPECT_FLOAT_EQ(100.f, prop_.get_value());
}

// ============================================================================
// AnimationTrack tests
// ============================================================================

class TrackTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        instance().plugin_registry().load_plugin_from_path(TEST_ANIMATOR_DLL_PATH);
        auto obj = instance().create<IObject>(ClassId::Animator);
        animator_ = interface_cast<IAnimator>(obj);
        animatorObj_ = obj;
    }

    Property<float> prop_ = create_property<float>(0.f);
    IAnimator* animator_ = nullptr;
    IObject::Ptr animatorObj_;
};

TEST_F(TrackTest, WalksKeyframes)
{
    auto kfs = vector<Keyframe<float>>{
        {sec(0.f), 0.f},
        {sec(1.f), 100.f, easing::linear},
        {sec(2.f), 50.f, easing::linear},
    };
    auto h = track(*animator_, prop_, kfs);

    flush();
    EXPECT_FLOAT_EQ(0.f, prop_.get_value());

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_NEAR(50.f, prop_.get_value(), 0.1f);

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_NEAR(100.f, prop_.get_value(), 0.1f);

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_NEAR(75.f, prop_.get_value(), 0.1f);

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_FLOAT_EQ(50.f, prop_.get_value());
    EXPECT_TRUE(h.is_finished());
}

TEST_F(TrackTest, PerSegmentEasing)
{
    auto h = track(*animator_,
                   prop_,
                   vector<Keyframe<float>>{
                       {sec(0.f), 0.f},
                       {sec(1.f), 100.f, easing::in_quad},
                   });

    animator_->tick(dt(0.5f));
    flush();
    EXPECT_NEAR(25.f, prop_.get_value(), 0.1f);
}

TEST_F(TrackTest, FinishesAtLastKeyframe)
{
    auto h = track(*animator_,
                   prop_,
                   vector<Keyframe<float>>{
                       {sec(0.f), 0.f},
                       {sec(1.f), 100.f},
                   });

    animator_->tick(dt(5.f));
    flush();
    EXPECT_FLOAT_EQ(100.f, prop_.get_value());
    EXPECT_TRUE(h.is_finished());
}

TEST_F(TrackTest, SingleKeyframeFinishesImmediately)
{
    auto h = track(*animator_,
                   prop_,
                   vector<Keyframe<float>>{
                       {sec(0.f), 42.f},
                   });

    animator_->tick(dt(0.1f));
    flush();
    EXPECT_FLOAT_EQ(42.f, prop_.get_value());
    EXPECT_TRUE(h.is_finished());
}

// ============================================================================
// Animation wrapper tests
// ============================================================================

TEST(AnimationWrapper, DefaultIsFinished)
{
    Animation h;
    EXPECT_TRUE(h.is_finished());
}

TEST(AnimationWrapper, CancelOnDefault)
{
    Animation h;
    h.cancel();
    EXPECT_TRUE(h.is_finished());
}

// ============================================================================
// default_animator integration test
// ============================================================================

TEST(DefaultAnimator, TicksViaUpdate)
{
    instance().plugin_registry().load_plugin_from_path(TEST_ANIMATOR_DLL_PATH);

    auto prop = create_property<float>(0.f);
    auto& da = default_animator();

    tween(da, prop, 0.f, 100.f, sec(1.f));

    instance().update({1'000'000});
    instance().update({1'500'000});

    EXPECT_GT(prop.get_value(), 0.f);

    da.cancel_all();
}

#endif // TEST_ANIMATOR_DLL_PATH
