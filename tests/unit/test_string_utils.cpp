#include <gtest/gtest.h>
#include "utils/string_utils.h"

using namespace uniconv::utils;

TEST(StringUtilsTest, ToLower) {
    EXPECT_EQ(to_lower("HELLO"), "hello");
    EXPECT_EQ(to_lower("Hello World"), "hello world");
    EXPECT_EQ(to_lower("already lower"), "already lower");
    EXPECT_EQ(to_lower(""), "");
}

TEST(StringUtilsTest, ToUpper) {
    EXPECT_EQ(to_upper("hello"), "HELLO");
    EXPECT_EQ(to_upper("Hello World"), "HELLO WORLD");
    EXPECT_EQ(to_upper("ALREADY UPPER"), "ALREADY UPPER");
    EXPECT_EQ(to_upper(""), "");
}

TEST(StringUtilsTest, Trim) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim("  "), "");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("\t\n hello \t\n"), "hello");
}

TEST(StringUtilsTest, Split) {
    auto parts = split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");

    parts = split("no-delimiter", ',');
    ASSERT_EQ(parts.size(), 1);
    EXPECT_EQ(parts[0], "no-delimiter");

    parts = split("", ',');
    EXPECT_TRUE(parts.empty());
}

TEST(StringUtilsTest, Join) {
    std::vector<std::string> parts = {"a", "b", "c"};
    EXPECT_EQ(join(parts, ","), "a,b,c");
    EXPECT_EQ(join(parts, " - "), "a - b - c");

    std::vector<std::string> empty;
    EXPECT_EQ(join(empty, ","), "");

    std::vector<std::string> single = {"only"};
    EXPECT_EQ(join(single, ","), "only");
}

TEST(StringUtilsTest, StartsWith) {
    EXPECT_TRUE(starts_with("hello world", "hello"));
    EXPECT_TRUE(starts_with("hello", "hello"));
    EXPECT_FALSE(starts_with("hello", "world"));
    EXPECT_TRUE(starts_with("hello", ""));
    EXPECT_FALSE(starts_with("", "hello"));
}

TEST(StringUtilsTest, EndsWith) {
    EXPECT_TRUE(ends_with("hello world", "world"));
    EXPECT_TRUE(ends_with("hello", "hello"));
    EXPECT_FALSE(ends_with("hello", "world"));
    EXPECT_TRUE(ends_with("hello", ""));
    EXPECT_FALSE(ends_with("", "hello"));
}

TEST(StringUtilsTest, ParseSize) {
    EXPECT_EQ(parse_size("100"), 100);
    EXPECT_EQ(parse_size("1KB"), 1024);
    EXPECT_EQ(parse_size("1kb"), 1024);
    EXPECT_EQ(parse_size("1K"), 1024);
    EXPECT_EQ(parse_size("1MB"), 1024 * 1024);
    EXPECT_EQ(parse_size("25MB"), 25 * 1024 * 1024);
    EXPECT_EQ(parse_size("1GB"), 1024ULL * 1024 * 1024);
    EXPECT_FALSE(parse_size("invalid").has_value());
    EXPECT_FALSE(parse_size("").has_value());
}

TEST(StringUtilsTest, FormatSize) {
    EXPECT_EQ(format_size(0), "0 B");
    EXPECT_EQ(format_size(500), "500 B");
    EXPECT_EQ(format_size(1024), "1.00 KB");
    EXPECT_EQ(format_size(1536), "1.50 KB");
    EXPECT_EQ(format_size(1024 * 1024), "1.00 MB");
    EXPECT_EQ(format_size(1024ULL * 1024 * 1024), "1.00 GB");
}
