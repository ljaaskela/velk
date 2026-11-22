#include "data_plugin.h"

#include "hierarchy_source.h"
#include "object_list.h"

#include <velk/ext/any.h>

namespace velk {

ReturnValue DataPlugin::initialize(IVelk& velk, PluginConfig&)
{
    auto rv = register_type<impl::ObjectList>(velk);
    rv &= register_type<impl::HierarchySource>(velk);
    rv &= velk.type_registry().register_type<ext::AnyValue<ItemChange>>();
    return rv;
}

} // namespace velk
