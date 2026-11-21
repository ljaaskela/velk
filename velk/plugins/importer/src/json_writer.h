#ifndef VELK_IMPORTER_JSON_WRITER_H
#define VELK_IMPORTER_JSON_WRITER_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_exporter_extension.h>
#include <velk/string.h>
#include <velk/vector.h>

namespace velk {

/** @brief Writes IExportData calls as compact JSON text. */
class JsonWriter final : public ext::ObjectCore<JsonWriter, IExportData>
{
public:
    VELK_CLASS_UID("55d86e47-c11e-4b0b-aa09-670aaa08196e", "JsonWriter");

    void null() override;
    void boolean(bool value) override;
    void integer(int64_t value) override;
    void number(float value) override;
    void number(double value) override;
    void text(string_view value) override;
    void begin_object() override;
    void key(string_view name) override;
    void end_object() override;
    void begin_array() override;
    void end_array() override;

    const string& json() const { return out_; }

private:
    /** @brief Writes the separator a value needs in its container. */
    void before_value();
    void quoted(string_view value);

    string out_;
    vector<uint8_t> first_; ///< Per open container: no member written yet.
    bool after_key_ = false;
};

} // namespace velk

#endif // VELK_IMPORTER_JSON_WRITER_H
