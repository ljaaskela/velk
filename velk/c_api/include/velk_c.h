/**
 * @file velk_c.h
 * @brief Flat C API for Velk. Provides FFI-friendly access to core Velk operations.
 *
 * All C++ objects are exposed as opaque typed handles. Functions that return handles
 * acquire a reference; the caller must release it with velk_release().
 */
#ifndef VELK_C_H
#define VELK_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef VELK_C_STATIC_DEFINE
#define VELK_C_API
#else
#ifdef VELK_C_EXPORTS
#ifdef _MSC_VER
#define VELK_C_API __declspec(dllexport)
#else
#define VELK_C_API __attribute__((visibility("default")))
#endif
#else
#ifdef _MSC_VER
#define VELK_C_API __declspec(dllimport)
#else
#define VELK_C_API __attribute__((visibility("default")))
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle types */
typedef struct velk_interface_s* velk_interface;
typedef struct velk_object_s*    velk_object;
typedef struct velk_property_s*  velk_property;
typedef struct velk_event_s*     velk_event;
typedef struct velk_function_s*  velk_function;

/* 128-bit UID, passed by value */
typedef struct velk_uid
{
    uint64_t hi;
    uint64_t lo;
} velk_uid;

/**
 * @brief Parses a UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx") into a velk_uid.
 * Returns a zero UID on invalid input.
 */
VELK_C_API velk_uid velk_uid_from_string(const char* uuid_str);

/* Return codes (matches velk::ReturnValue) */
typedef int16_t velk_result;
#define VELK_SUCCESS         ((velk_result)0)
#define VELK_NOTHING_TO_DO   ((velk_result)1)
#define VELK_FAIL            ((velk_result)-1)
#define VELK_INVALID_ARG     ((velk_result)-2)
#define VELK_READ_ONLY       ((velk_result)-3)

/* Lifecycle */

/**
 * @brief Creates an object of the registered type identified by class_id.
 * Returns NULL on failure. The returned handle has one reference.
 */
VELK_C_API velk_object velk_create(velk_uid class_id, uint32_t flags);

/** @brief Increments the reference count of any handle. */
VELK_C_API void velk_acquire(velk_interface obj);

/** @brief Decrements the reference count. When it reaches zero the object is destroyed. */
VELK_C_API void velk_release(velk_interface obj);

/* Interface dispatch */

/**
 * @brief Queries an interface from an object by UID.
 * Returns NULL if the interface is not supported. The returned handle shares
 * the object's lifetime (no additional ref is acquired).
 */
VELK_C_API velk_interface velk_cast(velk_interface obj, velk_uid interface_id);

/* Object info */

/** @brief Returns the class UID of an object. */
VELK_C_API velk_uid velk_class_uid(velk_object obj);

/**
 * @brief Returns the class name of an object.
 * The pointer is valid for the lifetime of the object. May return NULL.
 */
VELK_C_API const char* velk_class_name(velk_object obj);

/* Metadata: property/event lookup */

/**
 * @brief Looks up a property by name. Returns NULL if not found.
 * The returned handle has one reference; caller must velk_release() it.
 */
VELK_C_API velk_property velk_get_property(velk_object obj, const char* name);

/**
 * @brief Looks up an event by name. Returns NULL if not found.
 * The returned handle has one reference; caller must velk_release() it.
 */
VELK_C_API velk_event velk_get_event(velk_object obj, const char* name);

/* Property get/set (type-erased) */

/**
 * @brief Reads a property value into @p out.
 * @param prop  Property handle.
 * @param out   Destination buffer.
 * @param size  Size of the destination buffer in bytes.
 * @param type  Expected type UID (must match the property's stored type).
 */
VELK_C_API velk_result velk_property_get(velk_property prop, void* out, size_t size, velk_uid type);

/**
 * @brief Writes a property value from @p data.
 * @param prop  Property handle.
 * @param data  Source buffer.
 * @param size  Size of the source data in bytes.
 * @param type  Type UID of the data.
 */
VELK_C_API velk_result velk_property_set(velk_property prop, const void* data, size_t size, velk_uid type);

/* Property get/set (typed convenience) */

VELK_C_API velk_result velk_property_get_float(velk_property prop, float* out);
VELK_C_API velk_result velk_property_set_float(velk_property prop, float value);
VELK_C_API velk_result velk_property_get_int32(velk_property prop, int32_t* out);
VELK_C_API velk_result velk_property_set_int32(velk_property prop, int32_t value);
VELK_C_API velk_result velk_property_get_double(velk_property prop, double* out);
VELK_C_API velk_result velk_property_set_double(velk_property prop, double value);
VELK_C_API velk_result velk_property_get_bool(velk_property prop, int32_t* out);
VELK_C_API velk_result velk_property_set_bool(velk_property prop, int32_t value);

/* Events/callbacks */

/**
 * @brief Callback signature for C event handlers.
 * @param user_data  Opaque pointer passed at registration.
 * @param source     The property that changed (may be NULL for non-property events).
 */
typedef void (*velk_callback_fn)(void* user_data, velk_property source);

/**
 * @brief Creates a callback function wrapping a C function pointer.
 * Returns a handle with one reference; caller must velk_release() it.
 */
VELK_C_API velk_function velk_create_callback(velk_callback_fn fn, void* user_data);

/** @brief Adds a handler to an event. */
VELK_C_API velk_result velk_event_add(velk_event evt, velk_function handler);

/** @brief Removes a handler from an event. */
VELK_C_API velk_result velk_event_remove(velk_event evt, velk_function handler);

/* Update loop */

/**
 * @brief Advances the Velk runtime. Flushes deferred tasks and notifies plugins.
 * @param time_us  Current time in microseconds. Pass 0 to use the system clock.
 */
VELK_C_API void velk_update(int64_t time_us);

/* Well-known type UIDs */

VELK_C_API extern const velk_uid VELK_TYPE_FLOAT;
VELK_C_API extern const velk_uid VELK_TYPE_INT32;
VELK_C_API extern const velk_uid VELK_TYPE_DOUBLE;
VELK_C_API extern const velk_uid VELK_TYPE_BOOL;

#ifdef __cplusplus
}

/* C++ convenience: allow passing any handle type to velk_acquire / velk_release. */
inline void velk_acquire(velk_object    h) { velk_acquire(reinterpret_cast<velk_interface>(h)); }
inline void velk_acquire(velk_property  h) { velk_acquire(reinterpret_cast<velk_interface>(h)); }
inline void velk_acquire(velk_event     h) { velk_acquire(reinterpret_cast<velk_interface>(h)); }
inline void velk_acquire(velk_function  h) { velk_acquire(reinterpret_cast<velk_interface>(h)); }

inline void velk_release(velk_object    h) { velk_release(reinterpret_cast<velk_interface>(h)); }
inline void velk_release(velk_property  h) { velk_release(reinterpret_cast<velk_interface>(h)); }
inline void velk_release(velk_event     h) { velk_release(reinterpret_cast<velk_interface>(h)); }
inline void velk_release(velk_function  h) { velk_release(reinterpret_cast<velk_interface>(h)); }

inline velk_interface velk_cast(velk_object h, velk_uid id)
{
    return velk_cast(reinterpret_cast<velk_interface>(h), id);
}

#else

/* C convenience: cast any handle to velk_interface automatically. */
#define velk_acquire(h) velk_acquire((velk_interface)(h))
#define velk_release(h) velk_release((velk_interface)(h))
#define velk_cast(h, id) velk_cast((velk_interface)(h), id)

#endif

#endif /* VELK_C_H */
