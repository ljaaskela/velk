#ifndef VELK_EXT_PLUGIN_H
#define VELK_EXT_PLUGIN_H

#include <velk/ext/object.h>
#include <velk/interface/intf_plugin.h>

namespace velk::ext {

/**
 * @brief CRTP base for plugin implementations.
 *
 * Inherits ext::Object to get ref counting, shared_ptr support, factory
 * registration, and IObject identity (get_class_uid, get_class_name, get_self).
 * Use VELK_CLASS_UID to specify a stable plugin identifier.
 *
 * Provides a default get_name() returning the class name. Override to
 * provide a custom human-readable name.
 *
 * Derived classes implement initialize and shutdown.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 */
template<class FinalClass>
class Plugin : public Object<FinalClass, IPlugin>
{
public:
    /** @brief Returns the class name by default. Override for a custom display name. */
    string_view get_name() const override { return this->get_class_name(); }
};

} // namespace velk::ext

#endif // VELK_EXT_PLUGIN_H
