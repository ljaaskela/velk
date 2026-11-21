#include "tracking_importer.h"

#include "importer_plugin.h"
#include "json_importer.h"

#include <velk/api/velk.h>

namespace velk {

TrackingJsonImporter::TrackingJsonImporter() : plain_(ext::make_object<JsonImporter, IStoreImporter>()) {}

ImportResult TrackingJsonImporter::import_from(string_view text, string_view source) const
{
    auto result = plain_->import_from(text, source);
    if (result.store) {
        auto plugin = ::velk::instance().plugin_registry().find_plugin(PluginId::ImporterPlugin);
        if (auto* ip = interface_cast<IImporterPlugin>(plugin)) {
            static_cast<ImporterPlugin*>(ip)->record(*result.store, source);
        }
    }
    return result;
}

string TrackingJsonImporter::to_json(const IAny& value, Uid type) const
{
    auto* codec = interface_cast<IJsonValueCodec>(plain_);
    return codec ? codec->to_json(value, type) : string();
}

IAny::Ptr TrackingJsonImporter::from_json(string_view json, Uid type) const
{
    auto* codec = interface_cast<IJsonValueCodec>(plain_);
    return codec ? codec->from_json(json, type) : nullptr;
}

} // namespace velk
