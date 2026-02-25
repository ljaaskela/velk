#ifndef VELK_PLUGINS_HIVE_H
#define VELK_PLUGINS_HIVE_H

#include <velk/ext/core_object.h>
#include <velk/interface/hive/intf_hive.h>

#include <vector>

namespace velk {

/**
 * @brief Concrete implementation of IHive.
 *
 * Stores objects of a single class ID in a vector with slot reuse via a free list.
 */
class Hive final : public ext::ObjectCore<Hive, IHive>
{
public:
    VELK_CLASS_UID(ClassId::Hive);

    Hive() = default;

    /** @brief Initializes the hive for the given class UID. */
    void init(Uid classUid);

    // IHive overrides
    Uid get_element_class_uid() const override;
    size_t size() const override;
    bool empty() const override;
    IObject::Ptr add() override;
    ReturnValue remove(IObject& object) override;
    bool contains(const IObject& object) const override;
    void for_each(void* context, VisitorFn visitor) const override;

private:
    Uid element_class_uid_;
    std::vector<IObject::Ptr> objects_;
    std::vector<size_t> freelist_;
    size_t live_count_{};
};

} // namespace velk

#endif // VELK_PLUGINS_HIVE_H
