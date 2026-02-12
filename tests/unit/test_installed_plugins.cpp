#include <gtest/gtest.h>
#include "core/installed_plugins.h"
#include "core/plugin_manifest.h"
#include <filesystem>
#include <vector>

using namespace uniconv::core;

class InstalledPluginsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir_ = std::filesystem::temp_directory_path() / "uniconv_test_installed";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(InstalledPluginsTest, RecordAndGet)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("image-grayscale", "1.2.0");

    auto record = installed.get("image-grayscale");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->version, "1.2.0");
    EXPECT_EQ(record->source, "registry");
    EXPECT_FALSE(record->installed_at.empty());
}

TEST_F(InstalledPluginsTest, GetNonExistent)
{
    InstalledPlugins installed(test_dir_);
    auto record = installed.get("nonexistent");
    EXPECT_FALSE(record.has_value());
}

TEST_F(InstalledPluginsTest, IsRegistryInstalled)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("test-plugin", "1.0.0");

    EXPECT_TRUE(installed.is_registry_installed("test-plugin"));
    EXPECT_FALSE(installed.is_registry_installed("other-plugin"));
}

TEST_F(InstalledPluginsTest, RecordRemove)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("test-plugin", "1.0.0");
    EXPECT_TRUE(installed.is_registry_installed("test-plugin"));

    installed.record_remove("test-plugin");
    EXPECT_FALSE(installed.is_registry_installed("test-plugin"));
}

TEST_F(InstalledPluginsTest, SaveAndLoad)
{
    {
        InstalledPlugins installed(test_dir_);
        installed.record_install("plugin-a", "1.0.0");
        installed.record_install("plugin-b", "2.1.0");
        EXPECT_TRUE(installed.save());
    }

    {
        InstalledPlugins installed(test_dir_);
        EXPECT_TRUE(installed.load());

        auto a = installed.get("plugin-a");
        ASSERT_TRUE(a.has_value());
        EXPECT_EQ(a->version, "1.0.0");

        auto b = installed.get("plugin-b");
        ASSERT_TRUE(b.has_value());
        EXPECT_EQ(b->version, "2.1.0");
    }
}

TEST_F(InstalledPluginsTest, LoadNonExistent)
{
    InstalledPlugins installed(test_dir_);
    EXPECT_FALSE(installed.load());
}

TEST_F(InstalledPluginsTest, AllPlugins)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("a", "1.0.0");
    installed.record_install("b", "2.0.0");

    const auto &all = installed.all();
    EXPECT_EQ(all.size(), 2);
    EXPECT_TRUE(all.contains("a"));
    EXPECT_TRUE(all.contains("b"));
}

namespace
{
    PluginManifest make_manifest(const std::string &name, const std::string &version)
    {
        PluginManifest m;
        m.name = name;
        m.version = version;
        return m;
    }
} // namespace

TEST_F(InstalledPluginsTest, ReconcilePrunesOrphan)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("orphan-plugin", "1.0.0");

    std::vector<PluginManifest> on_disk; // empty — plugin no longer on disk
    EXPECT_TRUE(installed.reconcile(on_disk));
    EXPECT_FALSE(installed.get("orphan-plugin").has_value());
}

TEST_F(InstalledPluginsTest, ReconcileKeepsValid)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("good-plugin", "1.0.0");

    std::vector<PluginManifest> on_disk = {make_manifest("good-plugin", "1.0.0")};
    EXPECT_FALSE(installed.reconcile(on_disk));
    EXPECT_TRUE(installed.get("good-plugin").has_value());
}

TEST_F(InstalledPluginsTest, ReconcileUpdatesVersion)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("updated-plugin", "1.0.0");

    std::vector<PluginManifest> on_disk = {make_manifest("updated-plugin", "1.1.0")};
    EXPECT_TRUE(installed.reconcile(on_disk));

    auto record = installed.get("updated-plugin");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->version, "1.1.0");
}

TEST_F(InstalledPluginsTest, ReconcileNoChanges)
{
    InstalledPlugins installed(test_dir_);
    installed.record_install("plugin-a", "1.0.0");
    installed.record_install("plugin-b", "2.0.0");

    std::vector<PluginManifest> on_disk = {
        make_manifest("plugin-a", "1.0.0"),
        make_manifest("plugin-b", "2.0.0"),
    };
    EXPECT_FALSE(installed.reconcile(on_disk));
    EXPECT_EQ(installed.all().size(), 2);
}
