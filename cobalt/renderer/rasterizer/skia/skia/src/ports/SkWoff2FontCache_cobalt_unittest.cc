// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkWoff2FontCache_cobalt.h"

#include <optional>
#include <string>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sk_woff2_cache_cobalt {
namespace {

class SkWoff2FontCacheCobaltTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_cache_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_cache_dir_.GetPath());

    ASSERT_TRUE(temp_source_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { cache_override_.reset(); }

  base::FilePath CreateTestFile(const std::string& filename,
                                const std::string& content) {
    base::FilePath file_path = temp_source_dir_.GetPath().AppendASCII(filename);
    EXPECT_TRUE(base::WriteFile(file_path, content));
    return file_path;
  }

  base::ScopedTempDir temp_cache_dir_;
  base::ScopedTempDir temp_source_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_override_;
};

TEST_F(SkWoff2FontCacheCobaltTest, FeatureDisabledByDefault) {
  EXPECT_FALSE(IsMmapFontCacheEnabled());
}

TEST_F(SkWoff2FontCacheCobaltTest, FeatureEnabledViaScopedFeatureList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCobaltMmapFontCache);
  EXPECT_TRUE(IsMmapFontCacheEnabled());
}

TEST_F(SkWoff2FontCacheCobaltTest, NonWoff2ExtensionReturnsEmpty) {
  base::FilePath ttf_file = CreateTestFile("test_font.ttf", "mock_ttf_data");
  SkString cached_path =
      GetOrCreateCachedSfntPath(SkString(ttf_file.value().c_str()));
  EXPECT_TRUE(cached_path.isEmpty());

  base::FilePath txt_file = CreateTestFile("font.txt", "not a font");
  EXPECT_TRUE(
      GetOrCreateCachedSfntPath(SkString(txt_file.value().c_str())).isEmpty());
}

TEST_F(SkWoff2FontCacheCobaltTest, NonExistentFileReturnsEmpty) {
  base::FilePath non_existent =
      temp_source_dir_.GetPath().AppendASCII("does_not_exist.woff2");
  SkString cached_path =
      GetOrCreateCachedSfntPath(SkString(non_existent.value().c_str()));
  EXPECT_TRUE(cached_path.isEmpty());
}

TEST_F(SkWoff2FontCacheCobaltTest,
       CorruptWoff2FileReturnsEmptyWithoutCrashing) {
  // Create a file with .woff2 extension but corrupt/invalid content
  base::FilePath corrupt_woff2 = CreateTestFile(
      "corrupt_font.woff2", "wOF2_corrupted_garbage_bytes_12345");

  SkString cached_path =
      GetOrCreateCachedSfntPath(SkString(corrupt_woff2.value().c_str()));
  EXPECT_TRUE(cached_path.isEmpty());

  // Verify no corrupt permanent files were left in the font cache directory
  base::FilePath font_cache_dir =
      temp_cache_dir_.GetPath().AppendASCII("font_cache");
  if (base::PathExists(font_cache_dir)) {
    base::FileEnumerator enumerator(font_cache_dir, false,
                                    base::FileEnumerator::FILES);
    EXPECT_TRUE(enumerator.Next().empty());
  }
}

TEST_F(SkWoff2FontCacheCobaltTest, DecompressValidWoff2AndVerifyCacheHit) {
  // Try locating a sample WOFF2 font from system fonts directory or executable
  // directory
  base::FilePath sample_woff2;

  base::FilePath sys_fonts;
  if (base::PathService::Get(base::DIR_SYSTEM_FONTS, &sys_fonts)) {
    base::FileEnumerator enumerator(sys_fonts, false,
                                    base::FileEnumerator::FILES,
                                    FILE_PATH_LITERAL("*.woff2"));
    sample_woff2 = enumerator.Next();
  }

  if (sample_woff2.empty()) {
    base::FilePath exe_dir;
    if (base::PathService::Get(base::DIR_EXE, &exe_dir)) {
      base::FilePath candidate =
          exe_dir.AppendASCII("content").AppendASCII("fonts").AppendASCII(
              "Roboto-Regular-Subsetted.woff2");
      if (base::PathExists(candidate)) {
        sample_woff2 = candidate;
      }
    }
  }

  if (sample_woff2.empty()) {
    LOG(INFO) << "No sample WOFF2 font available in test environment, skipping "
                 "decompression test.";
    return;
  }

  // 1. Initial lookup (Cold path -> Decompresses to cache directory)
  SkString cached_path_1 =
      GetOrCreateCachedSfntPath(SkString(sample_woff2.value().c_str()));
  ASSERT_FALSE(cached_path_1.isEmpty());

  base::FilePath cached_file_path(cached_path_1.c_str());
  EXPECT_TRUE(base::PathExists(cached_file_path));

  std::optional<int64_t> file_size = base::GetFileSize(cached_file_path);
  ASSERT_TRUE(file_size.has_value());
  EXPECT_GT(file_size.value(), 0);

  // Verify the decompressed file is a valid SFNT/TrueType file (header check)
  std::string file_header;
  file_header.resize(4);
  base::File file(cached_file_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  EXPECT_EQ(file.Read(0, &file_header[0], 4), 4);
  // Valid TrueType/OpenType font versions: 0x00010000 ('\0\1\0\0') or 'OTTO' or
  // 'true' or 'typ1'
  uint32_t magic = (static_cast<uint8_t>(file_header[0]) << 24) |
                   (static_cast<uint8_t>(file_header[1]) << 16) |
                   (static_cast<uint8_t>(file_header[2]) << 8) |
                   static_cast<uint8_t>(file_header[3]);
  EXPECT_TRUE(magic == 0x00010000 || magic == 0x4F54544F ||
              magic == 0x74727565 || magic == 0x74746366);

  // Verify that SkStream::MakeFromFile successfully opens the cached file
  // (mmap)
  std::unique_ptr<SkStreamAsset> stream =
      SkStream::MakeFromFile(cached_path_1.c_str());
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getLength(), static_cast<size_t>(file_size.value()));

  // 2. Second lookup (Warm path -> Cache Hit)
  SkString cached_path_2 =
      GetOrCreateCachedSfntPath(SkString(sample_woff2.value().c_str()));
  EXPECT_STREQ(cached_path_1.c_str(), cached_path_2.c_str());
}

}  // namespace
}  // namespace sk_woff2_cache_cobalt
