#ifndef VELK_API_BINDING_H
#define VELK_API_BINDING_H

#include <velk/api/callback.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_binding.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object_ref.h>
#include <velk/string.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

#include <initializer_list>

namespace velk {

/**
 * @brief Wrapper around an IBinding.
 *
 * A Binding can be installed on one or more target properties via add_target().
 * All targets read the same evaluated value from the binding's source.
 */
class Binding
{
public:
    /** @brief Wraps an existing binding. */
    explicit Binding(IBinding::Ptr binding) : binding_(std::move(binding)) {}

    /** @brief Constructs a null binding. */
    Binding() = default;

    /** @brief Returns true if the underlying binding is valid. */
    operator bool() const { return binding_.operator bool(); }

    /** @brief Implicit conversion to IBinding::Ptr. */
    operator IBinding::Ptr() { return binding_; }
    operator const IBinding::ConstPtr() const { return binding_; }

    /** @brief Returns the source property, or null if this is a function binding. */
    IProperty::ConstPtr get_source_property() const
    {
        return binding_ ? binding_->get_source_property() : nullptr;
    }

    /** @brief Returns the source function, or null if this is a property binding. */
    IFunction::ConstPtr get_source_function() const
    {
        return binding_ ? binding_->get_source_function() : nullptr;
    }

    /** @brief Installs this binding on a target property. Returns false on type mismatch. */
    bool add_target(const IProperty::Ptr& target)
    {
        return binding_ && binding_->add_target(target);
    }

    /** @brief Installs this binding on a typed property target. Returns false on type mismatch. */
    template <class T>
    bool add_target(Property<T> target)
    {
        return add_target(IProperty::Ptr(target));
    }

    /** @brief Removes this binding from a target property. Returns false if not installed. */
    bool remove_target(const IProperty::Ptr& target)
    {
        return binding_ && binding_->remove_target(target);
    }

    /** @brief Removes this binding from a typed property target. Returns false if not installed. */
    template <class T>
    bool remove_target(Property<T> target)
    {
        return remove_target(IProperty::Ptr(target));
    }

    /** @brief Removes this binding from all targets and clears the handle. */
    void remove()
    {
        if (binding_) {
            binding_->uninstall();
        }
        binding_ = {};
    }

private:
    IBindingInternal* get_internal() { return interface_cast<IBindingInternal>(binding_); }

    IBinding::Ptr binding_;
};

/**
 * @brief Creates an unconfigured binding (no source, no targets).
 */
inline Binding create_binding()
{
    return Binding(instance().create<IBinding>(ClassId::Binding));
}

/**
 * @brief Creates a property-to-property binding.
 *
 * While bound, reads from targets return the source property's current value.
 * In OneWay mode (default), writes to targets are rejected.
 * In TwoWay mode, writes to targets are forwarded to the source property,
 * and the source's on_changed propagates the new value back to all targets.
 *
 * @param source The source property whose value will be read.
 * @param type Immediate (default) fires on_changed synchronously; Deferred batches to update().
 * @param mode OneWay (default) or TwoWay.
 * @return The binding (not yet installed on any target).
 */
inline Binding create_binding(const IProperty::Ptr& source, InvokeType type = Auto,
                              BindingMode mode = BindingMode::OneWay)
{
    auto b = create_binding();
    if (auto* internal = interface_cast<IBindingInternal>(IBinding::Ptr(b))) {
        internal->set_source_property(source);
        internal->set_invoke_type(type);
        internal->set_binding_mode(mode);
    }
    return b;
}

/**
 * @brief Creates a function binding with explicit deps.
 *
 * The function is invoked with dependency values as FnArgs. The result is cached
 * and returned on reads. When any dependency changes, the cache is invalidated
 * and the targets' on_changed fires.
 *
 * @param fn The function that computes the value.
 * @param deps Properties passed as arguments to the function.
 * @param type Immediate (default) fires on_changed synchronously; Deferred batches to update().
 * @return The binding (not yet installed on any target).
 */
inline Binding create_binding(const IFunction::ConstPtr& fn,
                              std::initializer_list<IProperty::ConstPtr> deps,
                              InvokeType type = Auto)
{
    auto b = create_binding();
    if (auto* internal = interface_cast<IBindingInternal>(IBinding::Ptr(b))) {
        internal->set_source_function(fn, vector<IProperty::ConstPtr>(deps.begin(), deps.end()));
        internal->set_invoke_type(type);
    }
    return b;
}

/**
 * @brief Creates a function binding with auto-detected dependencies.
 *
 * The function reads properties directly (no FnArgs). Dependencies are
 * discovered automatically during evaluation and re-tracked on each
 * re-evaluation, so dynamic dependencies are supported.
 *
 * @param fn The function that computes the value (reads properties directly).
 * @param type Immediate (default) fires on_changed synchronously; Deferred batches to update().
 * @return The binding (not yet installed on any target).
 */
inline Binding create_binding(const IFunction::ConstPtr& fn, InvokeType type = Auto)
{
    auto b = create_binding();
    if (auto* internal = interface_cast<IBindingInternal>(IBinding::Ptr(b))) {
        internal->set_source_function(fn);
        internal->set_invoke_type(type);
    }
    return b;
}

/**
 * @brief Creates a binding to the property at @p path from @p root, following ObjectRef properties.
 *
 * @p path is property names separated by dots, such as "item.label": each name but the last
 * is an ObjectRef property, and the next name is read from the object it references. The
 * binding follows the references: when one points to another object, the binding reads from
 * that object. While a reference is empty or a name is missing, targets keep their own value.
 * @p root is held weakly.
 *
 * @return The binding (not yet installed on any target), or an empty one for an empty path.
 */
inline Binding create_path_binding(const IObject::Ptr& root, string_view path, InvokeType type = Auto)
{
    vector<string> names;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('.', start);
        if (end == string_view::npos) {
            end = path.size();
        }
        if (end == start) {
            return {};
        }
        names.push_back(string(path.substr(start, end - start)));
        start = end + 1;
    }
    if (names.empty() || !root) {
        return {};
    }
    IObject::WeakPtr weak(root);
    Callback fn([weak, names = std::move(names)](FnArgs) -> IAny::Ptr {
        IObject::Ptr object = weak.lock();
        for (size_t i = 0; object && i < names.size(); ++i) {
            auto* meta = interface_cast<IMetadata>(object);
            auto prop = meta ? meta->get_property(names[i]) : nullptr;
            if (!prop) {
                return {};
            }
            // Reading records the property, so the binding follows it.
            auto value = prop->get_value();
            if (i + 1 == names.size()) {
                return value ? value->clone() : nullptr;
            }
            auto* ref = interface_cast<IObjectRef>(value);
            object = ref ? ref->get_object() : nullptr;
        }
        return {};
    });
    return create_binding(IFunction::ConstPtr(fn), type);
}

/** @brief Creates a property binding and installs it on a single target. */
inline Binding create_binding(const IProperty::Ptr& target, const IProperty::Ptr& source,
                              InvokeType type = Auto, BindingMode mode = BindingMode::OneWay)
{
    auto b = create_binding(source, type, mode);
    b.add_target(target);
    return b;
}

/** @brief Creates a function binding with explicit deps and installs it on a single target. */
inline Binding create_binding(const IProperty::Ptr& target, const IFunction::ConstPtr& fn,
                              std::initializer_list<IProperty::ConstPtr> deps,
                              InvokeType type = Auto)
{
    auto b = create_binding(fn, deps, type);
    b.add_target(target);
    return b;
}

/** @brief Creates an auto-tracked function binding and installs it on a single target. */
inline Binding create_binding(const IProperty::Ptr& target, const IFunction::ConstPtr& fn,
                              InvokeType type = Auto)
{
    auto b = create_binding(fn, type);
    b.add_target(target);
    return b;
}

} // namespace velk

#endif // VELK_API_BINDING_H
