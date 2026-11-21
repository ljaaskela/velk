#include "remote_plugin.h"

#include "remote_client.h"
#include "remote_server.h"
#include "socket.h"

#include <velk/api/velk.h>
#include <velk/ext/any.h>

#include <algorithm>

namespace velk::remote {

IRemoteDriver* find_driver()
{
    auto plugin = instance().plugin_registry().find_plugin(PluginId::RemotePlugin);
    return plugin ? interface_cast<IRemoteDriver>(plugin.get()) : nullptr;
}

namespace {

template <class T>
bool contains(const vector<T*>& list, T* item)
{
    return std::find(list.begin(), list.end(), item) != list.end();
}

template <class T>
void erase(vector<T*>& list, T* item)
{
    auto it = std::find(list.begin(), list.end(), item);
    if (it != list.end()) {
        list.erase(it);
    }
}

} // namespace

ReturnValue RemotePlugin::initialize(IVelk& velk, PluginConfig& config)
{
    config.enableUpdate = true;
    sockets_ = sockets_start();

    auto rv = register_type<RemoteServer>(velk);
    rv &= register_type<RemoteClient>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteRoots>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteObjects>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteDescriptions>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteChildrenList>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemotePropertyValues>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteResults>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteHandles>>(velk);
    rv &= register_type<::velk::ext::AnyValue<RemoteCallResults>>(velk);
    return rv;
}

ReturnValue RemotePlugin::shutdown(IVelk&)
{
    servers_.clear();
    clients_.clear();
    if (sockets_) {
        sockets_stop();
        sockets_ = false;
    }
    return ReturnValue::Success;
}

void RemotePlugin::pre_update(const IPlugin::PreUpdateInfo&)
{
    // Work on copies: running requests or firing events can create or destroy servers and clients.
    const auto servers = servers_;
    for (auto* s : servers) {
        if (contains(servers_, s)) {
            s->pump();
        }
    }
    const auto clients = clients_;
    for (auto* c : clients) {
        if (contains(clients_, c)) {
            c->pump();
        }
    }
}

void RemotePlugin::post_update(const IPlugin::PostUpdateInfo&)
{
    const auto servers = servers_;
    for (auto* s : servers) {
        if (contains(servers_, s)) {
            s->flush();
        }
    }
}

void RemotePlugin::add_server(RemoteServer* server)
{
    servers_.push_back(server);
}

void RemotePlugin::remove_server(RemoteServer* server)
{
    erase(servers_, server);
}

void RemotePlugin::add_client(RemoteClient* client)
{
    clients_.push_back(client);
}

void RemotePlugin::remove_client(RemoteClient* client)
{
    erase(clients_, client);
}

} // namespace velk::remote
