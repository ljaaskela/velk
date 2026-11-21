#include "remote_server.h"

#include "driver.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_hierarchy.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_type_registry.h>

#include <algorithm>

namespace velk::remote {

namespace {

void write_results(Writer& w, const vector<ReturnValue>& results)
{
    w.u32(static_cast<uint32_t>(results.size()));
    for (auto r : results) {
        w.i32(static_cast<int32_t>(r));
    }
}

template <class T>
T* cast(const IObject::Ptr& object)
{
    return object ? interface_cast<T>(object.get()) : nullptr;
}

} // namespace

void ChangeObserver::on_state_changed(string_view name, IMetadata& owner, Uid interfaceId)
{
    if (server_) {
        server_->changed(owner, name, interfaceId);
    }
}

RemoteServer::RemoteServer()
{
    observer_ = ::velk::ext::make_object<ChangeObserver, IMetadataObserver>();
    if (observer_) {
        static_cast<ChangeObserver*>(observer_.get())->set_server(this);
    }
    if (auto* driver = find_driver()) {
        driver->add_server(this);
    }
}

RemoteServer::~RemoteServer()
{
    if (auto* driver = find_driver()) {
        driver->remove_server(this);
    }
    close();
    for (auto& [handle, w] : watched_) {
        if (auto object = w.object.lock(); object && observer_) {
            if (auto* storage = interface_cast<IObjectStorage>(object.get())) {
                storage->remove_observer(observer_.get());
            }
        }
    }
    if (observer_) {
        static_cast<ChangeObserver*>(observer_.get())->set_server(nullptr);
    }
}

ReturnValue RemoteServer::publish(string_view name, const IObject::Ptr& object)
{
    if (!object) {
        return ReturnValue::InvalidArgument;
    }
    for (auto& root : roots_) {
        if (root.name == name) {
            root.object = object;
            return ReturnValue::Success;
        }
    }
    roots_.push_back({string(name), object});
    return ReturnValue::Success;
}

ReturnValue RemoteServer::unpublish(string_view name)
{
    for (size_t i = 0; i < roots_.size(); ++i) {
        if (roots_[i].name == name) {
            roots_.erase(roots_.begin() + i);
            return ReturnValue::Success;
        }
    }
    return ReturnValue::NothingToDo;
}

uint16_t RemoteServer::listen(uint16_t port)
{
    return listener_.start(port);
}

void RemoteServer::close()
{
    listener_.stop();
    for (auto& peer : peers_) {
        drop(*peer);
    }
    peers_.clear();
}

std::shared_ptr<Connection> RemoteServer::connect_local()
{
    std::shared_ptr<Connection> ours;
    std::shared_ptr<Connection> theirs;
    LocalConnection::make_pair(ours, theirs);
    auto peer = std::make_unique<Peer>();
    peer->connection = ours;
    peers_.push_back(std::move(peer));
    return theirs;
}

void RemoteServer::pump()
{
    vector<std::shared_ptr<Connection>> accepted;
    listener_.take(accepted);
    for (auto& c : accepted) {
        auto peer = std::make_unique<Peer>();
        peer->connection = c;
        peers_.push_back(std::move(peer));
    }

    for (size_t i = 0; i < peers_.size();) {
        Peer& peer = *peers_[i];
        vector<Message> messages;
        peer.connection->take(messages);
        for (auto& m : messages) {
            if (!peer.connection->is_open()) {
                break;
            }
            run(peer, m);
        }
        if (!peer.connection->is_open()) {
            drop(peer);
            peers_.erase(peers_.begin() + i);
        } else {
            ++i;
        }
    }
}

void RemoteServer::drop(Peer& peer)
{
    for (auto handle : peer.subscribed) {
        unwatch(handle);
    }
    peer.subscribed.clear();
    peer.created.clear();
    peer.connection->close();
}

void RemoteServer::watch(RemoteHandle handle, const IObject::Ptr& object)
{
    auto& w = watched_[handle];
    if (w.peers++ == 0) {
        w.object = object;
        if (auto* storage = interface_cast<IObjectStorage>(object.get()); storage && observer_) {
            storage->add_observer(observer_.get());
        }
    }
}

void RemoteServer::unwatch(RemoteHandle handle)
{
    auto it = watched_.find(handle);
    if (it == watched_.end() || --it->second.peers > 0) {
        return;
    }
    if (auto object = it->second.object.lock(); object && observer_) {
        if (auto* storage = interface_cast<IObjectStorage>(object.get())) {
            storage->remove_observer(observer_.get());
        }
    }
    watched_.erase(it);
}

void RemoteServer::changed(IMetadata& owner, string_view name, Uid interface_uid)
{
    auto* object = interface_cast<IObject>(&owner);
    const RemoteHandle handle = object ? handles_.find(object) : 0;
    if (handle && watched_.count(handle)) {
        changes_.push_back({handle, string(name), interface_uid});
    }
}

void RemoteServer::collect_changes(const Change& change, vector<RemotePropertyValue>& out)
{
    auto object = handles_.object_of(change.handle);
    auto* meta = cast<IMetadata>(object);
    if (!meta) {
        return;
    }
    const auto add = [&](string_view name, Uid interface_uid) {
        RemotePropertyValue v;
        v.object = change.handle;
        v.name = string(name);
        v.status = encode_property(*meta, name, interface_uid, handles_, v.value);
        out.push_back(std::move(v));
    };
    if (!change.name.empty()) {
        add(change.name, change.interface_uid);
        return;
    }
    // A whole-interface state write: report every property of that interface.
    for (auto& member : meta->get_static_metadata()) {
        if (member.propertyKind() && member.interfaceInfo && member.interfaceInfo->uid == change.interface_uid) {
            add(member.name, change.interface_uid);
        }
    }
}

void RemoteServer::flush()
{
    handles_.purge();

    // Released: watched objects that died.
    vector<RemoteHandle> released;
    for (auto& [handle, w] : watched_) {
        if (w.object.expired()) {
            released.push_back(handle);
        }
    }
    for (auto handle : released) {
        watched_.erase(handle);
    }

    // Coalesce: one entry per object, property and interface.
    vector<Change> changes;
    for (auto& c : changes_) {
        if (watched_.count(c.handle) == 0) {
            continue;
        }
        bool seen = false;
        for (auto& d : changes) {
            if (d.handle == c.handle && d.name == c.name && d.interface_uid == c.interface_uid) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            changes.push_back(c);
        }
    }
    changes_.clear();

    vector<RemotePropertyValue> values;
    for (auto& c : changes) {
        collect_changes(c, values);
    }

    for (auto& peer : peers_) {
        if (!peer->greeted) {
            continue;
        }
        vector<const RemotePropertyValue*> mine;
        for (auto& v : values) {
            if (peer->subscribed.count(v.object)) {
                mine.push_back(&v);
            }
        }
        if (!mine.empty()) {
            Writer w({Kind::Notification, Op::Changed, 0});
            w.u32(static_cast<uint32_t>(mine.size()));
            for (auto* v : mine) {
                w.u64(v->object);
                w.str(v->name);
                w.i32(static_cast<int32_t>(v->status));
                w.value(v->value);
            }
            peer->connection->send(w.message());
        }
        vector<RemoteHandle> gone;
        for (auto handle : released) {
            if (peer->subscribed.erase(handle)) {
                gone.push_back(handle);
            }
        }
        if (!gone.empty()) {
            Writer w({Kind::Notification, Op::Released, 0});
            w.u32(static_cast<uint32_t>(gone.size()));
            for (auto h : gone) {
                w.u64(h);
            }
            peer->connection->send(w.message());
        }
    }
}

void RemoteServer::describe(Writer& w, RemoteHandle handle)
{
    auto object = handles_.object_of(handle);
    w.u64(handle);
    w.u8(object ? 1 : 0);
    if (!object) {
        return;
    }
    w.uid(object->get_class_uid());
    w.str(object->get_class_name());

    const ClassInfo* info = instance().type_registry().get_class_info(object->get_class_uid());
    if (info) {
        w.u32(static_cast<uint32_t>(info->interfaces.size()));
        for (auto& i : info->interfaces) {
            w.str(i.name);
            w.uid(i.uid);
        }
    } else {
        w.u32(0);
    }

    auto* meta = cast<IMetadata>(object);
    struct Prop
    {
        string_view name;
        Uid type;
        Uid interface_uid;
        uint32_t flags;
    };
    vector<Prop> props;
    vector<const MemberDesc*> events;
    vector<const MemberDesc*> functions;
    vector<PropertyInfo> dynamic;
    if (meta) {
        for (auto& member : meta->get_static_metadata()) {
            const Uid owner = member.interfaceInfo ? member.interfaceInfo->uid : Uid{};
            if (const auto* kind = member.propertyKind()) {
                props.push_back({member.name, kind->typeUid, owner, kind->flags});
            } else if (member.kind == MemberKind::Event) {
                events.push_back(&member);
            } else if (member.kind == MemberKind::Function) {
                functions.push_back(&member);
            }
        }
        for (auto& p : meta->get_properties()) {
            if (p.interfaceUid == IDynamicMetadata::UID) {
                dynamic.push_back(p);
            }
        }
        for (auto& p : dynamic) {
            props.push_back({p.name, p.typeUid, p.interfaceUid, 0});
        }
    }
    w.u32(static_cast<uint32_t>(props.size()));
    for (auto& p : props) {
        RemoteValue value;
        const ReturnValue status = encode_property(*meta, p.name, p.interface_uid, handles_, value);
        w.str(p.name);
        w.uid(p.type);
        w.uid(p.interface_uid);
        w.u32(p.flags);
        w.i32(static_cast<int32_t>(status));
        w.value(value);
    }
    w.u32(static_cast<uint32_t>(events.size()));
    for (auto* e : events) {
        w.str(e->name);
        w.uid(e->interfaceInfo ? e->interfaceInfo->uid : Uid{});
    }
    w.u32(static_cast<uint32_t>(functions.size()));
    for (auto* f : functions) {
        w.str(f->name);
        w.uid(f->interfaceInfo ? f->interfaceInfo->uid : Uid{});
    }

    // Attachments, without the property objects that back dynamic properties.
    vector<IObject::Ptr> attachments;
    if (auto* storage = cast<IObjectStorage>(object)) {
        for (size_t i = 0; i < storage->attachment_count(); ++i) {
            auto a = interface_pointer_cast<IObject>(storage->get_attachment(i));
            if (a && !interface_cast<IProperty>(a.get())) {
                attachments.push_back(a);
            }
        }
    }
    w.u32(static_cast<uint32_t>(attachments.size()));
    for (auto& a : attachments) {
        w.object(handles_.describe(a));
    }

    auto* hierarchy = cast<IHierarchy>(object);
    w.u8(hierarchy ? 1 : 0);
    w.object(hierarchy ? handles_.describe(hierarchy->root()) : RemoteObject{});
}

void RemoteServer::run(Peer& peer, const Message& message)
{
    Reader r(message);
    const Header& h = r.header();
    if (!r.ok() || h.kind != Kind::Request) {
        peer.connection->close();
        return;
    }
    if (!peer.greeted && h.op != Op::Hello) {
        peer.connection->close();
        return;
    }

    Writer w({Kind::Reply, h.op, h.id});
    const auto object_at = [&](RemoteHandle handle) { return handles_.object_of(handle); };

    switch (h.op) {
    case Op::Hello: {
        const uint32_t version = r.u32();
        const bool match = r.ok() && version == kProtocolVersion;
        w.u8(match ? 1 : 0);
        w.u32(kProtocolVersion);
        peer.connection->send(w.message());
        if (!match) {
            peer.connection->close();
        }
        peer.greeted = match;
        return;
    }
    case Op::Roots: {
        w.u32(static_cast<uint32_t>(roots_.size()));
        for (auto& root : roots_) {
            w.str(root.name);
            w.object(handles_.describe(root.object));
        }
        break;
    }
    case Op::Instances: {
        vector<Uid> classes;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            classes.push_back(r.uid());
        }
        if (!r.ok()) {
            break;
        }
        vector<IObject::Ptr> found;
        for (auto uid : classes) {
            instance().type_registry().for_each_instance(uid, &found, [](void* ctx, IObject& o) {
                if (auto self = o.get_self()) {
                    static_cast<vector<IObject::Ptr>*>(ctx)->push_back(self);
                }
                return true;
            });
        }
        w.u32(static_cast<uint32_t>(found.size()));
        for (auto& o : found) {
            w.object(handles_.describe(o));
        }
        break;
    }
    case Op::Describe: {
        vector<RemoteHandle> objects;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            objects.push_back(r.u64());
        }
        if (!r.ok()) {
            break;
        }
        w.u32(static_cast<uint32_t>(objects.size()));
        for (auto handle : objects) {
            describe(w, handle);
        }
        break;
    }
    case Op::Children: {
        vector<RemoteNode> nodes;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemoteNode node;
            node.hierarchy = r.u64();
            node.node = r.u64();
            nodes.push_back(node);
        }
        if (!r.ok()) {
            break;
        }
        w.u32(static_cast<uint32_t>(nodes.size()));
        for (auto& node : nodes) {
            w.u64(node.hierarchy);
            w.u64(node.node);
            const auto owner = object_at(node.hierarchy);
            auto* hierarchy = cast<IHierarchy>(owner);
            auto parent = object_at(node.node);
            if (!hierarchy || !parent || !hierarchy->contains(parent)) {
                w.i32(static_cast<int32_t>(ReturnValue::Fail));
                w.u32(0);
                continue;
            }
            auto children = hierarchy->children_of(parent);
            w.i32(static_cast<int32_t>(ReturnValue::Success));
            w.u32(static_cast<uint32_t>(children.size()));
            for (auto& c : children) {
                w.object(handles_.describe(c));
            }
        }
        break;
    }
    case Op::Get: {
        vector<RemotePropertyName> names;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemotePropertyName p;
            p.object = r.u64();
            p.name = r.str();
            names.push_back(std::move(p));
        }
        if (!r.ok()) {
            break;
        }
        w.u32(static_cast<uint32_t>(names.size()));
        for (auto& p : names) {
            RemoteValue value;
            const auto object = object_at(p.object);
            auto* meta = cast<IMetadata>(object);
            const ReturnValue status =
                meta ? encode_property(*meta, p.name, {}, handles_, value) : ReturnValue::Fail;
            w.u64(p.object);
            w.str(p.name);
            w.i32(static_cast<int32_t>(status));
            w.value(value);
        }
        break;
    }
    case Op::Set:
    case Op::AddProperty: {
        vector<RemotePropertyValue> values;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemotePropertyValue v;
            v.object = r.u64();
            v.name = r.str();
            r.value(v.value);
            values.push_back(std::move(v));
        }
        if (!r.ok()) {
            break;
        }
        vector<ReturnValue> results;
        for (auto& v : values) {
            const auto object = object_at(v.object);
            auto* meta = cast<IMetadata>(object);
            auto any = meta ? decode_value(v.value, handles_) : nullptr;
            if (!any) {
                results.push_back(ReturnValue::Fail);
                continue;
            }
            if (h.op == Op::AddProperty) {
                results.push_back(meta->add_property(v.name, v.value.type, any.get()) ? ReturnValue::Success
                                                                                        : ReturnValue::Fail);
                continue;
            }
            auto property = meta->get_property(v.name);
            results.push_back(property ? property->set_value(*any, InvokeType::Immediate) : ReturnValue::Fail);
        }
        write_results(w, results);
        break;
    }
    case Op::Subscribe:
    case Op::Unsubscribe:
    case Op::Release: {
        vector<RemoteHandle> objects;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            objects.push_back(r.u64());
        }
        if (!r.ok()) {
            break;
        }
        vector<ReturnValue> results;
        for (auto handle : objects) {
            if (h.op == Op::Subscribe) {
                auto object = object_at(handle);
                if (!object) {
                    results.push_back(ReturnValue::Fail);
                } else if (!peer.subscribed.insert(handle).second) {
                    results.push_back(ReturnValue::NothingToDo);
                } else {
                    watch(handle, object);
                    results.push_back(ReturnValue::Success);
                }
            } else if (h.op == Op::Unsubscribe) {
                if (peer.subscribed.erase(handle)) {
                    unwatch(handle);
                    results.push_back(ReturnValue::Success);
                } else {
                    results.push_back(ReturnValue::NothingToDo);
                }
            } else {
                auto object = object_at(handle);
                auto it = std::find(peer.created.begin(), peer.created.end(), object);
                if (object && it != peer.created.end()) {
                    peer.created.erase(it);
                    results.push_back(ReturnValue::Success);
                } else {
                    results.push_back(ReturnValue::NothingToDo);
                }
            }
        }
        write_results(w, results);
        break;
    }
    case Op::Create: {
        vector<Uid> classes;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            classes.push_back(r.uid());
        }
        if (!r.ok()) {
            break;
        }
        w.u32(static_cast<uint32_t>(classes.size()));
        for (auto uid : classes) {
            auto object = instance().create<IObject>(uid);
            if (object) {
                peer.created.push_back(object);
            }
            w.object(handles_.describe(object));
        }
        break;
    }
    case Op::AddAttachment:
    case Op::RemoveAttachment: {
        vector<RemoteAttachment> items;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemoteAttachment a;
            a.target = r.u64();
            a.attachment = r.u64();
            items.push_back(a);
        }
        if (!r.ok()) {
            break;
        }
        vector<ReturnValue> results;
        for (auto& a : items) {
            const auto target = object_at(a.target);
            auto* storage = cast<IObjectStorage>(target);
            auto attachment = object_at(a.attachment);
            if (!storage || !attachment) {
                results.push_back(ReturnValue::Fail);
            } else if (h.op == Op::AddAttachment) {
                results.push_back(storage->add_attachment(attachment));
            } else {
                results.push_back(storage->remove_attachment(attachment));
            }
        }
        write_results(w, results);
        break;
    }
    case Op::Insert: {
        vector<RemoteInsert> items;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemoteInsert a;
            a.hierarchy = r.u64();
            a.parent = r.u64();
            a.index = r.u32();
            a.child = r.u64();
            items.push_back(a);
        }
        if (!r.ok()) {
            break;
        }
        vector<ReturnValue> results;
        for (auto& a : items) {
            const auto owner = object_at(a.hierarchy);
            auto* hierarchy = cast<IHierarchy>(owner);
            auto child = object_at(a.child);
            if (!hierarchy || !child) {
                results.push_back(ReturnValue::Fail);
                continue;
            }
            if (a.parent == 0) {
                results.push_back(hierarchy->set_root(child));
                continue;
            }
            auto parent = object_at(a.parent);
            if (!parent) {
                results.push_back(ReturnValue::Fail);
            } else if (a.index >= hierarchy->child_count(parent)) {
                results.push_back(hierarchy->add(parent, child));
            } else {
                results.push_back(hierarchy->insert(parent, a.index, child));
            }
        }
        write_results(w, results);
        break;
    }
    case Op::Remove: {
        vector<RemoteNode> nodes;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemoteNode node;
            node.hierarchy = r.u64();
            node.node = r.u64();
            nodes.push_back(node);
        }
        if (!r.ok()) {
            break;
        }
        vector<ReturnValue> results;
        for (auto& node : nodes) {
            const auto owner = object_at(node.hierarchy);
            auto* hierarchy = cast<IHierarchy>(owner);
            auto object = object_at(node.node);
            results.push_back(hierarchy && object ? hierarchy->remove(object) : ReturnValue::Fail);
        }
        write_results(w, results);
        break;
    }
    case Op::Call: {
        vector<RemoteCall> calls;
        for (uint32_t n = r.count(), i = 0; i < n; ++i) {
            RemoteCall c;
            c.object = r.u64();
            c.function = r.str();
            for (uint32_t m = r.count(), j = 0; j < m; ++j) {
                RemoteValue v;
                r.value(v);
                c.args.push_back(std::move(v));
            }
            calls.push_back(std::move(c));
        }
        if (!r.ok()) {
            break;
        }
        w.u32(static_cast<uint32_t>(calls.size()));
        for (auto& c : calls) {
            const auto object = object_at(c.object);
            auto* meta = cast<IMetadata>(object);
            auto function = meta ? meta->get_function(c.function) : nullptr;
            ReturnValue status = function ? ReturnValue::Success : ReturnValue::Fail;
            vector<IAny::Ptr> args;
            vector<const IAny*> pointers;
            for (auto& a : c.args) {
                auto any = decode_value(a, handles_);
                if (!any) {
                    status = ReturnValue::Fail;
                }
                pointers.push_back(any.get());
                args.push_back(std::move(any));
            }
            RemoteValue result;
            if (succeeded(status)) {
                auto value = function->invoke(FnArgs{pointers.data(), pointers.size()}, InvokeType::Immediate);
                if (value) {
                    const auto types = value->get_compatible_types();
                    status = types.empty() ? ReturnValue::Fail
                                           : encode_value(*value, types[0], {}, handles_, result);
                }
            }
            w.i32(static_cast<int32_t>(status));
            w.value(result);
        }
        break;
    }
    default:
        peer.connection->close();
        return;
    }

    if (!r.ok()) {
        peer.connection->close();
        return;
    }
    peer.connection->send(w.message());
}

} // namespace velk::remote
