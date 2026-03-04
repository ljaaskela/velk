#ifndef VELK_EXT_ANY_EXTENSION_H
#define VELK_EXT_ANY_EXTENSION_H

#include <velk/ext/any.h>
#include <velk/interface/intf_any_extension.h>

namespace velk::detail {

template <class T>
constexpr bool has_iany_extension_in_chain()
{
    if constexpr (std::is_same_v<T, IAnyExtension>) {
        return true;
    } else if constexpr (std::is_same_v<T, IInterface>) {
        return false;
    } else {
        return has_iany_extension_in_chain<typename T::ParentInterface>();
    }
}

} // namespace velk::detail

namespace velk::ext {

template <bool HasExt, class FinalClass, class... Interfaces>
struct AnyExtensionBase;

template <class FinalClass, class... Interfaces>
struct AnyExtensionBase<true, FinalClass, Interfaces...>
{
    using type = ObjectCore<FinalClass, Interfaces...>;
};

template <class FinalClass, class... Interfaces>
struct AnyExtensionBase<false, FinalClass, Interfaces...>
{
    using type = ObjectCore<FinalClass, IAnyExtension, Interfaces...>;
};

/**
 * @brief CRTP base for IAnyExtension implementations.
 *
 * Conditionally prepends IAnyExtension to the interface pack. If any interface
 * already has IAnyExtension in its parent chain, it is not added redundantly.
 *
 * Provides default passthrough implementations for all IAny methods that delegate
 * to the inner IAny. Subclasses override only the methods they intercept.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces beyond IAnyExtension.
 */
template <class FinalClass, class... Interfaces>
class AnyExtension
    : public AnyExtensionBase<(::velk::detail::has_iany_extension_in_chain<Interfaces>() || ...), FinalClass,
                              Interfaces...>::type
{
public: // IAnyExtension
    IAny::ConstPtr get_inner() const override { return inner_; }
    IAny::Ptr take_inner(IInterface&) override { return std::move(inner_); }
    void set_inner(IAny::Ptr inner, const IInterface::WeakPtr&) override { inner_ = std::move(inner); }

public: // IAny passthrough defaults
    array_view<Uid> get_compatible_types() const override
    {
        return inner_ ? inner_->get_compatible_types() : array_view<Uid>{};
    }

    size_t get_data_size(Uid type) const override { return inner_ ? inner_->get_data_size(type) : 0; }

    ReturnValue get_data(void* to, size_t toSize, Uid type) const override
    {
        return inner_ ? inner_->get_data(to, toSize, type) : ReturnValue::Fail;
    }

    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override
    {
        return inner_ ? inner_->set_data(from, fromSize, type) : ReturnValue::Fail;
    }

    ReturnValue copy_from(const IAny& other) override
    {
        return inner_ ? inner_->copy_from(other) : ReturnValue::Fail;
    }

    IAny::Ptr clone() const override { return inner_ ? inner_->clone() : nullptr; }

protected:
    IAny::Ptr inner_;
};

} // namespace velk::ext

#endif // VELK_EXT_ANY_EXTENSION_H
