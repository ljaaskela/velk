#include "json_importer.h"

#include "json_import_data.h"

#include <velk/api/binding.h>
#include <velk/api/hierarchy.h>
#include <velk/api/math_types.h>
#include <velk/api/store.h>
#include <velk/api/velk.h>
#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/interface/intf_object_ref.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/interface/resource/intf_resource_protocol.h>
#include <velk/interface/resource/intf_resource_store.h>
#include <velk/string.h>

#include <unordered_set>

namespace velk {

namespace {

void add_error(ImportResult& result, const char* msg)
{
    result.errors.push_back(::velk::string(msg));
}

void add_error(ImportResult& result, const ::velk::string_view msg)
{
    result.errors.push_back(msg);
}

const MemberDesc* find_property_desc(const ClassInfo& info, string_view name)
{
    for (size_t i = 0; i < info.members.size(); i++) {
        if (info.members[i].name == name && (info.members[i].kind == MemberKind::Property ||
                                             info.members[i].kind == MemberKind::ArrayProperty)) {
            return &info.members[i];
        }
    }
    return nullptr;
}

} // namespace

ImportResult JsonImporter::import_from(string_view json) const
{
    ImportResult result;

    JsonValue root;
    string parse_error;
    if (!json_parse(json.data(), json.size(), root, parse_error)) {
        add_error(result, parse_error);
        return result;
    }
    if (root.type() != JsonType::Object) {
        add_error(result, "root must be a JSON object");
        return result;
    }

    result.store = ::velk::instance().create<IStore>(ClassId::Store);
    if (!result.store) {
        add_error(result, "failed to create store");
        return result;
    }

    // Discover type extensions for custom type deserialization
    discover_type_extensions();

    // Register resource protocols (must be before resources and objects)
    process_resource_protocols(root, result);

    ImportContext ctx;

    // Parse resources (before objects, so refs from objects can find them)
    process_resources(root, *result.store, ctx, result);

    // Parse objects
    auto* objects = root.find("objects");
    if (objects && objects->type() == JsonType::Array) {
        for (auto& obj_node : objects->as_array()) {
            if (obj_node.type() != JsonType::Object) {
                continue;
            }
            auto* id_val = obj_node.find("id");
            if (!id_val || id_val->type() != JsonType::String) {
                add_error(result, "object missing 'id' string field");
                continue;
            }
            auto obj = create_object(obj_node, ctx, result);
            if (obj) {
                const auto& id_str = id_val->as_string();
                result.store->add(id_str, obj);
                register_imported_object(ctx, obj, obj_node, result);
            }
        }
    }

    auto& store = *result.store;

    // Build hierarchies
    auto* hierarchies = root.find("hierarchies");
    if (hierarchies && hierarchies->type() == JsonType::Object) {
        build_hierarchies(*hierarchies, store, result);
    }

    // Process attachments
    process_attachments(root, store, ctx, result);

    // Resolve deferred object references and inline bindings
    resolve_references(store, ctx, result);

    // Create bindings (top-level array + inline scalar refs handled in resolve_references)
    create_bindings(store, root, ctx, result);

    // Dispatch remaining top-level keys to registered importer extensions
    dispatch_extensions(root, store, ctx);

    return result;
}

Uid JsonImporter::resolve_class(string_view class_str, ImportResult& result) const
{
    // Delegate to the plugin which handles UUID, aliases, and class name lookup
    auto plugin = ::velk::instance().plugin_registry().find_plugin(PluginId::ImporterPlugin);
    auto* ip = interface_cast<IImporterPlugin>(plugin);
    if (ip) {
        Uid uid = ip->resolve_class(class_str);
        if (uid != Uid{}) {
            return uid;
        }
    }

    add_error(result, string("unknown class: ") + class_str);
    return {};
}

IObject::Ptr JsonImporter::create_object(const JsonValue& obj_node, const ImportContext& ctx,
                                         ImportResult& result) const
{
    auto* class_val = obj_node.find("class");
    if (!class_val || class_val->type() != JsonType::String) {
        add_error(result, "object missing 'class' string field");
        return {};
    }

    Uid class_uid = resolve_class(class_val->as_string(), result);
    if (class_uid == Uid{}) {
        return {};
    }

    auto obj = ::velk::instance().create<IObject>(class_uid);
    if (!obj) {
        add_error(result, string("failed to create object of class: ") + class_val->as_string());
        return {};
    }

    // Get class info for property type dispatch
    auto* info = ::velk::instance().type_registry().get_class_info(class_uid);
    if (!info) {
        return obj;
    }

    // Set properties
    auto* props = obj_node.find("properties");
    if (props && props->type() == JsonType::Object) {
        set_properties(*obj, *props, *info, ctx, result);
    }

    return obj;
}

void JsonImporter::set_properties(IObject& obj, const JsonValue& props, const ClassInfo& info,
                                  const ImportContext& ctx, ImportResult& result) const
{
    auto* meta = interface_cast<IMetadata>(&obj);
    if (!meta) {
        return;
    }

    for (auto& [name, val] : props.as_object()) {
        string_view name_sv = name;
        const MemberDesc* desc = find_property_desc(info, name_sv);
        if (!desc) {
            add_error(result, string("unknown property '") + name + "' on class '" + info.name + "'");
            continue;
        }

        auto prop = meta->get_property(name_sv);
        if (!prop) {
            continue;
        }

        auto* pi = interface_cast<IPropertyInternal>(prop);
        if (!pi) {
            continue;
        }

        auto* pk = desc->propertyKind();
        if (!pk) {
            continue;
        }

        // Handle object form: { "value": ..., "flags": ... }
        const JsonValue* value_node = &val;
        if (val.type() == JsonType::Object) {
            if (val.find("ref") || val.find("bind")) {
                // Defer ref/bind resolution until all objects are created
                DeferredRef dr;
                dr.object = obj.get_self();
                dr.info = &info;
                dr.property_name = string(name);
                dr.ref_node = &val;
                ctx.deferred_refs.push_back(std::move(dr));
                continue;
            }
            auto* inner = val.find("value");
            if (inner) {
                value_node = inner;
            }
        }

        set_property_value(*pi, *pk, *value_node);
    }
}

bool JsonImporter::set_property_value(IPropertyInternal& pi, const PropertyKind& pk,
                                      const JsonValue& val) const
{
    Uid typeUid = pk.typeUid;

    if (typeUid == type_uid<float>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        float v = static_cast<float>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<double>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        double v = val.as_number();
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<int32_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        int32_t v = static_cast<int32_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<uint32_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        uint32_t v = static_cast<uint32_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<int64_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        int64_t v = static_cast<int64_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<uint64_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        uint64_t v = static_cast<uint64_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<uint8_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        uint8_t v = static_cast<uint8_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<int8_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        int8_t v = static_cast<int8_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<uint16_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        uint16_t v = static_cast<uint16_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<int16_t>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        int16_t v = static_cast<int16_t>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<bool>()) {
        if (val.type() != JsonType::Bool) {
            return false;
        }
        bool v = val.as_bool();
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<int>()) {
        if (val.type() != JsonType::Number) {
            return false;
        }
        int v = static_cast<int>(val.as_number());
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (typeUid == type_uid<::velk::string>()) {
        if (val.type() != JsonType::String) {
            return false;
        }
        auto v = val.as_string();
        return succeeded(pi.set_data(&v, sizeof(v), typeUid));
    } else if (val.type() == JsonType::Array) {
        auto& arr = val.as_array();
        if (typeUid == type_uid<vec2>() && arr.size() == 2) {
            vec2 v{static_cast<float>(arr[0].as_number()), static_cast<float>(arr[1].as_number())};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<vec3>() && arr.size() == 3) {
            vec3 v{static_cast<float>(arr[0].as_number()),
                   static_cast<float>(arr[1].as_number()),
                   static_cast<float>(arr[2].as_number())};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<vec4>() && arr.size() == 4) {
            vec4 v{static_cast<float>(arr[0].as_number()),
                   static_cast<float>(arr[1].as_number()),
                   static_cast<float>(arr[2].as_number()),
                   static_cast<float>(arr[3].as_number())};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<quat>() && arr.size() == 4) {
            quat v{static_cast<float>(arr[0].as_number()),
                   static_cast<float>(arr[1].as_number()),
                   static_cast<float>(arr[2].as_number()),
                   static_cast<float>(arr[3].as_number())};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<size>() && arr.size() >= 2) {
            size v{static_cast<float>(arr[0].as_number()),
                   static_cast<float>(arr[1].as_number()),
                   arr.size() >= 3 ? static_cast<float>(arr[2].as_number()) : 0.f};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<rect>() && arr.size() == 4) {
            rect v{static_cast<float>(arr[0].as_number()),
                   static_cast<float>(arr[1].as_number()),
                   static_cast<float>(arr[2].as_number()),
                   static_cast<float>(arr[3].as_number())};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<color>() && arr.size() >= 3) {
            color v{static_cast<float>(arr[0].as_number()),
                    static_cast<float>(arr[1].as_number()),
                    static_cast<float>(arr[2].as_number()),
                    arr.size() >= 4 ? static_cast<float>(arr[3].as_number()) : 1.f};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<mat4>() && arr.size() == 16) {
            mat4 v{};
            for (int i = 0; i < 16; ++i) {
                v.m[i] = static_cast<float>(arr[i].as_number());
            }
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        }
    } else if (val.type() == JsonType::Object) {
        auto f = [&](string_view key) -> float {
            auto* n = val.find(key);
            return n ? static_cast<float>(n->as_number()) : 0.f;
        };
        auto f1 = [&](string_view key, float def) -> float {
            auto* n = val.find(key);
            return n ? static_cast<float>(n->as_number()) : def;
        };
        if (typeUid == type_uid<vec2>()) {
            vec2 v{f("x"), f("y")};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<vec3>()) {
            vec3 v{f("x"), f("y"), f("z")};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<vec4>()) {
            vec4 v{f("x"), f("y"), f("z"), f("w")};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<quat>()) {
            // Accept an object as either a raw quaternion {x,y,z,w} or an
            // Euler-degree triple {x,y,z} (no `w`), matching the legacy
            // ITrs authoring convention.
            auto* w_node = val.find(string_view{"w"});
            if (w_node) {
                quat v{f("x"), f("y"), f("z"), f("w")};
                return succeeded(pi.set_data(&v, sizeof(v), typeUid));
            } else {
                constexpr float kDegToRad = 0.0174532925199433f;
                quat v = quat::from_euler({
                    f("x") * kDegToRad,
                    f("y") * kDegToRad,
                    f("z") * kDegToRad,
                });
                return succeeded(pi.set_data(&v, sizeof(v), typeUid));
            }
        } else if (typeUid == type_uid<size>()) {
            size v{f("width"), f("height"), f("depth")};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<rect>()) {
            rect v{f("x"), f("y"), f("width"), f("height")};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<color>()) {
            color v{f("r"), f("g"), f("b"), f1("a", 1.f)};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        } else if (typeUid == type_uid<aabb>()) {
            auto* pos_node = val.find(string_view{"position"});
            auto* ext_node = val.find(string_view{"extent"});
            auto fp = [](const JsonValue* node, string_view key) -> float {
                if (!node) {
                    return 0.f;
                }
                auto* n = node->find(key);
                return n ? static_cast<float>(n->as_number()) : 0.f;
            };
            aabb v{{fp(pos_node, "x"), fp(pos_node, "y"), fp(pos_node, "z")},
                   {fp(ext_node, "width"), fp(ext_node, "height"), fp(ext_node, "depth")}};
            return succeeded(pi.set_data(&v, sizeof(v), typeUid));
        }
    }

    // Fallback: try registered type extensions for custom types
    auto ext_it = type_extensions_.find(typeUid);
    if (ext_it != type_extensions_.end()) {
        JsonImportData wrapped(val);
        auto result = ext_it->second->deserialize(typeUid, wrapped);
        if (result) {
            return succeeded(pi.set_value(*result));
        }
    }
    return false;
}

void JsonImporter::register_imported_object(ImportContext& ctx, IObject::Ptr obj, const JsonValue& obj_node,
                                            ImportResult& result) const
{
    auto* id_val = obj_node.find("id");
    const auto& id_str = id_val->as_string();

    ctx.name_to_object[id_str] = obj;
    auto* name_val = obj_node.find("name");
    if (name_val && name_val->type() == JsonType::String) {
        const auto& nm = name_val->as_string();
        ctx.name_to_object[nm] = obj;
        ctx.object_to_name[obj.get()] = nm;
    } else {
        ctx.object_to_name[obj.get()] = id_str;
    }

    auto* class_val = obj_node.find("class");
    if (class_val && class_val->type() == JsonType::String) {
        Uid uid = resolve_class(class_val->as_string(), result);
        if (uid != Uid{}) {
            ctx.object_to_class_info[obj.get()] = ::velk::instance().type_registry().get_class_info(uid);
        }
    }
}

void JsonImporter::build_hierarchies(const JsonValue& hierarchies, IStore& store, ImportResult& result) const
{
    for (auto& [name, hierarchy_val] : hierarchies.as_object()) {
        if (hierarchy_val.type() != JsonType::Object) {
            add_error(result, string("hierarchy '") + name + "' is not an object");
            continue;
        }
        build_hierarchy(name, hierarchy_val, store, result);
    }
}

void JsonImporter::build_hierarchy(string_view name, const JsonValue& tree_json, IStore& store,
                                   ImportResult& result) const
{
    auto hierarchy = ::velk::create_hierarchy();
    if (!hierarchy) {
        add_error(result, string("failed to create hierarchy '") + name + "'");
        return;
    }

    // Format: { "parent_id": ["child_id", ...], ... }
    // Root is a node that appears as a key but never in any child array.
    std::unordered_set<::velk::string> all_children;
    for (auto& [parent_id, children_val] : tree_json.as_object()) {
        if (children_val.type() != JsonType::Array) {
            continue;
        }
        for (auto& child_val : children_val.as_array()) {
            if (child_val.type() == JsonType::String) {
                all_children.insert(child_val.as_string());
            }
        }
    }

    string root_id;
    for (auto& [parent_id, children_val] : tree_json.as_object()) {
        if (all_children.find(parent_id) == all_children.end()) {
            root_id = parent_id;
            break;
        }
    }

    if (root_id.empty()) {
        if (!tree_json.as_object().empty()) {
            root_id = tree_json.as_object()[0].first;
        }
    }

    if (!root_id.empty()) {
        auto root_obj = store.find(root_id);
        if (root_obj) {
            hierarchy.set_root(root_obj);

            for (auto& [parent_id, children_val] : tree_json.as_object()) {
                if (children_val.type() != JsonType::Array) {
                    continue;
                }
                auto parent_obj = store.find(parent_id);
                if (!parent_obj) {
                    add_error(result,
                              string("hierarchy '") + name + "': parent object not found: " + parent_id);
                    continue;
                }
                for (auto& child_val : children_val.as_array()) {
                    if (child_val.type() != JsonType::String) {
                        continue;
                    }
                    auto child_obj = store.find(child_val.as_string());
                    if (child_obj) {
                        hierarchy.add(parent_obj, child_obj);
                    } else {
                        add_error(result,
                                  string("hierarchy '") + name +
                                      "': child object not found: " + child_val.as_string());
                    }
                }
            }
        } else {
            add_error(result, string("hierarchy '") + name + "': root object not found: " + root_id);
        }
    }

    // Add hierarchy to store with key "hierarchy:<name>"
    auto store_key = string("hierarchy:") + name;
    auto hierarchy_obj = interface_pointer_cast<IObject>(hierarchy.as_ptr<IHierarchy>());
    if (hierarchy_obj) {
        store.add(store_key, hierarchy_obj);
    }
}

IObject::Ptr JsonImporter::resolve_object(IStore& store, const ImportContext& ctx, string_view path) const
{
    if (path.empty()) {
        return {};
    }

    if (path[0] == '/') {
        return resolve_object_by_path(store, ctx, path);
    }

    // Namespace-qualified lookup: "resources.id" or "objects.id"
    string_view prefix_resources = "resources.";
    string_view prefix_objects = "objects.";

    if (path.size() > prefix_resources.size() &&
        string_view(path.data(), prefix_resources.size()) == prefix_resources) {
        auto id = path.substr(prefix_resources.size());
        auto store_key = string("resource:") + string(id);
        return store.find(store_key);
    }

    if (path.size() > prefix_objects.size() &&
        string_view(path.data(), prefix_objects.size()) == prefix_objects) {
        auto id = path.substr(prefix_objects.size());
        auto it = ctx.name_to_object.find(id);
        if (it != ctx.name_to_object.end()) {
            return it->second;
        }
        return store.find(id);
    }

    // Unqualified: try name map first (includes both ids and names), then store lookup
    auto it = ctx.name_to_object.find(path);
    if (it != ctx.name_to_object.end()) {
        return it->second;
    }
    return store.find(path);
}

IObject::Ptr JsonImporter::resolve_object_by_path(IStore& store, const ImportContext& ctx,
                                                  string_view path) const
{
    // Hierarchy path: /segment1/segment2/...
    vector<string> segments;
    size_t start = 1;
    while (start < path.size()) {
        auto end = path.find('/', start);
        if (end == string_view::npos) {
            end = path.size();
        }
        if (end > start) {
            segments.push_back(string(path.data() + start, end - start));
        }
        start = end + 1;
    }
    if (segments.empty()) {
        return {};
    }

    // Find hierarchy to walk
    IHierarchy* hierarchy = nullptr;
    size_t start_segment = 0;

    // Try first segment as hierarchy name
    auto h_key = string("hierarchy:") + segments[0];
    auto h_obj = store.find(h_key);
    if (h_obj) {
        hierarchy = interface_cast<IHierarchy>(h_obj);
        if (hierarchy) {
            start_segment = 1;
        }
    }

    // If not found by name, find any hierarchy in the store
    if (!hierarchy) {
        for (size_t i = 0; i < store.object_count(); i++) {
            auto obj = store.object_at(i);
            auto* h = interface_cast<IHierarchy>(obj);
            if (h) {
                hierarchy = h;
                break;
            }
        }
    }

    if (!hierarchy || segments.size() <= start_segment) {
        return {};
    }

    // Walk from root, matching names at each level
    auto current = hierarchy->root();
    if (!current) {
        return {};
    }

    // Match root against first segment
    auto root_it = ctx.object_to_name.find(current.get());
    if (root_it == ctx.object_to_name.end() || root_it->second != segments[start_segment]) {
        return {};
    }
    start_segment++;

    // Walk remaining segments through children
    for (size_t i = start_segment; i < segments.size(); i++) {
        IObject::Ptr found;
        auto children = hierarchy->children_of(current);
        for (auto& child : children) {
            auto it = ctx.object_to_name.find(child.get());
            if (it != ctx.object_to_name.end() && it->second == segments[i]) {
                found = child;
                break;
            }
        }
        if (!found) {
            return {};
        }
        current = found;
    }

    return current;
}

IProperty::Ptr JsonImporter::resolve_property(IStore& store, const ImportContext& ctx, string_view path) const
{
    auto dot = path.rfind('.');
    if (dot == string_view::npos) {
        return {};
    }

    auto obj_path = path.substr(0, dot);
    auto prop_name = path.substr(dot + 1);

    auto obj = resolve_object(store, ctx, obj_path);
    if (!obj) {
        return {};
    }

    auto* meta = interface_cast<IMetadata>(obj);
    if (!meta) {
        return {};
    }

    return meta->get_property(prop_name);
}

void JsonImporter::resolve_references(IStore& store, const ImportContext& ctx,
                                      ImportResult& result) const
{
    for (auto& ref : ctx.deferred_refs) {
        auto* meta = interface_cast<IMetadata>(ref.object);
        if (!meta || !ref.info) {
            continue;
        }

        string_view name_sv = ref.property_name;

        // "ref" is for object references only (ObjectRef properties)
        auto* ref_val = ref.ref_node->find("ref");
        if (ref_val && ref_val->type() == JsonType::String) {
            const MemberDesc* desc = find_property_desc(*ref.info, name_sv);
            if (!desc) {
                continue;
            }
            auto* pk = desc->propertyKind();
            if (!pk || pk->typeUid != ClassId::ObjectRef) {
                add_error(result,
                          string("property '") + ref.property_name +
                              "' is not an ObjectRef, use 'bind' for bindings");
                continue;
            }
            resolve_object_ref(*meta, ref.property_name, *ref.ref_node, store, ctx, result);
            continue;
        }

        // "bind" creates an inline one-way binding
        auto* bind_val = ref.ref_node->find("bind");
        if (bind_val && bind_val->type() == JsonType::String) {
            resolve_inline_binding(*meta, ref.property_name, bind_val->as_string(), store, ctx, result);
        }
    }
}

void JsonImporter::resolve_object_ref(IMetadata& meta, string_view name, const JsonValue& ref_node,
                                      IStore& store, const ImportContext& ctx, ImportResult& result) const
{
    auto* ref_val = ref_node.find("ref");
    const auto& ref_path = ref_val->as_string();

    auto resolved = resolve_object(store, ctx, ref_path);
    if (!resolved) {
        add_error(result,
                  string("unresolved object reference '") + ref_path + "' on property '" + name + "'");
        return;
    }

    // Get the existing ObjectRef from the property (backed by the State struct)
    auto pi = interface_pointer_cast<IPropertyInternal>(meta.get_property(name));
    if (!pi) {
        return;
    }
    auto existing = pi->get_any();
    auto* obj_ref = const_cast<IObjectRef*>(interface_cast<const IObjectRef>(existing));
    if (!obj_ref) {
        return;
    }

    auto* type_val = ref_node.find("type");
    if (type_val && type_val->type() == JsonType::String && type_val->as_string() == "weak") {
        obj_ref->set_owning(false);
    }
    obj_ref->set_object(resolved);
}

void JsonImporter::resolve_inline_binding(IMetadata& meta, string_view name, string_view ref_path,
                                          IStore& store, const ImportContext& ctx, ImportResult& result) const
{
    auto source_prop = resolve_property(store, ctx, ref_path);
    if (!source_prop) {
        add_error(result, string("unresolved property reference '") + ref_path + "' on '" + name + "'");
        return;
    }

    auto target_prop = meta.get_property(name);
    if (!target_prop) {
        return;
    }

    auto binding = ::velk::create_binding(source_prop);
    binding.add_target(target_prop);
}

void JsonImporter::create_bindings(IStore& store, const JsonValue& root, const ImportContext& ctx,
                                   ImportResult& result) const
{
    auto* bindings_val = root.find("bindings");
    if (!bindings_val || bindings_val->type() != JsonType::Array) {
        return;
    }

    for (auto& binding_node : bindings_val->as_array()) {
        if (binding_node.type() != JsonType::Object) {
            continue;
        }
        parse_binding(binding_node, store, ctx, result);
    }
}

void JsonImporter::parse_binding(const JsonValue& binding_node, IStore& store, const ImportContext& ctx,
                                 ImportResult& result) const
{
    auto* source_val = binding_node.find("source");
    if (!source_val || source_val->type() != JsonType::String) {
        add_error(result, "binding missing 'source' string");
        return;
    }

    auto* targets_val = binding_node.find("targets");
    if (!targets_val || targets_val->type() != JsonType::Array) {
        add_error(result, "binding missing 'targets' array");
        return;
    }

    auto source_prop = resolve_property(store, ctx, source_val->as_string());
    if (!source_prop) {
        add_error(result, string("binding: unresolved source '") + source_val->as_string() + "'");
        return;
    }

    // Determine binding mode
    BindingMode mode = BindingMode::OneWay;
    auto* mode_val = binding_node.find("mode");
    if (mode_val && mode_val->type() == JsonType::String && mode_val->as_string() == "twoway") {
        mode = BindingMode::TwoWay;
    }

    // Determine invoke type
    InvokeType invoke = Auto;
    auto* invoke_val = binding_node.find("invoke");
    if (invoke_val && invoke_val->type() == JsonType::String) {
        if (invoke_val->as_string() == "immediate") {
            invoke = Immediate;
        } else if (invoke_val->as_string() == "deferred") {
            invoke = Deferred;
        }
    }

    // Create the binding
    auto binding = ::velk::create_binding(source_prop, invoke, mode);
    if (!binding) {
        add_error(result, "failed to create binding");
        return;
    }

    // Add targets
    for (auto& target_node : targets_val->as_array()) {
        if (target_node.type() != JsonType::String) {
            continue;
        }

        auto target_prop = resolve_property(store, ctx, target_node.as_string());
        if (!target_prop) {
            add_error(result, string("binding: unresolved target '") + target_node.as_string() + "'");
            continue;
        }

        binding.add_target(target_prop);
    }
}

class ImportResolver final : public ext::InterfaceDispatch<IImportResolver>
{
public:
    ImportResolver(const JsonImporter& importer, IStore& store, const JsonImporter::ImportContext& ctx)
        : importer_(importer),
          store_(store),
          ctx_(ctx)
    {}

    IObject::Ptr resolve(string_view path) const override
    {
        // Check for a dot suffix indicating a property path
        auto dot = path.rfind('.');
        if (dot != string_view::npos && dot > 0) {
            // Could be "obj.prop" or "/hierarchy/path.prop"
            auto obj_path = path.substr(0, dot);
            auto prop_name = path.substr(dot + 1);

            auto obj = importer_.resolve_object(store_, ctx_, obj_path);
            if (obj) {
                auto* meta = interface_cast<IMetadata>(obj);
                if (meta) {
                    auto prop = meta->get_property(prop_name);
                    if (prop) {
                        return interface_pointer_cast<IObject>(prop);
                    }
                }
                // Dot didn't yield a property; fall through to try full path as object
            }
        }

        return importer_.resolve_object(store_, ctx_, path);
    }

private:
    const JsonImporter& importer_;
    IStore& store_;
    const JsonImporter::ImportContext& ctx_;
};

void JsonImporter::process_attachments(const JsonValue& root, IStore& store, const ImportContext& ctx,
                                       ImportResult& result) const
{
    auto* attachments = root.find("attachments");
    if (!attachments || attachments->type() != JsonType::Array) {
        return;
    }

    for (auto& entry : attachments->as_array()) {
        if (entry.type() != JsonType::Object) {
            continue;
        }

        auto* targets_val = entry.find("targets");
        if (!targets_val || targets_val->type() != JsonType::Array) {
            add_error(result, "attachment missing 'targets' array");
            continue;
        }

        auto obj = create_object(entry, ctx, result);
        if (!obj) {
            continue;
        }

        auto att = interface_pointer_cast<IInterface>(obj);

        for (auto& target_el : targets_val->as_array()) {
            if (target_el.type() != JsonType::String) {
                continue;
            }

            auto target_obj = resolve_object(store, ctx, target_el.as_string());
            if (!target_obj) {
                add_error(result, string("attachment: target '") + target_el.as_string() + "' not found");
                continue;
            }

            auto* storage = interface_cast<IObjectStorage>(target_obj);
            if (!storage) {
                add_error(result,
                          string("attachment: target '") + target_el.as_string() + "' has no storage");
                continue;
            }

            storage->add_attachment(att);
        }
    }
}

void JsonImporter::discover_type_extensions() const
{
    type_extensions_.clear();

    vector<Uid> extension_uids;
    auto& registry = ::velk::instance().type_registry();

    registry.for_each_class(&extension_uids, [](void* ctx, const ClassInfo& info) -> bool {
        auto* uids = static_cast<vector<Uid>*>(ctx);
        for (size_t i = 0; i < info.interfaces.size(); i++) {
            if (info.interfaces[i].uid == IImporterTypeExtension::UID) {
                uids->push_back(info.uid);
                break;
            }
        }
        return true;
    });

    for (auto uid : extension_uids) {
        auto instance = registry.create(uid);
        if (!instance) {
            continue;
        }
        auto ext = interface_pointer_cast<IImporterTypeExtension>(instance);
        if (!ext) {
            continue;
        }
        auto types = ext->supported_types();
        for (size_t i = 0; i < types.size(); i++) {
            type_extensions_[types[i]] = ext;
        }
    }

    // Writers, usually the same classes.
    exporter_extensions_.clear();
    vector<Uid> exporter_uids;
    registry.for_each_class(&exporter_uids, [](void* ctx, const ClassInfo& info) -> bool {
        for (size_t i = 0; i < info.interfaces.size(); i++) {
            if (info.interfaces[i].uid == IExporterTypeExtension::UID) {
                static_cast<vector<Uid>*>(ctx)->push_back(info.uid);
                break;
            }
        }
        return true;
    });
    for (auto uid : exporter_uids) {
        auto ext = interface_pointer_cast<IExporterTypeExtension>(registry.create(uid));
        if (!ext) {
            continue;
        }
        auto types = ext->supported_types();
        for (size_t i = 0; i < types.size(); i++) {
            exporter_extensions_[types[i]] = ext;
        }
    }
}

void JsonImporter::process_resource_protocols(const JsonValue& root, ImportResult& result) const
{
    auto* protocols = root.find("resource-protocols");
    if (!protocols || protocols->type() != JsonType::Array) {
        return;
    }

    auto& store = ::velk::instance().resource_store();
    for (auto& entry : protocols->as_array()) {
        if (entry.type() != JsonType::Object) {
            continue;
        }
        auto* scheme_val = entry.find("scheme");
        auto* path_val = entry.find("base_path");
        if (!scheme_val || scheme_val->type() != JsonType::String) {
            add_error(result, "resource-protocol entry missing 'scheme'");
            continue;
        }
        auto proto = ::velk::instance().create<IResourceProtocolInternal>(ClassId::FileProtocol);
        if (!proto) {
            add_error(result, "failed to create FileProtocol for resource-protocol");
            continue;
        }
        proto->set_scheme(scheme_val->as_string());
        if (path_val && path_val->type() == JsonType::String) {
            auto base = path_val->as_string();
            // If base_path is a URI (contains ://), resolve it to a filesystem path
            // by composing the referenced protocol's base_path with the path portion.
            auto sep = base.find("://");
            if (sep != string_view::npos) {
                auto base_scheme = string_view(base.data(), sep);
                auto path_part = string_view(base.data() + sep + 3, base.size() - sep - 3);
                auto existing = store.find_protocol(base_scheme);
                if (existing) {
                    auto* internal = interface_cast<IResourceProtocolInternal>(existing);
                    if (internal) {
                        proto->set_base_path(string(internal->base_path()) + string(path_part));
                    } else {
                        proto->set_base_path(base);
                    }
                } else {
                    add_error(result, string("resource-protocol: unknown scheme '") + base_scheme + "' in base_path");
                }
            } else {
                proto->set_base_path(base);
            }
        }
        store.register_protocol(interface_pointer_cast<IResourceProtocol>(proto));
    }
}

void JsonImporter::process_resources(const JsonValue& root, IStore& store, ImportContext& ctx,
                                     ImportResult& result) const
{
    auto* resources = root.find("resources");
    if (!resources || resources->type() != JsonType::Array) {
        return;
    }

    for (auto& obj_node : resources->as_array()) {
        if (obj_node.type() != JsonType::Object) {
            continue;
        }
        auto* id_val = obj_node.find("id");
        if (!id_val || id_val->type() != JsonType::String) {
            add_error(result, "resource missing 'id' string field");
            continue;
        }

        auto store_key = string("resource:") + id_val->as_string();

        // Decoder/URI form: { id, uri, persistent? } — no `class`. Fetches
        // via the resource store, applies persistence, registers under id.
        auto* class_val = obj_node.find("class");
        auto* uri_val = obj_node.find("uri");
        if (!class_val && uri_val && uri_val->type() == JsonType::String) {
            auto& res_store = ::velk::instance().resource_store();
            auto resource = res_store.get_resource(uri_val->as_string());
            if (!resource) {
                add_error(result,
                    string("resource '") + id_val->as_string() +
                    "': failed to load uri '" + string(uri_val->as_string()) + "'");
                continue;
            }
            auto* persistent_val = obj_node.find("persistent");
            if (persistent_val && persistent_val->type() == JsonType::Bool && persistent_val->as_bool()) {
                resource->set_persistent(true);
            }
            auto obj_view = interface_pointer_cast<IObject>(resource);
            if (!obj_view) {
                add_error(result,
                    string("resource '") + id_val->as_string() +
                    "': loaded resource does not implement IObject");
                continue;
            }
            store.add(store_key, obj_view);
            continue;
        }

        // Class form: { id, class, properties } — constructs an object.
        auto obj = create_object(obj_node, ctx, result);
        if (obj) {
            store.add(store_key, obj);
            register_imported_object(ctx, obj, obj_node, result);
        }
    }
}

void JsonImporter::dispatch_extensions(const JsonValue& root, IStore& store, const ImportContext& ctx) const
{
    // Discover all classes implementing IImporterExtension
    vector<Uid> extension_uids;
    auto& registry = ::velk::instance().type_registry();

    registry.for_each_class(&extension_uids, [](void* ctx, const ClassInfo& info) -> bool {
        auto* uids = static_cast<vector<Uid>*>(ctx);
        for (size_t i = 0; i < info.interfaces.size(); i++) {
            if (info.interfaces[i].uid == IImporterExtension::UID) {
                uids->push_back(info.uid);
                break;
            }
        }
        return true;
    });

    // Instantiate each extension, check if its key exists, and dispatch
    ImportResolver resolver(*this, store, ctx);
    for (auto uid : extension_uids) {
        auto instance = registry.create(uid);
        if (!instance) {
            continue;
        }
        auto* ext = interface_cast<IImporterExtension>(instance);
        if (!ext) {
            continue;
        }
        auto key = ext->collection_key();
        auto* json_node = root.find(key);
        if (json_node) {
            JsonImportData wrapped(*json_node);
            ext->process(wrapped, store, resolver);
        }
    }
}

} // namespace velk
