#ifndef VELK_IMPORTER_INTF_IMPORTER_PLUGIN_H
#define VELK_IMPORTER_INTF_IMPORTER_PLUGIN_H

#include <velk/interface/intf_plugin.h>
#include <velk/interface/intf_store.h>
#include <velk/string.h>
#include <velk/vector.h>

namespace velk {

/** @brief Result of an import operation. */
struct ImportResult
{
    IStore::Ptr store;       ///< The imported store, or null if parsing failed entirely.
    vector<string> errors;   ///< Collected errors (unknown classes, bad properties, parse errors, etc.).

    /** @brief Returns true if the ImportResult contains a valid IStore::Ptr and no errors. */
    explicit operator bool() const { return store && errors.empty(); }
};

/**
 * @brief Interface for a store importer.
 *
 * Chain: IInterface -> IStoreImporter
 */
class IStoreImporter : public Interface<IStoreImporter>
{
public:
    /**
     * @brief Imports @p text and returns a populated IStore with any errors.
     * @param source Where @p text came from, usually the file's path. Recorded for each
     *               imported object while source tracking is on (IImporterPlugin::set_track_sources).
     */
    virtual ImportResult import_from(string_view text, string_view source) const = 0;

    /** @brief Imports @p text with no source. */
    ImportResult import_from(string_view text) const { return import_from(text, {}); }
};

/** @brief Where an imported object came from. Empty when the object was not tracked. */
struct ImportSource
{
    string source; ///< The source passed to import_from, usually the file's path.
    string entry;  ///< The object's entry in the file: "objects/<id>", "resources/<id>" or "attachments/<id>".

    /** @brief Returns true if the object was tracked. */
    explicit operator bool() const { return !entry.empty(); }
};

/**
 * @brief Extended plugin interface for the importer plugin.
 *
 * Provides global configuration that applies to all importers.
 *
 * Chain: IInterface -> IImporterPlugin
 */
/**
 * @brief Converts single values to and from store-file JSON text.
 *
 * Implemented by the JSON importer (ClassId::JsonImporter). Built-in types are
 * converted directly; others through IImporterTypeExtension and
 * IExporterTypeExtension. Object references are not values here.
 */
class IJsonValueCodec : public Interface<IJsonValueCodec>
{
public:
    /** @brief @p value, of type @p type, as store-file JSON text; empty when it cannot be written. */
    virtual string to_json(const IAny& value, Uid type) const = 0;

    /** @brief A value of @p type read from store-file JSON text, or null when it cannot be read. */
    virtual IAny::Ptr from_json(string_view json, Uid type) const = 0;
};

class IImporterPlugin : public Interface<IImporterPlugin>
{
public:
    /**
     * @brief Registers a custom class name alias for use in import class fields.
     *
     * Not needed for normal use. Importers resolve class names automatically
     * via the type registry (UUID strings and ClassInfo::name). This method exists
     * for application-level code that needs to map custom names (e.g. "myapp.Widget")
     * to class UIDs that have different registered names.
     */
    virtual void register_class_alias(string_view alias, Uid class_uid) = 0;

    /** @brief Resolves a class name or UUID string to a class UID, or zero Uid if not found. */
    virtual Uid resolve_class(string_view name) const = 0;

    /**
     * @brief Turns source tracking on or off. Off by default.
     *
     * While it is on, create_json_importer() returns an importer that records, for each
     * object it creates with an id, the source and the object's entry in the file
     * (source_of). Importers created while it is off record nothing. Turn it on before
     * any content is imported.
     */
    virtual void set_track_sources(bool on) = 0;

    /** @brief Returns true if source tracking is on. */
    virtual bool track_sources() const = 0;

    /**
     * @brief Where @p object was imported from, or an empty ImportSource.
     *
     * Only objects imported while source tracking was on have a source. Tracking holds
     * objects weakly, so it does not keep them alive.
     */
    virtual ImportSource source_of(const IObject& object) const = 0;
};

} // namespace velk

#endif // VELK_IMPORTER_INTF_IMPORTER_PLUGIN_H
