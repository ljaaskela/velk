#include <velk/json.h>

#include <gtest/gtest.h>

#include <cstring>

using namespace velk;

namespace {

JsonValue parse(string_view text)
{
    JsonValue v;
    string error;
    EXPECT_TRUE(json_parse(text, v, error)) << error.c_str();
    return v;
}

} // namespace

TEST(Json, ParsesAllTypes)
{
    auto v = parse(R"({"n": null, "b": true, "x": -1.5e2, "s": "a\"ä", "a": [1, 2], "o": {}})");
    ASSERT_TRUE(v.is_object());
    EXPECT_TRUE(v["n"].is_null());
    EXPECT_TRUE(v["b"].as_bool());
    EXPECT_DOUBLE_EQ(v["x"].as_number(), -150.0);
    EXPECT_EQ(string_view(v["s"].as_string()), string_view("a\"\xc3\xa4"));
    EXPECT_EQ(v["a"].size(), 2u);
    EXPECT_TRUE(v["o"].is_object());
    EXPECT_TRUE(v["missing"].is_null());
}

TEST(Json, ReportsErrorsWithOffset)
{
    JsonValue v;
    string error;
    EXPECT_FALSE(json_parse(string_view("{\"a\": }"), v, error));
    EXPECT_NE(error.find("offset 6"), string::npos) << error.c_str();
    EXPECT_FALSE(json_parse(string_view("[1] x"), v, error));
    EXPECT_FALSE(json_parse(string_view(""), v, error));
}

TEST(Json, KeepsMemberOrder)
{
    auto v = parse(R"({"z": 1, "a": 2, "m": 3})");
    auto& members = v.as_object();
    ASSERT_EQ(members.size(), 3u);
    EXPECT_EQ(string_view(members[0].first), string_view("z"));
    EXPECT_EQ(string_view(members[1].first), string_view("a"));
    EXPECT_EQ(string_view(members[2].first), string_view("m"));
}

TEST(Json, EditsMembers)
{
    auto v = parse(R"({"a": 1, "b": 2})");
    EXPECT_TRUE(v.replace("a", JsonValue::string("x")));
    EXPECT_FALSE(v.replace("c", JsonValue::null()));
    EXPECT_FALSE(v.insert("a", JsonValue::null()));
    EXPECT_TRUE(v.insert("c", JsonValue::number(3)));
    EXPECT_TRUE(v.insert("first", JsonValue::boolean(true), 0));
    EXPECT_TRUE(v.remove("b"));
    EXPECT_FALSE(v.remove("b"));
    EXPECT_TRUE(v.set("c", JsonValue::number(4)));
    EXPECT_TRUE(v.set("d", JsonValue::number(5)));
    EXPECT_EQ(json_write_inline(v), string(R"({ "first": true, "a": "x", "c": 4, "d": 5 })"));

    JsonValue array = JsonValue::array();
    EXPECT_FALSE(array.insert("a", JsonValue::null()));
    EXPECT_EQ(array.find("a"), nullptr);
}

TEST(Json, WritesParsedNumbersAsWritten)
{
    auto v = parse(R"({"a": 1.0, "b": 0.10, "c": 1e3})");
    EXPECT_EQ(json_write_inline(v), string(R"({ "a": 1.0, "b": 0.10, "c": 1e3 })"));
    EXPECT_EQ(json_write_inline(JsonValue::number(0.1)), string("0.1"));
    EXPECT_EQ(json_write_inline(JsonValue::number(2)), string("2"));
}

TEST(Json, WriteRoundTrips)
{
    const char* text = R"({"version": 1, "objects": [{"id": "a", "class": "x.Y", "properties": {"text": "line\nbreak", "w": 0.25}}], "empty": [], "none": {}})";
    auto v = parse(string_view(text, std::strlen(text)));
    auto written = json_write(v);
    auto again = parse(written);
    EXPECT_TRUE(v == again);
    EXPECT_EQ(json_write(again), written);
}

TEST(Json, WritesShortContainersOnOneLine)
{
    auto v = parse(R"({"version": 1, "objects": [{"id": "a", "class": "x.Y"}, {"id": "b", "class": "x.Y"}], "h": {"root": ["a", "b"]}})");
    JsonWriteOptions narrow;
    narrow.line_width = 40;
    EXPECT_EQ(json_write(v, narrow), string("{\n"
                                            "  \"version\": 1,\n"
                                            "  \"objects\": [\n"
                                            "    { \"id\": \"a\", \"class\": \"x.Y\" },\n"
                                            "    { \"id\": \"b\", \"class\": \"x.Y\" }\n"
                                            "  ],\n"
                                            "  \"h\": { \"root\": [\"a\", \"b\"] }\n"
                                            "}\n"));
}

TEST(Json, EditChangesOneLine)
{
    auto v = parse(R"({"objects": [{"id": "a", "properties": {"w": 1}}, {"id": "b", "properties": {"w": 2}}]})");
    JsonWriteOptions narrow;
    narrow.line_width = 50;
    auto before = json_write(v, narrow);
    v.as_object()[0].second.as_array()[1].find("properties")->replace("w", JsonValue::number(3));
    auto after = json_write(v, narrow);
    size_t changed = 0;
    size_t a = 0;
    size_t b = 0;
    while (a < before.size() && b < after.size()) {
        size_t ea = before.find("\n", a);
        size_t eb = after.find("\n", b);
        if (before.substr(a, ea - a) != after.substr(b, eb - b)) {
            ++changed;
        }
        a = ea + 1;
        b = eb + 1;
    }
    EXPECT_EQ(changed, 1u);
}

TEST(Json, QuotesControlCharacters)
{
    string out;
    json_quote(out, string_view("a\"\\\n\x01", 5));
    EXPECT_EQ(out, string("\"a\\\"\\\\\\n\\u0001\""));
}
