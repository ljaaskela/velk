#ifndef CONTAINER_H
#define CONTAINER_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_container.h>

#include <vector>

namespace velk {

/**
 * @brief Default implementation of IContainer backed by a std::vector.
 */
class ContainerImpl final : public ext::ObjectCore<ContainerImpl, IContainer>
{
public:
    VELK_CLASS_UID(ClassId::Container);

    ReturnValue add(const IObject::Ptr& child) override;
    ReturnValue remove(const IObject::Ptr& child) override;
    ReturnValue insert(size_t index, const IObject::Ptr& child) override;
    ReturnValue replace(size_t index, const IObject::Ptr& child) override;
    IObject::Ptr get_at(size_t index) const override;
    vector<IObject::Ptr> get_all() const override;
    size_t size() const override;
    void clear() override;

private:
    std::vector<IObject::Ptr> children_;
};

} // namespace velk

#endif // CONTAINER_H
