#ifndef VELK_IMPORTER_PLUGIN_H
#define VELK_IMPORTER_PLUGIN_H

#include <velk/ext/plugin.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

namespace velk {

class ImporterPlugin final : public ext::Plugin<ImporterPlugin, IImporterPlugin>
{
public:
    VELK_PLUGIN_UID("51952e9e-6802-4ebf-acff-c742ecd079fe");
    VELK_PLUGIN_NAME("importer");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk&) override;

    void register_class_alias(string_view alias, Uid class_uid) override;
    Uid resolve_class(string_view name) const override;

    void set_track_sources(bool on) override;
    bool track_sources() const override;
    ImportSource source_of(const IObject& object) const override;

    /** @brief Records the source and entry of each object in @p store. Called by TrackingJsonImporter. */
    void record(const IStore& store, string_view source);

private:
    struct Tracked
    {
        IObject::WeakPtr object;
        string source;
        string entry;
    };

    /** @brief Drops entries whose objects died. Called with sources_mutex_ held. */
    void prune();

    std::unordered_map<std::string, Uid> aliases_;

    std::atomic<bool> track_sources_{false};
    mutable std::mutex sources_mutex_;
    std::unordered_map<const IObject*, Tracked> sources_;
    size_t prune_at_ = 1024;
};

} // namespace velk

#endif // VELK_IMPORTER_PLUGIN_H
