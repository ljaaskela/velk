#ifndef VELK_INTF_EXPORTER_EXTENSION_H
#define VELK_INTF_EXPORTER_EXTENSION_H

#include <velk/array_view.h>
#include <velk/interface/intf_any.h>
#include <velk/interface/intf_interface.h>
#include <velk/string_view.h>

#include <cstdint>

namespace velk {

/**
 * @brief Receives a value as it is written out: the format-neutral counterpart of IImportData.
 *
 * Objects take key() before each member value. The JSON writer is one implementation.
 *
 * Chain: IInterface -> IExportData
 */
class IExportData : public Interface<IExportData>
{
public:
    virtual void null() = 0;
    virtual void boolean(bool value) = 0;
    virtual void integer(int64_t value) = 0;
    /** @brief A float, written with the fewest digits that read back to the same float. */
    virtual void number(float value) = 0;
    /** @brief A double, written with the fewest digits that read back to the same double. */
    virtual void number(double value) = 0;
    virtual void text(string_view value) = 0;
    virtual void begin_object() = 0;
    virtual void key(string_view name) = 0;
    virtual void end_object() = 0;
    virtual void begin_array() = 0;
    virtual void end_array() = 0;
};

/**
 * @brief Extension point for writing custom types, the counterpart of IImporterTypeExtension.
 *
 * Any plugin can register a class implementing this interface; a type's
 * extension usually implements both. What it writes must read back through
 * the type's IImporterTypeExtension.
 *
 * Definition only: no implementations live in velk.dll.
 *
 * Chain: IInterface -> IExporterTypeExtension
 */
class IExporterTypeExtension : public Interface<IExporterTypeExtension>
{
public:
    /** @brief Returns the type UIDs this extension can serialize. */
    virtual array_view<Uid> supported_types() const = 0;

    /** @brief Writes @p value, of type @p type_uid, to @p out. */
    virtual ReturnValue serialize(Uid type_uid, const IAny& value, IExportData& out) const = 0;
};

} // namespace velk

#endif // VELK_INTF_EXPORTER_EXTENSION_H
