# Remote plugin

The remote plugin (`velk_remote`) lets another program, or another part of the same one, read and edit a running program's objects: the object tree, properties, change notifications, object creation and hierarchy edits. Editors and debuggers use it.

A program runs an `IRemoteServer`; a tool runs an `IRemoteClient`. They talk over TCP on localhost, or directly when both are in one process. The client code is the same either way.

## Contents

- [Server](#server)
- [Client](#client)
- [Objects and handles](#objects-and-handles)
- [Values](#values)
- [Changes](#changes)
- [Threading](#threading)
- [Requests](#requests)

## Server

```cpp
#include <velk/plugins/remote/api/remote.h>

auto server = velk::create_remote_server();
server.publish("scene", scene_hierarchy);
uint16_t port = server.listen(0); // 0 picks a free port
```

Clients see what the program publishes: each published object is a root with a name. Objects no root reaches can be found by class (see `instances` below). `listen()` accepts connections on 127.0.0.1 only.

## Client

```cpp
auto client = velk::create_remote_client();
client.connect("127.0.0.1", port);       // or client.connect_local(server) in the same process

client.roots().then([](const velk::RemoteRoots& roots) {
    for (auto& root : roots.items) { /* root.name, root.object.handle */ }
});
```

Every request takes a list and returns one future, so one round trip reads or writes many things. When the connection closes, outstanding and later requests resolve with empty results and `on_disconnected` fires.

## Objects and handles

The server refers to objects by `RemoteHandle`, a 64-bit number it assigns the first time it reports an object. Handles are never reused: an object that dies keeps its handle, and requests on it fail.

The server holds objects weakly, except the ones a client creates with `create()`, which stay alive until the client calls `release()`. Place created objects (as an attachment or in a hierarchy) before releasing them.

## Values

Values travel as `RemoteValue`:

| Form | Used for | Content |
|---|---|---|
| `Bytes` | Numbers, bool, vectors, colors, matrices, enums | The value's memory |
| `String` | `string` | UTF-8 |
| `Object` | `ObjectRef` | The referenced object's handle |
| `Array` | Array properties and array results | Plain data elements packed in `data`; other elements one value each in `elements` |

Because plain data travels as memory, the client and server must share the memory layout of those types: build them from the same source. Property types that hold pointers or heap memory other than `string`, `ObjectRef` and arrays are not supported.

`make_remote_value()`, `make_remote_reference()` and `read_remote_value()` convert to and from C++ values.

## Changes

`subscribe()` reports the objects' property changes through `on_changed`, once per server update, and objects that died through `on_released`.

* A change to a property is reported for that property.
* A `write<T>()` or `write_state<T>()` of a whole state block is reported as every property of that interface.
* Writes into a state block that skip both (for example bulk writes through a hive's `for_each_state`) are not reported.

## Threading

Server and client belong to the thread that created them. The server runs requests during `instance().update()`, before deferred tasks; it sends changes after the update. The client resolves futures and fires its events during `instance().update()` too. Socket input and output happen on threads of their own.

## Requests

| Request | Input | Result |
|---|---|---|
| `roots()` | | `RemoteRoots` |
| `instances(classes)` | Class uids | `RemoteObjects`: live objects of those classes created from their hive (the default for classes under 1 KB); objects created with `CreationPolicy::Alloc` are not listed |
| `describe(objects)` | Handles | `RemoteDescriptions`: class, interfaces, properties with values and flags, events, functions, attachments, hierarchy root |
| `children(nodes)` | Hierarchy and node handles | `RemoteChildrenList` |
| `get(properties)` | Handle and property name | `RemotePropertyValues` |
| `set(values)` | Handle, name, value | `RemoteResults`; all applied in one server update |
| `subscribe(objects)`, `unsubscribe(objects)` | Handles | `RemoteResults` |
| `create(classes)` | Class uids | `RemoteObjects`, handle 0 where creation failed |
| `release(objects)` | Handles of created objects | `RemoteResults` |
| `add_attachment(items)`, `remove_attachment(items)` | Target and attachment handles | `RemoteResults` |
| `add_property(values)` | Handle, name, value (typed by the value) | `RemoteResults` |
| `insert(items)` | Hierarchy, parent (0 sets the root), index (past the end appends), child | `RemoteResults` |
| `remove(nodes)` | Hierarchy and node; removes the subtree | `RemoteResults` |
| `call(calls)` | Handle, function name (`FN` members), arguments | `RemoteCallResults`: status and returned value per call |
