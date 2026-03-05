#include <velk_c.h>

#include <velk/api/velk.h>
#include <velk/common.h>
#include <velk/interface/intf_any.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_velk.h>
#include <velk/interface/types.h>
#include <velk/uid.h>

#include <cstring>

using namespace velk;

// UID conversion helpers

static Uid to_uid(velk_uid u)
{
    return {u.hi, u.lo};
}

static velk_uid from_uid(Uid u)
{
    return {u.hi, u.lo};
}

// Handle conversion helpers
//
// Handles are raw IInterface* with an extra ref held by the caller.
// to_handle: takes a shared_ptr, refs the raw pointer, returns opaque handle.
// from_handle: reinterpret_casts the opaque handle back to IInterface*.

template <class Handle>
static Handle to_handle(IInterface* raw)
{
    if (!raw) {
        return nullptr;
    }
    raw->ref();
    return reinterpret_cast<Handle>(raw);
}

template <class Handle, class T>
static Handle to_handle(const shared_ptr<T>& ptr)
{
    return to_handle<Handle>(static_cast<IInterface*>(ptr.get()));
}

static IInterface* from_handle(velk_interface h)
{
    return reinterpret_cast<IInterface*>(h);
}

// UID from string

velk_uid velk_uid_from_string(const char* uuid_str)
{
    if (!uuid_str) {
        return {0, 0};
    }
    Uid uid(string_view(uuid_str, strlen(uuid_str)));
    return from_uid(uid);
}

// Lifecycle

velk_object velk_create(velk_uid class_id, uint32_t flags)
{
    auto ptr = ::velk::instance().create(to_uid(class_id), flags);
    if (!ptr) {
        return nullptr;
    }
    auto* obj = interface_cast<IObject>(ptr);
    return to_handle<velk_object>(static_cast<IInterface*>(obj));
}

void velk_acquire(velk_interface obj)
{
    if (obj) {
        from_handle(obj)->ref();
    }
}

void velk_release(velk_interface obj)
{
    if (obj) {
        from_handle(obj)->unref();
    }
}

// Interface dispatch

velk_interface velk_cast(velk_interface obj, velk_uid interface_id)
{
    if (!obj) {
        return nullptr;
    }
    auto* result = from_handle(obj)->get_interface(to_uid(interface_id));
    // No extra ref: the returned interface shares the object's lifetime.
    return reinterpret_cast<velk_interface>(result);
}

// Object info

velk_uid velk_class_uid(velk_object obj)
{
    if (!obj) {
        return {0, 0};
    }
    auto* o = static_cast<IObject*>(from_handle(reinterpret_cast<velk_interface>(obj)));
    return from_uid(o->get_class_uid());
}

const char* velk_class_name(velk_object obj)
{
    if (!obj) {
        return nullptr;
    }
    auto* o = static_cast<IObject*>(from_handle(reinterpret_cast<velk_interface>(obj)));
    auto name = o->get_class_name();
    // Class names come from compile-time type_name_holder which appends '\0'.
    return name.data();
}

// Metadata lookup

velk_property velk_get_property(velk_object obj, const char* name)
{
    if (!obj || !name) {
        return nullptr;
    }
    auto* meta = interface_cast<IMetadata>(from_handle(reinterpret_cast<velk_interface>(obj)));
    if (!meta) {
        return nullptr;
    }
    auto prop = meta->get_property(string_view(name, strlen(name)));
    return to_handle<velk_property>(prop);
}

velk_event velk_get_event(velk_object obj, const char* name)
{
    if (!obj || !name) {
        return nullptr;
    }
    auto* meta = interface_cast<IMetadata>(from_handle(reinterpret_cast<velk_interface>(obj)));
    if (!meta) {
        return nullptr;
    }
    auto evt = meta->get_event(string_view(name, strlen(name)));
    return to_handle<velk_event>(evt);
}

// Property get/set (type-erased)

velk_result velk_property_get(velk_property prop, void* out, size_t size, velk_uid type)
{
    if (!prop || !out) {
        return VELK_INVALID_ARG;
    }
    auto* p = static_cast<IProperty*>(from_handle(reinterpret_cast<velk_interface>(prop)));
    auto val = p->get_value();
    if (!val) {
        return VELK_FAIL;
    }
    return static_cast<velk_result>(val->get_data(out, size, to_uid(type)));
}

velk_result velk_property_set(velk_property prop, const void* data, size_t size, velk_uid type)
{
    if (!prop || !data) {
        return VELK_INVALID_ARG;
    }
    auto* pi = interface_cast<IPropertyInternal>(from_handle(reinterpret_cast<velk_interface>(prop)));
    if (!pi) {
        return VELK_FAIL;
    }
    return static_cast<velk_result>(pi->set_data(data, size, to_uid(type)));
}

// Property get/set (typed convenience)

velk_result velk_property_get_float(velk_property prop, float* out)
{
    velk_uid t = from_uid(type_uid<float>());
    return velk_property_get(prop, out, sizeof(float), t);
}

velk_result velk_property_set_float(velk_property prop, float value)
{
    velk_uid t = from_uid(type_uid<float>());
    return velk_property_set(prop, &value, sizeof(float), t);
}

velk_result velk_property_get_int32(velk_property prop, int32_t* out)
{
    velk_uid t = from_uid(type_uid<int>());
    return velk_property_get(prop, out, sizeof(int32_t), t);
}

velk_result velk_property_set_int32(velk_property prop, int32_t value)
{
    velk_uid t = from_uid(type_uid<int>());
    return velk_property_set(prop, &value, sizeof(int32_t), t);
}

velk_result velk_property_get_double(velk_property prop, double* out)
{
    velk_uid t = from_uid(type_uid<double>());
    return velk_property_get(prop, out, sizeof(double), t);
}

velk_result velk_property_set_double(velk_property prop, double value)
{
    velk_uid t = from_uid(type_uid<double>());
    return velk_property_set(prop, &value, sizeof(double), t);
}

velk_result velk_property_get_bool(velk_property prop, int32_t* out)
{
    if (!prop || !out) {
        return VELK_INVALID_ARG;
    }
    bool tmp = false;
    velk_uid t = from_uid(type_uid<bool>());
    velk_result r = velk_property_get(prop, &tmp, sizeof(bool), t);
    if (r >= 0) {
        *out = tmp ? 1 : 0;
    }
    return r;
}

velk_result velk_property_set_bool(velk_property prop, int32_t value)
{
    bool tmp = value != 0;
    velk_uid t = from_uid(type_uid<bool>());
    return velk_property_set(prop, &tmp, sizeof(bool), t);
}

// Callbacks

struct CallbackContext
{
    velk_callback_fn fn;
    void* user_data;
};

static void callback_deleter(void* ctx)
{
    delete static_cast<CallbackContext*>(ctx);
}

static IAny::Ptr callback_trampoline(void* ctx, FnArgs args)
{
    auto* cb = static_cast<CallbackContext*>(ctx);
    // The first argument (if any) is the property-as-IAny. We pass it as a handle.
    velk_property source = nullptr;
    if (args.count > 0 && args[0]) {
        auto* intf = const_cast<IInterface*>(static_cast<const IInterface*>(args[0]));
        auto* prop = interface_cast<IProperty>(intf);
        if (prop) {
            source = reinterpret_cast<velk_property>(static_cast<IInterface*>(prop));
        }
    }
    cb->fn(cb->user_data, source);
    return nullptr;
}

velk_function velk_create_callback(velk_callback_fn fn, void* user_data)
{
    if (!fn) {
        return nullptr;
    }
    auto* ctx = new CallbackContext{fn, user_data};
    auto func = ::velk::instance().create_owned_callback(ctx, &callback_trampoline, &callback_deleter);
    return to_handle<velk_function>(func);
}

// Events

velk_result velk_event_add(velk_event evt, velk_function handler)
{
    if (!evt || !handler) {
        return VELK_INVALID_ARG;
    }
    auto* e = static_cast<IEvent*>(from_handle(reinterpret_cast<velk_interface>(evt)));
    // Build a const shared_ptr to the handler without taking ownership.
    // The caller still owns the handler ref.
    auto* fn_raw = static_cast<IFunction*>(from_handle(reinterpret_cast<velk_interface>(handler)));
    // We need a shared_ptr<const IFunction>. Create one that refs/unrefs.
    fn_raw->ref();
    IFunction::ConstPtr fn_ptr(static_cast<const IFunction*>(fn_raw));
    return static_cast<velk_result>(e->add_handler(fn_ptr));
}

velk_result velk_event_remove(velk_event evt, velk_function handler)
{
    if (!evt || !handler) {
        return VELK_INVALID_ARG;
    }
    auto* e = static_cast<IEvent*>(from_handle(reinterpret_cast<velk_interface>(evt)));
    auto* fn_raw = static_cast<IFunction*>(from_handle(reinterpret_cast<velk_interface>(handler)));
    fn_raw->ref();
    IFunction::ConstPtr fn_ptr(static_cast<const IFunction*>(fn_raw));
    return static_cast<velk_result>(e->remove_handler(fn_ptr));
}

// Update loop

void velk_update(int64_t time_us)
{
    ::velk::instance().update(Duration{time_us});
}

// Type UID constants

extern const velk_uid VELK_TYPE_FLOAT  = from_uid(type_uid<float>());
extern const velk_uid VELK_TYPE_INT32  = from_uid(type_uid<int>());
extern const velk_uid VELK_TYPE_DOUBLE = from_uid(type_uid<double>());
extern const velk_uid VELK_TYPE_BOOL   = from_uid(type_uid<bool>());
