#include <gtest/gtest.h>
#include "utils/file_utils.h"

using namespace uniconv::utils;
using namespace uniconv::core;

TEST(FileUtilsTest, DetectFormat) {
    EXPECT_EQ(detect_format("/path/to/image.jpg"), "jpg");
    EXPECT_EQ(detect_format("/path/to/image.JPEG"), "jpeg");
    EXPECT_EQ(detect_format("/path/to/image.PNG"), "png");
    EXPECT_EQ(detect_format("/path/to/video.mp4"), "mp4");
    EXPECT_EQ(detect_format("/path/to/file"), "");
    EXPECT_EQ(detect_format("simple.txt"), "txt");
}

TEST(FileUtilsTest, DetectCategory) {
    EXPECT_EQ(detect_category("jpg"), FileCategory::Image);
    EXPECT_EQ(detect_category("jpeg"), FileCategory::Image);
    EXPECT_EQ(detect_category("png"), FileCategory::Image);
    EXPECT_EQ(detect_category("heic"), FileCategory::Image);
    EXPECT_EQ(detect_category("webp"), FileCategory::Image);
    EXPECT_EQ(detect_category("gif"), FileCategory::Image);

    EXPECT_EQ(detect_category("mp4"), FileCategory::Video);
    EXPECT_EQ(detect_category("mov"), FileCategory::Video);
    EXPECT_EQ(detect_category("avi"), FileCategory::Video);
    EXPECT_EQ(detect_category("mkv"), FileCategory::Video);

    EXPECT_EQ(detect_category("mp3"), FileCategory::Audio);
    EXPECT_EQ(detect_category("wav"), FileCategory::Audio);
    EXPECT_EQ(detect_category("flac"), FileCategory::Audio);

    EXPECT_EQ(detect_category("pdf"), FileCategory::Document);
    EXPECT_EQ(detect_category("doc"), FileCategory::Document);

    EXPECT_EQ(detect_category("xyz"), FileCategory::Unknown);
}

TEST(FileUtilsTest, GetMimeType) {
    EXPECT_EQ(get_mime_type("jpg"), "image/jpeg");
    EXPECT_EQ(get_mime_type("jpeg"), "image/jpeg");
    EXPECT_EQ(get_mime_type("png"), "image/png");
    EXPECT_EQ(get_mime_type("gif"), "image/gif");
    EXPECT_EQ(get_mime_type("webp"), "image/webp");
    EXPECT_EQ(get_mime_type("heic"), "image/heic");

    EXPECT_EQ(get_mime_type("mp4"), "video/mp4");
    EXPECT_EQ(get_mime_type("mov"), "video/quicktime");

    EXPECT_EQ(get_mime_type("mp3"), "audio/mpeg");
    EXPECT_EQ(get_mime_type("wav"), "audio/wav");

    EXPECT_EQ(get_mime_type("pdf"), "application/pdf");

    EXPECT_EQ(get_mime_type("unknown"), "application/octet-stream");
}

TEST(FileUtilsTest, IsUrl) {
    EXPECT_TRUE(is_url("http://example.com"));
    EXPECT_TRUE(is_url("https://example.com/path"));
    EXPECT_TRUE(is_url("HTTP://EXAMPLE.COM"));

    EXPECT_FALSE(is_url("/path/to/file"));
    EXPECT_FALSE(is_url("file.txt"));
    EXPECT_FALSE(is_url(""));
}

TEST(FileUtilsTest, IsDirectory) {
    EXPECT_TRUE(is_directory("/tmp"));
    EXPECT_FALSE(is_directory("/nonexistent/path"));
}
