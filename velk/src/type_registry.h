#ifndef VELK_TYPE_REGISTRY_H
#define VELK_TYPE_REGISTRY_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_log.h>
#include <velk/interface/intf_type_registry.h>

#include <vector>

namespace velk {

/**
 * @brief Concrete implementation of ITypeRegistry.
 *
 * Maintains a sorted vector of type factories keyed by class UID.
 * Owned as a stack member by VelkInstance.
 */
class TypeRegistry final : public ext::InterfaceDispatch<ITypeRegistry>
{
public:
    explicit TypeRegistry(ILog& log);

    // ITypeRegistry overrides
    ReturnValue register_type(const IObjectFactory& factory) override;
    ReturnValue unregister_type(const IObjectFactory& factory) override;
    const ClassInfo* get_class_info(Uid classUid) const override;

    /** @brief Creates an instance of a registered type by its UID. */
    IInterface::Ptr create(Uid uid) const;
    /** @brief Sets the current owner context for subsequent register_type calls. */
    void set_owner(Uid uid);
    /** @brief Erases all entries owned by the given plugin UID. */
    void sweep_owner(Uid uid);

private:
    /** @brief Registry entry mapping a class UID to its factory. */
    struct Entry
    {
        Uid uid;                       ///< Class UID.
        const IObjectFactory* factory; ///< Factory that creates instances of this class.
        Uid owner;                     ///< Plugin that registered this type (Uid{} = builtin).
        bool operator<(const Entry& o) const { return uid < o.uid; }
    };

    /** @brief Finds the factory for the given class UID, or nullptr if not registered. */
    const IObjectFactory* find(Uid uid) const;

    std::vector<Entry> types_; ///< Sorted registry of class factories.
    Uid current_owner_;        ///< Owner context for type registration.
    ILog& log_;
};

} // namespace velk

#endif // VELK_TYPE_REGISTRY_H
