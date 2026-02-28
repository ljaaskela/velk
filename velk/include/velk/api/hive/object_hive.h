#ifndef VELK_API_OBJECT_HIVE_H
#define VELK_API_OBJECT_HIVE_H

#include <velk/interface/hive/intf_hive_store.h>
#include <velk/interface/intf_metadata.h>

#include <limits>
#include <type_traits>

namespace velk {
namespace detail {

inline ptrdiff_t compute_state_offset(IObjectHive& hive, Uid interfaceUid)
{
    if (hive.empty()) {
        return -1;
    }
    struct Ctx
    {
        Uid uid;
        ptrdiff_t result;
    } ctx{interfaceUid, -1};

    hive.for_each(&ctx, [](void* c, IObject& obj) -> bool {
        auto& ctx = *static_cast<Ctx*>(c);
        auto* ps = interface_cast<IPropertyState>(&obj);
        if (ps) {
            void* state = ps->get_property_state(ctx.uid);
            if (state) {
                ctx.result = static_cast<ptrdiff_t>(reinterpret_cast<uintptr_t>(state) -
                                                    reinterpret_cast<uintptr_t>(&obj));
            }
        }
        return false; // stop after first
    });
    return ctx.result;
}

/** @brief Non-template storage and operations for ObjectHive<T>. */
class ObjectHiveCore
{
protected:
    ObjectHiveCore() = delete;
    explicit ObjectHiveCore(IObjectHive::Ptr hive) : hive_(std::move(hive)) {}
    ObjectHiveCore(IHiveStore& store, Uid classUid) : hive_(store.get_hive(classUid)) {}

    ObjectHiveCore(const ObjectHiveCore&) = delete;
    ObjectHiveCore& operator=(const ObjectHiveCore&) = delete;
    ObjectHiveCore(ObjectHiveCore&&) = default;
    ObjectHiveCore& operator=(ObjectHiveCore&&) = default;

    /** @brief Visitor callback for for_each_as. Return false to stop early. */
    using TypedVisitorFn = bool (*)(void* context, IObject& obj, void* typed);

    /**
     * @brief Iterates all live objects, resolving an interface offset once.
     *
     * On the first element, resolves the interface via get_interface(uid) and
     * caches the byte offset from IObject to the interface pointer. Subsequent
     * elements apply the offset via pointer arithmetic with no virtual dispatch.
     *
     * @param uid The interface UID to resolve.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called with (context, obj, typed_ptr). Return false to stop early.
     */
    void for_each_as(Uid uid, void* context, TypedVisitorFn visitor) const
    {
        if (!hive_) {
            return;
        }
        struct Core
        {
            void* context;
            TypedVisitorFn visitor;
            Uid uid;
            ptrdiff_t offset;
        };
        constexpr ptrdiff_t unset = std::numeric_limits<ptrdiff_t>::min();
        Core core{context, visitor, uid, unset};
        hive_->for_each(&core, [](void* c, IObject& obj) -> bool {
            auto& core = *static_cast<Core*>(c);
            void* typed;
            if (core.offset == std::numeric_limits<ptrdiff_t>::min()) {
                typed = obj.get_interface(core.uid);
                if (!typed) {
                    return false;
                }
                core.offset = static_cast<char*>(typed) - reinterpret_cast<char*>(&obj);
            } else {
                typed = reinterpret_cast<char*>(&obj) + core.offset;
            }
            return core.visitor(core.context, obj, typed);
        });
    }

public:
    /** @brief Returns true if the underlying IObjectHive is valid. */
    operator bool() const { return hive_.operator bool(); }

    /** @brief Returns the number of live elements. */
    size_t size() const { return hive_ ? hive_->size() : 0; }

    /** @brief Returns true if the hive is empty. */
    bool empty() const { return !hive_ || hive_->empty(); }

    /**
     * @brief Iterates with direct state access, bypassing interface_cast per element.
     *
     * All objects in a hive share the same class layout, so the byte offset from
     * object start to a given State struct is constant. The offset is computed once
     * from the first live object, then for_each_state passes a pre-computed state
     * pointer to each visitor call with no virtual dispatch in the hot loop.
     *
     * @tparam StateInterface The interface whose State struct to access.
     * @param fn Callable as bool(IObject&, StateInterface::State&). Return false to stop early.
     */
    template <class StateInterface, class Fn>
    void for_each(Fn&& fn) const
    {
        static_assert(
            std::is_invocable_r_v<bool, std::decay_t<Fn>, IObject&, typename StateInterface::State&>,
            "ObjectHive::for_each<StateInterface> visitor must be callable as "
            "bool(IObject&, StateInterface::State&)");
        if (!hive_) {
            return;
        }
        ptrdiff_t offset = compute_state_offset(*hive_, StateInterface::UID);
        if (offset > 0) {
            hive_->for_each_state(offset, &fn, [](void* ctx, IObject& obj, void* state) -> bool {
                auto& f = *static_cast<std::decay_t<Fn>*>(ctx);
                return f(obj, *static_cast<typename StateInterface::State*>(state));
            });
        }
    }

    /** @brief Removes all objects from the hive. */
    void clear()
    {
        if (hive_) {
            hive_->clear();
        }
    }

    /** @brief Returns the underlying IObjectHive. */
    IObjectHive& raw() { return *hive_; }
    /** @brief Returns the underlying IObjectHive (const). */
    const IObjectHive& raw() const { return *hive_; }

protected:
    IObjectHive::Ptr hive_;
};

} // namespace detail

/**
 * @brief Typed convenience wrapper around IObjectHive.
 *
 * Provides lambda-friendly for_each() and state-access for_each<StateInterface>()
 * on top of the raw IObjectHive interface. The template parameter T controls the
 * interface type used by add(), remove(), contains(), and for_each():
 *
 *   ObjectHive<IMyWidget> hive(store, MyWidget::class_id());
 *   IMyWidget::Ptr w = hive.add();   // returns IMyWidget::Ptr
 *   hive.for_each([](IMyWidget& w) { ... });
 *
 * Use ObjectHive<> (defaults to IObject) when you don't need a specific interface.
 *
 * @tparam T The interface type for add/remove/contains/for_each. Defaults to IObject.
 */
template <class T = IObject>
class ObjectHive : public detail::ObjectHiveCore
{
    using Base = detail::ObjectHiveCore;

public:
    /** @brief Wraps an existing IObjectHive. */
    explicit ObjectHive(IObjectHive::Ptr hive) : Base(std::move(hive)) {}

    /** @brief Creates an ObjectHive for the given class UID from a hive store. */
    ObjectHive(IHiveStore& store, Uid classUid) : Base(store, classUid) {}

    /** @brief Creates an ObjectHive for class C from a hive store. */
    template <class C>
    explicit ObjectHive(IHiveStore& store, C* = nullptr) : Base(store.get_hive<C>())
    {}

    using Base::for_each;

    /** @brief Creates a new object in the hive and returns a shared pointer to T. */
    typename T::Ptr add()
    {
        if (!hive_) {
            return {};
        }
        return interface_pointer_cast<T>(hive_->add());
    }

    /** @brief Removes an object from the hive. Returns Success or Fail if not found. */
    ReturnValue remove(T& object) { return hive_ ? hive_->remove(object) : ReturnValue::Fail; }

    /** @brief Returns true if the given object is in this hive. */
    bool contains(const T& object) const { return hive_ && hive_->contains(object); }

    /**
     * @brief Iterates all live objects with a typed callback.
     *
     * The interface offset from IObject to T is computed once via get_interface
     * on the first element, then applied via pointer arithmetic for the rest.
     *
     * @param fn Callable as void(T&) or bool(T&). Return false to stop early.
     */
    template <class Fn>
    void for_each(Fn&& fn) const
    {
        static_assert(std::is_invocable_v<std::decay_t<Fn>, T&>,
                      "ObjectHive::for_each visitor must be callable as void(T&) or bool(T&)");
        for_each_as(T::UID, &fn, [](void* ctx, IObject&, void* typed) -> bool {
            auto& f = *static_cast<std::decay_t<Fn>*>(ctx);
            auto& obj = *static_cast<T*>(typed);
            if constexpr (std::is_same_v<decltype(f(obj)), bool>) {
                return f(obj);
            } else {
                f(obj);
                return true;
            }
        });
    }
};

} // namespace velk

#endif // VELK_API_OBJECT_HIVE_H
