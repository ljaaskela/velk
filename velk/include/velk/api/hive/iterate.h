#ifndef VELK_API_HIVE_ITERATE_H
#define VELK_API_HIVE_ITERATE_H

#include <velk/interface/hive/intf_hive.h>
#include <velk/interface/intf_metadata.h>

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

} // namespace detail

/** @brief Callback signature for for_each_hive: bool(IObject&). */
using HiveVisitorFn = bool(IObject&);

/** @brief Callback signature for for_each_hive with state: bool(IObject&, State&). */
template <class StateInterface>
using HiveStateVisitorFn = bool(IObject&, typename StateInterface::State&);

/**
 * @brief Iterates all live objects in a hive with a typed lambda.
 *
 * Convenience wrapper around IObjectHive::for_each that accepts a capturing lambda
 * instead of a void* context + function pointer pair.
 *
 * @tparam Fn Callable matching HiveVisitorFn. Return false to stop early.
 * @param hive The hive to iterate.
 * @param fn Visitor function.
 */
template <class Fn>
void for_each_hive(IObjectHive& hive, Fn&& fn)
{
    static_assert(std::is_invocable_r_v<bool, Fn, IObject&>,
                  "Visitor must be callable as bool(IObject&)");

    struct Context
    {
        Fn* fn;
    };
    Context ctx{&fn};

    hive.for_each(&ctx, [](void* c, IObject& obj) -> bool {
        auto& ctx = *static_cast<Context*>(c);
        return (*ctx.fn)(obj);
    });
}

/**
 * @brief Iterates a hive with direct state access, bypassing interface_cast per element.
 *
 * All objects in a hive share the same class layout, so the byte offset from
 * object start to a given State struct is constant. The offset is computed once
 * from the first live object, then for_each_state passes a pre-computed state
 * pointer to each visitor call with no virtual dispatch in the hot loop.
 *
 * @tparam StateInterface The interface whose State struct to access (must have a UID).
 * @tparam Fn Callable matching HiveStateVisitorFn<StateInterface>. Return false to stop early.
 * @param hive The hive to iterate.
 * @param fn Visitor function.
 */
template <class StateInterface, class Fn>
void for_each_hive(IObjectHive& hive, Fn&& fn)
{
    static_assert(std::is_invocable_r_v<bool, Fn, IObject&, typename StateInterface::State&>,
                  "Visitor must be callable as bool(IObject&, StateInterface::State&)");

    struct Context
    {
        Fn* fn;
    };

    ptrdiff_t offset = detail::compute_state_offset(hive, StateInterface::UID);
    if (offset) {
        Context ctx{&fn};
        hive.for_each_state(offset, &ctx, [](void* c, IObject& obj, void* state) -> bool {
            auto& ctx = *static_cast<Context*>(c);
            return (*ctx.fn)(obj, *static_cast<typename StateInterface::State*>(state));
        });
    }
}

} // namespace velk

#endif // VELK_API_HIVE_ITERATE_H
