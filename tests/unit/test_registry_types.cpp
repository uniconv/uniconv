#include <gtest/gtest.h>
#include "core/registry_types.h"
#include "core/plugin_manifest.h"

using namespace uniconv::core;

TEST(DependencyTest, ToJson)
{
    Dependency dep;
    dep.name = "python3";
    dep.type = "system";
    dep.version = ">=3.8";

    auto j = dep.to_json();
    EXPECT_EQ(j["name"], "python3");
    EXPECT_EQ(j["type"], "system");
    EXPECT_EQ(j["version"], ">=3.8");
    EXPECT_FALSE(j.contains("check"));
}

TEST(DependencyTest, ToJsonWithCheck)
{
    Dependency dep;
    dep.name = "libvips-dev";
    dep.type = "system";
    dep.check = "pkg-config --exists vips";

    auto j = dep.to_json();
    EXPECT_EQ(j["check"], "pkg-config --exists vips");
    EXPECT_FALSE(j.contains("version"));
}

TEST(DependencyTest, FromJson)
{
    nlohmann::json j = {
        {"name", "Pillow"},
        {"type", "python"},
        {"version", ">=9.0"}};

    auto dep = Dependency::from_json(j);
    EXPECT_EQ(dep.name, "Pillow");
    EXPECT_EQ(dep.type, "python");
    ASSERT_TRUE(dep.version.has_value());
    EXPECT_EQ(*dep.version, ">=9.0");
    EXPECT_FALSE(dep.check.has_value());
}

TEST(DependencyTest, FromJsonMinimal)
{
    nlohmann::json j = {{"name", "ffmpeg"}};

    auto dep = Dependency::from_json(j);
    EXPECT_EQ(dep.name, "ffmpeg");
    EXPECT_EQ(dep.type, "system"); // default
    EXPECT_FALSE(dep.version.has_value());
    EXPECT_FALSE(dep.check.has_value());
}

TEST(DependencyTest, RoundTrip)
{
    Dependency dep;
    dep.name = "python3";
    dep.type = "system";
    dep.version = ">=3.8";
    dep.check = "python3 --version";

    auto j = dep.to_json();
    auto dep2 = Dependency::from_json(j);

    EXPECT_EQ(dep.name, dep2.name);
    EXPECT_EQ(dep.type, dep2.type);
    EXPECT_EQ(dep.version, dep2.version);
    EXPECT_EQ(dep.check, dep2.check);
}

TEST(PluginManifestTest, WithDependencies)
{
    nlohmann::json j = {
        {"name", "image-grayscale"},
        {"version", "1.0.0"},
        {"interface", "cli"},
        {"executable", "grayscale.py"},
        {"targets", {"grayscale"}},
        {"dependencies",
         {{{"name", "python3"}, {"type", "system"}, {"version", ">=3.8"}},
          {{"name", "Pillow"}, {"type", "python"}, {"version", ">=9.0"}}}}};

    auto manifest = PluginManifest::from_json(j);
    ASSERT_EQ(manifest.dependencies.size(), 2);
    EXPECT_EQ(manifest.dependencies[0].name, "python3");
    EXPECT_EQ(manifest.dependencies[0].type, "system");
    EXPECT_EQ(manifest.dependencies[1].name, "Pillow");
    EXPECT_EQ(manifest.dependencies[1].type, "python");
}

TEST(PluginManifestTest, WithoutDependencies)
{
    nlohmann::json j = {
        {"name", "image-invert"},
        {"version", "1.0.0"},
        {"interface", "native"},
        {"library", "libimage_invert"},
        {"targets", {"invert"}}};

    auto manifest = PluginManifest::from_json(j);
    EXPECT_TRUE(manifest.dependencies.empty());
}

TEST(PluginManifestTest, DependenciesRoundTrip)
{
    PluginManifest m;
    m.name = "test-plugin";
    m.scope = "test-plugin";
    m.version = "1.0.0";
    m.targets = {"test"};

    Dependency dep;
    dep.name = "python3";
    dep.type = "system";
    dep.version = ">=3.8";
    m.dependencies.push_back(dep);

    auto j = m.to_json();
    ASSERT_TRUE(j.contains("dependencies"));
    ASSERT_EQ(j["dependencies"].size(), 1);

    auto m2 = PluginManifest::from_json(j);
    ASSERT_EQ(m2.dependencies.size(), 1);
    EXPECT_EQ(m2.dependencies[0].name, "python3");
}

TEST(RegistryArtifactTest, RoundTrip)
{
    RegistryArtifact a;
    a.url = "https://example.com/plugin.tar.gz";
    a.sha256 = "abc123def456";

    auto j = a.to_json();
    auto a2 = RegistryArtifact::from_json(j);

    EXPECT_EQ(a.url, a2.url);
    EXPECT_EQ(a.sha256, a2.sha256);
}

TEST(RegistryReleaseTest, RoundTrip)
{
    RegistryRelease r;
    r.version = "1.2.0";
    r.uniconv_compat = ">=0.3.0";
    r.iface = "cli";

    Dependency dep;
    dep.name = "python3";
    dep.type = "system";
    r.dependencies.push_back(dep);

    RegistryArtifact a;
    a.url = "https://example.com/plugin.tar.gz";
    a.sha256 = "abc123";
    r.artifacts["any"] = a;

    auto j = r.to_json();
    auto r2 = RegistryRelease::from_json(j);

    EXPECT_EQ(r.version, r2.version);
    EXPECT_EQ(r.uniconv_compat, r2.uniconv_compat);
    EXPECT_EQ(r.iface, r2.iface);
    ASSERT_EQ(r2.dependencies.size(), 1);
    ASSERT_EQ(r2.artifacts.size(), 1);
    EXPECT_EQ(r2.artifacts.at("any").url, a.url);
}

TEST(RegistryPluginEntryTest, RoundTrip)
{
    RegistryPluginEntry e;
    e.name = "image-grayscale";
    e.description = "Convert to grayscale";
    e.author = "somedev";
    e.license = "MIT";
    e.repository = "https://github.com/somedev/plugin";
    e.keywords = {"image", "grayscale"};

    RegistryRelease r;
    r.version = "1.0.0";
    r.iface = "cli";
    e.releases.push_back(r);

    auto j = e.to_json();
    auto e2 = RegistryPluginEntry::from_json(j);

    EXPECT_EQ(e.name, e2.name);
    EXPECT_EQ(e.description, e2.description);
    EXPECT_EQ(e.author, e2.author);
    EXPECT_EQ(e.license, e2.license);
    EXPECT_EQ(e.repository, e2.repository);
    EXPECT_EQ(e.keywords, e2.keywords);
    ASSERT_EQ(e2.releases.size(), 1);
    EXPECT_EQ(e2.releases[0].version, "1.0.0");
}

TEST(RegistryIndexTest, RoundTrip)
{
    RegistryIndex idx;
    idx.version = 1;
    idx.updated_at = "2026-02-03T12:00:00Z";

    RegistryIndexEntry entry;
    entry.name = "image-grayscale";
    entry.description = "Convert to grayscale";
    entry.keywords = {"image"};
    entry.latest = "1.0.0";
    entry.author = "somedev";
    entry.iface = "cli";
    idx.plugins.push_back(entry);

    auto j = idx.to_json();
    auto idx2 = RegistryIndex::from_json(j);

    EXPECT_EQ(idx.version, idx2.version);
    EXPECT_EQ(idx.updated_at, idx2.updated_at);
    ASSERT_EQ(idx2.plugins.size(), 1);
    EXPECT_EQ(idx2.plugins[0].name, "image-grayscale");
    EXPECT_EQ(idx2.plugins[0].latest, "1.0.0");
}

TEST(InstalledPluginRecordTest, RoundTrip)
{
    InstalledPluginRecord r;
    r.version = "1.2.0";
    r.installed_at = "2026-02-03T12:00:00Z";
    r.source = "registry";

    auto j = r.to_json();
    auto r2 = InstalledPluginRecord::from_json(j);

    EXPECT_EQ(r.version, r2.version);
    EXPECT_EQ(r.installed_at, r2.installed_at);
    EXPECT_EQ(r.source, r2.source);
}
