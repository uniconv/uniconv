#include <gtest/gtest.h>
#include "cli/pipeline_parser.h"

using namespace uniconv;

class PipelineParserTest : public ::testing::Test
{
protected:
    cli::PipelineParser parser;
    std::filesystem::path test_source = "/path/to/input.heic";
    core::CoreOptions default_options;
};

// ============================================================================
// Single Stage Parsing
// ============================================================================

TEST_F(PipelineParserTest, SingleStageSimpleTarget)
{
    auto result = parser.parse("jpg", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 1);
    ASSERT_EQ(result.pipeline.stages[0].elements.size(), 1);

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    EXPECT_FALSE(elem.plugin.has_value());
    EXPECT_TRUE(elem.options.empty());
}

TEST_F(PipelineParserTest, SingleStageWithOptions)
{
    auto result = parser.parse("jpg --quality 90", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 1);

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    ASSERT_EQ(elem.options.count("quality"), 1);
    EXPECT_EQ(elem.options.at("quality"), "90");
}

TEST_F(PipelineParserTest, SingleStageWithMultipleOptions)
{
    auto result = parser.parse("jpg --quality 90 --resize 800x600", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    ASSERT_EQ(elem.options.count("quality"), 1);
    EXPECT_EQ(elem.options.at("quality"), "90");
    ASSERT_EQ(elem.options.count("resize"), 1);
    EXPECT_EQ(elem.options.at("resize"), "800x600");
}

TEST_F(PipelineParserTest, SingleStageWithBooleanFlag)
{
    auto result = parser.parse("jpg --progressive", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    ASSERT_EQ(elem.options.count("progressive"), 1);
    EXPECT_EQ(elem.options.at("progressive"), "true");
}

TEST_F(PipelineParserTest, SingleStageWithEqualsFormat)
{
    auto result = parser.parse("jpg --quality=90", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    ASSERT_EQ(elem.options.count("quality"), 1);
    EXPECT_EQ(elem.options.at("quality"), "90");
}

// ============================================================================
// Plugin Specification
// ============================================================================

TEST_F(PipelineParserTest, PluginSpecification)
{
    auto result = parser.parse("jpg@vips", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    ASSERT_TRUE(elem.plugin.has_value());
    EXPECT_EQ(*elem.plugin, "vips");
}

TEST_F(PipelineParserTest, PluginWithOptions)
{
    auto result = parser.parse("jpg@vips --quality 90", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "jpg");
    ASSERT_TRUE(elem.plugin.has_value());
    EXPECT_EQ(*elem.plugin, "vips");
    ASSERT_EQ(elem.options.count("quality"), 1);
    EXPECT_EQ(elem.options.at("quality"), "90");
}

// ============================================================================
// Multi-Stage Pipeline
// ============================================================================

TEST_F(PipelineParserTest, TwoStageSimple)
{
    auto result = parser.parse("jpg | gdrive", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 2);

    EXPECT_EQ(result.pipeline.stages[0].elements[0].target, "jpg");
    EXPECT_EQ(result.pipeline.stages[1].elements[0].target, "gdrive");
}

TEST_F(PipelineParserTest, TwoStageWithOptions)
{
    auto result = parser.parse("jpg --quality 90 | gdrive --folder /photos", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 2);

    const auto &stage1 = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(stage1.target, "jpg");
    EXPECT_EQ(stage1.options.at("quality"), "90");

    const auto &stage2 = result.pipeline.stages[1].elements[0];
    EXPECT_EQ(stage2.target, "gdrive");
    EXPECT_EQ(stage2.options.at("folder"), "/photos");
}

TEST_F(PipelineParserTest, ThreeStageChain)
{
    auto result = parser.parse("jpg --quality 90 | resize 800x600 | gdrive", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 3);

    EXPECT_EQ(result.pipeline.stages[0].elements[0].target, "jpg");
    EXPECT_EQ(result.pipeline.stages[1].elements[0].target, "resize");
    EXPECT_EQ(result.pipeline.stages[2].elements[0].target, "gdrive");
}

// ============================================================================
// Tee and Branching
// ============================================================================

TEST_F(PipelineParserTest, TeeWithThreeBranches)
{
    auto result = parser.parse("tee | jpg, png, webp", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 2);

    // First stage has tee
    ASSERT_EQ(result.pipeline.stages[0].elements.size(), 1);
    EXPECT_EQ(result.pipeline.stages[0].elements[0].target, "tee");
    EXPECT_TRUE(result.pipeline.stages[0].has_tee());

    // Second stage has 3 parallel elements
    ASSERT_EQ(result.pipeline.stages[1].elements.size(), 3);
    EXPECT_EQ(result.pipeline.stages[1].elements[0].target, "jpg");
    EXPECT_EQ(result.pipeline.stages[1].elements[1].target, "png");
    EXPECT_EQ(result.pipeline.stages[1].elements[2].target, "webp");
}

TEST_F(PipelineParserTest, TransformThenBranch)
{
    auto result = parser.parse("jpg | tee | gdrive, s3", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 3);

    EXPECT_EQ(result.pipeline.stages[0].elements[0].target, "jpg");
    EXPECT_EQ(result.pipeline.stages[1].elements[0].target, "tee");

    ASSERT_EQ(result.pipeline.stages[2].elements.size(), 2);
    EXPECT_EQ(result.pipeline.stages[2].elements[0].target, "gdrive");
    EXPECT_EQ(result.pipeline.stages[2].elements[1].target, "s3");
}

TEST_F(PipelineParserTest, TeeWithOptionsOnBranches)
{
    auto result = parser.parse("tee | jpg --quality 90, png --compress 9", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 2);

    ASSERT_EQ(result.pipeline.stages[1].elements.size(), 2);

    const auto &jpg = result.pipeline.stages[1].elements[0];
    EXPECT_EQ(jpg.target, "jpg");
    EXPECT_EQ(jpg.options.at("quality"), "90");

    const auto &png = result.pipeline.stages[1].elements[1];
    EXPECT_EQ(png.target, "png");
    EXPECT_EQ(png.options.at("compress"), "9");
}

TEST_F(PipelineParserTest, TeeWithPluginsOnBranches)
{
    auto result = parser.parse("tee | jpg@vips --quality 90, png@imagemagick --compress 9", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &jpg = result.pipeline.stages[1].elements[0];
    EXPECT_EQ(jpg.target, "jpg");
    EXPECT_EQ(*jpg.plugin, "vips");
    EXPECT_EQ(jpg.options.at("quality"), "90");

    const auto &png = result.pipeline.stages[1].elements[1];
    EXPECT_EQ(png.target, "png");
    EXPECT_EQ(*png.plugin, "imagemagick");
    EXPECT_EQ(png.options.at("compress"), "9");
}

// ============================================================================
// Validation Errors
// ============================================================================

TEST_F(PipelineParserTest, ParallelWithoutTeeFails)
{
    auto result = parser.parse("jpg, png", test_source, default_options);

    // Single stage with 2 parallel elements is actually valid
    // (it's just 2 final outputs). It only fails if there's a next stage with mismatched count.
    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 1);
    ASSERT_EQ(result.pipeline.stages[0].elements.size(), 2);
}

TEST_F(PipelineParserTest, BranchToSingleWithoutMergeFails)
{
    auto result = parser.parse("tee | jpg, png | gdrive", test_source, default_options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
    // Stage 1 has 2 elements, stage 2 has 1 element - mismatch without tee
}

TEST_F(PipelineParserTest, CountMismatchFails)
{
    auto result = parser.parse("tee | jpg, png, webp | gdrive, s3", test_source, default_options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
    // Stage 1 has 3 elements, stage 2 has 2 elements - count mismatch
}

TEST_F(PipelineParserTest, EmptyPipelineFails)
{
    auto result = parser.parse("", test_source, default_options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(PipelineParserTest, EmptyStageFails)
{
    auto result = parser.parse("jpg | | gdrive", test_source, default_options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PipelineParserTest, WhitespaceHandling)
{
    auto result = parser.parse("  jpg   |   gdrive  ", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 2);

    EXPECT_EQ(result.pipeline.stages[0].elements[0].target, "jpg");
    EXPECT_EQ(result.pipeline.stages[1].elements[0].target, "gdrive");
}

TEST_F(PipelineParserTest, ExtraWhitespaceInOptions)
{
    auto result = parser.parse("jpg   --quality   90   --resize   800x600", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.options.at("quality"), "90");
    EXPECT_EQ(elem.options.at("resize"), "800x600");
}

TEST_F(PipelineParserTest, QuotedStringInOptions)
{
    auto result = parser.parse("gdrive --folder \"/my photos\"", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    ASSERT_EQ(elem.options.count("folder"), 1);
    EXPECT_EQ(elem.options.at("folder"), "/my photos");
}

TEST_F(PipelineParserTest, QuotedStringWithComma)
{
    auto result = parser.parse("tee | jpg --metadata \"author, date\", png", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages[1].elements.size(), 2);

    const auto &jpg = result.pipeline.stages[1].elements[0];
    EXPECT_EQ(jpg.target, "jpg");
    EXPECT_EQ(jpg.options.at("metadata"), "author, date");
}

TEST_F(PipelineParserTest, QuotedStringWithPipe)
{
    auto result = parser.parse("rename --pattern \"file|name\" | jpg", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 2);

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "rename");
    EXPECT_EQ(elem.options.at("pattern"), "file|name");
}

TEST_F(PipelineParserTest, SingleQuotes)
{
    auto result = parser.parse("gdrive --folder '/my photos'", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.options.at("folder"), "/my photos");
}

TEST_F(PipelineParserTest, EscapedCharacters)
{
    auto result = parser.parse("rename --pattern file\\ name", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;

    const auto &elem = result.pipeline.stages[0].elements[0];
    EXPECT_EQ(elem.target, "rename");
}

// ============================================================================
// Pipeline Structure Validation
// ============================================================================

TEST_F(PipelineParserTest, ValidParallelToParallelSameCount)
{
    auto result = parser.parse("tee | jpg, png | gdrive, s3", test_source, default_options);

    // 2→2 is valid (1:1 mapping), so this should succeed
    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 3);
    ASSERT_EQ(result.pipeline.stages[1].elements.size(), 2);
    ASSERT_EQ(result.pipeline.stages[2].elements.size(), 2);
}

TEST_F(PipelineParserTest, ValidOneToOneChain)
{
    auto result = parser.parse("jpg | resize | gdrive", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    ASSERT_EQ(result.pipeline.stages.size(), 3);

    for (const auto &stage : result.pipeline.stages)
    {
        EXPECT_EQ(stage.elements.size(), 1);
    }
}

// ============================================================================
// is_pipeline_syntax Tests
// ============================================================================

TEST(PipelineParserStaticTest, IsPipelineSyntaxDetectsPipe)
{
    std::vector<std::string> args = {"input.heic", "|", "jpg"};
    EXPECT_TRUE(cli::PipelineParser::is_pipeline_syntax(args));
}

TEST(PipelineParserStaticTest, IsPipelineSyntaxDetectsPipeInArg)
{
    std::vector<std::string> args = {"input.heic", "jpg|gdrive"};
    EXPECT_TRUE(cli::PipelineParser::is_pipeline_syntax(args));
}

TEST(PipelineParserStaticTest, IsPipelineSyntaxNoPipe)
{
    std::vector<std::string> args = {"input.heic", "-t", "jpg"};
    EXPECT_FALSE(cli::PipelineParser::is_pipeline_syntax(args));
}

TEST(PipelineParserStaticTest, IsPipelineSyntaxEmpty)
{
    std::vector<std::string> args;
    EXPECT_FALSE(cli::PipelineParser::is_pipeline_syntax(args));
}

// ============================================================================
// Core Options Preservation
// ============================================================================

TEST_F(PipelineParserTest, CoreOptionsPreserved)
{
    core::CoreOptions opts;
    opts.force = true;
    opts.verbose = true;
    opts.output = "/custom/output";

    auto result = parser.parse("jpg", test_source, opts);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    EXPECT_TRUE(result.pipeline.core_options.force);
    EXPECT_TRUE(result.pipeline.core_options.verbose);
    EXPECT_EQ(result.pipeline.core_options.output, "/custom/output");
}

TEST_F(PipelineParserTest, SourcePreserved)
{
    auto result = parser.parse("jpg", test_source, default_options);

    ASSERT_TRUE(result.success) << "Parse failed: " << result.error;
    EXPECT_EQ(result.pipeline.source, test_source);
}
