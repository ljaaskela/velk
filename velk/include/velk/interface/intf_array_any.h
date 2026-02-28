#ifndef VELK_INTF_ARRAY_ANY_H
#define VELK_INTF_ARRAY_ANY_H

#include <velk/interface/intf_any.h>

namespace velk {

/**
 * @brief Extension of IAny for array/vector values with element-level access.
 *
 * Implemented by ArrayAnyRef<T> to provide typed element operations without
 * requiring the caller to know the element type at compile time.
 */
class IArrayAny : public Interface<IArrayAny, IAny>
{
public:
    /** @brief Returns the number of elements. */
    virtual size_t array_size() const = 0;
    /** @brief Reads element at @p index into @p out. */
    virtual ReturnValue get_at(size_t index, IAny& out) const = 0;
    /** @brief Writes @p value to element at @p index. */
    virtual ReturnValue set_at(size_t index, const IAny& value) = 0;
    /** @brief Appends @p value to the end. */
    virtual ReturnValue push_back(const IAny& value) = 0;
    /** @brief Erases element at @p index. */
    virtual ReturnValue erase_at(size_t index) = 0;
    /** @brief Removes all elements. */
    virtual void clear_array() = 0;
    /** @brief Bulk-sets contents from a raw element buffer. */
    virtual ReturnValue set_from_buffer(const void* data, size_t count, Uid elementType) = 0;
};

} // namespace velk

#endif // VELK_INTF_ARRAY_ANY_H
