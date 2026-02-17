#include "velk_impl.h"

#include "velk_export.h"

namespace velk {

VELK_EXPORT IVelk &instance()
{
    static VelkImpl r;
    return *(r.get_interface<IVelk>());
}

} // namespace velk
