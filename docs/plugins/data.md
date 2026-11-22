# Data

The data plugin (`velk_data`) gives list and tree views their items: a source interface with change notifications, a list container, and a source over an `IHierarchy`. Views live elsewhere (velk-ui); this plugin only holds the data.

```cpp
#include <velk/plugins/data/api/data.h>

load_data_plugin();
auto list = create_object_list();
list->add(item);
```

## IItemSource

Items are velk objects; an item's identity is its object. Items form a tree: the top level has the null parent, and a list is a tree whose items have no children.

| Member | Description |
|--------|-------------|
| `child_count(parent)` | Number of children of `parent`; null for the top level |
| `child_at(parent, index)` | The child at `index`, or null |
| `has_children(item)` | True when `item` has children |
| `on_items_changed` | Fires an `ItemChange` |

`ItemChange` is `{type, parent, index, count}`:

| Type | Meaning |
|------|---------|
| `Inserted` | `count` items inserted under `parent` from `index` |
| `Removed` | `count` items removed under `parent` from `index`, with their children |
| `Reset` | Anything may have changed; read the source again |

Changes to an item's own properties are not source changes. Views see them through their bindings.

## ObjectList

`ClassId::ObjectList`, `IObjectList`: an ordered list of objects. `add`, `insert` (several items as one change), `remove(index, count)`, `move(from, to)` (a removal, then an insertion) and `clear` (a Reset). Thread-safe; events fire outside its lock.

## HierarchySource

`ClassId::HierarchySource`: an `IItemSource` over the `IHierarchy` in its `IHierarchySource::hierarchy` property. The top level is the hierarchy's root, and each object's items are its children. Adding or inserting becomes Inserted, removing Removed, replacing Removed then Inserted, and setting the root or clearing Reset.

```cpp
auto source = create_hierarchy_source(hierarchy);
```
