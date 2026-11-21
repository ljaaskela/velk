#include "importer_plugin.h"
#include "json_importer.h"
#include "tracking_importer.h"

#include <velk/string.h>

namespace velk {

namespace {

bool starts_with(string_view s, string_view prefix)
{
    return s.size() >= prefix.size() && string_view(s.data(), prefix.size()) == prefix;
}

/** @brief The file entry for a store id: "objects/<id>", "resources/<id>" or "attachments/<id>". */
string entry_for(string_view id)
{
    const string_view resource = "resource:";
    const string_view attachment = "attachment:";
    if (starts_with(id, resource)) {
        return string("resources/") + id.substr(resource.size());
    }
    if (starts_with(id, attachment)) {
        return string("attachments/") + id.substr(attachment.size());
    }
    return string("objects/") + id;
}

} // namespace

ReturnValue ImporterPlugin::initialize(IVelk& velk, PluginConfig&)
{
    auto rv = ::velk::register_type<JsonImporter>(velk);
    if (failed(rv)) {
        return rv;
    }
    return ::velk::register_type<TrackingJsonImporter>(velk);
}

ReturnValue ImporterPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

void ImporterPlugin::register_class_alias(string_view alias, Uid class_uid)
{
    aliases_[std::string(alias.data(), alias.size())] = class_uid;
}

Uid ImporterPlugin::resolve_class(string_view name) const
{
    std::string name_str(name.data(), name.size());

    // Try UUID format first
    if (is_valid_uid_format(name)) {
        Uid uid(name);
        if (::velk::instance().type_registry().get_class_info(uid)) {
            return uid;
        }
    }

    // Try registered aliases
    auto it = aliases_.find(name_str);
    if (it != aliases_.end()) {
        return it->second;
    }

    // Look up by registered class name
    return ::velk::instance().type_registry().find_class_by_name(name);
}

void ImporterPlugin::set_track_sources(bool on)
{
    track_sources_.store(on, std::memory_order_relaxed);
}

bool ImporterPlugin::track_sources() const
{
    return track_sources_.load(std::memory_order_relaxed);
}

ImportSource ImporterPlugin::source_of(const IObject& object) const
{
    std::lock_guard lock(sources_mutex_);
    auto it = sources_.find(&object);
    if (it == sources_.end()) {
        return {};
    }
    // A dead object's address can be reused by a new one, which has no source.
    auto live = it->second.object.lock();
    if (live.get() != &object) {
        return {};
    }
    return {it->second.source, it->second.entry};
}

void ImporterPlugin::record(const IStore& store, string_view source)
{
    const string_view hierarchy = "hierarchy:";
    std::lock_guard lock(sources_mutex_);
    const size_t n = store.object_count();
    for (size_t i = 0; i < n; ++i) {
        auto id = store.id_at(i);
        auto object = store.object_at(i);
        if (!object || starts_with(id, hierarchy)) {
            continue;
        }
        sources_[object.get()] = Tracked{object, string(source), entry_for(id)};
    }
    if (sources_.size() >= prune_at_) {
        prune();
    }
}

void ImporterPlugin::prune()
{
    for (auto it = sources_.begin(); it != sources_.end();) {
        if (it->second.object.lock()) {
            ++it;
        } else {
            it = sources_.erase(it);
        }
    }
    prune_at_ = sources_.size() * 2 > 1024 ? sources_.size() * 2 : 1024;
}

} // namespace velk

VELK_PLUGIN(velk::ImporterPlugin)
