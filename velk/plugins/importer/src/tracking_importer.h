#ifndef VELK_IMPORTER_TRACKING_IMPORTER_H
#define VELK_IMPORTER_TRACKING_IMPORTER_H

#include <velk/ext/core_object.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>

namespace velk {

/**
 * @brief JSON importer that records where each imported object came from.
 *
 * Imports with a plain JsonImporter, then hands the store to the importer plugin,
 * which records each object's source and entry. Created by create_json_importer()
 * while source tracking is on.
 */
class TrackingJsonImporter : public ext::ObjectCore<TrackingJsonImporter, IStoreImporter, IJsonValueCodec>
{
public:
    VELK_CLASS_UID(ClassId::TrackingJsonImporter, "TrackingJsonImporter");

    TrackingJsonImporter();

    ImportResult import_from(string_view text, string_view source = {}) const override;

    // IJsonValueCodec
    string to_json(const IAny& value, Uid type) const override;
    IAny::Ptr from_json(string_view json, Uid type) const override;

private:
    IStoreImporter::Ptr plain_;
};

} // namespace velk

#endif // VELK_IMPORTER_TRACKING_IMPORTER_H
