#include "transition.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_event.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>

#include <algorithm>

namespace velk {

// ============================================================================
// TransitionDriver
// ============================================================================

void TransitionDriver::init(const IAny& inner)
{
    display = inner.clone();
    from = inner.clone();
    target = inner.clone();
    result = inner.clone();
}

bool TransitionDriver::start(const void* data, size_t size, Uid type)
{
    if (!display) {
        return false;
    }

    // Capture from = current display value
    if (from) {
        from->copy_from(*display);
    }

    // Capture target = new value
    if (target) {
        target->set_data(data, size, type);
    }

    // Reset animation
    elapsed = {};
    animating = true;
    return true;
}

bool TransitionDriver::tick(Duration dt, Duration duration, easing::EasingFn easing,
                            InterpolatorFn interpolator, IAny& inner,
                            const IInterface::WeakPtr& owner)
{
    if (!animating) {
        return false;
    }

    elapsed.us += dt.us;

    float t = 1.f;
    if (duration.us > 0) {
        t = static_cast<float>(elapsed.us) / static_cast<float>(duration.us);
        t = std::min(t, 1.f);
    }

    float eased = easing ? easing(t) : t;

    if (interpolator && from && target && result) {
        if (succeeded(interpolator(*from, *target, eased, *result))) {
            display->copy_from(*result);
            inner.copy_from(*result);
        }
    }

    // Fire on_changed on the property
    auto prop = owner.lock();
    if (prop) {
        auto* propIntf = interface_cast<IProperty>(prop);
        if (propIntf && propIntf->on_changed()) {
            invoke_event(propIntf->on_changed(), display.get());
        }
    }

    if (t >= 1.f) {
        animating = false;
    }

    return animating;
}

void TransitionDriver::clear()
{
    display = nullptr;
    from = nullptr;
    target = nullptr;
    result = nullptr;
    elapsed = {};
    animating = false;
}

// ============================================================================
// TransitionProxy
// ============================================================================

void TransitionProxy::set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner)
{
    owner_ = owner;
    inner_ = std::move(inner);

    if (!inner_) {
        return;
    }

    // Resolve interpolator from the inner's compatible type
    auto types = inner_->get_compatible_types();
    if (!types.empty()) {
        interpolator_ = instance().type_registry().find_interpolator(types[0]);
    }

    driver.init(*inner_);
}

IAny::Ptr TransitionProxy::take_inner(IInterface&)
{
    owner_ = IInterface::WeakPtr{};
    driver.clear();
    interpolator_ = nullptr;
    return std::move(inner_);
}

ReturnValue TransitionProxy::get_data(void* to, size_t toSize, Uid type) const
{
    return driver.display ? driver.display->get_data(to, toSize, type) : ReturnValue::Fail;
}

ReturnValue TransitionProxy::set_data(void const* from, size_t size, Uid type)
{
    if (!driver.display || !interpolator_) {
        // No interpolator: fall through to direct write
        if (driver.display) {
            auto ret = driver.display->set_data(from, size, type);
            if (inner_ && ret == ReturnValue::Success) {
                inner_->set_data(from, size, type);
            }
            return ret;
        }
        return ReturnValue::Fail;
    }

    driver.start(from, size, type);
    return ReturnValue::NothingToDo;
}

ReturnValue TransitionProxy::copy_from(const IAny& other)
{
    // Passthrough: used by deferred flush (set_value_silent path).
    ReturnValue ret = ReturnValue::Fail;
    if (driver.display) {
        ret = driver.display->copy_from(other);
    }
    if (inner_ && succeeded(ret)) {
        inner_->copy_from(other);
    }
    return ret;
}

IAny::Ptr TransitionProxy::clone() const
{
    return driver.display ? driver.display->clone() : nullptr;
}

// ============================================================================
// TransitionImpl
// ============================================================================

TransitionImpl::~TransitionImpl()
{
    uninstall();
}

void TransitionImpl::uninstall()
{
    for (auto& child : children_) {
        auto prop = child.property.lock();
        if (!prop) {
            continue;
        }
        auto* pi = interface_cast<IPropertyInternal>(prop);
        if (!pi) {
            continue;
        }
        if (interface_cast<IAnyExtension>(child.proxy.get()) == installed_proxy_) {
            // This proxy lives behind TransitionImpl in the chain; remove self
            pi->remove_extension(get_self<IAnyExtension>());
        } else {
            // add_target proxy: directly in the property's chain
            pi->remove_extension(child.proxy);
        }
    }
    children_.clear();
    installed_proxy_ = nullptr;
}

void TransitionImpl::ensure_registered()
{
    if (!registered_) {
        auto* ap = get_or_load_plugin<IAnimatorPlugin>(PluginId::AnimatorPlugin);
        if (ap) {
            ap->register_transition(ITransition::WeakPtr(get_self<ITransition>()));
            registered_ = true;
        }
    }
}

ITransition::State* TransitionImpl::state()
{
    return static_cast<ITransition::State*>(get_property_state(ITransition::UID));
}

void TransitionImpl::notify_state()
{
    notify(MemberKind::Property, ITransition::UID, Notification::Changed);
}

void TransitionImpl::set_easing(easing::EasingFn easing)
{
    easing_ = easing;
}

void TransitionImpl::add_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    if (!pi) {
        return;
    }

    auto proxy = ext::make_object<TransitionProxy, IAnyExtension>();
    pi->install_extension(proxy);

    children_.push_back({IProperty::WeakPtr(target), proxy});
    ensure_registered();
}

void TransitionImpl::remove_target(const IProperty::Ptr& target)
{
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        auto prop = it->property.lock();
        if (prop && prop.get() == target.get()) {
            auto* pi = interface_cast<IPropertyInternal>(prop);
            if (pi) {
                pi->remove_extension(it->proxy);
            }
            children_.erase(it);
            return;
        }
    }
}

// IAnyExtension: installing TransitionImpl on a property creates a proxy child

IAny::ConstPtr TransitionImpl::get_inner() const
{
    return installed_proxy_ ? installed_proxy_->get_inner() : nullptr;
}

void TransitionImpl::set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner)
{
    // The property calls set_inner during install_extension, before putting us
    // at the head of the chain. We create a proxy, initialize it directly with
    // the inner/owner, and store it as a child. Our IAny methods forward to it.
    auto proxy = ext::make_object<TransitionProxy, IAnyExtension>();
    auto* p = static_cast<TransitionProxy*>(interface_cast<IAnyExtension>(proxy.get()));
    p->set_inner(std::move(inner), owner);

    auto propPtr = interface_pointer_cast<IProperty>(owner.lock());
    children_.push_back({IProperty::WeakPtr(propPtr), proxy});
    installed_proxy_ = p;
    ensure_registered();
}

IAny::Ptr TransitionImpl::take_inner(IInterface& owner)
{
    if (!installed_proxy_) {
        return nullptr;
    }
    auto inner = installed_proxy_->take_inner(owner);
    // Remove the installed child entry
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (interface_cast<IAnyExtension>(it->proxy.get()) == installed_proxy_) {
            children_.erase(it);
            break;
        }
    }
    installed_proxy_ = nullptr;
    return inner;
}

// IAny passthrough to installed proxy

array_view<Uid> TransitionImpl::get_compatible_types() const
{
    return installed_proxy_ ? installed_proxy_->get_compatible_types() : array_view<Uid>{};
}

size_t TransitionImpl::get_data_size(Uid type) const
{
    return installed_proxy_ ? installed_proxy_->get_data_size(type) : 0;
}

ReturnValue TransitionImpl::get_data(void* to, size_t toSize, Uid type) const
{
    return installed_proxy_ ? installed_proxy_->get_data(to, toSize, type) : ReturnValue::Fail;
}

ReturnValue TransitionImpl::set_data(void const* from, size_t fromSize, Uid type)
{
    return installed_proxy_ ? installed_proxy_->set_data(from, fromSize, type) : ReturnValue::Fail;
}

ReturnValue TransitionImpl::copy_from(const IAny& other)
{
    return installed_proxy_ ? installed_proxy_->copy_from(other) : ReturnValue::Fail;
}

IAny::Ptr TransitionImpl::clone() const
{
    return installed_proxy_ ? installed_proxy_->clone() : nullptr;
}

// ITransition

bool TransitionImpl::tick(Duration dt)
{
    auto* s = state();
    Duration duration = s ? s->duration : Duration{};

    bool anyAnimating = false;
    for (auto& child : children_) {
        auto* proxy = static_cast<TransitionProxy*>(
            interface_cast<IAnyExtension>(child.proxy.get()));
        if (proxy && proxy->inner_ptr()) {
            bool still = proxy->driver.tick(
                dt, duration, easing_, proxy->interpolator_,
                *proxy->inner_ptr(), proxy->owner_);
            anyAnimating = anyAnimating || still;
        }
    }

    if (s) {
        if (s->animating != anyAnimating) {
            s->animating = anyAnimating;
            notify_state();
        }
    }

    return anyAnimating;
}

} // namespace velk
