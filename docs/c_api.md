# C API

Velk provides a flat C API (`velk_c.dll`) for consuming Velk from languages other than C++. The API uses opaque typed handles and manual reference counting, making it suitable for FFI bindings in Rust, Python, C#, Zig, and similar languages.

Header: [`velk/c_api/include/velk_c.h`](../velk/c_api/include/velk_c.h)

## Handles

All Velk objects are exposed as opaque pointer types. Different handle types prevent accidentally passing a property where an event is expected:

| Handle | C++ equivalent |
|---|---|
| `velk_interface` | `IInterface*` (base handle, accepted by `velk_acquire`/`velk_release`/`velk_cast`) |
| `velk_object` | `IObject*` |
| `velk_property` | `IProperty*` |
| `velk_event` | `IEvent*` |
| `velk_function` | `IFunction*` |

All handle types can be passed directly to `velk_acquire`, `velk_release`, and `velk_cast` without casting (via C macros or C++ overloads).

## Reference counting

Functions that return handles acquire one reference. The caller owns that reference and must release it when done:

```c
velk_object obj = velk_create(class_id, 0);   // ref count = 1
velk_property p = velk_get_property(obj, "width");  // ref count = 1

// ... use obj and p ...

velk_release(p);    // release property
velk_release(obj);  // release object
```

`velk_acquire` adds a reference (for sharing a handle across multiple owners). `velk_release` removes one. When the count reaches zero, the object is destroyed.

**Exception:** `velk_cast` does not acquire a ref. The returned interface pointer shares the object's lifetime.

## UIDs

Velk identifies types and interfaces with 128-bit UIDs, represented in C as:

```c
typedef struct velk_uid { uint64_t hi, lo; } velk_uid;
```

Parse a UUID string at runtime:

```c
velk_uid id = velk_uid_from_string("a66badbf-c750-4580-b035-b5446806d67e");
```

Well-known type UIDs are provided as constants: `VELK_TYPE_FLOAT`, `VELK_TYPE_INT32`, `VELK_TYPE_DOUBLE`, `VELK_TYPE_BOOL`.

## Return codes

Most mutating functions return `velk_result` (`int16_t`). Non-negative values indicate success:

| Code | Value | Meaning |
|---|---|---|
| `VELK_SUCCESS` | 0 | Operation succeeded |
| `VELK_NOTHING_TO_DO` | 1 | Succeeded but had no effect (e.g. value unchanged) |
| `VELK_FAIL` | -1 | Operation failed |
| `VELK_INVALID_ARG` | -2 | Invalid argument |
| `VELK_READ_ONLY` | -3 | Write rejected: target is read-only |

## Usage example

Types must be registered on the C++ side before they can be created through the C API. Once registered, C consumers interact entirely through handles:

```c
#include <velk_c.h>

// Class UID obtained from C++ side or documentation
velk_uid widget_class = velk_uid_from_string("...");

// Create an object
velk_object obj = velk_create(widget_class, 0);

// Read and write properties
velk_property width = velk_get_property(obj, "width");

float val;
velk_property_get_float(width, &val);     // read default
velk_property_set_float(width, 42.5f);    // write new value

// Listen for events
void on_clicked(void* ctx, velk_property src) {
    // handle event
}

velk_event evt = velk_get_event(obj, "on_clicked");
velk_function handler = velk_create_callback(on_clicked, NULL);
velk_event_add(evt, handler);

// Advance the runtime (call each frame)
velk_update(0);

// Cleanup
velk_release(handler);
velk_release(evt);
velk_release(width);
velk_release(obj);
```

## API reference

### Lifecycle

| Function | Description |
|---|---|
| `velk_create(class_id, flags)` | Create an object by class UID. Returns `velk_object` with one ref, or NULL |
| `velk_acquire(handle)` | Increment reference count |
| `velk_release(handle)` | Decrement reference count; destroys when zero |

### Interface dispatch

| Function | Description |
|---|---|
| `velk_cast(handle, interface_id)` | Query an interface by UID. No ref acquired. Returns NULL if unsupported |

### Object info

| Function | Description |
|---|---|
| `velk_class_uid(obj)` | Get the class UID |
| `velk_class_name(obj)` | Get the class name as a C string |

### Metadata lookup

| Function | Description |
|---|---|
| `velk_get_property(obj, name)` | Look up a property by name. Returns handle with one ref |
| `velk_get_event(obj, name)` | Look up an event by name. Returns handle with one ref |

### Property access

Type-erased (works with any type, requires a type UID):

| Function | Description |
|---|---|
| `velk_property_get(prop, out, size, type)` | Read value into buffer |
| `velk_property_set(prop, data, size, type)` | Write value from buffer |

Typed convenience (no UID needed):

| Function | Description |
|---|---|
| `velk_property_get_float` / `velk_property_set_float` | `float` |
| `velk_property_get_int32` / `velk_property_set_int32` | `int32_t` |
| `velk_property_get_double` / `velk_property_set_double` | `double` |
| `velk_property_get_bool` / `velk_property_set_bool` | `int32_t` (0 = false, nonzero = true) |

### Events and callbacks

| Function | Description |
|---|---|
| `velk_create_callback(fn, user_data)` | Wrap a C function pointer as a Velk function handle |
| `velk_event_add(evt, handler)` | Add a handler to an event |
| `velk_event_remove(evt, handler)` | Remove a handler from an event |

The callback signature is:

```c
typedef void (*velk_callback_fn)(void* user_data, velk_property source);
```

`source` is the property that triggered the event, or NULL for non-property events.

### Update loop

| Function | Description |
|---|---|
| `velk_update(time_us)` | Flush deferred tasks and notify plugins. Pass 0 for system clock |

### Type UID constants

| Constant | C++ type |
|---|---|
| `VELK_TYPE_FLOAT` | `float` |
| `VELK_TYPE_INT32` | `int` |
| `VELK_TYPE_DOUBLE` | `double` |
| `VELK_TYPE_BOOL` | `bool` |
