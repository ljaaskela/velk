#include <velk/api/event.h>
#include <velk/api/hierarchy.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_hierarchy.h>
#include <velk/plugins/remote/api/remote.h>

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

namespace velk {

class ITestRemoteWidget : public Interface<ITestRemoteWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 10.f),
        (PROP, string, label, "start"),
        (PROP, int, count, 0),
        (PROP, ObjectRef, target, {}),
        (RPROP, int, version, 3)
    )
};

class TestRemoteWidget : public ext::Object<TestRemoteWidget, ITestRemoteWidget>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-0000000000a1", "TestRemoteWidget");
};

class RemoteTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().plugin_registry().load_plugin_from_path(TEST_REMOTE_DLL_PATH);
        instance().type_registry().register_type<TestRemoteWidget>();
    }

    static void TearDownTestSuite()
    {
        instance().type_registry().unregister_type<TestRemoteWidget>();
    }

    void SetUp() override
    {
        ASSERT_TRUE(instance().plugin_registry().find_plugin(PluginId::RemotePlugin));
        server_ = create_remote_server();
        client_ = create_remote_client();
        ASSERT_TRUE(server_);
        ASSERT_TRUE(client_);
        widget_ = instance().create<IObject>(TestRemoteWidget::static_class_id());
        server_.publish("widget", widget_);
    }

    void TearDown() override
    {
        client_ = {};
        server_ = {};
        widget_ = {};
        instance().update();
    }

    void connect_socket()
    {
        const uint16_t port = server_.listen(0);
        ASSERT_NE(0, port);
        ASSERT_TRUE(succeeded(client_.connect("127.0.0.1", port)));
    }

    /** @brief Runs updates until @p f resolves (or about two seconds pass). */
    template <class T>
    T wait(Future<T> f)
    {
        for (int i = 0; i < 2000 && !f.is_ready(); ++i) {
            instance().update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_TRUE(f.is_ready());
        return f.is_ready() ? f.get_result().get_value() : T{};
    }

    /** @brief Runs updates until @p done returns true (or about two seconds pass). */
    template <class F>
    void wait_until(F&& done)
    {
        for (int i = 0; i < 2000 && !done(); ++i) {
            instance().update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_TRUE(done());
    }

    RemoteHandle widget_handle()
    {
        auto roots = wait(client_.roots());
        for (auto& r : roots.items) {
            if (r.name == "widget") {
                return r.object.handle;
            }
        }
        return 0;
    }

    RemoteServer server_;
    RemoteClient client_;
    IObject::Ptr widget_;
};

TEST_F(RemoteTest, RootsOverSocket)
{
    connect_socket();
    auto roots = wait(client_.roots());
    ASSERT_EQ(1u, roots.items.size());
    EXPECT_EQ(string("widget"), roots.items[0].name);
    EXPECT_NE(0u, roots.items[0].object.handle);
    EXPECT_EQ(TestRemoteWidget::static_class_id(), roots.items[0].object.class_uid);
}

TEST_F(RemoteTest, RootsInProcess)
{
    ASSERT_TRUE(succeeded(client_.connect_local(server_)));
    auto roots = wait(client_.roots());
    ASSERT_EQ(1u, roots.items.size());
    EXPECT_EQ(string("widget"), roots.items[0].name);
}

TEST_F(RemoteTest, Describe)
{
    connect_socket();
    const RemoteHandle h = widget_handle();
    const RemoteHandle handles[] = {h};
    auto described = wait(client_.describe(handles));
    ASSERT_EQ(1u, described.items.size());
    const auto& d = described.items[0];
    EXPECT_TRUE(d.alive);
    EXPECT_EQ(string("TestRemoteWidget"), d.class_name);

    const RemoteProperty* width = nullptr;
    const RemoteProperty* label = nullptr;
    const RemoteProperty* version = nullptr;
    for (auto& p : d.properties) {
        if (p.name == "width") width = &p;
        if (p.name == "label") label = &p;
        if (p.name == "version") version = &p;
    }
    ASSERT_NE(nullptr, width);
    ASSERT_NE(nullptr, label);
    ASSERT_NE(nullptr, version);
    float w = 0.f;
    EXPECT_TRUE(read_remote_value(width->value, w));
    EXPECT_FLOAT_EQ(10.f, w);
    string text;
    EXPECT_TRUE(read_remote_value(label->value, text));
    EXPECT_EQ(string("start"), text);
    EXPECT_NE(0u, version->flags & ObjectFlags::ReadOnly);

    const RemoteHandle dead[] = {9999999};
    auto missing = wait(client_.describe(dead));
    ASSERT_EQ(1u, missing.items.size());
    EXPECT_FALSE(missing.items[0].alive);
}

TEST_F(RemoteTest, GetAndSet)
{
    connect_socket();
    const RemoteHandle h = widget_handle();

    const RemotePropertyValue values[] = {
        {h, "width", {}, make_remote_value(42.5f)},
        {h, "label", {}, make_remote_value(string_view("remote"))},
        {h, "version", {}, make_remote_value(7)},
        {h, "missing", {}, make_remote_value(1)},
    };
    auto results = wait(client_.set(values));
    ASSERT_EQ(4u, results.items.size());
    EXPECT_TRUE(succeeded(results.items[0]));
    EXPECT_TRUE(succeeded(results.items[1]));
    EXPECT_FALSE(succeeded(results.items[2])); // read-only
    EXPECT_FALSE(succeeded(results.items[3]));

    auto state = read_state<ITestRemoteWidget>(widget_);
    ASSERT_TRUE(state);
    EXPECT_FLOAT_EQ(42.5f, state->width);
    EXPECT_EQ(string("remote"), state->label);

    const RemotePropertyName names[] = {{h, "width"}, {h, "label"}};
    auto got = wait(client_.get(names));
    ASSERT_EQ(2u, got.items.size());
    float w = 0.f;
    EXPECT_TRUE(read_remote_value(got.items[0].value, w));
    EXPECT_FLOAT_EQ(42.5f, w);
    string text;
    EXPECT_TRUE(read_remote_value(got.items[1].value, text));
    EXPECT_EQ(string("remote"), text);
}

TEST_F(RemoteTest, ObjectReference)
{
    connect_socket();
    const RemoteHandle h = widget_handle();
    const Uid classes[] = {TestRemoteWidget::static_class_id()};
    auto created = wait(client_.create(classes));
    ASSERT_EQ(1u, created.items.size());
    const RemoteHandle other = created.items[0].handle;
    ASSERT_NE(0u, other);

    const RemotePropertyValue values[] = {{h, "target", {}, make_remote_reference(other)}};
    auto results = wait(client_.set(values));
    ASSERT_EQ(1u, results.items.size());
    EXPECT_TRUE(succeeded(results.items[0]));

    const RemotePropertyName names[] = {{h, "target"}};
    auto got = wait(client_.get(names));
    ASSERT_EQ(1u, got.items.size());
    EXPECT_EQ(RemoteValue::Form::Object, got.items[0].value.form);
    EXPECT_EQ(other, got.items[0].value.object);
}

TEST_F(RemoteTest, Changes)
{
    connect_socket();
    const RemoteHandle h = widget_handle();
    vector<RemotePropertyValue> changes;
    ScopedHandler on_changed(client_.as_ptr<IRemoteClient>()->on_changed(),
                             [&](const RemotePropertyValues& c) {
                                 for (auto& v : c.items) {
                                     changes.push_back(v);
                                 }
                             });
    const RemoteHandle handles[] = {h};
    auto subscribed = wait(client_.subscribe(handles));
    ASSERT_EQ(1u, subscribed.items.size());
    EXPECT_TRUE(succeeded(subscribed.items[0]));

    // A change to one property.
    if (auto prop = interface_cast<IMetadata>(widget_)->get_property("width")) {
        prop->set_value(*Any<float>(3.f).get_any_interface());
    }
    wait_until([&] {
        for (auto& v : changes) {
            float w = 0.f;
            if (v.name == "width" && read_remote_value(v.value, w) && w == 3.f) {
                return true;
            }
        }
        return false;
    });

    // A whole-interface write reports every property of the interface.
    changes.clear();
    write_state<ITestRemoteWidget>(widget_, [](ITestRemoteWidget::State& s) { s.label = "batch"; });
    wait_until([&] {
        bool label = false;
        bool width = false;
        for (auto& v : changes) {
            string text;
            label = label || (v.name == "label" && read_remote_value(v.value, text) && text == "batch");
            width = width || v.name == "width";
        }
        return label && width;
    });
}

TEST_F(RemoteTest, Released)
{
    connect_socket();
    const Uid classes[] = {TestRemoteWidget::static_class_id()};
    auto created = wait(client_.create(classes));
    ASSERT_EQ(1u, created.items.size());
    const RemoteHandle h = created.items[0].handle;

    vector<RemoteHandle> released;
    ScopedHandler on_released(client_.as_ptr<IRemoteClient>()->on_released(),
                              [&](const RemoteHandles& r) {
                                  for (auto x : r.items) {
                                      released.push_back(x);
                                  }
                              });
    const RemoteHandle handles[] = {h};
    wait(client_.subscribe(handles));
    auto results = wait(client_.release(handles));
    ASSERT_EQ(1u, results.items.size());
    EXPECT_TRUE(succeeded(results.items[0]));
    wait_until([&] { return !released.empty() && released[0] == h; });
}

TEST_F(RemoteTest, Instances)
{
    connect_socket();
    auto extra = instance().create<IObject>(TestRemoteWidget::static_class_id());
    const Uid classes[] = {TestRemoteWidget::static_class_id()};
    auto found = wait(client_.instances(classes));
    EXPECT_GE(found.items.size(), 2u);
    for (auto& o : found.items) {
        EXPECT_EQ(TestRemoteWidget::static_class_id(), o.class_uid);
    }
}

TEST_F(RemoteTest, HierarchyEdits)
{
    connect_socket();
    auto hierarchy = create_hierarchy();
    server_.publish("tree", hierarchy);
    RemoteHandle tree = 0;
    for (auto& r : wait(client_.roots()).items) {
        if (r.name == "tree") {
            tree = r.object.handle;
        }
    }
    ASSERT_NE(0u, tree);

    const Uid classes[] = {TestRemoteWidget::static_class_id(), TestRemoteWidget::static_class_id(),
                           TestRemoteWidget::static_class_id()};
    auto created = wait(client_.create(classes));
    ASSERT_EQ(3u, created.items.size());
    const RemoteHandle root = created.items[0].handle;
    const RemoteHandle a = created.items[1].handle;
    const RemoteHandle b = created.items[2].handle;

    const RemoteInsert inserts[] = {{tree, 0, 0, root}, {tree, root, 0, b}, {tree, root, 0, a}};
    auto inserted = wait(client_.insert(inserts));
    ASSERT_EQ(3u, inserted.items.size());
    for (auto r : inserted.items) {
        EXPECT_TRUE(succeeded(r));
    }

    const RemoteNode nodes[] = {{tree, root}};
    auto children = wait(client_.children(nodes));
    ASSERT_EQ(1u, children.items.size());
    ASSERT_EQ(2u, children.items[0].children.size());
    EXPECT_EQ(a, children.items[0].children[0].handle);
    EXPECT_EQ(b, children.items[0].children[1].handle);

    const RemoteHandle described[] = {tree};
    auto d = wait(client_.describe(described));
    ASSERT_EQ(1u, d.items.size());
    EXPECT_TRUE(d.items[0].is_hierarchy);
    EXPECT_EQ(root, d.items[0].hierarchy_root.handle);

    const RemoteNode removed[] = {{tree, a}};
    auto results = wait(client_.remove(removed));
    ASSERT_EQ(1u, results.items.size());
    EXPECT_TRUE(succeeded(results.items[0]));
    auto after = wait(client_.children(nodes));
    ASSERT_EQ(1u, after.items.size());
    ASSERT_EQ(1u, after.items[0].children.size());
    EXPECT_EQ(b, after.items[0].children[0].handle);
}

TEST_F(RemoteTest, AttachmentsAndDynamicProperties)
{
    connect_socket();
    const RemoteHandle h = widget_handle();
    const Uid classes[] = {TestRemoteWidget::static_class_id()};
    const RemoteHandle attachment = wait(client_.create(classes)).items[0].handle;

    const RemoteAttachment items[] = {{h, attachment}};
    auto attached = wait(client_.add_attachment(items));
    ASSERT_EQ(1u, attached.items.size());
    EXPECT_TRUE(succeeded(attached.items[0]));

    const RemotePropertyValue added[] = {{h, "extra", {}, make_remote_value(1.5f)}};
    auto results = wait(client_.add_property(added));
    ASSERT_EQ(1u, results.items.size());
    EXPECT_TRUE(succeeded(results.items[0]));

    const RemoteHandle handles[] = {h};
    auto d = wait(client_.describe(handles));
    ASSERT_EQ(1u, d.items.size());
    bool has_attachment = false;
    for (auto& a : d.items[0].attachments) {
        has_attachment = has_attachment || a.handle == attachment;
    }
    EXPECT_TRUE(has_attachment);
    bool has_extra = false;
    for (auto& p : d.items[0].properties) {
        float v = 0.f;
        has_extra = has_extra || (p.name == "extra" && read_remote_value(p.value, v) && v == 1.5f);
    }
    EXPECT_TRUE(has_extra);
}

TEST_F(RemoteTest, Disconnect)
{
    connect_socket();
    bool disconnected = false;
    ScopedHandler on_disconnected(client_.as_ptr<IRemoteClient>()->on_disconnected(),
                                  [&]() { disconnected = true; });
    EXPECT_TRUE(client_.is_connected());
    server_.close();
    wait_until([&] { return disconnected; });
    EXPECT_FALSE(client_.is_connected());

    // Requests after the disconnect resolve empty.
    auto roots = wait(client_.roots());
    EXPECT_TRUE(roots.items.empty());
}

} // namespace velk
