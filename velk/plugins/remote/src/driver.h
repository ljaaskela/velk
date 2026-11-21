#ifndef VELK_REMOTE_DRIVER_H
#define VELK_REMOTE_DRIVER_H

#include <velk/interface/intf_interface.h>

namespace velk::remote {

class RemoteServer;
class RemoteClient;

/** @brief How servers and clients sign up to be driven by the plugin's update hooks. */
class IRemoteDriver : public Interface<IRemoteDriver>
{
public:
    virtual void add_server(RemoteServer* server) = 0;
    virtual void remove_server(RemoteServer* server) = 0;
    virtual void add_client(RemoteClient* client) = 0;
    virtual void remove_client(RemoteClient* client) = 0;
};

/** @brief The loaded plugin's driver, or null. */
IRemoteDriver* find_driver();

} // namespace velk::remote

#endif // VELK_REMOTE_DRIVER_H
