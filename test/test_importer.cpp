#include <velk/api/any.h>
#include <velk/api/hierarchy.h>
#include <velk/api/math_types.h>
#include <velk/api/object_ref.h>
#include <velk/api/store.h>
#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/ext/object.h>
#include <velk/ext/plugin.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/interface/intf_object_ref.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/interface/resource/intf_resource.h>
#include <velk/interface/resource/intf_resource_decoder.h>
#include <velk/interface/resource/intf_resource_store.h>
#include <velk/plugins/animator/plugin.h>
#include <velk/plugins/importer/api/importer.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>
#include <velk/string.h>

#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

namespace velk {

class ITestImportWidget : public Interface<ITestImportWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 0.f),
        (PROP, float, height, 0.f),
        (PROP, int, count, 0),
        (PROP, bool, visible, true),
        (PROP, string, label, "")
    )
};

class TestImportWidget : public ext::Object<TestImportWidget, ITestImportWidget>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000010", "SimpleType");
};

class ITestImportPanel : public Interface<ITestImportPanel>
{
public:
    VELK_INTERFACE(
        (PROP, double, opacity, 1.0)
    )
};

class TestImportPanel : public ext::Object<TestImportPanel, ITestImportPanel>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000011", "AnotherType");
};

class IPluginWidget : public Interface<IPluginWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 0.f),
        (PROP, float, height, 0.f)
    )
};

class PluginOwnedWidget : public ext::Object<PluginOwnedWidget, IPluginWidget>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000020", "Widget");
};

class ITestImportContainer : public Interface<ITestImportContainer>
{
public:
    VELK_INTERFACE(
        (PROP, ObjectRef, target, {}),
        (PROP, float, width, 0.f)
    )
};

class TestImportContainer : public ext::Object<TestImportContainer, ITestImportContainer>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000012", "Container");
};

class ITestResource : public Interface<ITestResource>
{
public:
    VELK_INTERFACE(
        (PROP, string, uri, "")
    )
};

class TestResource : public ext::Object<TestResource, ITestResource>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000013", "TestResource");
};

// A decoded resource produced by the test decoder. Tracks its persistent
// flag so importer tests can verify the importer applied it.
class DecodedTestResource : public ext::ObjectCore<DecodedTestResource, IResource>
{
public:
    string_view uri() const override { return uri_; }
    bool exists() const override { return true; }
    int64_t size() const override { return 1; }
    bool is_persistent() const override { return persistent_; }
    void set_persistent(bool value) override { persistent_ = value; }

    ::velk::string uri_;
    bool persistent_{false};
};

// A decoder named "td" (test decoder) that wraps an inner resource.
class TestDecoder : public ext::ObjectCore<TestDecoder, IResourceDecoder>
{
public:
    string_view name() const override { return "td"; }

    IResource::Ptr decode(const IResource::Ptr& inner) const override
    {
        if (!inner) return nullptr;
        auto obj = ext::make_object<DecodedTestResource>();
        auto* dr = static_cast<DecodedTestResource*>(obj.get());
        dr->uri_ = ::velk::string("td:") + ::velk::string(inner->uri());
        return interface_pointer_cast<IResource>(obj);
    }
};

class WidgetPlugin : public ext::Plugin<WidgetPlugin>
{
public:
    VELK_PLUGIN_UID("c0000000-0000-0000-0000-0000000000a0");
    VELK_PLUGIN_NAME("velk-ui");

    ReturnValue initialize(IVelk& velk, PluginConfig&) override
    {
        return ::velk::register_type<PluginOwnedWidget>(velk);
    }

    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

} // namespace velk

using namespace velk;

class ImporterTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ::velk::instance().type_registry().register_type<TestImportWidget>();
        ::velk::instance().type_registry().register_type<TestImportPanel>();
        ::velk::instance().type_registry().register_type<TestImportContainer>();
        ::velk::instance().type_registry().register_type<TestResource>();
    }

    static void TearDownTestSuite()
    {
        ::velk::instance().type_registry().unregister_type<TestImportWidget>();
        ::velk::instance().type_registry().unregister_type<TestImportPanel>();
        ::velk::instance().type_registry().unregister_type<TestImportContainer>();
        ::velk::instance().type_registry().unregister_type<TestResource>();
    }

    void load_importer()
    {
        auto& reg = ::velk::instance().plugin_registry();
        reg.load_plugin_from_path(TEST_IMPORTER_DLL_PATH);
        auto plugin = reg.find_plugin(PluginId::ImporterPlugin);
        ASSERT_TRUE(plugin);
        auto* ip = interface_cast<IImporterPlugin>(plugin);
        ASSERT_NE(nullptr, ip);
        ip->register_class_alias("test.Widget", TestImportWidget::static_class_id());
        ip->register_class_alias("test.Panel", TestImportPanel::static_class_id());
        ip->register_class_alias("test.Container", TestImportContainer::static_class_id());
        ip->register_class_alias("test.Resource", TestResource::static_class_id());

        auto obj = ::velk::instance().create<IStoreImporter>(ClassId::JsonImporter);
        ASSERT_TRUE(obj);
        importer_ = obj;
    }

    void TearDown() override
    {
        importer_ = nullptr;
        auto& reg = ::velk::instance().plugin_registry();
        if (reg.find_plugin(PluginId::ImporterPlugin)) {
            reg.unload_plugin(PluginId::ImporterPlugin);
        }
    }

    IStoreImporter::Ptr importer_;
};

TEST_F(ImporterTest, ImportSingleObject)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "widget_1",
                "class": "test.Widget",
                "properties": {
                    "width": 800.0,
                    "height": 600.0,
                    "count": 42,
                    "visible": false,
                    "label": "Hello"
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(1u, result.store->object_count());

    auto obj = result.store->find("widget_1");
    ASSERT_TRUE(obj);

    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(800.0f, tw->width().get_value());
    EXPECT_FLOAT_EQ(600.0f, tw->height().get_value());
    EXPECT_EQ(42, tw->count().get_value());
    EXPECT_FALSE(tw->visible().get_value());
    EXPECT_EQ(::velk::string_view("Hello"), ::velk::string_view(tw->label().get_value()));
}

TEST_F(ImporterTest, ImportMultipleObjects)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "widget_1",
                "class": "test.Widget",
                "properties": { "width": 100.0 }
            },
            {
                "id": "panel_1",
                "class": "test.Panel",
                "properties": { "opacity": 0.5 }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(2u, result.store->object_count());

    auto w = result.store->find("widget_1");
    ASSERT_TRUE(w);
    EXPECT_FLOAT_EQ(100.0f, interface_cast<ITestImportWidget>(w)->width().get_value());

    auto p = result.store->find("panel_1");
    ASSERT_TRUE(p);
    EXPECT_DOUBLE_EQ(0.5, interface_cast<ITestImportPanel>(p)->opacity().get_value());
}

TEST_F(ImporterTest, ImportWithHierarchy)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "root", "class": "test.Widget", "properties": { "width": 1.0 } },
            { "id": "child_a", "class": "test.Widget", "properties": { "width": 2.0 } },
            { "id": "child_b", "class": "test.Widget", "properties": { "width": 3.0 } }
        ],
        "hierarchies": {
            "scene": {
                "root": ["child_a", "child_b"]
            }
        }
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(3u + 1u, result.store->object_count()); // 3 objects + 1 hierarchy

    auto hierarchy_obj = result.store->find("hierarchy:scene");
    ASSERT_TRUE(hierarchy_obj);

    Hierarchy h(hierarchy_obj);
    ASSERT_TRUE(h);

    auto root_node = h.root();
    ASSERT_TRUE(root_node);

    auto root_obj = result.store->find("root");
    EXPECT_EQ(root_obj, root_node.object());

    auto children = root_node.get_children();
    EXPECT_EQ(2u, children.size());
}

TEST_F(ImporterTest, ImportByClassUid)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "widget_uid",
                "class": "c0000000-0000-0000-0000-000000000010",
                "properties": { "width": 42.0 }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    auto obj = result.store->find("widget_uid");
    ASSERT_TRUE(obj);

    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(42.0f, tw->width().get_value());
}

TEST_F(ImporterTest, UnknownClassReportsError)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "unknown", "class": "test.Unknown", "properties": {} },
            { "id": "known", "class": "test.Widget", "properties": { "width": 1.0 } }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_EQ(1u, result.store->object_count());
    EXPECT_FALSE(result.store->find("unknown"));
    EXPECT_TRUE(result.store->find("known"));

    ASSERT_EQ(1u, result.errors.size());
    EXPECT_NE(::velk::string::npos, result.errors[0].find("test.Unknown"));
}

TEST_F(ImporterTest, InvalidJsonReportsError)
{
    load_importer();
    auto result = importer_->import_from("{invalid");
    EXPECT_FALSE(result.store);
    ASSERT_FALSE(result.errors.empty());
}

TEST_F(ImporterTest, AppConfigReportsError)
{
    load_importer();
    auto result = importer_->import_from(R"({ "app": { "store": "scene.velk" } })");
    EXPECT_FALSE(result.store);
    ASSERT_EQ(1u, result.errors.size());
    EXPECT_NE(::velk::string::npos, result.errors[0].find("app.velk"));
}

TEST_F(ImporterTest, UnknownPropertyReportsError)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": {
                    "width": 1.0,
                    "nonexistent": 42
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_EQ(1u, result.store->object_count());

    ASSERT_EQ(1u, result.errors.size());
    EXPECT_NE(::velk::string::npos, result.errors[0].find("nonexistent"));
}

TEST_F(ImporterTest, PropertyObjectForm)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": {
                    "width": { "value": 999.0 }
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    EXPECT_FLOAT_EQ(999.0f, interface_cast<ITestImportWidget>(obj)->width().get_value());
}

TEST_F(ImporterTest, DefaultValues)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": {} }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);

    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(0.0f, tw->width().get_value());
    EXPECT_TRUE(tw->visible().get_value());
}

TEST_F(ImporterTest, EmptyStore)
{
    load_importer();
    auto result = importer_->import_from(R"({ "version": 1, "objects": [] })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(0u, result.store->object_count());
}

TEST_F(ImporterTest, ImportByScopedFriendlyName)
{
    load_importer();

    // Load a plugin that registers PluginOwnedWidget with friendly name "Widget"
    auto plugin = ext::make_object<WidgetPlugin, IPlugin>(); // Plugin name: velk-ui
    ::velk::instance().plugin_registry().load_plugin(plugin);

    // Import using "velk-ui.Widget" with no alias registration
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "velk-ui.Widget",
                "properties": {
                    "width": 320.0,
                    "height": 240.0
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    // Expect an instance of "velk-ui.Widget" to be there
    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);

    auto* pw = interface_cast<IPluginWidget>(obj);
    ASSERT_NE(nullptr, pw);
    EXPECT_FLOAT_EQ(320.0f, pw->width().get_value());
    EXPECT_FLOAT_EQ(240.0f, pw->height().get_value());

    obj.reset();
    result = {};
    ::velk::instance().plugin_registry().unload_plugin(WidgetPlugin::static_class_id());
}

TEST_F(ImporterTest, ObjectRefByDirectName)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 10.0 } },
            {
                "id": "c1",
                "class": "test.Container",
                "properties": {
                    "target": { "ref": "w1" }
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto c1 = result.store->find("c1");
    ASSERT_TRUE(c1);
    auto* ci = interface_cast<ITestImportContainer>(c1);
    ASSERT_NE(nullptr, ci);

    auto val = ci->target().get_value();
    ASSERT_TRUE(val);
    auto* obj_ref = interface_cast<IObjectRef>(val);
    ASSERT_TRUE(obj_ref);
    EXPECT_TRUE(obj_ref->is_owning());

    auto w1 = result.store->find("w1");
    EXPECT_EQ(obj_ref->get_object(), w1);
}

TEST_F(ImporterTest, ObjectRefByHierarchyPath)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "root", "name": "root", "class": "test.Widget", "properties": { "width": 1.0 } },
            { "id": "child_a", "name": "child_a", "class": "test.Widget", "properties": { "width": 2.0 } },
            {
                "id": "c1",
                "name": "c1",
                "class": "test.Container",
                "properties": {
                    "target": { "ref": "/root/child_a" }
                }
            }
        ],
        "hierarchies": {
            "scene": {
                "root": ["child_a", "c1"]
            }
        }
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto c1 = result.store->find("c1");
    auto* ci = interface_cast<ITestImportContainer>(c1);
    ASSERT_NE(nullptr, ci);

    auto val = ci->target().get_value();
    auto* obj_ref = interface_cast<IObjectRef>(val);
    ASSERT_TRUE(obj_ref);

    auto child_a = result.store->find("child_a");
    EXPECT_EQ(obj_ref->get_object(), child_a);
}

TEST_F(ImporterTest, ObjectRefWeak)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 10.0 } },
            {
                "id": "c1",
                "class": "test.Container",
                "properties": {
                    "target": { "ref": "w1", "type": "weak" }
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto c1 = result.store->find("c1");
    auto* ci = interface_cast<ITestImportContainer>(c1);
    auto val = ci->target().get_value();
    auto* obj_ref = interface_cast<IObjectRef>(val);
    ASSERT_TRUE(obj_ref);
    EXPECT_FALSE(obj_ref->is_owning());
    EXPECT_EQ(obj_ref->get_object(), result.store->find("w1"));
}

TEST_F(ImporterTest, ObjectRefNonexistentReportsError)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "c1",
                "class": "test.Container",
                "properties": {
                    "target": { "ref": "nonexistent" }
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    ASSERT_FALSE(result.errors.empty());
    bool found = false;
    for (auto& e : result.errors) {
        if (e.find("nonexistent") != ::velk::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ImporterTest, TopLevelBindingSourceDrivesTarget)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "src", "class": "test.Widget", "properties": { "width": 100.0 } },
            { "id": "dst", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "bindings": [
            { "source": "src.width", "targets": ["dst.width"] }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto src = result.store->find("src");
    auto dst = result.store->find("dst");
    auto* src_w = interface_cast<ITestImportWidget>(src);
    auto* dst_w = interface_cast<ITestImportWidget>(dst);

    // Binding should make dst.width read the same as src.width
    EXPECT_FLOAT_EQ(100.0f, dst_w->width().get_value());

    // Changing source should propagate
    src_w->width().set_value(200.0f);
    EXPECT_FLOAT_EQ(200.0f, dst_w->width().get_value());
}

TEST_F(ImporterTest, TopLevelBindingMultiTarget)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "src", "class": "test.Widget", "properties": { "width": 50.0 } },
            { "id": "dst1", "class": "test.Widget", "properties": { "width": 0.0 } },
            { "id": "dst2", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "bindings": [
            { "source": "src.width", "targets": ["dst1.width", "dst2.width"] }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto* dst1_w = interface_cast<ITestImportWidget>(result.store->find("dst1"));
    auto* dst2_w = interface_cast<ITestImportWidget>(result.store->find("dst2"));

    EXPECT_FLOAT_EQ(50.0f, dst1_w->width().get_value());
    EXPECT_FLOAT_EQ(50.0f, dst2_w->width().get_value());
}

TEST_F(ImporterTest, InlineBindCreatesBinding)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "src", "class": "test.Widget", "properties": { "width": 42.0 } },
            {
                "id": "dst",
                "class": "test.Widget",
                "properties": {
                    "width": { "bind": "src.width" }
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto* src_w = interface_cast<ITestImportWidget>(result.store->find("src"));
    auto* dst_w = interface_cast<ITestImportWidget>(result.store->find("dst"));

    // Inline ref creates a binding, so dst reads source value
    EXPECT_FLOAT_EQ(42.0f, dst_w->width().get_value());

    // Changing source propagates
    src_w->width().set_value(99.0f);
    EXPECT_FLOAT_EQ(99.0f, dst_w->width().get_value());
}

TEST_F(ImporterTest, BindingNonexistentSourceReportsError)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "dst", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "bindings": [
            { "source": "nonexistent.width", "targets": ["dst.width"] }
        ]
    })");

    ASSERT_TRUE(result.store);
    ASSERT_FALSE(result.errors.empty());
    bool found = false;
    for (auto& e : result.errors) {
        if (e.find("nonexistent") != ::velk::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ImporterTest, BindingNonexistentTargetReportsError)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "src", "class": "test.Widget", "properties": { "width": 10.0 } }
        ],
        "bindings": [
            { "source": "src.width", "targets": ["nonexistent.width"] }
        ]
    })");

    ASSERT_TRUE(result.store);
    ASSERT_FALSE(result.errors.empty());
    bool found = false;
    for (auto& e : result.errors) {
        if (e.find("nonexistent") != ::velk::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

namespace velk {

// Static storage for mock extension test results
struct MockExtensionResult
{
    bool was_called = false;
    size_t item_count = 0;
    double first_value = 0.0;
    std::string first_name;
};

static MockExtensionResult g_mock_result;

class MockImportExtension : public ext::ObjectCore<MockImportExtension, IImporterExtension>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000099", "MockImportExtension");

    string_view collection_key() const override { return "custom_data"; }

    void process(const IImportData& data, IStore&, const IImportResolver&) const override
    {
        g_mock_result.was_called = true;
        g_mock_result.item_count = data.count();
        if (data.count() > 0) {
            auto& first = data.at(0);
            auto& name_node = first.find("name");
            auto& value_node = first.find("value");
            if (!name_node.is_null()) {
                auto sv = name_node.as_string();
                g_mock_result.first_name = std::string(sv.data(), sv.size());
            }
            if (!value_node.is_null()) {
                g_mock_result.first_value = value_node.as_number();
            }
        }
    }
};

} // namespace velk

TEST_F(ImporterTest, ExtensionDispatch)
{
    ::velk::g_mock_result = {};
    ::velk::instance().type_registry().register_type<::velk::MockImportExtension>();

    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [],
        "custom_data": [
            { "name": "alpha", "value": 42.0 },
            { "name": "beta", "value": 7.0 }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    EXPECT_TRUE(::velk::g_mock_result.was_called);
    EXPECT_EQ(2u, ::velk::g_mock_result.item_count);
    EXPECT_EQ("alpha", ::velk::g_mock_result.first_name);
    EXPECT_DOUBLE_EQ(42.0, ::velk::g_mock_result.first_value);

    ::velk::instance().type_registry().unregister_type<::velk::MockImportExtension>();
}

TEST_F(ImporterTest, ExtensionNotCalledWhenKeyAbsent)
{
    ::velk::g_mock_result = {};
    ::velk::instance().type_registry().register_type<::velk::MockImportExtension>();

    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": []
    })");

    ASSERT_TRUE(result.store);
    EXPECT_FALSE(::velk::g_mock_result.was_called);

    ::velk::instance().type_registry().unregister_type<::velk::MockImportExtension>();
}

TEST_F(ImporterTest, NullImportDataChaining)
{
    ::velk::g_mock_result = {};
    ::velk::instance().type_registry().register_type<::velk::MockImportExtension>();

    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [],
        "custom_data": []
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(::velk::g_mock_result.was_called);
    EXPECT_EQ(0u, ::velk::g_mock_result.item_count);

    ::velk::instance().type_registry().unregister_type<::velk::MockImportExtension>();
}

// ============================================================================
// Animator extension tests (require both velk_importer.dll and velk_animator.dll)
// ============================================================================

class AnimatorImportTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ::velk::instance().type_registry().register_type<TestImportWidget>();
    }

    static void TearDownTestSuite()
    {
        auto& reg = ::velk::instance().plugin_registry();
        if (reg.find_plugin(PluginId::AnimatorPlugin)) {
            reg.unload_plugin(PluginId::AnimatorPlugin);
        }
        ::velk::instance().type_registry().unregister_type<TestImportWidget>();
    }

    void SetUp() override
    {
        auto& reg = ::velk::instance().plugin_registry();
        reg.load_plugin_from_path(TEST_ANIMATOR_DLL_PATH);
        reg.load_plugin_from_path(TEST_IMPORTER_DLL_PATH);
        auto plugin = reg.find_plugin(PluginId::ImporterPlugin);
        ASSERT_TRUE(plugin);
        auto* ip = interface_cast<IImporterPlugin>(plugin);
        ASSERT_NE(nullptr, ip);
        ip->register_class_alias("test.Widget", TestImportWidget::static_class_id());

        importer_ = ::velk::instance().create<IStoreImporter>(ClassId::JsonImporter);
        ASSERT_TRUE(importer_);
    }

    void TearDown() override
    {
        importer_ = nullptr;
        auto& reg = ::velk::instance().plugin_registry();
        if (reg.find_plugin(PluginId::ImporterPlugin)) {
            reg.unload_plugin(PluginId::ImporterPlugin);
        }
    }

    static bool has_animation_extension(const IProperty::Ptr& prop)
    {
        auto chain = get_any_chain(*prop);
        return chain.size() > 1;
    }

    IStoreImporter::Ptr importer_;
};

TEST_F(AnimatorImportTest, TransitionCreatedFromAnimationsKey)
{
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": { "width": 100.0 }
            }
        ],
        "animations": [
            {
                "type": "transition",
                "targets": ["w1.width"],
                "duration": 0.5,
                "easing": "out_cubic"
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    EXPECT_TRUE(has_animation_extension(IProperty::Ptr(tw->width())));
}

TEST_F(AnimatorImportTest, MultipleTransitions)
{
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": { "width": 10.0, "height": 20.0 }
            }
        ],
        "animations": [
            {
                "type": "transition",
                "targets": ["w1.width"],
                "duration": 0.3
            },
            {
                "type": "transition",
                "targets": ["w1.height"],
                "duration": 0.7,
                "easing": "in_quad"
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    EXPECT_TRUE(has_animation_extension(IProperty::Ptr(tw->width())));
    EXPECT_TRUE(has_animation_extension(IProperty::Ptr(tw->height())));
}

TEST_F(AnimatorImportTest, MultiTargetTransition)
{
    // Seed the clock
    ::velk::instance().update({1'000'000});

    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": { "width": 0.0, "height": 0.0 }
            }
        ],
        "animations": [
            {
                "type": "transition",
                "targets": ["w1.width", "w1.height"],
                "duration": 1.0,
                "easing": "linear"
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // Both properties should have a transition installed
    ASSERT_TRUE(has_animation_extension(IProperty::Ptr(tw->width())));
    ASSERT_TRUE(has_animation_extension(IProperty::Ptr(tw->height())));

    // Trigger both transitions
    tw->width().set_value(100.0f);
    tw->height().set_value(200.0f);

    // Advance halfway
    ::velk::instance().update({1'500'000});
    float mid_w = tw->width().get_value();
    float mid_h = tw->height().get_value();
    EXPECT_GT(mid_w, 10.f);
    EXPECT_LT(mid_w, 90.f);
    EXPECT_GT(mid_h, 20.f);
    EXPECT_LT(mid_h, 180.f);

    // Advance past the end
    ::velk::instance().update({2'100'000});
    EXPECT_FLOAT_EQ(100.0f, tw->width().get_value());
    EXPECT_FLOAT_EQ(200.0f, tw->height().get_value());
}

TEST_F(AnimatorImportTest, UnknownTargetSilentlySkipped)
{
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 1.0 } }
        ],
        "animations": [
            {
                "type": "transition",
                "targets": ["nonexistent.width"],
                "duration": 0.5
            }
        ]
    })");

    ASSERT_TRUE(result.store);
}

TEST_F(AnimatorImportTest, EmptyAnimationsArray)
{
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 1.0 } }
        ],
        "animations": []
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    EXPECT_FALSE(has_animation_extension(IProperty::Ptr(tw->width())));
}

TEST_F(AnimatorImportTest, DefaultEasingIsLinear)
{
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 1.0 } }
        ],
        "animations": [
            {
                "type": "transition",
                "targets": ["w1.width"],
                "duration": 1.0
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);

    EXPECT_TRUE(has_animation_extension(IProperty::Ptr(tw->width())));
}

TEST_F(AnimatorImportTest, TransitionAnimatesProperty)
{
    // Seed the clock
    ::velk::instance().update({1'000'000});

    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "animations": [
            {
                "type": "transition",
                "targets": ["w1.width"],
                "duration": 1.0,
                "easing": "linear"
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // Set a new value to trigger the transition
    tw->width().set_value(100.0f);

    // Value should not jump immediately
    EXPECT_NEAR(0.f, tw->width().get_value(), 0.1f);

    // Advance halfway through the 1s transition
    ::velk::instance().update({1'500'000});
    float mid = tw->width().get_value();
    EXPECT_GT(mid, 10.f);
    EXPECT_LT(mid, 90.f);

    // Advance past the end
    ::velk::instance().update({2'100'000});
    EXPECT_FLOAT_EQ(100.0f, tw->width().get_value());
}

TEST_F(AnimatorImportTest, TrackAnimatesProperty)
{
    // Seed the clock
    ::velk::instance().update({3'000'000});

    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "animations": [
            {
                "type": "track",
                "targets": ["w1.width"],
                "keyframes": [
                    { "time": 0.0, "value": 0.0 },
                    { "time": 1.0, "value": 100.0 }
                ]
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());

    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // Track autoplays by default, advance halfway
    ::velk::instance().update({3'500'000});
    float mid = tw->width().get_value();
    EXPECT_GT(mid, 10.f);
    EXPECT_LT(mid, 90.f);

    // Advance past the end
    ::velk::instance().update({4'100'000});
    EXPECT_FLOAT_EQ(100.0f, tw->width().get_value());
}

TEST_F(AnimatorImportTest, TrackMultipleKeyframes)
{
    // Seed the clock
    ::velk::instance().update({5'000'000});

    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "animations": [
            {
                "type": "track",
                "targets": ["w1.width"],
                "keyframes": [
                    { "time": 0.0, "value": 0.0 },
                    { "time": 0.5, "value": 200.0 },
                    { "time": 1.0, "value": 100.0 }
                ]
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // At t=0.5s the value should be near 200 (peak of the V shape)
    ::velk::instance().update({5'500'000});
    EXPECT_NEAR(200.f, tw->width().get_value(), 5.f);

    // At t=1.0s the value should be back to 100
    ::velk::instance().update({6'100'000});
    EXPECT_FLOAT_EQ(100.0f, tw->width().get_value());
}

TEST_F(AnimatorImportTest, TrackMultiTarget)
{
    // Seed the clock
    ::velk::instance().update({7'000'000});

    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 0.0, "height": 0.0 } }
        ],
        "animations": [
            {
                "type": "track",
                "targets": ["w1.width", "w1.height"],
                "keyframes": [
                    { "time": 0.0, "value": 0.0 },
                    { "time": 1.0, "value": 50.0 }
                ]
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // Both properties should be animated to 50
    ::velk::instance().update({8'100'000});
    EXPECT_FLOAT_EQ(50.0f, tw->width().get_value());
    EXPECT_FLOAT_EQ(50.0f, tw->height().get_value());
}

TEST_F(AnimatorImportTest, TrackAutoplayFalse)
{
    // Seed the clock
    ::velk::instance().update({9'000'000});

    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 0.0 } }
        ],
        "animations": [
            {
                "type": "track",
                "targets": ["w1.width"],
                "keyframes": [
                    { "time": 0.0, "value": 0.0 },
                    { "time": 1.0, "value": 100.0 }
                ],
                "autoplay": false
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    auto obj = result.store->find("w1");
    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // Advance time but track should not play
    ::velk::instance().update({10'100'000});
    EXPECT_FLOAT_EQ(0.0f, tw->width().get_value());
}

TEST_F(ImporterTest, ResourceProtocols)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "resource-protocols": [
            { "scheme": "testassets", "base_path": "./test_assets/" }
        ],
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 10 } }
        ]
    })");

    EXPECT_TRUE(result.errors.empty());

    // Verify the protocol was registered
    auto proto = ::velk::instance().resource_store().find_protocol("testassets");
    EXPECT_TRUE(proto);
    if (proto) {
        EXPECT_EQ(string_view("testassets"), proto->scheme());
    }

    // Cleanup
    if (proto) {
        ::velk::instance().resource_store().unregister_protocol(proto);
    }
}

TEST_F(ImporterTest, Resources)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "resources": [
            { "id": "font1", "class": "test.Resource", "properties": { "uri": "app://fonts/arial.ttf" } }
        ],
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": { "width": 10 } }
        ]
    })");

    EXPECT_TRUE(result.errors.empty());
    ASSERT_TRUE(result.store);

    // Resource is stored with "resource:" prefix
    auto res = result.store->find("resource:font1");
    ASSERT_TRUE(res);

    auto* tr = interface_cast<ITestResource>(res);
    ASSERT_NE(nullptr, tr);
    EXPECT_EQ(string_view("app://fonts/arial.ttf"), tr->uri().get_value());
}

TEST_F(ImporterTest, ResourcesNoClassDecoderForm)
{
    load_importer();

    // Create a temp file the inner URI will resolve to.
    auto tmp = testing::TempDir();
    ::velk::string path(tmp.c_str(), tmp.size());
    path.append("rs_importer_test.txt");
#ifdef _WIN32
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
#else
    FILE* f = fopen(path.c_str(), "wb");
#endif
    ASSERT_TRUE(f);
    const char* content = "decoder payload";
    fwrite(content, 1, std::strlen(content), f);
    fclose(f);

    // Register the test decoder for the duration of this test.
    auto& store = ::velk::instance().resource_store();
    auto dec = ext::make_object<TestDecoder>();
    auto dec_iface = interface_pointer_cast<IResourceDecoder>(dec);
    store.register_decoder(dec_iface);

    // Forward slashes for JSON-friendly path (works on Windows too).
    ::velk::string fwd_path = path;
    for (size_t i = 0; i < fwd_path.size(); ++i) {
        if (fwd_path[i] == '\\') fwd_path[i] = '/';
    }
    auto file_uri = ::velk::string("file://") + fwd_path;
    auto outer_uri = ::velk::string("td:") + file_uri;

    // Build the JSON inline.
    ::velk::string json;
    json += R"({ "version": 1, "resources": [ { "id": "decoded", "uri": ")";
    json += outer_uri;
    json += R"(", "persistent": true } ] })";

    auto result = importer_->import_from(string_view(json));
    EXPECT_TRUE(result.errors.empty());
    ASSERT_TRUE(result.store);

    // The resource should be in the store under the "resource:" prefix.
    auto stored = result.store->find("resource:decoded");
    ASSERT_TRUE(stored);

    // It should be the same instance as what the resource store cached.
    auto from_store = store.get_resource(string_view(outer_uri));
    ASSERT_TRUE(from_store);
    auto stored_as_res = interface_pointer_cast<IResource>(stored);
    ASSERT_TRUE(stored_as_res);
    EXPECT_EQ(from_store.get(), stored_as_res.get());

    // Persistence should have been applied by the importer.
    EXPECT_TRUE(stored_as_res->is_persistent());

    // Cleanup: unpin and unregister.
    stored_as_res->set_persistent(false);
    store.unregister_decoder(dec_iface);
}

TEST_F(ImporterTest, ResourceRef)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "c1", "class": "test.Container", "properties": {
                "target": { "ref": "resources.font1" }
            }}
        ],
        "resources": [
            { "id": "font1", "class": "test.Resource", "properties": { "uri": "app://fonts/arial.ttf" } }
        ]
    })");

    EXPECT_TRUE(result.errors.empty());
    ASSERT_TRUE(result.store);

    auto container = result.store->find("c1");
    ASSERT_TRUE(container);

    auto* tc = interface_cast<ITestImportContainer>(container);
    ASSERT_NE(nullptr, tc);

    // The ObjectRef should point to the resource
    auto val = tc->target().get_value();
    ASSERT_TRUE(val);
    auto* obj_ref = interface_cast<IObjectRef>(val);
    ASSERT_TRUE(obj_ref);

    auto target_obj = obj_ref->get_object();
    ASSERT_TRUE(target_obj);

    auto* tr = interface_cast<ITestResource>(target_obj);
    ASSERT_NE(nullptr, tr);
    EXPECT_EQ(string_view("app://fonts/arial.ttf"), tr->uri().get_value());
}


namespace velk {

namespace {

/** @brief Converts @p value to JSON and back; returns the JSON text. */
template <class T>
string round_trip(IJsonValueCodec& codec, const T& value, T& back)
{
    Any<T> any(value);
    string json = codec.to_json(*any.get_any_interface(), type_uid<T>());
    auto read = codec.from_json(json, type_uid<T>());
    if (read) {
        read->get_data(&back, sizeof(T), type_uid<T>());
    }
    return json;
}

} // namespace

TEST_F(ImporterTest, JsonValueCodecBuiltIns)
{
    load_importer();
    auto* codec = interface_cast<IJsonValueCodec>(importer_);
    ASSERT_NE(nullptr, codec);

    float f = 0.f;
    EXPECT_EQ(string("0.1"), round_trip(*codec, 0.1f, f));
    EXPECT_EQ(0.1f, f);

    int32_t i = 0;
    EXPECT_EQ(string("-42"), round_trip(*codec, int32_t(-42), i));
    EXPECT_EQ(-42, i);

    bool b = false;
    EXPECT_EQ(string("true"), round_trip(*codec, true, b));
    EXPECT_TRUE(b);

    string s;
    EXPECT_EQ(string("\"a \\\"quoted\\\" word\""), round_trip(*codec, string("a \"quoted\" word"), s));
    EXPECT_EQ(string("a \"quoted\" word"), s);

    color c{};
    EXPECT_EQ(string("{\"r\":0.1,\"g\":0.2,\"b\":0.3,\"a\":1}"), round_trip(*codec, color{0.1f, 0.2f, 0.3f, 1.f}, c));
    EXPECT_EQ(0.2f, c.g);

    vec3 v{};
    EXPECT_EQ(string("{\"x\":1,\"y\":2.5,\"z\":-3}"), round_trip(*codec, vec3{1.f, 2.5f, -3.f}, v));
    EXPECT_EQ(2.5f, v.y);

    aabb box{};
    round_trip(*codec, aabb{{1.f, 2.f, 3.f}, {4.f, 5.f, 6.f}}, box);
    EXPECT_EQ(5.f, box.extent.height);

    EXPECT_FALSE(codec->from_json("not json", type_uid<float>()));
    EXPECT_FALSE(codec->from_json("\"text\"", type_uid<float>()));
}

TEST_F(ImporterTest, JsonValueCodecArrays)
{
    load_importer();
    auto* codec = interface_cast<IJsonValueCodec>(importer_);
    ASSERT_NE(nullptr, codec);
    const float values[] = {1.f, 2.5f};
    auto any = instance().create_any(type_uid<vector<float>>());
    auto* array = interface_cast<IArrayAny>(any);
    ASSERT_NE(nullptr, array);
    ASSERT_TRUE(succeeded(array->set_from_buffer(values, 2, type_uid<float>())));
    EXPECT_EQ(string("[1,2.5]"), codec->to_json(*any, type_uid<vector<float>>()));
}

TEST_F(ImporterTest, AttachmentIdResolves)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget" },
            { "id": "c1", "class": "test.Container", "properties": { "target": { "ref": "panel" } } },
            { "id": "c2", "class": "test.Container", "properties": { "target": { "ref": "attachments.panel" } } }
        ],
        "attachments": [
            { "id": "panel", "class": "test.Panel", "targets": ["w1"], "properties": { "opacity": 0.5 } },
            { "class": "test.Panel", "targets": ["w1"] }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto panel = result.store->find("attachment:panel");
    ASSERT_TRUE(panel);
    EXPECT_DOUBLE_EQ(0.5, interface_cast<ITestImportPanel>(panel)->opacity().get_value());

    auto* storage = interface_cast<IObjectStorage>(result.store->find("w1"));
    ASSERT_NE(nullptr, storage);
    EXPECT_EQ(2u, storage->attachment_count());

    for (string_view id : {string_view("c1"), string_view("c2")}) {
        auto* container = interface_cast<ITestImportContainer>(result.store->find(id));
        ASSERT_NE(nullptr, container);
        auto* ref = interface_cast<IObjectRef>(container->target().get_value());
        ASSERT_NE(nullptr, ref);
        EXPECT_EQ(panel, ref->get_object()) << id;
    }
}

TEST_F(ImporterTest, AttachmentIdErrors)
{
    load_importer();
    auto result = importer_->import_from(R"({
        "version": 1,
        "objects": [ { "id": "w1", "class": "test.Widget" } ],
        "attachments": [
            { "id": 5, "class": "test.Panel", "targets": ["w1"] },
            { "id": "p", "class": "test.Panel", "targets": ["w1"] },
            { "id": "p", "class": "test.Panel", "targets": ["w1"] }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_EQ(2u, result.errors.size());
}

TEST_F(ImporterTest, SourceTrackingOffRecordsNothing)
{
    load_importer();
    auto* ip = interface_cast<IImporterPlugin>(instance().plugin_registry().find_plugin(PluginId::ImporterPlugin));
    ASSERT_NE(nullptr, ip);
    EXPECT_FALSE(ip->track_sources());

    auto importer = create_json_importer();
    auto result = importer.import_from(R"({ "version": 1, "objects": [ { "id": "w1", "class": "test.Widget" } ] })",
                                       "scene.json");
    ASSERT_TRUE(result.store);
    auto w1 = result.store->find("w1");
    ASSERT_TRUE(w1);
    EXPECT_FALSE(ip->source_of(*w1));
}

TEST_F(ImporterTest, SourceTrackingRecordsEntries)
{
    load_importer();
    auto* ip = interface_cast<IImporterPlugin>(instance().plugin_registry().find_plugin(PluginId::ImporterPlugin));
    ASSERT_NE(nullptr, ip);
    ip->set_track_sources(true);
    EXPECT_TRUE(ip->track_sources());

    auto importer = create_json_importer();
    auto result = importer.import_from(R"({
        "version": 1,
        "objects": [ { "id": "w1", "class": "test.Widget" } ],
        "resources": [ { "id": "r1", "class": "test.Resource" } ],
        "hierarchies": { "scene": { "w1": [] } },
        "attachments": [
            { "id": "panel", "class": "test.Panel", "targets": ["w1"] },
            { "class": "test.Panel", "targets": ["w1"] }
        ]
    })", "scene.json");
    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << result.errors[0];

    auto expect_source = [&](const IObject::Ptr& object, const char* entry) {
        ASSERT_TRUE(object) << entry;
        auto source = ip->source_of(*object);
        EXPECT_EQ(string("scene.json"), source.source) << entry;
        EXPECT_EQ(string(entry), source.entry);
    };
    expect_source(result.store->find("w1"), "objects/w1");
    expect_source(result.store->find("resource:r1"), "resources/r1");
    expect_source(result.store->find("attachment:panel"), "attachments/panel");

    // An attachment without an id has no entry.
    auto* storage = interface_cast<IObjectStorage>(result.store->find("w1"));
    ASSERT_NE(nullptr, storage);
    ASSERT_EQ(2u, storage->attachment_count());
    auto unnamed = interface_pointer_cast<IObject>(storage->get_attachment(1));
    ASSERT_TRUE(unnamed);
    EXPECT_NE(result.store->find("attachment:panel"), unnamed);
    EXPECT_FALSE(ip->source_of(*unnamed));

    // Hierarchies are not entries.
    auto hierarchy = result.store->find("hierarchy:scene");
    ASSERT_TRUE(hierarchy);
    EXPECT_FALSE(ip->source_of(*hierarchy));

    // The plain importer records nothing, even with tracking on.
    auto plain = importer_->import_from(R"({ "version": 1, "objects": [ { "id": "w2", "class": "test.Widget" } ] })",
                                        "plain.json");
    ASSERT_TRUE(plain.store);
    EXPECT_FALSE(ip->source_of(*plain.store->find("w2")));
}

TEST_F(ImporterTest, SourceTrackingHoldsObjectsWeakly)
{
    load_importer();
    auto* ip = interface_cast<IImporterPlugin>(instance().plugin_registry().find_plugin(PluginId::ImporterPlugin));
    ASSERT_NE(nullptr, ip);
    ip->set_track_sources(true);

    IObject::WeakPtr weak;
    {
        auto result = create_json_importer().import_from(
            R"({ "version": 1, "objects": [ { "id": "w1", "class": "test.Widget" } ] })", "scene.json");
        ASSERT_TRUE(result.store);
        auto w1 = result.store->find("w1");
        ASSERT_TRUE(w1);
        EXPECT_TRUE(ip->source_of(*w1));
        weak = w1;
    }
    EXPECT_FALSE(weak.lock());

    // A new object, possibly at the dead one's address, has no source.
    auto fresh = instance().create<IObject>(TestImportWidget::static_class_id());
    ASSERT_TRUE(fresh);
    EXPECT_FALSE(ip->source_of(*fresh));
}

} // namespace velk
