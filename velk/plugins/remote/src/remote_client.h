#ifndef VELK_REMOTE_CLIENT_H
#define VELK_REMOTE_CLIENT_H

#include "codec.h"
#include "connection.h"

#include <velk/api/future.h>
#include <velk/ext/object.h>
#include <velk/plugins/remote/interface/intf_remote.h>
#include <velk/plugins/remote/plugin.h>

#include <memory>
#include <unordered_map>

namespace velk::remote {

class RemoteClient final : public ::velk::ext::Object<RemoteClient, IRemoteClient>
{
public:
    VELK_CLASS_UID(ClassId::RemoteClient, "RemoteClient");

    RemoteClient();
    ~RemoteClient() override;

    // IRemoteClient
    ReturnValue connect(string_view host, uint16_t port) override;
    ReturnValue connect_local(IRemoteServer& server) override;
    void disconnect() override;
    bool is_connected() const override;

    IFuture::Ptr roots() override;
    IFuture::Ptr instances(array_view<Uid> classes) override;
    IFuture::Ptr describe(array_view<RemoteHandle> objects) override;
    IFuture::Ptr children(array_view<RemoteNode> nodes) override;
    IFuture::Ptr get(array_view<RemotePropertyName> properties) override;
    IFuture::Ptr set(array_view<RemotePropertyValue> values) override;
    IFuture::Ptr subscribe(array_view<RemoteHandle> objects) override;
    IFuture::Ptr unsubscribe(array_view<RemoteHandle> objects) override;
    IFuture::Ptr create(array_view<Uid> classes) override;
    IFuture::Ptr release(array_view<RemoteHandle> objects) override;
    IFuture::Ptr add_attachment(array_view<RemoteAttachment> attachments) override;
    IFuture::Ptr remove_attachment(array_view<RemoteAttachment> attachments) override;
    IFuture::Ptr add_property(array_view<RemotePropertyValue> properties) override;
    IFuture::Ptr insert(array_view<RemoteInsert> inserts) override;
    IFuture::Ptr remove(array_view<RemoteNode> nodes) override;
    IFuture::Ptr call(array_view<RemoteCall> calls) override;

    /** @brief Resolves replies and fires events for the messages received. */
    void pump();

private:
    struct Pending
    {
        Op op;
        Promise promise;
    };

    ReturnValue attach(std::shared_ptr<Connection> connection);
    Writer begin(Op op);
    IFuture::Ptr send(Writer& w);
    void resolve(Reader& r, Pending& pending);
    /** @brief Resolves every outstanding request with an empty result. */
    void fail_pending();

    std::shared_ptr<Connection> connection_;
    uint32_t next_id_ = 1;
    std::unordered_map<uint32_t, Pending> pending_;
    bool was_connected_ = false;
};

} // namespace velk::remote

#endif // VELK_REMOTE_CLIENT_H
