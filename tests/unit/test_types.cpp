#include <gtest/gtest.h>
#include "core/types.h"

using namespace uniconv::core;

TEST(TypesTest, ETLTypeToString) {
    EXPECT_EQ(etl_type_to_string(ETLType::Transform), "transform");
    EXPECT_EQ(etl_type_to_string(ETLType::Extract), "extract");
    EXPECT_EQ(etl_type_to_string(ETLType::Load), "load");
}

TEST(TypesTest, ETLTypeFromString) {
    EXPECT_EQ(etl_type_from_string("transform"), ETLType::Transform);
    EXPECT_EQ(etl_type_from_string("t"), ETLType::Transform);
    EXPECT_EQ(etl_type_from_string("extract"), ETLType::Extract);
    EXPECT_EQ(etl_type_from_string("e"), ETLType::Extract);
    EXPECT_EQ(etl_type_from_string("load"), ETLType::Load);
    EXPECT_EQ(etl_type_from_string("l"), ETLType::Load);
    EXPECT_FALSE(etl_type_from_string("invalid").has_value());
}

TEST(TypesTest, ResultStatusToString) {
    EXPECT_EQ(result_status_to_string(ResultStatus::Success), "success");
    EXPECT_EQ(result_status_to_string(ResultStatus::Error), "error");
    EXPECT_EQ(result_status_to_string(ResultStatus::Skipped), "skipped");
}

TEST(TypesTest, FileCategoryToString) {
    EXPECT_EQ(file_category_to_string(FileCategory::Image), "image");
    EXPECT_EQ(file_category_to_string(FileCategory::Video), "video");
    EXPECT_EQ(file_category_to_string(FileCategory::Audio), "audio");
    EXPECT_EQ(file_category_to_string(FileCategory::Document), "document");
    EXPECT_EQ(file_category_to_string(FileCategory::Unknown), "unknown");
}

TEST(TypesTest, ETLResultSuccess) {
    auto result = ETLResult::success(
        ETLType::Transform,
        "jpg",
        "image-core",
        "/input/test.heic",
        "/output/test.jpg",
        1000,
        500
    );

    EXPECT_EQ(result.status, ResultStatus::Success);
    EXPECT_EQ(result.etl, ETLType::Transform);
    EXPECT_EQ(result.target, "jpg");
    EXPECT_EQ(result.plugin_used, "image-core");
    EXPECT_EQ(result.input_size, 1000);
    EXPECT_EQ(result.output_size, 500);
}

TEST(TypesTest, ETLResultFailure) {
    auto result = ETLResult::failure(
        ETLType::Transform,
        "jpg",
        "/input/test.heic",
        "File not found"
    );

    EXPECT_EQ(result.status, ResultStatus::Error);
    EXPECT_TRUE(result.error.has_value());
    EXPECT_EQ(*result.error, "File not found");
}

TEST(TypesTest, PresetToJson) {
    Preset preset;
    preset.name = "web-jpg";
    preset.description = "JPEG for web";
    preset.etl = ETLType::Transform;
    preset.target = "jpg";
    preset.core_options.quality = 85;

    auto j = preset.to_json();

    EXPECT_EQ(j["name"], "web-jpg");
    EXPECT_EQ(j["description"], "JPEG for web");
    EXPECT_EQ(j["etl"], "transform");
    EXPECT_EQ(j["target"], "jpg");
}

TEST(TypesTest, PresetFromJson) {
    nlohmann::json j = {
        {"name", "web-jpg"},
        {"description", "JPEG for web"},
        {"etl", "transform"},
        {"target", "jpg"},
        {"core_options", {{"quality", 85}}}
    };

    auto preset = Preset::from_json(j);

    EXPECT_EQ(preset.name, "web-jpg");
    EXPECT_EQ(preset.description, "JPEG for web");
    EXPECT_EQ(preset.etl, ETLType::Transform);
    EXPECT_EQ(preset.target, "jpg");
    EXPECT_EQ(preset.core_options.quality, 85);
}
