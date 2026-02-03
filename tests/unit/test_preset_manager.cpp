#include <gtest/gtest.h>
#include "core/preset_manager.h"
#include <filesystem>

using namespace uniconv::core;
namespace fs = std::filesystem;

class PresetManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a temp directory for tests
        test_dir_ = fs::temp_directory_path() / "uniconv_test";
        fs::create_directories(test_dir_);
        manager_ = std::make_unique<PresetManager>(test_dir_);
    }

    void TearDown() override
    {
        // Clean up test directory
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    std::unique_ptr<PresetManager> manager_;
};

TEST_F(PresetManagerTest, CreateAndLoadPreset)
{
    Preset preset;
    preset.name = "test-preset";
    preset.description = "A test preset";
    preset.target = "jpg";
    preset.plugin_options = {"--quality", "85"};

    manager_->create(preset);

    auto loaded = manager_->load("test-preset");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "test-preset");
    EXPECT_EQ(loaded->description, "A test preset");
    EXPECT_EQ(loaded->target, "jpg");
    EXPECT_EQ(loaded->plugin_options.size(), 2);
}

TEST_F(PresetManagerTest, PresetExists)
{
    EXPECT_FALSE(manager_->exists("nonexistent"));

    Preset preset;
    preset.name = "exists-test";
    preset.target = "png";

    manager_->create(preset);
    EXPECT_TRUE(manager_->exists("exists-test"));
}

TEST_F(PresetManagerTest, RemovePreset)
{
    Preset preset;
    preset.name = "to-remove";
    preset.target = "jpg";

    manager_->create(preset);
    EXPECT_TRUE(manager_->exists("to-remove"));

    EXPECT_TRUE(manager_->remove("to-remove"));
    EXPECT_FALSE(manager_->exists("to-remove"));

    // Removing non-existent returns false
    EXPECT_FALSE(manager_->remove("nonexistent"));
}

TEST_F(PresetManagerTest, ListPresets)
{
    // Initially empty
    auto presets = manager_->list();
    EXPECT_TRUE(presets.empty());

    // Add some presets
    Preset p1;
    p1.name = "alpha";
    p1.target = "jpg";
    manager_->create(p1);

    Preset p2;
    p2.name = "beta";
    p2.target = "audio";
    manager_->create(p2);

    presets = manager_->list();
    EXPECT_EQ(presets.size(), 2);

    // Should be sorted by name
    EXPECT_EQ(presets[0].name, "alpha");
    EXPECT_EQ(presets[1].name, "beta");
}

TEST_F(PresetManagerTest, ListNames)
{
    Preset p1;
    p1.name = "first";
    p1.target = "jpg";
    manager_->create(p1);

    Preset p2;
    p2.name = "second";
    p2.target = "png";
    manager_->create(p2);

    auto names = manager_->list_names();
    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "first");
    EXPECT_EQ(names[1], "second");
}

TEST_F(PresetManagerTest, InvalidPresetName)
{
    Preset preset;
    preset.name = "invalid/name"; // Contains slash
    preset.target = "jpg";

    EXPECT_THROW(manager_->create(preset), std::invalid_argument);
}

TEST_F(PresetManagerTest, ExportImportPreset)
{
    Preset preset;
    preset.name = "export-test";
    preset.description = "Export test preset";
    preset.target = "webp";
    preset.plugin_options = {"--quality", "90"};

    manager_->create(preset);

    // Export to file
    auto export_path = test_dir_ / "exported.json";
    manager_->export_preset("export-test", export_path);
    EXPECT_TRUE(fs::exists(export_path));

    // Remove original
    manager_->remove("export-test");
    EXPECT_FALSE(manager_->exists("export-test"));

    // Import back
    manager_->import_preset(export_path);
    EXPECT_TRUE(manager_->exists("export-test"));

    auto loaded = manager_->load("export-test");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "export-test");
    EXPECT_EQ(loaded->plugin_options.size(), 2);
}
