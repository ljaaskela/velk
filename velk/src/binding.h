#ifndef VELK_BINDING_IMPL_H
#define VELK_BINDING_IMPL_H

#include <velk/ext/core_object.h>
#include <velk/ext/event.h>
#include <velk/interface/intf_binding.h>
#include <velk/interface/types.h>
#include <velk/vector.h>

#include <memory>

namespace velk::impl {

struct BindingData
{
    virtual ~BindingData() = default;
    virtual IAny::ConstPtr evaluate() const = 0;
    virtual void subscribe(const IFunction::Ptr& handler) = 0;
    virtual void unsubscribe(const IFunction::Ptr& handler) = 0;
    virtual IProperty::Ptr get_source_property() const { return nullptr; }
    virtual IFunction::ConstPtr get_source_function() const { return nullptr; }
    virtual void invalidate() {}
    virtual ReturnValue write_to_source(const IAny& value) { (void)value; return ReturnValue::Fail; }
};

/**
 * @brief IAnyExtension that intercepts property reads to return values from a
 *        source property or computed function result.
 *
 * Can be installed on multiple target properties. All targets read the same
 * evaluated value. Mode-specific state (source property, function, deps, cache)
 * is held in a BindingData subclass.
 *
 * Writes (set_data/copy_from) return Fail while the binding is active.
 * Subscribes to source on_changed events and propagates them to all targets.
 */
class Binding final : public ext::ObjectCore<Binding, IBindingInternal>
{
public:
    VELK_CLASS_UID(ClassId::Binding, "Binding");

    ~Binding();

    // IBinding
    IProperty::Ptr get_source_property() const override;
    IFunction::ConstPtr get_source_function() const override;
    bool add_target(const IProperty::Ptr& target) override;
    bool remove_target(const IProperty::Ptr& target) override;
    void uninstall() override;

    // IBindingInternal
    void set_source_property(const IProperty::Ptr& source) override;
    void set_source_function(const IFunction::ConstPtr& fn, vector<IProperty::ConstPtr> deps) override;
    void set_source_function(const IFunction::ConstPtr& fn) override;
    void set_invoke_type(InvokeType type) override;
    void set_binding_mode(BindingMode mode) override;

    // IAnyExtension
    IAny::ConstPtr get_inner() const override;
    bool set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner) override;
    IAny::Ptr take_inner(IInterface& owner) override;

    // IAny overrides (intercept reads, block writes)
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

private:
    struct TargetEntry
    {
        IInterface::WeakPtr owner;
        IAny::Ptr inner;
    };

    IAny::ConstPtr evaluate() const;
    void ensure_handler();
    void subscribe();
    void unsubscribe();
    void on_source_changed();
    void queue_writeback(const IAny& value);
    IAny::Ptr first_inner() const;
    /** @brief Tells @p prop's owner, or else its listeners, that its value changed. */
    void notify_target(IProperty* prop) const;
    /** @brief Marks @p prop's value in its owner's state as out of date. */
    void mark_stale(IProperty* prop) const;

    std::unique_ptr<BindingData> data_;
    vector<TargetEntry> targets_;
    IFunction::Ptr handler_;
    InvokeType invoke_type_ = Auto;
    bool subscribed_ = false;
    BindingMode mode_ = BindingMode::OneWay;
    bool pending_writeback_ = false;
};

} // namespace velk::impl

#endif // VELK_BINDING_IMPL_H
