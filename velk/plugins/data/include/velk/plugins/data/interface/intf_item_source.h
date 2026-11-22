#ifndef VELK_PLUGINS_DATA_INTF_ITEM_SOURCE_H
#define VELK_PLUGINS_DATA_INTF_ITEM_SOURCE_H

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>

namespace velk {

/**
 * @brief Items for a list or tree view, with change notifications.
 *
 * An item is a velk object; views show its properties through bindings. Items form a
 * tree: the top level has the null parent, and a list is a tree whose items have no
 * children. An item's identity is its object.
 *
 * on_items_changed carries an ItemChange. Changes to an item's own properties are not
 * source changes; views see them through their bindings.
 *
 * Chain: IInterface -> IItemSource
 */
class IItemSource : public Interface<IItemSource>
{
public:
    VELK_INTERFACE(
        (EVT, on_items_changed)
    )

    /** @brief The number of children of @p parent; null for the top level. */
    virtual size_t child_count(const IObject::Ptr& parent) const = 0;

    /** @brief The child of @p parent at @p index, or null when out of range. */
    virtual IObject::Ptr child_at(const IObject::Ptr& parent, size_t index) const = 0;

    /** @brief True when @p item has children. */
    virtual bool has_children(const IObject::Ptr& item) const = 0;
};

/** @brief A change to an IItemSource's items, carried by on_items_changed. */
struct ItemChange
{
    enum class Type : uint8_t
    {
        Inserted, ///< @c count items were inserted under @c parent from @c index.
        Removed,  ///< @c count items were removed under @c parent from @c index, with their children.
        Reset     ///< Anything may have changed: read the source again.
    };
    Type type{};
    IObject::Ptr parent; ///< Null for the top level.
    size_t index{};
    size_t count{};

    friend bool operator==(const ItemChange& a, const ItemChange& b)
    {
        return a.type == b.type && a.parent == b.parent && a.index == b.index && a.count == b.count;
    }
    friend bool operator!=(const ItemChange& a, const ItemChange& b) { return !(a == b); }
};

} // namespace velk

#endif // VELK_PLUGINS_DATA_INTF_ITEM_SOURCE_H
