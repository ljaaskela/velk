#include "remote_client.h"

#include "driver.h"
#include "remote_server.h"

#include <velk/api/event.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_metadata.h>

namespace velk::remote {

namespace {

vector<ReturnValue> read_results(Reader& r)
{
    vector<ReturnValue> out;
    for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
        out.push_back(static_cast<ReturnValue>(r.i32()));
    }
    return out;
}

vector<RemoteObject> read_objects(Reader& r)
{
    vector<RemoteObject> out;
    for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
        out.push_back(r.object());
    }
    return out;
}

vector<RemoteMember> read_members(Reader& r)
{
    vector<RemoteMember> out;
    for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
        RemoteMember m;
        m.name = r.str();
        m.uid = r.uid();
        out.push_back(std::move(m));
    }
    return out;
}

RemoteDescription read_description(Reader& r)
{
    RemoteDescription d;
    d.handle = r.u64();
    d.alive = r.u8() != 0;
    if (!d.alive) {
        return d;
    }
    d.class_uid = r.uid();
    d.class_name = r.str();
    d.interfaces = read_members(r);
    for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
        RemoteProperty p;
        p.name = r.str();
        p.type = r.uid();
        p.interface_uid = r.uid();
        p.flags = r.u32();
        p.status = static_cast<ReturnValue>(r.i32());
        r.value(p.value);
        d.properties.push_back(std::move(p));
    }
    d.events = read_members(r);
    d.functions = read_members(r);
    d.attachments = read_objects(r);
    d.is_hierarchy = r.u8() != 0;
    d.hierarchy_root = r.object();
    return d;
}

vector<RemotePropertyValue> read_property_values(Reader& r)
{
    vector<RemotePropertyValue> out;
    for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
        RemotePropertyValue v;
        v.object = r.u64();
        v.name = r.str();
        v.status = static_cast<ReturnValue>(r.i32());
        r.value(v.value);
        out.push_back(std::move(v));
    }
    return out;
}

void write_handles(Writer& w, array_view<RemoteHandle> handles)
{
    w.u32(static_cast<uint32_t>(handles.size()));
    for (auto h : handles) {
        w.u64(h);
    }
}

void write_uids(Writer& w, array_view<Uid> uids)
{
    w.u32(static_cast<uint32_t>(uids.size()));
    for (auto u : uids) {
        w.uid(u);
    }
}

void write_nodes(Writer& w, array_view<RemoteNode> nodes)
{
    w.u32(static_cast<uint32_t>(nodes.size()));
    for (auto& n : nodes) {
        w.u64(n.hierarchy);
        w.u64(n.node);
    }
}

void write_property_values(Writer& w, array_view<RemotePropertyValue> values)
{
    w.u32(static_cast<uint32_t>(values.size()));
    for (auto& v : values) {
        w.u64(v.object);
        w.str(v.name);
        w.value(v.value);
    }
}

void write_attachments(Writer& w, array_view<RemoteAttachment> items)
{
    w.u32(static_cast<uint32_t>(items.size()));
    for (auto& a : items) {
        w.u64(a.target);
        w.u64(a.attachment);
    }
}

/// Resolves @p promise with the empty reply of @p op.
void resolve_empty(Op op, Promise& promise)
{
    switch (op) {
    case Op::Roots:
        promise.set_value(RemoteRoots{});
        break;
    case Op::Instances:
    case Op::Create:
        promise.set_value(RemoteObjects{});
        break;
    case Op::Describe:
        promise.set_value(RemoteDescriptions{});
        break;
    case Op::Children:
        promise.set_value(RemoteChildrenList{});
        break;
    case Op::Get:
        promise.set_value(RemotePropertyValues{});
        break;
    case Op::Call:
        promise.set_value(RemoteCallResults{});
        break;
    default:
        promise.set_value(RemoteResults{});
        break;
    }
}

} // namespace

RemoteClient::RemoteClient()
{
    if (auto* driver = find_driver()) {
        driver->add_client(this);
    }
}

RemoteClient::~RemoteClient()
{
    if (auto* driver = find_driver()) {
        driver->remove_client(this);
    }
    if (connection_) {
        connection_->close();
    }
}

ReturnValue RemoteClient::attach(std::shared_ptr<Connection> connection)
{
    disconnect();
    if (!connection) {
        return ReturnValue::Fail;
    }
    connection_ = std::move(connection);
    was_connected_ = true;
    Writer w = begin(Op::Hello);
    w.u32(kProtocolVersion);
    send(w);
    return ReturnValue::Success;
}

ReturnValue RemoteClient::connect(string_view host, uint16_t port)
{
    const SocketHandle s = socket_connect(host, port);
    if (s == kNoSocket) {
        return ReturnValue::Fail;
    }
    return attach(std::make_shared<SocketConnection>(s));
}

ReturnValue RemoteClient::connect_local(IRemoteServer& server)
{
    auto* internal = interface_cast<IRemoteServerInternal>(&server);
    return internal ? attach(internal->connect_local()) : ReturnValue::Fail;
}

void RemoteClient::disconnect()
{
    if (connection_) {
        connection_->close();
        pump();
    }
}

bool RemoteClient::is_connected() const
{
    return connection_ && connection_->is_open();
}

Writer RemoteClient::begin(Op op)
{
    return Writer({Kind::Request, op, next_id_++});
}

IFuture::Ptr RemoteClient::send(Writer& w)
{
    Reader header(w.message());
    IFuture::Ptr future = instance().create_future();
    Pending pending{header.header().op, Promise(future)};
    if (!is_connected()) {
        resolve_empty(pending.op, pending.promise);
    } else {
        pending_.emplace(header.header().id, pending);
        connection_->send(w.message());
    }
    return future;
}

IFuture::Ptr RemoteClient::roots()
{
    Writer w = begin(Op::Roots);
    return send(w);
}

IFuture::Ptr RemoteClient::instances(array_view<Uid> classes)
{
    Writer w = begin(Op::Instances);
    write_uids(w, classes);
    return send(w);
}

IFuture::Ptr RemoteClient::describe(array_view<RemoteHandle> objects)
{
    Writer w = begin(Op::Describe);
    write_handles(w, objects);
    return send(w);
}

IFuture::Ptr RemoteClient::children(array_view<RemoteNode> nodes)
{
    Writer w = begin(Op::Children);
    write_nodes(w, nodes);
    return send(w);
}

IFuture::Ptr RemoteClient::get(array_view<RemotePropertyName> properties)
{
    Writer w = begin(Op::Get);
    w.u32(static_cast<uint32_t>(properties.size()));
    for (auto& p : properties) {
        w.u64(p.object);
        w.str(p.name);
    }
    return send(w);
}

IFuture::Ptr RemoteClient::set(array_view<RemotePropertyValue> values)
{
    Writer w = begin(Op::Set);
    write_property_values(w, values);
    return send(w);
}

IFuture::Ptr RemoteClient::subscribe(array_view<RemoteHandle> objects)
{
    Writer w = begin(Op::Subscribe);
    write_handles(w, objects);
    return send(w);
}

IFuture::Ptr RemoteClient::unsubscribe(array_view<RemoteHandle> objects)
{
    Writer w = begin(Op::Unsubscribe);
    write_handles(w, objects);
    return send(w);
}

IFuture::Ptr RemoteClient::create(array_view<Uid> classes)
{
    Writer w = begin(Op::Create);
    write_uids(w, classes);
    return send(w);
}

IFuture::Ptr RemoteClient::release(array_view<RemoteHandle> objects)
{
    Writer w = begin(Op::Release);
    write_handles(w, objects);
    return send(w);
}

IFuture::Ptr RemoteClient::add_attachment(array_view<RemoteAttachment> attachments)
{
    Writer w = begin(Op::AddAttachment);
    write_attachments(w, attachments);
    return send(w);
}

IFuture::Ptr RemoteClient::remove_attachment(array_view<RemoteAttachment> attachments)
{
    Writer w = begin(Op::RemoveAttachment);
    write_attachments(w, attachments);
    return send(w);
}

IFuture::Ptr RemoteClient::add_property(array_view<RemotePropertyValue> properties)
{
    Writer w = begin(Op::AddProperty);
    write_property_values(w, properties);
    return send(w);
}

IFuture::Ptr RemoteClient::insert(array_view<RemoteInsert> inserts)
{
    Writer w = begin(Op::Insert);
    w.u32(static_cast<uint32_t>(inserts.size()));
    for (auto& i : inserts) {
        w.u64(i.hierarchy);
        w.u64(i.parent);
        w.u32(i.index);
        w.u64(i.child);
    }
    return send(w);
}

IFuture::Ptr RemoteClient::remove(array_view<RemoteNode> nodes)
{
    Writer w = begin(Op::Remove);
    write_nodes(w, nodes);
    return send(w);
}

IFuture::Ptr RemoteClient::call(array_view<RemoteCall> calls)
{
    Writer w = begin(Op::Call);
    w.u32(static_cast<uint32_t>(calls.size()));
    for (auto& c : calls) {
        w.u64(c.object);
        w.str(c.function);
        w.u32(static_cast<uint32_t>(c.args.size()));
        for (auto& a : c.args) {
            w.value(a);
        }
    }
    return send(w);
}

void RemoteClient::resolve(Reader& r, Pending& pending)
{
    switch (pending.op) {
    case Op::Hello: {
        const bool match = r.u8() != 0;
        const uint32_t version = r.u32();
        if (!match) {
            VELK_LOG(E, "velk_remote: the server speaks protocol %u, this client %u", version, kProtocolVersion);
            connection_->close();
        }
        pending.promise.complete();
        return;
    }
    case Op::Roots: {
        RemoteRoots roots;
        for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
            RemoteRoot root;
            root.name = r.str();
            root.object = r.object();
            roots.items.push_back(std::move(root));
        }
        pending.promise.set_value(roots);
        return;
    }
    case Op::Instances:
    case Op::Create:
        pending.promise.set_value(RemoteObjects{read_objects(r)});
        return;
    case Op::Describe: {
        RemoteDescriptions out;
        for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
            out.items.push_back(read_description(r));
        }
        pending.promise.set_value(out);
        return;
    }
    case Op::Children: {
        RemoteChildrenList out;
        for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
            RemoteChildren c;
            c.node.hierarchy = r.u64();
            c.node.node = r.u64();
            c.status = static_cast<ReturnValue>(r.i32());
            c.children = read_objects(r);
            out.items.push_back(std::move(c));
        }
        pending.promise.set_value(out);
        return;
    }
    case Op::Get:
        pending.promise.set_value(RemotePropertyValues{read_property_values(r)});
        return;
    case Op::Call: {
        RemoteCallResults out;
        for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
            RemoteCallResult c;
            c.status = static_cast<ReturnValue>(r.i32());
            r.value(c.value);
            out.items.push_back(std::move(c));
        }
        pending.promise.set_value(out);
        return;
    }
    default:
        pending.promise.set_value(RemoteResults{read_results(r)});
        return;
    }
}

void RemoteClient::fail_pending()
{
    auto pending = std::move(pending_);
    pending_.clear();
    for (auto& [id, p] : pending) {
        resolve_empty(p.op, p.promise);
    }
}

void RemoteClient::pump()
{
    if (!connection_) {
        return;
    }
    // Hold on to the connection: an event handler below may disconnect or connect again.
    auto connection = connection_;
    vector<Message> messages;
    connection->take(messages);
    const IInterface* self = get_interface(IInterface::UID);
    for (auto& m : messages) {
        Reader r(m);
        const Header& h = r.header();
        if (!r.ok()) {
            connection->close();
            break;
        }
        if (h.kind == Kind::Reply) {
            auto it = pending_.find(h.id);
            if (it == pending_.end()) {
                continue;
            }
            Pending pending = it->second;
            pending_.erase(it);
            resolve(r, pending);
        } else if (h.kind == Kind::Notification && h.op == Op::Changed) {
            RemotePropertyValues changes{read_property_values(r)};
            if (r.ok()) {
                invoke_event(self, "on_changed", changes);
            }
        } else if (h.kind == Kind::Notification && h.op == Op::Released) {
            RemoteHandles released;
            for (uint32_t n = r.count(), i = 0; i < n && r.ok(); ++i) {
                released.items.push_back(r.u64());
            }
            if (r.ok()) {
                invoke_event(self, "on_released", released);
            }
        }
    }
    if (!connection->is_open() && connection == connection_) {
        connection_.reset();
        fail_pending();
        if (was_connected_) {
            was_connected_ = false;
            invoke_event(self, "on_disconnected");
        }
    }
}

} // namespace velk::remote
