#ifndef VELK_PLUGINS_REMOTE_API_REMOTE_H
#define VELK_PLUGINS_REMOTE_API_REMOTE_H

#include <velk/api/future.h>
#include <velk/api/object.h>
#include <velk/api/velk.h>
#include <velk/plugins/remote/interface/intf_remote.h>
#include <velk/plugins/remote/plugin.h>

namespace velk {

/**
 * @brief Convenience wrapper around IRemoteServer.
 *
 *   auto server = create_remote_server();
 *   server.publish("scene", scene_hierarchy);
 *   uint16_t port = server.listen(0);
 */
class RemoteServer : public Object
{
public:
    RemoteServer() = default;
    explicit RemoteServer(IObject::Ptr obj) : Object(check_object<IRemoteServer>(obj)) {}

    operator IRemoteServer::Ptr() const { return as_ptr<IRemoteServer>(); }

    ReturnValue publish(string_view name, const IObject::Ptr& object)
    {
        return with<IRemoteServer>([&](auto& s) { return s.publish(name, object); });
    }
    ReturnValue unpublish(string_view name)
    {
        return with<IRemoteServer>([&](auto& s) { return s.unpublish(name); });
    }
    uint16_t listen(uint16_t port) { return with<IRemoteServer>([&](auto& s) { return s.listen(port); }); }
    void close() { with<IRemoteServer>([](auto& s) { s.close(); }); }
};

/**
 * @brief Convenience wrapper around IRemoteClient with typed futures.
 *
 *   auto client = create_remote_client();
 *   client.connect("127.0.0.1", port);
 *   client.roots().then([](const RemoteRoots& roots) { ... });
 */
class RemoteClient : public Object
{
public:
    RemoteClient() = default;
    explicit RemoteClient(IObject::Ptr obj) : Object(check_object<IRemoteClient>(obj)) {}

    operator IRemoteClient::Ptr() const { return as_ptr<IRemoteClient>(); }

    ReturnValue connect(string_view host, uint16_t port)
    {
        return with<IRemoteClient>([&](auto& c) { return c.connect(host, port); });
    }
    ReturnValue connect_local(const RemoteServer& server)
    {
        auto s = server.as_ptr<IRemoteServer>();
        return s ? with<IRemoteClient>([&](auto& c) { return c.connect_local(*s); }) : ReturnValue::Fail;
    }
    void disconnect() { with<IRemoteClient>([](auto& c) { c.disconnect(); }); }
    bool is_connected() const { return with<IRemoteClient>([](auto& c) { return c.is_connected(); }); }

    Future<RemoteRoots> roots() { return call<RemoteRoots>([](auto& c) { return c.roots(); }); }
    Future<RemoteObjects> instances(array_view<Uid> classes)
    {
        return call<RemoteObjects>([&](auto& c) { return c.instances(classes); });
    }
    Future<RemoteDescriptions> describe(array_view<RemoteHandle> objects)
    {
        return call<RemoteDescriptions>([&](auto& c) { return c.describe(objects); });
    }
    Future<RemoteChildrenList> children(array_view<RemoteNode> nodes)
    {
        return call<RemoteChildrenList>([&](auto& c) { return c.children(nodes); });
    }
    Future<RemotePropertyValues> get(array_view<RemotePropertyName> properties)
    {
        return call<RemotePropertyValues>([&](auto& c) { return c.get(properties); });
    }
    Future<RemoteResults> set(array_view<RemotePropertyValue> values)
    {
        return call<RemoteResults>([&](auto& c) { return c.set(values); });
    }
    Future<RemoteResults> subscribe(array_view<RemoteHandle> objects)
    {
        return call<RemoteResults>([&](auto& c) { return c.subscribe(objects); });
    }
    Future<RemoteResults> unsubscribe(array_view<RemoteHandle> objects)
    {
        return call<RemoteResults>([&](auto& c) { return c.unsubscribe(objects); });
    }
    Future<RemoteObjects> create(array_view<Uid> classes)
    {
        return call<RemoteObjects>([&](auto& c) { return c.create(classes); });
    }
    Future<RemoteResults> release(array_view<RemoteHandle> objects)
    {
        return call<RemoteResults>([&](auto& c) { return c.release(objects); });
    }
    Future<RemoteResults> add_attachment(array_view<RemoteAttachment> attachments)
    {
        return call<RemoteResults>([&](auto& c) { return c.add_attachment(attachments); });
    }
    Future<RemoteResults> remove_attachment(array_view<RemoteAttachment> attachments)
    {
        return call<RemoteResults>([&](auto& c) { return c.remove_attachment(attachments); });
    }
    Future<RemoteResults> add_property(array_view<RemotePropertyValue> properties)
    {
        return call<RemoteResults>([&](auto& c) { return c.add_property(properties); });
    }
    Future<RemoteResults> insert(array_view<RemoteInsert> inserts)
    {
        return call<RemoteResults>([&](auto& c) { return c.insert(inserts); });
    }
    Future<RemoteResults> remove(array_view<RemoteNode> nodes)
    {
        return call<RemoteResults>([&](auto& c) { return c.remove(nodes); });
    }

private:
    template <class T, class F>
    Future<T> call(F&& fn)
    {
        return Future<T>(with<IRemoteClient>([&](auto& c) { return fn(c); }));
    }
};

/** @brief Loads velk_remote from the plugins folder unless it is already loaded. */
inline void load_remote_plugin()
{
    auto& registry = instance().plugin_registry();
    if (!registry.find_plugin(PluginId::RemotePlugin)) {
        registry.load_plugin("plugin:velk_remote");
    }
}

/** @brief Creates a server. Loads the remote plugin if needed. */
inline RemoteServer create_remote_server()
{
    load_remote_plugin();
    return RemoteServer(instance().create<IObject>(ClassId::RemoteServer));
}

/** @brief Creates a client. Loads the remote plugin if needed. */
inline RemoteClient create_remote_client()
{
    load_remote_plugin();
    return RemoteClient(instance().create<IObject>(ClassId::RemoteClient));
}

} // namespace velk

#endif // VELK_PLUGINS_REMOTE_API_REMOTE_H
