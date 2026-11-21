#ifndef VELK_PLUGINS_REMOTE_INTF_REMOTE_H
#define VELK_PLUGINS_REMOTE_INTF_REMOTE_H

#include <velk/array_view.h>
#include <velk/interface/intf_future.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>
#include <velk/plugins/remote/types.h>

namespace velk {

/**
 * @brief Serves a program's objects to remote clients.
 *
 * Clients start from the published roots, or find objects by class. Requests
 * run on the thread that created the server, during instance().update().
 */
class IRemoteServer : public Interface<IRemoteServer>
{
public:
    /** @brief Makes @p object visible to clients as a root named @p name, replacing one of the same name. */
    virtual ReturnValue publish(string_view name, const IObject::Ptr& object) = 0;

    /** @brief Removes a root. */
    virtual ReturnValue unpublish(string_view name) = 0;

    /**
     * @brief Accepts clients on @p port of localhost.
     * @param port The port, or 0 for a free one.
     * @return The port listened on, or 0 on failure.
     */
    virtual uint16_t listen(uint16_t port) = 0;

    /** @brief Stops accepting clients and disconnects the connected ones. */
    virtual void close() = 0;
};

/**
 * @brief Reads and edits the objects of a program that runs an IRemoteServer.
 *
 * Every request takes a list and is answered with one future, so one round
 * trip does many reads or writes. Futures resolve and events fire on the
 * thread that created the client, during instance().update().
 */
class IRemoteClient : public Interface<IRemoteClient>
{
public:
    VELK_INTERFACE(
        (EVT, on_changed, (RemotePropertyValues, changes)), ///< Property changes of subscribed objects.
        (EVT, on_released, (RemoteHandles, objects)),       ///< Subscribed objects that no longer exist.
        (EVT, on_disconnected)                              ///< The connection closed.
    )

    /** @brief Connects to a server listening on @p host and @p port. */
    virtual ReturnValue connect(string_view host, uint16_t port) = 0;

    /** @brief Connects to a server in this process. */
    virtual ReturnValue connect_local(IRemoteServer& server) = 0;

    /** @brief Closes the connection. */
    virtual void disconnect() = 0;

    /** @brief Returns true while connected. */
    virtual bool is_connected() const = 0;

    /** @brief The published roots. Resolves to RemoteRoots. */
    virtual IFuture::Ptr roots() = 0;

    /**
     * @brief Live objects of @p classes. Resolves to RemoteObjects.
     *
     * Covers objects created from their class's hive, the default for classes
     * under 1 KB; objects created with CreationPolicy::Alloc are not listed.
     */
    virtual IFuture::Ptr instances(array_view<Uid> classes) = 0;

    /** @brief Class, interfaces, properties with values, events, functions, attachments. Resolves to RemoteDescriptions. */
    virtual IFuture::Ptr describe(array_view<RemoteHandle> objects) = 0;

    /** @brief The children of hierarchy nodes. Resolves to RemoteChildrenList. */
    virtual IFuture::Ptr children(array_view<RemoteNode> nodes) = 0;

    /** @brief Property values. Resolves to RemotePropertyValues. */
    virtual IFuture::Ptr get(array_view<RemotePropertyName> properties) = 0;

    /** @brief Sets property values, all in one server update. Resolves to RemoteResults. */
    virtual IFuture::Ptr set(array_view<RemotePropertyValue> values) = 0;

    /**
     * @brief Reports the objects' property changes through on_changed, and their end through on_released.
     *
     * A whole-interface state write is reported as that interface's properties.
     * Writes into state that skip the property system are not seen. Resolves to RemoteResults.
     */
    virtual IFuture::Ptr subscribe(array_view<RemoteHandle> objects) = 0;

    /** @brief Stops reporting the objects' changes. Resolves to RemoteResults. */
    virtual IFuture::Ptr unsubscribe(array_view<RemoteHandle> objects) = 0;

    /**
     * @brief Creates objects. Resolves to RemoteObjects, with handle 0 where creation failed.
     *
     * The server keeps created objects alive until they are released.
     */
    virtual IFuture::Ptr create(array_view<Uid> classes) = 0;

    /** @brief Lets the server drop the objects this client created. Resolves to RemoteResults. */
    virtual IFuture::Ptr release(array_view<RemoteHandle> objects) = 0;

    /** @brief Resolves to RemoteResults. */
    virtual IFuture::Ptr add_attachment(array_view<RemoteAttachment> attachments) = 0;

    /** @brief Resolves to RemoteResults. */
    virtual IFuture::Ptr remove_attachment(array_view<RemoteAttachment> attachments) = 0;

    /** @brief Adds dynamic properties, typed by their values. Resolves to RemoteResults. */
    virtual IFuture::Ptr add_property(array_view<RemotePropertyValue> properties) = 0;

    /** @brief Inserts objects into hierarchies. Resolves to RemoteResults. */
    virtual IFuture::Ptr insert(array_view<RemoteInsert> inserts) = 0;

    /** @brief Removes nodes, with their subtrees, from hierarchies. Resolves to RemoteResults. */
    virtual IFuture::Ptr remove(array_view<RemoteNode> nodes) = 0;

    /** @brief Calls functions on objects, in order, in one server update. Resolves to RemoteCallResults. */
    virtual IFuture::Ptr call(array_view<RemoteCall> calls) = 0;
};

} // namespace velk

#endif // VELK_PLUGINS_REMOTE_INTF_REMOTE_H
