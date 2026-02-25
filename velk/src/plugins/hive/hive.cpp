#include "hive.h"

#include <velk/api/velk.h>

namespace velk {

void Hive::init(Uid classUid)
{
    element_class_uid_ = classUid;
}

Uid Hive::get_element_class_uid() const
{
    return element_class_uid_;
}

size_t Hive::size() const
{
    return live_count_;
}

bool Hive::empty() const
{
    return live_count_ == 0;
}

IObject::Ptr Hive::add()
{
    auto obj = interface_pointer_cast<IObject>(instance().create(element_class_uid_));
    if (!obj) {
        return {};
    }

    if (!freelist_.empty()) {
        size_t idx = freelist_.back();
        freelist_.pop_back();
        objects_[idx] = obj;
    } else {
        objects_.push_back(obj);
    }

    ++live_count_;
    return obj;
}

ReturnValue Hive::remove(IObject& object)
{
    for (size_t i = 0; i < objects_.size(); ++i) {
        if (objects_[i].get() == &object) {
            objects_[i].reset();
            freelist_.push_back(i);
            --live_count_;
            return ReturnValue::Success;
        }
    }
    return ReturnValue::Fail;
}

bool Hive::contains(const IObject& object) const
{
    for (auto& obj : objects_) {
        if (obj.get() == &object) {
            return true;
        }
    }
    return false;
}

void Hive::for_each(void* context, VisitorFn visitor) const
{
    for (auto& obj : objects_) {
        if (obj) {
            if (!visitor(context, *obj)) {
                return;
            }
        }
    }
}

} // namespace velk
