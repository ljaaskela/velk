#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/ext/plugin.h>
#include <velk/interface/types.h>

#include <gtest/gtest.h>

using namespace velk;

// A minimal interface for plugin-registered types
class IPluginWidget : public Interface<IPluginWidget>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 0)
    )
};

class PluginWidget : public ext::Object<PluginWidget, IPluginWidget>
{};

// A test plugin that registers PluginWidget
class TestPlugin : public ext::Plugin<TestPlugin>
{
public:
    VELK_PLUGIN_UID("a0000000-0000-0000-0000-000000000001");
    VELK_PLUGIN_NAME("TestPlugin");
    VELK_PLUGIN_VERSION(2, 1, 0);

    ReturnValue initialize(IVelk& velk) override
    {
        initCount++;
        return register_type<PluginWidget>(velk);
    }

    ReturnValue shutdown(IVelk& velk) override
    {
        shutdownCount++;
        return ReturnValue::Success;
    }

    int initCount = 0;
    int shutdownCount = 0;
};

// A plugin whose initialize() fails
class FailingPlugin : public ext::Plugin<FailingPlugin>
{
public:
    VELK_PLUGIN_UID("a0000000-0000-0000-0000-000000000002");

    ReturnValue initialize(IVelk&) override { return ReturnValue::Fail; }
    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

// A plugin that depends on TestPlugin
class DependentPlugin : public ext::Plugin<DependentPlugin>
{
public:
    VELK_PLUGIN_UID("a0000000-0000-0000-0000-000000000003");
    VELK_PLUGIN_DEPS(TestPlugin::class_uid);

    ReturnValue initialize(IVelk&) override { return ReturnValue::Success; }
    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

// A plugin that depends on TestPlugin >= 2.1.0 (should succeed)
class VersionOkPlugin : public ext::Plugin<VersionOkPlugin>
{
public:
    VELK_PLUGIN_UID("a0000000-0000-0000-0000-000000000004");
    VELK_PLUGIN_DEPS({TestPlugin::class_uid, make_version(2, 1, 0)});

    ReturnValue initialize(IVelk&) override { return ReturnValue::Success; }
    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

// A plugin that depends on TestPlugin >= 3.0.0 (should fail, TestPlugin is 2.1.0)
class VersionTooNewPlugin : public ext::Plugin<VersionTooNewPlugin>
{
public:
    VELK_PLUGIN_UID("a0000000-0000-0000-0000-000000000005");
    VELK_PLUGIN_DEPS({TestPlugin::class_uid, make_version(3, 0, 0)});

    ReturnValue initialize(IVelk&) override { return ReturnValue::Success; }
    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

// UIDs for DLL test plugins (must match test_plugin_dll.cpp)
static constexpr Uid DllTestPluginUid{"b0000000-0000-0000-0000-000000000001"};
static constexpr Uid DllSubPluginUid{"b0000000-0000-0000-0000-000000000002"};

class PluginTest : public ::testing::Test
{
protected:
    IVelk& velk_ = instance();
    IPlugin::Ptr plugin_ = ext::make_object<TestPlugin, IPlugin>();
    TestPlugin* tp_ = static_cast<TestPlugin*>(plugin_.get());

    void TearDown() override
    {
        auto& reg = velk_.plugin_registry();
        // Unload dependents before their dependencies.
        if (reg.find_plugin(DllTestPluginUid)) {
            reg.unload_plugin(DllTestPluginUid);
        }
        if (reg.find_plugin(DllSubPluginUid)) {
            reg.unload_plugin(DllSubPluginUid);
        }
        if (reg.find_plugin<VersionOkPlugin>()) {
            reg.unload_plugin<VersionOkPlugin>();
        }
        if (reg.find_plugin<VersionTooNewPlugin>()) {
            reg.unload_plugin<VersionTooNewPlugin>();
        }
        if (reg.find_plugin<DependentPlugin>()) {
            reg.unload_plugin<DependentPlugin>();
        }
        if (reg.find_plugin<FailingPlugin>()) {
            reg.unload_plugin<FailingPlugin>();
        }
        if (reg.find_plugin<TestPlugin>()) {
            reg.unload_plugin<TestPlugin>();
        }
    }
};

TEST_F(PluginTest, LoadPlugin)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(plugin_));
    EXPECT_EQ(1, tp_->initCount);
    EXPECT_EQ(0, tp_->shutdownCount);
}

TEST_F(PluginTest, FindPlugin)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);
    EXPECT_EQ(plugin_.get(), reg.find_plugin<TestPlugin>());
    EXPECT_EQ(nullptr, reg.find_plugin(Uid{"ffffffff-ffff-ffff-ffff-ffffffffffff"}));
}

TEST_F(PluginTest, PluginCount)
{
    auto& reg = velk_.plugin_registry();

    EXPECT_EQ(0u, reg.plugin_count());
    reg.load_plugin(plugin_);
    EXPECT_EQ(1u, reg.plugin_count());
}

TEST_F(PluginTest, PluginTypesRegistered)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);

    auto obj = velk_.create<IObject>(PluginWidget::class_id());
    ASSERT_TRUE(obj);
}

TEST_F(PluginTest, UnloadPlugin)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);
    ASSERT_EQ(ReturnValue::Success, reg.unload_plugin<TestPlugin>());
    EXPECT_EQ(1, tp_->shutdownCount);
    EXPECT_EQ(nullptr, reg.find_plugin<TestPlugin>());
    EXPECT_EQ(0u, reg.plugin_count());
}

TEST_F(PluginTest, UnloadAutoUnregistersTypes)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);

    ASSERT_NE(nullptr, velk_.type_registry().get_class_info(PluginWidget::class_id()));

    reg.unload_plugin<TestPlugin>();

    EXPECT_EQ(nullptr, velk_.type_registry().get_class_info(PluginWidget::class_id()));
}

TEST_F(PluginTest, DoubleLoadReturnsNothingToDo)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(plugin_));
    EXPECT_EQ(ReturnValue::NothingToDo, reg.load_plugin(plugin_));
    EXPECT_EQ(1, tp_->initCount);
}

TEST_F(PluginTest, InvalidReturnsError)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::InvalidArgument, reg.load_plugin({}));
    EXPECT_EQ(0, tp_->initCount);
}

TEST_F(PluginTest, UnloadUnknownReturnsError)
{
    auto& reg = velk_.plugin_registry();
    EXPECT_EQ(ReturnValue::InvalidArgument, reg.unload_plugin(Uid{"ffffffff-ffff-ffff-ffff-ffffffffffff"}));
}

TEST_F(PluginTest, BuiltinTypesSurvivePluginUnload)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);
    reg.unload_plugin<TestPlugin>();

    auto prop = velk_.create_property<int>();
    ASSERT_TRUE(prop);
}

TEST_F(PluginTest, FailedInitializeDoesNotLoad)
{
    auto& reg = velk_.plugin_registry();
    auto fp = ext::make_object<FailingPlugin, IPlugin>();

    EXPECT_EQ(ReturnValue::Fail, reg.load_plugin(fp));
    EXPECT_EQ(nullptr, reg.find_plugin<FailingPlugin>());
    EXPECT_EQ(0u, reg.plugin_count());
}

TEST_F(PluginTest, LoadFromPathNonExistentFails)
{
    auto& reg = velk_.plugin_registry();
    EXPECT_EQ(ReturnValue::Fail, reg.load_plugin_from_path("nonexistent_plugin.dll"));
    EXPECT_EQ(0u, reg.plugin_count());
}

TEST_F(PluginTest, LoadFromPathNullFails)
{
    auto& reg = velk_.plugin_registry();
    EXPECT_EQ(ReturnValue::InvalidArgument, reg.load_plugin_from_path(nullptr));
    EXPECT_EQ(ReturnValue::InvalidArgument, reg.load_plugin_from_path(""));
}

TEST_F(PluginTest, LoadFromPathInvalidDllFails)
{
    // Use the test executable itself as a "DLL" with no velk_create_plugin symbol.
    // On Windows LoadLibrary will fail on an .exe, which is fine (tests FAIL path).
    auto& reg = velk_.plugin_registry();
    EXPECT_EQ(ReturnValue::Fail, reg.load_plugin_from_path("tests.exe"));
    EXPECT_EQ(0u, reg.plugin_count());
}

TEST_F(PluginTest, PluginInfoCollectsStaticMetadata)
{
    // Accessible without an instance via the static function
    auto& info = TestPlugin::plugin_info();
    EXPECT_EQ(TestPlugin::class_id(), info.uid());
    EXPECT_EQ(string_view("TestPlugin"), info.name);
    EXPECT_TRUE(info.dependencies.empty());

    // Same object returned through the virtual
    EXPECT_EQ(&info, &tp_->get_plugin_info());
}

TEST_F(PluginTest, PluginInfoCollectsDependencies)
{
    auto dp = ext::make_object<DependentPlugin, IPlugin>();
    auto& info = dp->get_plugin_info();
    EXPECT_EQ(DependentPlugin::class_id(), info.uid());
    ASSERT_EQ(1u, info.dependencies.size());
    EXPECT_EQ(TestPlugin::class_id(), info.dependencies[0].uid);
}

TEST_F(PluginTest, PluginInfoDefaultName)
{
    // FailingPlugin has no plugin_name, so name defaults to class_name()
    auto fp = ext::make_object<FailingPlugin, IPlugin>();
    auto& info = fp->get_plugin_info();
    EXPECT_EQ(FailingPlugin::class_name(), info.name);
}

TEST_F(PluginTest, PluginVersion)
{
    auto& info = TestPlugin::plugin_info();
    EXPECT_EQ(make_version(2, 1, 0), info.version);
    EXPECT_EQ(2, version_major(info.version));
    EXPECT_EQ(1, version_minor(info.version));
    EXPECT_EQ(0, version_patch(info.version));
}

TEST_F(PluginTest, PluginVersionDefault)
{
    // FailingPlugin has no VELK_PLUGIN_VERSION, defaults to 0
    auto& info = FailingPlugin::plugin_info();
    EXPECT_EQ(0u, info.version);
}

TEST_F(PluginTest, VersionedDependencySatisfied)
{
    auto& reg = velk_.plugin_registry();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(plugin_));

    auto vp = ext::make_object<VersionOkPlugin, IPlugin>();
    EXPECT_EQ(ReturnValue::Success, reg.load_plugin(vp));
}

TEST_F(PluginTest, VersionedDependencyTooNew)
{
    auto& reg = velk_.plugin_registry();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(plugin_));

    auto vp = ext::make_object<VersionTooNewPlugin, IPlugin>();
    EXPECT_EQ(ReturnValue::Fail, reg.load_plugin(vp));
    EXPECT_EQ(nullptr, reg.find_plugin<VersionTooNewPlugin>());
}

TEST_F(PluginTest, DependencyNotLoadedFails)
{
    auto& reg = velk_.plugin_registry();
    auto dp = ext::make_object<DependentPlugin, IPlugin>();

    EXPECT_EQ(ReturnValue::Fail, reg.load_plugin(dp));
    EXPECT_EQ(nullptr, reg.find_plugin<DependentPlugin>());
}

TEST_F(PluginTest, DependencyLoadedSucceeds)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(plugin_));

    auto dp = ext::make_object<DependentPlugin, IPlugin>();
    EXPECT_EQ(ReturnValue::Success, reg.load_plugin(dp));
    EXPECT_NE(nullptr, reg.find_plugin<DependentPlugin>());
}

TEST_F(PluginTest, UnloadRejectsWhenDependentsLoaded)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(plugin_));
    auto dp = ext::make_object<DependentPlugin, IPlugin>();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin(dp));

    // TestPlugin cannot be unloaded while DependentPlugin depends on it
    EXPECT_EQ(ReturnValue::Fail, reg.unload_plugin<TestPlugin>());
    EXPECT_NE(nullptr, reg.find_plugin<TestPlugin>());

    // Unload the dependent first, then the dependency succeeds
    ASSERT_EQ(ReturnValue::Success, reg.unload_plugin<DependentPlugin>());
    EXPECT_EQ(ReturnValue::Success, reg.unload_plugin<TestPlugin>());
}

#ifdef TEST_PLUGIN_DLL_PATH
TEST_F(PluginTest, LoadFromPathSuccess)
{
    auto& reg = velk_.plugin_registry();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin_from_path(TEST_PLUGIN_DLL_PATH));
    EXPECT_EQ(2u, reg.plugin_count()); // host + sub-plugin

    auto* plugin = reg.find_plugin(DllTestPluginUid);
    ASSERT_NE(nullptr, plugin);
    EXPECT_EQ(string_view("DllTestPlugin"), plugin->get_name());
}

TEST_F(PluginTest, LoadFromPathDoubleLoadReturnsNothingToDo)
{
    auto& reg = velk_.plugin_registry();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin_from_path(TEST_PLUGIN_DLL_PATH));
    EXPECT_EQ(ReturnValue::NothingToDo, reg.load_plugin_from_path(TEST_PLUGIN_DLL_PATH));
    EXPECT_EQ(2u, reg.plugin_count()); // host + sub-plugin
}

TEST_F(PluginTest, LoadFromPathThenUnload)
{
    auto& reg = velk_.plugin_registry();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin_from_path(TEST_PLUGIN_DLL_PATH));
    ASSERT_EQ(ReturnValue::Success, reg.unload_plugin(DllTestPluginUid));
    EXPECT_EQ(0u, reg.plugin_count());
    EXPECT_EQ(nullptr, reg.find_plugin(DllTestPluginUid));
}

TEST_F(PluginTest, LoadFromPathMultiPlugin)
{
    auto& reg = velk_.plugin_registry();
    ASSERT_EQ(ReturnValue::Success, reg.load_plugin_from_path(TEST_PLUGIN_DLL_PATH));

    // Host plugin and sub-plugin should both be loaded
    EXPECT_EQ(2u, reg.plugin_count());
    EXPECT_NE(nullptr, reg.find_plugin(DllTestPluginUid));
    EXPECT_NE(nullptr, reg.find_plugin(DllSubPluginUid));
    EXPECT_EQ(string_view("DllSubPlugin"), reg.find_plugin(DllSubPluginUid)->get_name());

    // Unloading the host should also unload the sub-plugin (via shutdown)
    ASSERT_EQ(ReturnValue::Success, reg.unload_plugin(DllTestPluginUid));
    EXPECT_EQ(0u, reg.plugin_count());
    EXPECT_EQ(nullptr, reg.find_plugin(DllSubPluginUid));
}
#endif
