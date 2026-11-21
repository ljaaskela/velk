#ifndef VELK_PLUGINS_IMPORTER_PLUGIN_H
#define VELK_PLUGINS_IMPORTER_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief JSON scene importer. Create via instance().create<IStoreImporter>(ClassId::JsonImporter). */
inline constexpr Uid JsonImporter{"b2e3f4a5-6c7d-8e9f-a0b1-c2d3e4f5a6b7"};
/** @brief JSON importer that records where objects came from (IImporterPlugin::source_of). */
inline constexpr Uid TrackingJsonImporter{"39104da9-d8a8-4b60-b119-acca7a8a2352"};
} // namespace ClassId

namespace PluginId {
/** @brief Scene importer plugin (velk_importer). */
inline constexpr Uid ImporterPlugin{"51952e9e-6802-4ebf-acff-c742ecd079fe"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_IMPORTER_PLUGIN_H
