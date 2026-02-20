#include <gtest/gtest.h>

#include <velk/common.h>
#include <velk/uid.h>

using namespace velk;

TEST(Uid, ConstructFromTwoUint64)
{
    constexpr Uid u(0xcc262192d151941f, 0xd542d4c622b50b09);
    EXPECT_EQ(u.hi, 0xcc262192d151941fULL);
    EXPECT_EQ(u.lo, 0xd542d4c622b50b09ULL);
}

TEST(Uid, ConstructFromString)
{
    constexpr Uid u("cc262192-d151-941f-d542-d4c622b50b09");
    EXPECT_EQ(u.hi, 0xcc262192d151941fULL);
    EXPECT_EQ(u.lo, 0xd542d4c622b50b09ULL);
}

TEST(Uid, RoundTripIntsEqualString)
{
    constexpr Uid a(0xcc262192d151941f, 0xd542d4c622b50b09);
    constexpr Uid b("cc262192-d151-941f-d542-d4c622b50b09");
    static_assert(a == b, "Uid from ints must equal Uid from string");
    EXPECT_EQ(a, b);
}

TEST(Uid, DefaultIsZero)
{
    constexpr Uid u;
    EXPECT_EQ(u.hi, 0u);
    EXPECT_EQ(u.lo, 0u);
}

TEST(Uid, EqualityAndInequality)
{
    constexpr Uid a(1, 2);
    constexpr Uid b(1, 2);
    constexpr Uid c(1, 3);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(Uid, LessThan)
{
    constexpr Uid a(1, 0);
    constexpr Uid b(2, 0);
    constexpr Uid c(1, 1);
    EXPECT_LT(a, b);
    EXPECT_LT(a, c);
}

TEST(Uid, TypeUidConsistent)
{
    constexpr Uid a = type_uid<int>();
    constexpr Uid b = type_uid<int>();
    static_assert(a == b, "type_uid must be consistent");
    EXPECT_EQ(a, b);

    constexpr Uid c = type_uid<float>();
    EXPECT_NE(a, c);
}

TEST(Uid, IsValidUidFormat_Accepts)
{
    static_assert(is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b09"));
    static_assert(is_valid_uid_format("00000000-0000-0000-0000-000000000000"));
    static_assert(is_valid_uid_format("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE"));
    EXPECT_TRUE(is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b09"));
}

TEST(Uid, IsValidUidFormat_Rejects)
{
    static_assert(!is_valid_uid_format("not-a-uid"));
    static_assert(!is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b0"));   // too short
    static_assert(!is_valid_uid_format("cc262192Xd151-941f-d542-d4c622b50b09"));  // wrong dash
    static_assert(!is_valid_uid_format("gg262192-d151-941f-d542-d4c622b50b09"));  // bad hex
    static_assert(!is_valid_uid_format("cc262192-d151-941f-d542-d4c622b50b090")); // too long
    EXPECT_FALSE(is_valid_uid_format("not-a-uid"));
}

TEST(Uid, StreamOutput)
{
    Uid u("a0b1c2d3-e4f5-6789-abcd-ef0123456789");
    std::ostringstream oss;
    oss << u;
    EXPECT_EQ(oss.str(), "a0b1c2d3-e4f5-6789-abcd-ef0123456789");
}

TEST(Uid, MakeHashDeterministic)
{
    constexpr Uid a = make_hash("hello");
    constexpr Uid b = make_hash("hello");
    static_assert(a == b);
    EXPECT_EQ(a, b);

    constexpr Uid c = make_hash("world");
    EXPECT_NE(a, c);
}
