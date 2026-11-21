#ifndef VELK_REMOTE_SERVER_H
#define VELK_REMOTE_SERVER_H

#include "codec.h"
#include "connection.h"
#include "values.h"

#include <velk/ext/object.h>
#include <velk/interface/intf_metadata_observer.h>
#include <velk/plugins/remote/interface/intf_remote.h>
#include <velk/plugins/remote/plugin.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace velk::remote {

class RemoteServer;

/** @brief A call request whose reply waits for its results. */
struct CallBatch
{
    std::shared_ptr<Connection> connection;
    uint32_t id = 0;
    vector<RemoteCallResult> results;
    size_t outstanding = 0; ///< Calls still waiting for their future.
};

/** @brief Sends the reply of @p batch. */
void send_call_reply(const CallBatch& batch);

/** @brief Forwards property changes of subscribed objects to the server. */
class ChangeObserver final : public ::velk::ext::ObjectCore<ChangeObserver, IMetadataObserver>
{
public:
    VELK_CLASS_UID("6b0e8f3a-2c51-4d7e-9a14-0f3b6c2d8e71", "RemoteChangeObserver");

    void set_server(RemoteServer* server) { server_ = server; }
    void on_state_changed(string_view name, IMetadata& owner, Uid interfaceId) override;

private:
    RemoteServer* server_ = nullptr;
};

/** @brief The server side of an in-process connection. */
class IRemoteServerInternal : public Interface<IRemoteServerInternal>
{
public:
    /** @brief Connects an in-process client; returns the client's end. */
    virtual std::shared_ptr<Connection> connect_local() = 0;
};

class RemoteServer final : public ::velk::ext::Object<RemoteServer, IRemoteServer, IRemoteServerInternal>
{
public:
    VELK_CLASS_UID(ClassId::RemoteServer, "RemoteServer");

    RemoteServer();
    ~RemoteServer() override;

    // IRemoteServer
    ReturnValue publish(string_view name, const IObject::Ptr& object) override;
    ReturnValue unpublish(string_view name) override;
    uint16_t listen(uint16_t port) override;
    void close() override;

    // IRemoteServerInternal
    std::shared_ptr<Connection> connect_local() override;

    /** @brief Before deferred tasks: accepts clients and runs their requests. */
    void pump();

    /** @brief After the update: sends property changes and released objects. */
    void flush();

    /** @brief From the observer: a property, or with an empty name an interface's state, changed. */
    void changed(IMetadata& owner, string_view name, Uid interface_uid);

    /** @brief Encodes what a function returned; null means it returns nothing. */
    ReturnValue encode_result(const IAny* value, RemoteValue& out);

private:
    struct Peer
    {
        std::shared_ptr<Connection> connection;
        bool greeted = false;
        std::unordered_set<RemoteHandle> subscribed;
        vector<IObject::Ptr> created; ///< Objects this client created, alive until released.
    };
    struct Watch
    {
        IObject::WeakPtr object;
        size_t peers = 0;
    };
    struct Change
    {
        RemoteHandle handle;
        string name;
        Uid interface_uid;
    };
    struct Root
    {
        string name;
        IObject::Ptr object;
    };

    void run(Peer& peer, const Message& message);
    void drop(Peer& peer);
    void watch(RemoteHandle handle, const IObject::Ptr& object);
    void unwatch(RemoteHandle handle);
    void describe(Writer& w, RemoteHandle handle);
    void collect_changes(const Change& change, vector<RemotePropertyValue>& out);

    HandleTable handles_;
    vector<Root> roots_;
    Listener listener_;
    vector<std::unique_ptr<Peer>> peers_;
    std::unordered_map<RemoteHandle, Watch> watched_;
    vector<Change> changes_;
    IMetadataObserver::Ptr observer_;
};

} // namespace velk::remote

#endif // VELK_REMOTE_SERVER_H
