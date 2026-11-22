#ifndef VELK_PLUGINS_DATA_INTF_HIERARCHY_SOURCE_H
#define VELK_PLUGINS_DATA_INTF_HIERARCHY_SOURCE_H

#include <velk/api/object_ref.h>
#include <velk/plugins/data/interface/intf_item_source.h>

namespace velk {

/**
 * @brief Settings of an item source over an IHierarchy. The object is also an IItemSource.
 *
 * The top level is the hierarchy's root; each object's items are its children in the
 * hierarchy. Changes to the hierarchy become ItemChanges: adding or inserting is
 * Inserted, removing is Removed, replacing is Removed then Inserted, and setting the
 * root or clearing is Reset.
 *
 * Chain: IInterface -> IHierarchySource
 */
class IHierarchySource : public Interface<IHierarchySource>
{
public:
    VELK_INTERFACE(
        (PROP, ObjectRef, hierarchy, {}) ///< The IHierarchy to show.
    )
};

} // namespace velk

#endif // VELK_PLUGINS_DATA_INTF_HIERARCHY_SOURCE_H
