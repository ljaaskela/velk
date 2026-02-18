#include <gtest/gtest.h>
#include <string_view.h>

using velk::string_view;

TEST(StringView, DefaultConstructedIsEmpty)
{
    string_view sv;
    EXPECT_TRUE(sv.empty());
    EXPECT_EQ(sv.size(), 0u);
    EXPECT_EQ(sv.data(), nullptr);
}

TEST(StringView, ConstructFromLiteral)
{
    string_view sv("hello");
    EXPECT_EQ(sv.size(), 5u);
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[4], 'o');
    EXPECT_FALSE(sv.empty());
}

TEST(StringView, ConstructFromPointerAndSize)
{
    const char* str = "hello world";
    string_view sv(str, 5);
    EXPECT_EQ(sv.size(), 5u);
    EXPECT_EQ(sv[0], 'h');
    EXPECT_EQ(sv[4], 'o');
}

TEST(StringView, BeginEnd)
{
    string_view sv("abc");
    EXPECT_EQ(sv.end() - sv.begin(), 3);
    std::string result;
    for (char c : sv) result += c;
    EXPECT_EQ(result, "abc");
}

TEST(StringView, Equality)
{
    string_view a("hello");
    string_view b("hello");
    string_view c("world");
    string_view d("hell");

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(StringView, Substr)
{
    string_view sv("hello world");
    auto sub = sv.substr(6, 5);
    EXPECT_EQ(sub, string_view("world"));
    EXPECT_EQ(sub.size(), 5u);

    // Substr from middle to end
    auto rest = sv.substr(6);
    EXPECT_EQ(rest, string_view("world"));

    // Substr with count exceeding size
    auto clamped = sv.substr(6, 100);
    EXPECT_EQ(clamped, string_view("world"));
}

TEST(StringView, Find)
{
    string_view sv("hello world hello");
    EXPECT_EQ(sv.find(string_view("world")), 6u);
    EXPECT_EQ(sv.find(string_view("hello")), 0u);
    EXPECT_EQ(sv.find(string_view("hello"), 1), 12u);
    EXPECT_EQ(sv.find(string_view("xyz")), string_view::npos);
    EXPECT_EQ(sv.find(string_view("")), 0u);
}

TEST(StringView, Rfind)
{
    string_view sv("hello world hello");
    EXPECT_EQ(sv.rfind(string_view("hello")), 12u);
    EXPECT_EQ(sv.rfind(string_view("world")), 6u);
    EXPECT_EQ(sv.rfind(string_view("hello"), 5), 0u);
    EXPECT_EQ(sv.rfind(string_view("xyz")), string_view::npos);
}

TEST(StringView, ConstexprConstruction)
{
    constexpr string_view sv("constexpr");
    static_assert(sv.size() == 9);
    static_assert(sv[0] == 'c');
}

TEST(StringView, ConstexprFind)
{
    constexpr string_view sv("type_name_array<int>(void)");
    constexpr auto pos = sv.find(string_view("type_name_array<"));
    static_assert(pos == 0);
    constexpr auto end = sv.rfind(string_view(">(void)"));
    static_assert(end == 19);
}

TEST(StringView, ConstexprSubstr)
{
    constexpr string_view sv("prefix::value::suffix");
    constexpr auto start = sv.find(string_view("::")) + 2;
    constexpr auto end = sv.rfind(string_view("::"));
    constexpr auto result = sv.substr(start, end - start);
    static_assert(result == string_view("value"));
}
