#ifndef VELK_SRC_RAW_HIVE_H
#define VELK_SRC_RAW_HIVE_H

#include "page_allocator.h"

#include <velk/ext/core_object.h>
#include <velk/interface/hive/intf_hive.h>

#include <cstddef>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace velk {

/**
 * @brief Concrete implementation of IRawHive.
 *
 * Provides type-erased slot allocation using the same SimpleHivePage
 * infrastructure. Does not construct or destroy objects; callers are
 * responsible for placement-new and explicit destructor calls
 * (or use RawHive<T> for automatic lifetime).
 */
class RawHiveImpl final : public ext::ObjectCore<RawHiveImpl, IRawHive>
{
public:
    VELK_CLASS_UID(ClassId::RawHive);

    RawHiveImpl() = default;
    ~RawHiveImpl() override;

    /** @brief Initializes the hive for the given element UID, size, and alignment. */
    void init(Uid elementUid, size_t elementSize, size_t elementAlign);

    // IHive overrides
    Uid get_element_uid() const override;
    size_t size() const override;
    bool empty() const override;

    // IRawHive overrides
    void* allocate() override;
    void deallocate(void* ptr) override;
    bool contains(const void* ptr) const override;
    void for_each(void* context, RawVisitorFn visitor) const override;
    void clear(void* context, DestroyFn destroy) override;

private:
    void* slot_ptr(const SimpleHivePage& page, size_t index) const;
    void alloc_page(size_t capacity);

    mutable std::shared_mutex mutex_;
    Uid element_uid_;
    size_t slot_size_{0};
    size_t slot_align_{0};
    size_t live_count_{0};
    SimpleHivePage* current_page_{nullptr};
    std::vector<std::unique_ptr<SimpleHivePage>> pages_;
};

} // namespace velk

#endif // VELK_SRC_RAW_HIVE_H
