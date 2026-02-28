#ifndef VELK_INTF_ARRAY_PROPERTY_H
#define VELK_INTF_ARRAY_PROPERTY_H

#include <velk/interface/intf_property.h>

namespace velk {

/** @brief Interface for array properties with element-level access. Inherits IProperty for whole-vector
 * get/set. */
class IArrayProperty : public Interface<IArrayProperty, IProperty>
{
public:
    /** @brief Returns the number of elements in the array. */
    virtual size_t array_size() const = 0;
    /** @brief Reads the element at @p index into @p out. */
    virtual ReturnValue get_at(size_t index, IAny& out) const = 0;
    /** @brief Writes @p value to the element at @p index. */
    virtual ReturnValue set_at(size_t index, const IAny& value) = 0;
    /** @brief Appends @p value to the end of the array. */
    virtual ReturnValue push_back(const IAny& value) = 0;
    /** @brief Erases the element at @p index. */
    virtual ReturnValue erase_at(size_t index) = 0;
    /** @brief Removes all elements from the array. */
    virtual void clear_array() = 0;
};

} // namespace velk

#endif // VELK_INTF_ARRAY_PROPERTY_H
