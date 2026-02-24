#ifndef VELK_API_STATE_H
#define VELK_API_STATE_H

#include <velk/api/callback.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>

namespace velk {

/**
 * @brief Type-safe property state access. Returns a typed pointer to T::State.
 * @tparam T The interface type whose State struct to retrieve.
 * @param object The object whose property state to return.
 */
template <class T, class U>
typename T::State* get_property_state(U* object)
{
    auto state = interface_cast<IPropertyState>(object);
    return state ? state->template get_property_state<T>() : nullptr;
}

/** @brief Convenience free function: read-only access to T::State via IMetadata. */
template <class T, class U>
detail::StateReader<T> read_state(U* object)
{
    auto* meta = interface_cast<IMetadata>(object);
    return meta ? meta->template read<T>() : detail::StateReader<T>();
}

/** @brief Convenience free function: write access to T::State via IMetadata. */
template <class T, class U>
detail::StateWriter<T> write_state(U* object)
{
    auto* meta = interface_cast<IMetadata>(object);
    return meta ? meta->template write<T>() : detail::StateWriter<T>();
}

/**
 * @brief Returns a shared_ptr to the object, optionally cast to interface T.
 * @tparam T The target interface type (defaults to IObject).
 * @param object The object to retrieve the self pointer from.
 */
template <class T = IObject, class U>
typename T::Ptr get_self(U* object)
{
    auto* obj = interface_cast<IObject>(object);
    return obj ? interface_pointer_cast<T>(obj->get_self()) : typename T::Ptr{};
}

/**
 * @brief Writes to T::State via a callback, with optional deferral.
 *
 * When @p type is Immediate, the callback executes synchronously and on_changed fires when it returns.
 * When @p type is Deferred, the callback is queued and executed on the next update() call.
 * If the object is destroyed before update(), the queued callback is silently skipped.
 *
 * @tparam T The interface type whose State struct to write.
 * @param object The object to modify.
 * @param fn Callback receiving a mutable T::State reference.
 * @param type Immediate or Deferred.
 */
template <class T, class U, class Fn>
void write_state(U* object, Fn&& fn, InvokeType type = Immediate)
{
    auto* meta = interface_cast<IMetadata>(object);
    auto* state = get_property_state<T>(meta);
    if (!state) {
        return;
    }
    if (type == Immediate) {
        fn(*state);
        meta->notify(MemberKind::Property, T::UID, Notification::Changed);
        return;
    }
    IObject::WeakPtr weak(get_self(object));
    if (!weak.lock()) {
        return;
    }
    Callback cb([weak, f = std::forward<Fn>(fn)](FnArgs) mutable -> ReturnValue {
        auto locked = weak.lock();
        if (!locked) {
            return ReturnValue::Fail;
        }
        auto* m = interface_cast<IMetadata>(locked);
        if (!m) {
            return ReturnValue::Fail;
        }
        auto* s = m->template get_property_state<T>();
        if (!s) {
            return ReturnValue::Fail;
        }
        f(*s);
        m->notify(MemberKind::Property, T::UID, Notification::Changed);
        return ReturnValue::Success;
    });
    DeferredTask task{cb, {}};
    instance().queue_deferred_tasks({&task, 1});
}

} // namespace velk

#endif // VELK_API_STATE_H
