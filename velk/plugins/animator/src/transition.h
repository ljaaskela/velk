#ifndef VELK_ANIMATOR_TRANSITION_IMPL_H
#define VELK_ANIMATOR_TRANSITION_IMPL_H

#include <velk/ext/any_extension.h>
#include <velk/ext/object.h>
#include <velk/plugins/animator/interface/intf_transition.h>
#include <velk/plugins/animator/plugin.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief Per-property transition state and interpolation logic.
 *
 * Plain C++ struct that holds the animation buffers (display, from, target, result)
 * and manages interpolation. Used by both TransitionImpl (direct install) and
 * TransitionProxy (multi-target children).
 */
struct TransitionDriver
{
    IAny::Ptr display, from, target, result;
    Duration elapsed{};
    bool animating = false;

    /** @brief Clones all scratch buffers from an inner IAny. */
    void init(const IAny& inner);

    /** @brief Captures from=display, target=new data, resets elapsed. Returns false if no display. */
    bool start(const void* data, size_t size, Uid type);

    /**
     * @brief Interpolates one tick.
     * @return true if still animating.
     */
    bool tick(Duration dt, Duration duration, easing::EasingFn easing,
              InterpolatorFn interpolator, IAny& inner, const IInterface::WeakPtr& owner);

    /** @brief Resets all state. */
    void clear();
};

namespace ProxyId {
inline constexpr Uid TransitionProxy{"a1b2c3d4-e5f6-4789-abcd-ef0123456789"};
} // namespace ProxyId

/**
 * @brief Lightweight IAnyExtension proxy for multi-target transitions.
 *
 * Installed on child properties by TransitionImpl::add_target. Uses
 * ext::AnyExtension for ref counting and default IAny passthrough.
 * Only overrides the methods needed for animation interception.
 */
class TransitionProxy : public ext::AnyExtension<TransitionProxy>
{
public:
    VELK_CLASS_UID(ProxyId::TransitionProxy);

    // IAnyExtension overrides
    void set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner) override;
    IAny::Ptr take_inner(IInterface& owner) override;

    // IAny overrides (animation interception)
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t size, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

    IAny* inner_ptr() { return inner_.get(); }

    TransitionDriver driver;
    IInterface::WeakPtr owner_;
    InterpolatorFn interpolator_ = nullptr;
};

/**
 * @brief Transition that is itself an IAnyExtension.
 *
 * Installed on a property via install_extension. Intercepts set_data
 * to capture animation targets, and interpolates during tick().
 *
 * Can also manage child transitions via add_target/remove_target: each child
 * is a lightweight TransitionProxy with the same duration/easing, installed on
 * its own property. Config changes propagate to all children.
 */
class TransitionImpl final : public ext::Object<TransitionImpl, ITransition, IAnyExtension>
{
public:
    VELK_CLASS_UID(ClassId::Transition);
    ~TransitionImpl();

    // ITransition
    bool tick(Duration dt) override;
    void set_easing(easing::EasingFn easing) override;
    void add_target(const IProperty::Ptr& target) override;
    void remove_target(const IProperty::Ptr& target) override;
    void uninstall() override;

    // IAnyExtension: installing on a property creates a proxy child
    IAny::ConstPtr get_inner() const override;
    void set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner) override;
    IAny::Ptr take_inner(IInterface& owner) override;

    // IAny passthrough to installed proxy
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

private:
    struct ChildEntry
    {
        IProperty::WeakPtr property;
        IAnyExtension::Ptr proxy;
    };

    void ensure_registered();
    ITransition::State* state();
    void notify_state();

    easing::EasingFn easing_ = easing::linear;
    vector<ChildEntry> children_;
    TransitionProxy* installed_proxy_ = nullptr;
    bool registered_ = false;
};

} // namespace velk

#endif // VELK_ANIMATOR_TRANSITION_IMPL_H
