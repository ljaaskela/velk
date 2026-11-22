#include "object_storage.h"

#include <velk/interface/intf_any_extension.h>

#include "array_property.h"
#include "event.h"
#include "function.h"
#include "property.h"
#include "standalone_property.h"

#include <velk/api/velk.h>
#include <velk/ext/any.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/types.h>

#include <algorithm>

namespace velk {

ObjectStorage::ObjectStorage(array_view<MemberDesc> members, IInterface* owner)
    : members_(members),
      owner_(owner)
{}

ObjectStorage::~ObjectStorage()
{
    for (auto& s : metadata_) {
        destroy_slot(s);
    }
    metadata_.clear();
    attachments_.clear();
}

array_view<MemberDesc> ObjectStorage::get_static_metadata() const
{
    return members_;
}

IInterface::Ptr ObjectStorage::create(MemberDesc desc) const
{
    switch (desc.kind) {
    case MemberKind::Property: {
        auto* pk = desc.propertyKind();
        return ext::make_object<impl::Property>(pk ? pk->flags : ObjectFlags::None);
    }
    case MemberKind::ArrayProperty: {
        auto* pk = desc.propertyKind();
        return ext::make_object<impl::ArrayProperty>(pk ? pk->flags : ObjectFlags::None);
    }
    case MemberKind::Event:
        return ext::make_object<impl::Event>();
    case MemberKind::Function:
        return ext::make_object<impl::Function>();
    }
    return {};
}

void ObjectStorage::bind(const MemberDesc& m, uint16_t slot_index) const
{
    auto* fk = m.functionKind();
    if (fk && fk->trampoline && owner_) {
        void* intf_ptr = owner_->get_interface(m.interfaceInfo->uid);
        if (intf_ptr) {
            auto& cb = data(slot_index).callback;
            cb.target_context = intf_ptr;
            cb.target_fn = fk->trampoline;
        }
    }
}

static uint16_t member_index_of(const IInterface::Ptr& inst)
{
    auto* owned = interface_cast<IStorageOwned>(inst);
    return owned ? owned->get_member_index() : IStorageOwned::InvalidIndex;
}

void ObjectStorage::release_callback(MemberSlot& s) const
{
    auto& cb = s.data.callback;
    if (cb.context_deleter && cb.owned_context) {
        cb.context_deleter(cb.owned_context);
    }
    cb.owned_context = nullptr;
    cb.context_deleter = nullptr;
}

void ObjectStorage::destroy_slot(MemberSlot& s) const
{
    auto index = member_index_of(s.instance);
    if (index == IStorageOwned::InvalidIndex) {
        VELK_LOG(E, "ObjectStorage: Invalid slot index for slot destruction");
        return;
    }
    auto kind = members_[index].kind;
    if (kind == MemberKind::Property || kind == MemberKind::ArrayProperty) {
        // Walk the extension chain before destroying the property instance,
        // so extensions can clean up (e.g. break ref cycles).
        auto current = std::move(s.data.property.data);
        while (auto* ext = interface_cast<IAnyExtension>(current)) {
            current = ext->take_inner(*s.instance);
        }
        s.instance.reset();
        s.data.property.data.~shared_ptr();
        s.data.property.event.~shared_ptr();
    } else {
        s.instance.reset();
        release_callback(s);
    }
}

IInterface::Ptr ObjectStorage::find_or_create(string_view name, MemberKind kind, Resolve mode,
                                              Uid interfaceUid) const
{
    auto matches = [&](const MemberDesc& m) {
        if (m.kind != kind || m.name != name) return false;
        if (interfaceUid != Uid{} && m.interfaceInfo && m.interfaceInfo->uid != interfaceUid) return false;
        return true;
    };

    // Check cache first
    for (auto& s : metadata_) {
        if (auto idx = member_index_of(s.instance);
            idx != IStorageOwned::InvalidIndex && matches(members_[idx])) {
            return s.instance;
        }
    }
    if (mode == Resolve::Existing) {
        return {};
    }
    IInterface::Ptr created;
    // Find in static metadata and create if found
    for (size_t i = 0; i < members_.size(); ++i) {
        auto& m = members_[i];
        if (!matches(m)) {
            continue;
        }
        created = create(m);
        auto slot_index = static_cast<uint16_t>(metadata_.size());
        // Push slot first (zero-initialized union), then set owner so storage_id is valid
        metadata_.push_back(MemberSlot(created));
        if (auto* owned = interface_cast<IStorageOwned>(created)) {
            owned->set_owner(const_cast<ObjectStorage*>(this), static_cast<uint16_t>(i), slot_index);
        }
        // Init property data or bind function trampoline
        if (kind == MemberKind::Property || kind == MemberKind::ArrayProperty) {
            // Write initial data directly to slot (no change notification during creation).
            auto& pd = data(slot_index).property;
            if (auto* pk = m.propertyKind()) {
                if (pk->createRef && owner_) {
                    if (auto* ps = interface_cast<IPropertyState>(owner_)) {
                        if (void* base = ps->get_property_state(m.interfaceInfo->uid)) {
                            pd.data = pk->createRef(base);
                        }
                    }
                }
                if (!pd.data && pk->getDefault) {
                    if (auto* def = pk->getDefault()) {
                        pd.data = def->clone();
                    }
                }
            }
        } else {
            bind(m, slot_index);
        }
    }
    return created;
}

IProperty::Ptr ObjectStorage::get_property(string_view name, Uid interfaceUid, Resolve mode) const
{
    bool search_static = (interfaceUid == Uid{}) || (interfaceUid != IDynamicMetadata::UID);
    bool search_dynamic = (interfaceUid == Uid{}) || (interfaceUid == IDynamicMetadata::UID);
    bool static_only = (interfaceUid != Uid{}) && (interfaceUid != IDynamicMetadata::UID);

    if (search_static) {
        auto result = find_or_create(name, MemberKind::Property, mode,
                                     static_only ? interfaceUid : Uid{});
        if (!result) {
            result = find_or_create(name, MemberKind::ArrayProperty, mode,
                                    static_only ? interfaceUid : Uid{});
        }
        if (result) {
            return interface_pointer_cast<IProperty>(result);
        }
    }

    if (search_dynamic) {
        return find_dynamic_property(name);
    }

    return {};
}

IEvent::Ptr ObjectStorage::get_event(string_view name, Resolve mode) const
{
    return interface_pointer_cast<IEvent>(find_or_create(name, MemberKind::Event, mode));
}

IFunction::Ptr ObjectStorage::get_function(string_view name, Resolve mode) const
{
    return interface_pointer_cast<IFunction>(find_or_create(name, MemberKind::Function, mode));
}

void ObjectStorage::notify_observers(string_view name, Uid interfaceUid) const
{
    if (!observers_.empty()) {
        if (auto* meta = interface_cast<IMetadata>(owner_)) {
            for (auto* observer : observers_) {
                observer->on_state_changed(name, *meta, interfaceUid);
            }
        }
    }
}

void ObjectStorage::notify(MemberKind kind, Uid interfaceUid, Notification notification) const
{
    for (auto& s : metadata_) {
        auto& m = members_[member_index_of(s.instance)];
        if (m.kind != kind || !m.interfaceInfo || m.interfaceInfo->uid != interfaceUid) {
            continue;
        }

        switch (notification) {
        case Notification::Changed:
            if (kind == MemberKind::Property || kind == MemberKind::ArrayProperty) {
                auto* property = interface_cast<IProperty>(s.instance);
                if (property && s.data.property.event) {
                    invoke_event(s.data.property.event, property->get_value().get());
                }
            }
            break;
        default:
            break;
        }
    }
    if (notification == Notification::Changed) {
        notify_observers({}, interfaceUid);
    }
}

ReturnValue ObjectStorage::add_attachment(const IInterface::Ptr& attachment)
{
    if (!attachment) {
        return ReturnValue::InvalidArgument;
    }
    auto it = std::find(attachments_.begin(), attachments_.end(), attachment);
    if (it == attachments_.end()) {
        attachments_.push_back(attachment);
        return ReturnValue::Success;
    }
    return ReturnValue::NothingToDo; // Already there
}

ReturnValue ObjectStorage::remove_attachment(const IInterface::Ptr& attachment)
{
    if (!attachment) {
        return ReturnValue::InvalidArgument;
    }
    auto it = std::find(attachments_.begin(), attachments_.end(), attachment);
    if (it != attachments_.end()) {
        attachments_.erase(it);
        return ReturnValue::Success;
    }
    return ReturnValue::Fail; // Not found
}

size_t ObjectStorage::attachment_count() const
{
    return attachments_.size();
}

IInterface::Ptr ObjectStorage::get_attachment(size_t index) const
{
    return index < attachments_.size() ? attachments_[index] : IInterface::Ptr{};
}

IInterface::Ptr ObjectStorage::find_attachment(const AttachmentQuery& query, Resolve mode)
{
    const bool matchInterface = query.interfaceUid != Uid{};
    const bool matchClass = query.classUid != Uid{};

    for (auto& att : attachments_) {
        if (matchInterface && !att->get_interface(query.interfaceUid)) {
            continue;
        }
        if (matchClass) {
            if (auto* obj = att->get_interface<IObject>()) {
                if (obj->get_class_uid() != query.classUid) {
                    continue;
                }
            }
        }
        return att;
    }
    if (mode == Resolve::Create && matchClass) {
        auto created = instance().create<IInterface>(query.classUid);
        if (created) {
            // Route through the owner's IObjectStorage so any
            // add_attachment override on the owning object (e.g. to
            // index attachments into a side structure) participates.
            // Without this, calling our own add_attachment here would
            // bypass virtual dispatch on the outer object.
            auto* owner_storage = owner_ ? interface_cast<IObjectStorage>(owner_) : nullptr;
            if (owner_storage && owner_storage != this) {
                owner_storage->add_attachment(created);
            } else {
                add_attachment(created);
            }
        }
        return created;
    }
    return {};
}

vector<IInterface::Ptr> ObjectStorage::find_attachments(const AttachmentQuery& query) const
{
    vector<IInterface::Ptr> matches;
    const bool matchInterface = query.interfaceUid != Uid{};
    const bool matchClass = query.classUid != Uid{};
    for (auto& att : attachments_) {
        if (matchInterface && !att->get_interface(query.interfaceUid)) {
            continue;
        }
        if (matchClass) {
            if (auto* obj = att->get_interface<IObject>()) {
                if (obj->get_class_uid() != query.classUid) {
                    continue;
                }
            }
        }
        matches.push_back(att);
    }
    return matches;
}

IEvent::Ptr ObjectStorage::get_property_event(size_t storage_id, Resolve mode) const
{
    if (storage_id >= metadata_.size()) {
        return {};
    }
    auto& pd = data(storage_id).property;
    if (!pd.event && mode == Resolve::Create) {
        instance().type_registry().create_event_once(pd.event);
    }
    return pd.event;
}

void ObjectStorage::invoke_property_changed(size_t storage_id, IProperty* property) const
{
    if (storage_id >= metadata_.size()) {
        return;
    }
    auto& s = slot(storage_id);
    if (!property) {
        property = interface_cast<IProperty>(s.instance);
    }
    auto& event = s.data.property.event;
    if (property && event) {
        invoke_event(event, property->get_value().get());
    }
    if (!observers_.empty()) {
        auto& m = members_[member_index_of(s.instance)];
        if (m.interfaceInfo) {
            notify_observers(m.name, m.interfaceInfo->uid);
        }
    }
}

void ObjectStorage::mark_binding_stale(size_t storage_id)
{
    if (storage_id >= metadata_.size()) {
        return;
    }
    auto* owned = interface_cast<IStorageOwned>(slot(storage_id).instance);
    if (!owned || owned->binding_stale()) {
        return;
    }
    owned->set_binding_stale(true);
    // The owning object keeps the bit its state reads test; it forwards here again, which
    // returns above.
    if (auto* owner = interface_cast<IObjectStorage>(owner_); owner && owner != this) {
        owner->mark_binding_stale(storage_id);
    }
}

bool ObjectStorage::refresh_stale_bindings(Uid interfaceUid)
{
    bool remaining = false;
    for (auto& s : metadata_) {
        if (!s.instance) {
            continue;
        }
        auto* owned = interface_cast<IStorageOwned>(s.instance);
        if (!owned || !owned->binding_stale()) {
            continue;
        }
        auto index = owned->get_member_index();
        if (index == IStorageOwned::InvalidIndex) {
            continue;
        }
        auto& m = members_[index];
        if (!m.interfaceInfo || m.interfaceInfo->uid != interfaceUid) {
            remaining = true;
            continue;
        }
        owned->set_binding_stale(false);
        auto& pd = s.data.property;
        // The value reads come from the head of the extension chain; the state is the any at
        // its end. Copying writes the state member directly, without events.
        const IAny::Ptr head = pd.data;
        IAny::ConstPtr state = head;
        while (auto* ext = interface_cast<IAnyExtension>(state)) {
            auto inner = ext->get_inner();
            if (!inner) {
                break;
            }
            state = inner;
        }
        if (head && state && state != head) {
            const_cast<IAny&>(*state).copy_from(*head);
        }
    }
    return remaining;
}

void ObjectStorage::add_observer(IMetadataObserver* observer)
{
    if (observer) {
        auto it = std::find(observers_.begin(), observers_.end(), observer);
        if (it == observers_.end()) {
            observers_.push_back(observer);
        }
    }
}

void ObjectStorage::remove_observer(IMetadataObserver* observer)
{
    if (observer) {
        auto it = std::find(observers_.begin(), observers_.end(), observer);
        if (it != observers_.end()) {
            observers_.erase(it);
        }
    }
}

void ObjectStorage::on_runtime_property_changed(void* ctx, string_view name, IProperty*)
{
    if (auto* storage = static_cast<ObjectStorage*>(ctx)) {
        storage->notify_observers(name, IDynamicMetadata::UID);
    }
}

IProperty::Ptr ObjectStorage::find_dynamic_property(string_view name) const
{
    for (auto& att : attachments_) {
        if (!interface_cast<IStandaloneProperty>(att)) {
            continue;
        }
        auto* obj = interface_cast<IObject>(att);
        if (obj && obj->get_name() == name) {
            return interface_pointer_cast<IProperty>(att);
        }
    }
    return {};
}

IProperty::Ptr ObjectStorage::add_property(string_view name, Uid typeUid,
                                           const IAny* defaultValue) const
{
    // Check if a dynamic property with this name already exists
    if (auto existing = find_dynamic_property(name)) {
        return existing;
    }

    // Create and configure a standalone property
    auto prop_inst = ext::make_object<impl::StandaloneProperty>();
    auto* sp = interface_cast<IStandaloneProperty>(prop_inst);
    if (!sp) {
        return {};
    }
    sp->set_name(name);
    sp->set_notify(const_cast<ObjectStorage*>(this), &ObjectStorage::on_runtime_property_changed);

    // Initialize with default value
    auto* pi = interface_cast<IPropertyInternal>(prop_inst);
    if (pi) {
        if (defaultValue) {
            pi->set_any(defaultValue->clone());
        } else if (typeUid != Uid{}) {
            auto any = instance().type_registry().create_any(typeUid);
            if (any) {
                pi->set_any(std::move(any));
            }
        }
    }

    attachments_.push_back(prop_inst);
    notify_observers(name, IDynamicMetadata::UID);
    return interface_pointer_cast<IProperty>(prop_inst);
}

ReturnValue ObjectStorage::remove_property(string_view name) const
{
    // Search dynamic properties first (StandaloneProperty attachments)
    for (auto it = attachments_.begin(); it != attachments_.end(); ++it) {
        auto* sp = interface_cast<IStandaloneProperty>(*it);
        if (!sp) {
            continue;
        }
        auto* obj = interface_cast<IObject>(*it);
        if (obj && obj->get_name() == name) {
            sp->set_notify(nullptr, nullptr);
            attachments_.erase(it);
            return ReturnValue::Success;
        }
    }

    // Search static properties: remove cached wrapper only (tombstone the slot)
    for (auto& s : metadata_) {
        auto idx = member_index_of(s.instance);
        if (idx != IStorageOwned::InvalidIndex && members_[idx].name == name
            && (members_[idx].kind == MemberKind::Property
                || members_[idx].kind == MemberKind::ArrayProperty)) {
            destroy_slot(s);
            return ReturnValue::Success;
        }
    }

    return ReturnValue::Fail;
}

vector<PropertyInfo> ObjectStorage::get_properties() const
{
    vector<PropertyInfo> result;

    // Static properties from member descriptors
    for (auto& m : members_) {
        if (m.kind == MemberKind::Property || m.kind == MemberKind::ArrayProperty) {
            auto* pk = m.propertyKind();
            Uid iuid = m.interfaceInfo ? m.interfaceInfo->uid : Uid{};
            result.push_back({m.name, pk ? pk->typeUid : Uid{}, iuid});
        }
    }

    // Dynamic properties from attachments
    for (auto& att : attachments_) {
        auto* prop = interface_cast<IStandaloneProperty>(att);
        if (!prop) {
            continue;
        }
        auto propName = ::velk::get_name(att);
        if (!propName.empty()) {
            Uid typeUid{};
            if (auto val = prop->get_value()) {
                auto types = val->get_compatible_types();
                if (!types.empty()) {
                    typeUid = types[0];
                }
            }
            result.push_back({std::move(propName), typeUid, IDynamicMetadata::UID});
        }
    }

    return result;
}

} // namespace velk
