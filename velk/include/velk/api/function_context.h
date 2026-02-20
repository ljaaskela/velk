#ifndef VELK_API_FUNCTION_CONTEXT_H
#define VELK_API_FUNCTION_CONTEXT_H

#include <velk/api/any.h>
#include <velk/interface/intf_function.h>

namespace velk {

/**
 * @brief Lightweight view over FnArgs for convenient indexed access.
 *
 * Usage in callbacks:
 * @code
 * ReturnValue fn_reset(FnArgs args) override {
 *     if (auto ctx = FunctionContext(args, 1)) { // Expecting 1 argument 
 *         auto a = Any<const float>(ctx[0]);
 *     }
 * }
 * @endcode
 */
class FunctionContext {
    FnArgs args_;

public:
    /** @brief Default constructor (empty context). */
    FunctionContext() = default;

    /**
     * @brief Constructs a context from @p args, accepting only if the count matches @p expectedCount.
     * @param args The function arguments view.
     * @param expectedCount Required number of arguments. If @p args.count differs, the context remains empty.
     */
    FunctionContext(FnArgs args, size_t expectedCount)
    {
        if (args.count == expectedCount) {
            args_ = args;
        }
    }

    /**
     * @brief Returns the argument at index @p i, or nullptr if out of range.
     * @param i Argument index.
     */
    const IAny *arg(size_t i) const { return i < args_.count ? args_[i] : nullptr; }

    /** @brief Returns the number of arguments. */
    size_t size() const { return args_.count; }

    /** @brief Returns true if the context has any arguments. */
    operator bool() const { return args_.count > 0; }

    /**
     * @brief Returns the argument at index @p i as a typed Any wrapper.
     * @tparam T The value type to wrap. Automatically made const-qualified.
     * @param i Argument index.
     * @return A read-only Any wrapping the argument, or an empty Any if out of range.
     */
    template<class T>
    Any<const std::remove_const_t<T>> arg(size_t i) const
    {
        using AnyType = Any<const std::remove_const_t<T>>;
        return AnyType(i < args_.count ? args_[i] : nullptr);
    }
};

} // namespace velk

#endif // VELK_API_FUNCTION_CONTEXT_H
