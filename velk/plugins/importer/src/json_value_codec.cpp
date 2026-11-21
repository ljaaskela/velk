#include "json_importer.h"
#include "json_writer.h"

#include <velk/api/math_types.h>
#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_property.h>

namespace velk {

namespace {

template <class T>
bool read(const IAny& any, Uid type, T& out)
{
    return succeeded(any.get_data(&out, sizeof(T), type));
}

} // namespace

bool JsonImporter::write_value(const IAny& value, Uid type, IExportData& out) const
{
    // The same shapes the importer reads (objects with named members), so the result reads back.
    if (type == type_uid<bool>()) {
        bool v{};
        return read(value, type, v) ? (out.boolean(v), true) : false;
    }
    if (type == type_uid<float>()) {
        float v{};
        return read(value, type, v) ? (out.number(v), true) : false;
    }
    if (type == type_uid<double>()) {
        double v{};
        return read(value, type, v) ? (out.number(v), true) : false;
    }
#define VELK_WRITE_INTEGER(T)                                                                    \
    if (type == type_uid<T>()) {                                                                 \
        T v{};                                                                                   \
        return read(value, type, v) ? (out.integer(static_cast<int64_t>(v)), true) : false;      \
    }
    VELK_WRITE_INTEGER(int8_t)
    VELK_WRITE_INTEGER(uint8_t)
    VELK_WRITE_INTEGER(int16_t)
    VELK_WRITE_INTEGER(uint16_t)
    VELK_WRITE_INTEGER(int32_t)
    VELK_WRITE_INTEGER(uint32_t)
    VELK_WRITE_INTEGER(int64_t)
    VELK_WRITE_INTEGER(uint64_t)
#undef VELK_WRITE_INTEGER
    if (type == type_uid<string>()) {
        string v;
        return read(value, type, v) ? (out.text(v), true) : false;
    }
    const auto member = [&](string_view name, float v) {
        out.key(name);
        out.number(v);
    };
    if (type == type_uid<vec2>()) {
        vec2 v{};
        if (!read(value, type, v)) return false;
        out.begin_object(); member("x", v.x); member("y", v.y); out.end_object();
        return true;
    }
    if (type == type_uid<vec3>()) {
        vec3 v{};
        if (!read(value, type, v)) return false;
        out.begin_object(); member("x", v.x); member("y", v.y); member("z", v.z); out.end_object();
        return true;
    }
    if (type == type_uid<vec4>()) {
        vec4 v{};
        if (!read(value, type, v)) return false;
        out.begin_object(); member("x", v.x); member("y", v.y); member("z", v.z); member("w", v.w); out.end_object();
        return true;
    }
    if (type == type_uid<quat>()) {
        quat v{};
        if (!read(value, type, v)) return false;
        out.begin_object(); member("x", v.x); member("y", v.y); member("z", v.z); member("w", v.w); out.end_object();
        return true;
    }
    if (type == type_uid<size>()) {
        size v{};
        if (!read(value, type, v)) return false;
        out.begin_object();
        member("width", v.width); member("height", v.height); member("depth", v.depth);
        out.end_object();
        return true;
    }
    if (type == type_uid<rect>()) {
        rect v{};
        if (!read(value, type, v)) return false;
        out.begin_object();
        member("x", v.x); member("y", v.y); member("width", v.width); member("height", v.height);
        out.end_object();
        return true;
    }
    if (type == type_uid<color>()) {
        color v{};
        if (!read(value, type, v)) return false;
        out.begin_object(); member("r", v.r); member("g", v.g); member("b", v.b); member("a", v.a); out.end_object();
        return true;
    }
    if (type == type_uid<mat4>()) {
        mat4 v{};
        if (!read(value, type, v)) return false;
        out.begin_array();
        for (float f : v.m) {
            out.number(f);
        }
        out.end_array();
        return true;
    }
    if (type == type_uid<aabb>()) {
        aabb v{};
        if (!read(value, type, v)) return false;
        out.begin_object();
        out.key("position");
        out.begin_object(); member("x", v.position.x); member("y", v.position.y); member("z", v.position.z); out.end_object();
        out.key("extent");
        out.begin_object();
        member("width", v.extent.width); member("height", v.extent.height); member("depth", v.extent.depth);
        out.end_object();
        out.end_object();
        return true;
    }
    if (auto* array = interface_cast<IArrayAny>(const_cast<IAny*>(&value))) {
        const Uid element_type = array->element_type();
        out.begin_array();
        for (size_t i = 0; i < array->array_size(); ++i) {
            auto element = instance().create_any(element_type);
            if (!element || failed(array->get_at(i, *element)) || !write_value(*element, element_type, out)) {
                return false;
            }
        }
        out.end_array();
        return true;
    }

    auto ext = exporter_extensions_.find(type);
    if (ext == exporter_extensions_.end()) {
        discover_type_extensions();
        ext = exporter_extensions_.find(type);
    }
    return ext != exporter_extensions_.end() && succeeded(ext->second->serialize(type, value, out));
}

string JsonImporter::to_json(const IAny& value, Uid type) const
{
    auto writer = ext::make_object<JsonWriter, IExportData>();
    if (!writer || !write_value(value, type, *writer)) {
        return {};
    }
    return static_cast<JsonWriter*>(writer.get())->json();
}

IAny::Ptr JsonImporter::from_json(string_view json, Uid type) const
{
    JsonValue parsed;
    string error;
    if (!json_parse(json.data(), json.size(), parsed, error)) {
        return nullptr;
    }
    if (type_extensions_.empty()) {
        discover_type_extensions();
    }
    // The importer's own conversion, applied to a standalone property of the type.
    auto initial = instance().create_any(type);
    auto property = initial ? instance().type_registry().create_property(type, initial, ObjectFlags::None) : nullptr;
    auto* internal = interface_cast<IPropertyInternal>(property);
    PropertyKind kind{};
    kind.typeUid = type;
    if (!internal || !set_property_value(*internal, kind, parsed)) {
        return nullptr;
    }
    auto value = property->get_value();
    return value ? value->clone() : nullptr;
}

} // namespace velk
