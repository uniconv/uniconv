#include <gtest/gtest.h>
#include "core/types.h"

using namespace uniconv::core;

TEST(TypesTest, DataTypeToString)
{
    EXPECT_EQ(data_type_to_string(DataType::File), "file");
    EXPECT_EQ(data_type_to_string(DataType::Image), "image");
    EXPECT_EQ(data_type_to_string(DataType::Video), "video");
    EXPECT_EQ(data_type_to_string(DataType::Audio), "audio");
    EXPECT_EQ(data_type_to_string(DataType::Text), "text");
    EXPECT_EQ(data_type_to_string(DataType::Json), "json");
    EXPECT_EQ(data_type_to_string(DataType::Binary), "binary");
    EXPECT_EQ(data_type_to_string(DataType::Stream), "stream");
}

TEST(TypesTest, DataTypeFromString)
{
    EXPECT_EQ(data_type_from_string("file"), DataType::File);
    EXPECT_EQ(data_type_from_string("image"), DataType::Image);
    EXPECT_EQ(data_type_from_string("video"), DataType::Video);
    EXPECT_EQ(data_type_from_string("audio"), DataType::Audio);
    EXPECT_EQ(data_type_from_string("text"), DataType::Text);
    EXPECT_EQ(data_type_from_string("json"), DataType::Json);
    EXPECT_EQ(data_type_from_string("binary"), DataType::Binary);
    EXPECT_EQ(data_type_from_string("stream"), DataType::Stream);
    EXPECT_FALSE(data_type_from_string("invalid").has_value());
}

TEST(TypesTest, ResultStatusToString)
{
    EXPECT_EQ(result_status_to_string(ResultStatus::Success), "success");
    EXPECT_EQ(result_status_to_string(ResultStatus::Error), "error");
    EXPECT_EQ(result_status_to_string(ResultStatus::Skipped), "skipped");
}

TEST(TypesTest, FileCategoryToString)
{
    EXPECT_EQ(file_category_to_string(FileCategory::Image), "image");
    EXPECT_EQ(file_category_to_string(FileCategory::Video), "video");
    EXPECT_EQ(file_category_to_string(FileCategory::Audio), "audio");
    EXPECT_EQ(file_category_to_string(FileCategory::Document), "document");
    EXPECT_EQ(file_category_to_string(FileCategory::Unknown), "unknown");
}

TEST(TypesTest, ResultSuccess)
{
    auto result = Result::success(
        "jpg",
        "image-core",
        "/input/test.heic",
        "/output/test.jpg",
        1000,
        500);

    EXPECT_EQ(result.status, ResultStatus::Success);
    EXPECT_EQ(result.target, "jpg");
    EXPECT_EQ(result.plugin_used, "image-core");
    EXPECT_EQ(result.input_size, 1000);
    EXPECT_EQ(result.output_size, 500);
}

TEST(TypesTest, ResultFailure)
{
    auto result = Result::failure(
        "jpg",
        "/input/test.heic",
        "File not found");

    EXPECT_EQ(result.status, ResultStatus::Error);
    EXPECT_TRUE(result.error.has_value());
    EXPECT_EQ(*result.error, "File not found");
}

TEST(TypesTest, PresetToJson)
{
    Preset preset;
    preset.name = "web-jpg";
    preset.description = "JPEG for web";
    preset.target = "jpg";
    preset.plugin_options = {"--quality", "85"};

    auto j = preset.to_json();

    EXPECT_EQ(j["name"], "web-jpg");
    EXPECT_EQ(j["description"], "JPEG for web");
    EXPECT_EQ(j["target"], "jpg");
    EXPECT_EQ(j["plugin_options"].size(), 2);
}

TEST(TypesTest, PresetFromJson)
{
    nlohmann::json j = {
        {"name", "web-jpg"},
        {"description", "JPEG for web"},
        {"target", "jpg"},
        {"plugin_options", {"--quality", "85"}}};

    auto preset = Preset::from_json(j);

    EXPECT_EQ(preset.name, "web-jpg");
    EXPECT_EQ(preset.description, "JPEG for web");
    EXPECT_EQ(preset.target, "jpg");
    EXPECT_EQ(preset.plugin_options.size(), 2);
}
