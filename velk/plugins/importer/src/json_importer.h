#ifndef VELK_IMPORTER_JSON_IMPORTER_H
#define VELK_IMPORTER_JSON_IMPORTER_H

#include <velk/json.h>

#include <velk/api/binding.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_exporter_extension.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/member_desc.h>
#include <velk/interface/types.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>

#include <velk/string.h>

#include <unordered_map>

namespace velk {

class JsonImporter : public ext::ObjectCore<JsonImporter, IStoreImporter, IJsonValueCodec>
{
public:
    VELK_CLASS_UID(ClassId::JsonImporter, "JsonImporter");

    ImportResult import_from(string_view json, string_view source) const override;

    // IJsonValueCodec
    string to_json(const IAny& value, Uid type) const override;
    IAny::Ptr from_json(string_view json, Uid type) const override;

private:
    friend class ImportResolver;

    /** @brief Deferred object reference to resolve after all objects are created. */
    struct DeferredRef
    {
        IObject::Ptr object;
        const ClassInfo* info;
        string property_name;
        const JsonValue* ref_node;
    };

    struct ImportContext
    {
        std::unordered_map<string, IObject::Ptr> name_to_object;
        std::unordered_map<IObject*, string> object_to_name;
        std::unordered_map<IObject*, const ClassInfo*> object_to_class_info;
        mutable vector<DeferredRef> deferred_refs;
    };

    Uid resolve_class(string_view class_str, ImportResult& result) const;
    IObject::Ptr create_object(const JsonValue& obj_node, const ImportContext& ctx,
                               ImportResult& result) const;
    void set_properties(IObject& obj, const JsonValue& props, const ClassInfo& info,
                        const ImportContext& ctx, ImportResult& result) const;
    /** @brief Writes @p val, converted to the property's type; returns true when it was written. */
    bool set_property_value(IPropertyInternal& pi, const PropertyKind& pk, const JsonValue& val) const;
    void register_imported_object(ImportContext& ctx, IObject::Ptr obj, const JsonValue& obj_node,
                                  ImportResult& result) const;
    void build_hierarchies(const JsonValue& hierarchies, IStore& store, ImportResult& result) const;
    void build_hierarchy(string_view name, const JsonValue& tree_json, IStore& store,
                         ImportResult& result) const;
    void resolve_references(IStore& store, const ImportContext& ctx,
                            ImportResult& result) const;
    void resolve_object_ref(IMetadata& meta, string_view name, const JsonValue& ref_node,
                            IStore& store, const ImportContext& ctx, ImportResult& result) const;
    void resolve_inline_binding(IMetadata& meta, string_view name, string_view ref_path,
                                IStore& store, const ImportContext& ctx, ImportResult& result) const;
    void create_bindings(IStore& store, const JsonValue& root, const ImportContext& ctx,
                         ImportResult& result) const;
    void parse_binding(const JsonValue& binding_node, IStore& store, const ImportContext& ctx,
                       ImportResult& result) const;
    IObject::Ptr resolve_object(IStore& store, const ImportContext& ctx,
                                string_view path) const;
    IObject::Ptr resolve_object_by_path(IStore& store, const ImportContext& ctx,
                                        string_view path) const;
    IProperty::Ptr resolve_property(IStore& store, const ImportContext& ctx,
                                    string_view path) const;
    /**
     * @brief A binding source: the property at @p path, or a path through ObjectRef properties
     *        ("row.item.label": object "row", then "item", then "label" on what it references).
     * @return The binding, not installed on any target; empty when nothing resolves.
     */
    Binding resolve_binding_source(IStore& store, const ImportContext& ctx, string_view path,
                                   InvokeType invoke, BindingMode mode) const;
    void process_attachments(const JsonValue& root, IStore& store, ImportContext& ctx,
                             ImportResult& result) const;
    void process_resource_protocols(const JsonValue& root, ImportResult& result) const;
    void process_resources(const JsonValue& root, IStore& store, ImportContext& ctx,
                           ImportResult& result) const;
    void dispatch_extensions(const JsonValue& root, IStore& store, const ImportContext& ctx) const;
    void discover_type_extensions() const;
    /** @brief Writes @p value, of @p type, to @p out. Returns false for types it cannot write. */
    bool write_value(const IAny& value, Uid type, IExportData& out) const;

    mutable std::unordered_map<Uid, IImporterTypeExtension::Ptr> type_extensions_;
    mutable std::unordered_map<Uid, IExporterTypeExtension::Ptr> exporter_extensions_;
};

} // namespace velk

#endif // VELK_IMPORTER_JSON_IMPORTER_H
