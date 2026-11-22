#ifndef VELK_PLUGINS_DATA_INTF_OBJECT_LIST_H
#define VELK_PLUGINS_DATA_INTF_OBJECT_LIST_H

#include <velk/plugins/data/interface/intf_item_source.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief An ordered list of objects that is also an IItemSource.
 *
 * Every change fires on_items_changed with its range. Its items have no children.
 *
 * Chain: IInterface -> IItemSource -> IObjectList
 */
class IObjectList : public Interface<IObjectList, IItemSource>
{
public:
    /** @brief The number of items. */
    virtual size_t size() const = 0;
    /** @brief The item at @p index, or null when out of range. */
    virtual IObject::Ptr at(size_t index) const = 0;
    /** @brief The index of @p item, or size() when it is not in the list. */
    virtual size_t index_of(const IObject::Ptr& item) const = 0;

    /** @brief Appends @p item. */
    virtual ReturnValue add(const IObject::Ptr& item) = 0;
    /** @brief Inserts @p items at @p index (at most size()), as one change. */
    virtual ReturnValue insert(size_t index, array_view<const IObject::Ptr> items) = 0;
    /** @brief Removes @p count items from @p index, as one change. */
    virtual ReturnValue remove(size_t index, size_t count = 1) = 0;
    /** @brief Moves the item at @p from so it ends up at @p to: a removal, then an insertion. */
    virtual ReturnValue move(size_t from, size_t to) = 0;
    /** @brief Removes all items: a Reset. */
    virtual void clear() = 0;
};

} // namespace velk

#endif // VELK_PLUGINS_DATA_INTF_OBJECT_LIST_H
