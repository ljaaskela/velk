#ifndef VELK_PLUGINS_DATA_API_DATA_H
#define VELK_PLUGINS_DATA_API_DATA_H

#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_hierarchy.h>
#include <velk/plugins/data/interface/intf_hierarchy_source.h>
#include <velk/plugins/data/interface/intf_object_list.h>
#include <velk/plugins/data/plugin.h>

namespace velk {

/** @brief Loads the velk_data plugin, if it is not loaded yet. */
inline ReturnValue load_data_plugin()
{
    return instance().plugin_registry().load_plugin("plugin:velk_data");
}

/** @brief A new, empty ObjectList; null when velk_data is not loaded. */
inline IObjectList::Ptr create_object_list()
{
    return instance().create<IObjectList>(ClassId::ObjectList);
}

/** @brief A new HierarchySource over @p hierarchy; null when velk_data is not loaded. */
inline IItemSource::Ptr create_hierarchy_source(const IHierarchy::Ptr& hierarchy)
{
    auto source = instance().create<IItemSource>(ClassId::HierarchySource);
    if (source && hierarchy) {
        write_state<IHierarchySource>(source, [&](IHierarchySource::State& s) { set_object_ref(s.hierarchy, hierarchy); });
    }
    return source;
}

} // namespace velk

#endif // VELK_PLUGINS_DATA_API_DATA_H
