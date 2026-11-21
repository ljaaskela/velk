#ifndef VELK_REMOTE_PLUGIN_IMPL_H
#define VELK_REMOTE_PLUGIN_IMPL_H

#include "driver.h"

#include <velk/ext/plugin.h>
#include <velk/plugins/remote/plugin.h>
#include <velk/vector.h>

namespace velk::remote {

class RemotePlugin final : public ::velk::ext::Plugin<RemotePlugin, IRemoteDriver>
{
public:
    VELK_PLUGIN_UID(PluginId::RemotePlugin);
    VELK_PLUGIN_NAME("remote");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk& velk) override;
    void pre_update(const IPlugin::PreUpdateInfo& info) override;
    void post_update(const IPlugin::PostUpdateInfo& info) override;

    // IRemoteDriver
    void add_server(RemoteServer* server) override;
    void remove_server(RemoteServer* server) override;
    void add_client(RemoteClient* client) override;
    void remove_client(RemoteClient* client) override;

private:
    vector<RemoteServer*> servers_;
    vector<RemoteClient*> clients_;
    bool sockets_ = false;
};

} // namespace velk::remote

VELK_PLUGIN(velk::remote::RemotePlugin)

#endif // VELK_REMOTE_PLUGIN_IMPL_H
