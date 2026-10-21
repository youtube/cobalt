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

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sk_woff2_cache_cobalt {
namespace {

// Returns a sample WOFF2 font from the system fonts directory or the
// executable directory, or an empty path if none is available.
base::FilePath FindSampleWoff2() {
  base::FilePath sys_fonts;
  if (base::PathService::Get(base::DIR_SYSTEM_FONTS, &sys_fonts)) {
    base::FileEnumerator enumerator(sys_fonts, false,
                                    base::FileEnumerator::FILES,
                                    FILE_PATH_LITERAL("*.woff2"));
    base::FilePath sample_woff2 = enumerator.Next();
    if (!sample_woff2.empty()) {
      return sample_woff2;
    }
  }

  base::FilePath exe_dir;
  if (base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    base::FilePath candidate =
        exe_dir.AppendASCII("content").AppendASCII("fonts").AppendASCII(
            "Roboto-Regular-Subsetted.woff2");
    if (base::PathExists(candidate)) {
      return candidate;
    }
  }
  return base::FilePath();
}

sk_sp<SkData> MakeData(std::string_view bytes) {
  return SkData::MakeWithCopy(bytes.data(), bytes.size());
}

std::string ReadFile(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return contents;
}

class SkWoff2FontCacheCobaltTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_cache_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_cache_dir_.GetPath());

    ASSERT_TRUE(temp_source_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    // Let scheduled cache work finish while DIR_CACHE is still overridden.
    task_environment_.RunUntilIdle();
    cache_override_.reset();
  }

  base::FilePath CreateTestFile(const std::string& filename,
                                const std::string& content) {
    base::FilePath file_path = temp_source_dir_.GetPath().AppendASCII(filename);
    EXPECT_TRUE(base::WriteFile(file_path, content));
    return file_path;
  }

  // Returns the cache file path for |woff2_path|, keyed the way the cache has
  // always been keyed: on base::GetFileInfo()'s size and mtime.
  base::FilePath ExpectedCacheFile(const base::FilePath& woff2_path) {
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(woff2_path, &info));
    return font_cache_dir().Append(
        woff2_path.BaseName().RemoveExtension().value() + "." +
        base::NumberToString(info.size) + "." +
        base::NumberToString(
            static_cast<int64_t>(info.last_modified.ToTimeT())) +
        ".ttf");
  }

  // The cache code's view of <DIR_CACHE>/font_cache. Uses PathService rather
  // than |temp_cache_dir_| because the override is made absolute (resolving
  // any symlinks), so that path strings compare equal.
  base::FilePath font_cache_dir() const {
    return base::PathService::CheckedGet(base::DIR_CACHE)
        .AppendASCII("font_cache");
  }

  // Returns the sorted paths of the files and directories in font_cache_dir().
  std::vector<base::FilePath> ListFontCacheDir() const {
    std::vector<base::FilePath> entries;
    base::FileEnumerator enumerator(
        font_cache_dir(), false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      entries.push_back(path);
    }
    std::sort(entries.begin(), entries.end());
    return entries;
  }

  base::test::TaskEnvironment task_environment_;
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
  EXPECT_FALSE(DecompressWoff2(SkString(ttf_file.value().c_str())));
  EXPECT_TRUE(GetCachedSfntPath(SkString(ttf_file.value().c_str())).isEmpty());

  base::FilePath txt_file = CreateTestFile("font.txt", "not a font");
  EXPECT_FALSE(DecompressWoff2(SkString(txt_file.value().c_str())));
}

TEST_F(SkWoff2FontCacheCobaltTest, NonExistentFileReturnsEmpty) {
  base::FilePath non_existent =
      temp_source_dir_.GetPath().AppendASCII("does_not_exist.woff2");
  EXPECT_FALSE(DecompressWoff2(SkString(non_existent.value().c_str())));
  EXPECT_TRUE(
      GetCachedSfntPath(SkString(non_existent.value().c_str())).isEmpty());
}

TEST_F(SkWoff2FontCacheCobaltTest,
       CorruptWoff2FileReturnsEmptyWithoutCrashing) {
  // Create a file with .woff2 extension but corrupt/invalid content
  base::FilePath corrupt_woff2 = CreateTestFile(
      "corrupt_font.woff2", "wOF2_corrupted_garbage_bytes_12345");
  EXPECT_FALSE(DecompressWoff2(SkString(corrupt_woff2.value().c_str())));

  // An empty file cannot even be mapped.
  base::FilePath empty_woff2 = CreateTestFile("empty_font.woff2", "");
  EXPECT_FALSE(DecompressWoff2(SkString(empty_woff2.value().c_str())));

  // Decompression never writes to the font cache directory.
  EXPECT_FALSE(base::PathExists(font_cache_dir()));
}

TEST_F(SkWoff2FontCacheCobaltTest, DecompressValidWoff2AndVerifyCacheHit) {
  // Try locating a sample WOFF2 font from system fonts directory or executable
  // directory
  base::FilePath sample_woff2 = FindSampleWoff2();

  if (sample_woff2.empty()) {
    LOG(INFO) << "No sample WOFF2 font available in test environment, skipping "
                 "decompression test.";
    return;
  }
  const SkString font_path(sample_woff2.value().c_str());

  // 1. Cold path: decompresses into memory, without writing the cache file.
  sk_sp<SkData> sfnt = DecompressWoff2(font_path);
  ASSERT_TRUE(sfnt);
  ASSERT_GE(sfnt->size(), 4u);
  EXPECT_TRUE(GetCachedSfntPath(font_path).isEmpty());

  // Verify the decompressed bytes are a valid SFNT/TrueType font (header
  // check). Valid TrueType/OpenType font versions: 0x00010000 ('\0\1\0\0'),
  // 'OTTO', 'true', or 'ttcf' for a font collection.
  const uint8_t* header = sfnt->bytes();
  uint32_t magic = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) |
                   static_cast<uint32_t>(header[3]);
  EXPECT_TRUE(magic == 0x00010000 || magic == 0x4F54544F ||
              magic == 0x74727565 || magic == 0x74746366);

  // 2. The background write stores the same bytes in the cache file.
  ScheduleCacheFileWrite(font_path, sfnt);
  task_environment_.RunUntilIdle();
  const SkString cached_path = GetCachedSfntPath(font_path);
  ASSERT_FALSE(cached_path.isEmpty());
  EXPECT_EQ(
      ReadFile(base::FilePath(cached_path.c_str())),
      std::string_view(static_cast<const char*>(sfnt->data()), sfnt->size()));

  // 3. Warm path (Cache Hit): SkStream::MakeFromFile opens (mmaps) the cache
  // file.
  std::unique_ptr<SkStreamAsset> stream =
      SkStream::MakeFromFile(cached_path.c_str());
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getLength(), sfnt->size());
}

// The decompression buffer is sized from the WOFF2 header, so check that each
// installed font (e.g. NotoSansCJK-Regular.woff2, a font collection) fits.
TEST_F(SkWoff2FontCacheCobaltTest, DecompressesAllInstalledWoff2Fonts) {
  base::FilePath sys_fonts;
  if (!base::PathService::Get(base::DIR_SYSTEM_FONTS, &sys_fonts)) {
    GTEST_SKIP() << "No system fonts directory.";
  }
  base::FileEnumerator enumerator(sys_fonts, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.woff2"));
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    EXPECT_TRUE(DecompressWoff2(SkString(path.value().c_str())))
        << path.value();
  }
}

TEST_F(SkWoff2FontCacheCobaltTest, LookupMissReturnsEmptyWithoutWriting) {
  base::FilePath woff2 = CreateTestFile("font.woff2", "not really woff2");
  EXPECT_TRUE(GetCachedSfntPath(SkString(woff2.value().c_str())).isEmpty());

  base::FilePath non_existent =
      temp_source_dir_.GetPath().AppendASCII("does_not_exist.woff2");
  EXPECT_TRUE(
      GetCachedSfntPath(SkString(non_existent.value().c_str())).isEmpty());

  base::FilePath ttf_file = CreateTestFile("test_font.ttf", "mock_ttf_data");
  EXPECT_TRUE(GetCachedSfntPath(SkString(ttf_file.value().c_str())).isEmpty());

  // The lookup never creates the cache directory (or anything else).
  EXPECT_FALSE(base::PathExists(font_cache_dir()));
}

TEST_F(SkWoff2FontCacheCobaltTest, LookupFindsExistingCacheFile) {
  base::FilePath woff2 = CreateTestFile("font.woff2", "not really woff2");
  const SkString font_path(woff2.value().c_str());
  const base::FilePath cache_file = ExpectedCacheFile(woff2);
  ASSERT_TRUE(base::CreateDirectory(cache_file.DirName()));

  // An empty cache file is a miss.
  ASSERT_TRUE(base::WriteFile(cache_file, ""));
  EXPECT_TRUE(GetCachedSfntPath(font_path).isEmpty());

  ASSERT_TRUE(base::WriteFile(cache_file, "sfnt bytes"));
  EXPECT_STREQ(GetCachedSfntPath(font_path).c_str(),
               cache_file.value().c_str());
}

// Regression test: typefaces can be created where blocking is disallowed
// (e.g. on the browser UI thread, as in I18nBrowserTest), so the calls made
// on that path must not trip base's blocking-call assertions.
TEST_F(SkWoff2FontCacheCobaltTest, FontLoadingCallsWhereBlockingDisallowed) {
  base::FilePath uncached_woff2 =
      CreateTestFile("uncached.woff2", "not really woff2");
  base::FilePath cached_woff2 =
      CreateTestFile("cached.woff2", "not really woff2 either");
  const base::FilePath cache_file = ExpectedCacheFile(cached_woff2);
  ASSERT_TRUE(base::CreateDirectory(cache_file.DirName()));
  ASSERT_TRUE(base::WriteFile(cache_file, "sfnt bytes"));
  base::FilePath sample_woff2 = FindSampleWoff2();

  {
    base::ScopedDisallowBlocking disallow_blocking;
    base::ScopedDisallowBaseSyncPrimitives disallow_sync_primitives;

    const SkString uncached_path(uncached_woff2.value().c_str());
    EXPECT_TRUE(GetCachedSfntPath(uncached_path).isEmpty());
    EXPECT_FALSE(DecompressWoff2(uncached_path));
    ScheduleCacheFileWrite(uncached_path, MakeData("sfnt bytes"));

    if (!sample_woff2.empty()) {
      EXPECT_TRUE(DecompressWoff2(SkString(sample_woff2.value().c_str())));
    }

    EXPECT_STREQ(
        GetCachedSfntPath(SkString(cached_woff2.value().c_str())).c_str(),
        cache_file.value().c_str());

    // The font manager schedules the cleanup when it is created.
    ScheduleCacheCleanup({uncached_woff2.value(), cached_woff2.value()});
  }
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ListFontCacheDir(), (std::vector<base::FilePath>{
                                    cache_file,
                                    ExpectedCacheFile(uncached_woff2),
                                }));
}

TEST_F(SkWoff2FontCacheCobaltTest, ScheduledWriteCreatesCacheFile) {
  base::FilePath woff2 = CreateTestFile("font.woff2", "woff2 bytes");
  const SkString font_path(woff2.value().c_str());
  EXPECT_TRUE(GetCachedSfntPath(font_path).isEmpty());

  ScheduleCacheFileWrite(font_path, MakeData("sfnt bytes"));
  task_environment_.RunUntilIdle();

  const base::FilePath cache_file = ExpectedCacheFile(woff2);
  EXPECT_STREQ(GetCachedSfntPath(font_path).c_str(),
               cache_file.value().c_str());
  EXPECT_EQ(ReadFile(cache_file), "sfnt bytes");
  // The cache file is the only file left behind (no temporary files).
  EXPECT_EQ(ListFontCacheDir(), std::vector<base::FilePath>{cache_file});
}

// A cache file that failed to load (e.g. a truncated one) is replaced when the
// font is decompressed and written again.
TEST_F(SkWoff2FontCacheCobaltTest, ScheduledWriteReplacesExistingCacheFile) {
  base::FilePath woff2 = CreateTestFile("font.woff2", "woff2 bytes");
  const base::FilePath cache_file = ExpectedCacheFile(woff2);
  ASSERT_TRUE(base::CreateDirectory(cache_file.DirName()));
  ASSERT_TRUE(base::WriteFile(cache_file, "truncated"));

  ScheduleCacheFileWrite(SkString(woff2.value().c_str()),
                         MakeData("sfnt bytes"));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(ReadFile(cache_file), "sfnt bytes");
  EXPECT_EQ(ListFontCacheDir(), std::vector<base::FilePath>{cache_file});
}

TEST_F(SkWoff2FontCacheCobaltTest, ScheduleNonWoff2IsNoOp) {
  base::FilePath ttf_file = CreateTestFile("test_font.ttf", "mock_ttf_data");
  ScheduleCacheFileWrite(SkString(ttf_file.value().c_str()),
                         MakeData("sfnt bytes"));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(font_cache_dir()));
}

// The cleanup keeps the cache files of the current version of the listed
// fonts (whether or not they are used in the session) and deletes everything
// else.
TEST_F(SkWoff2FontCacheCobaltTest, CleanupDeletesAllButCurrentCacheFiles) {
  base::FilePath woff2_a = CreateTestFile("FontA.woff2", "woff2 bytes");
  base::FilePath woff2_b = CreateTestFile("FontB.woff2", "other woff2 bytes");
  const base::FilePath cache_file_a = ExpectedCacheFile(woff2_a);
  const base::FilePath cache_file_b = ExpectedCacheFile(woff2_b);
  const base::FilePath cache_dir = font_cache_dir();
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  ASSERT_TRUE(base::WriteFile(cache_file_a, "sfnt bytes"));
  ASSERT_TRUE(base::WriteFile(cache_file_b, "sfnt bytes"));

  // Stale files: the cache file of an earlier version of a current font (e.g.
  // before a firmware update), the cache file of a removed font, and the
  // temporary file of an interrupted cache file write.
  ASSERT_TRUE(base::WriteFile(cache_dir.AppendASCII("FontA.1234.5678.ttf"),
                              "stale sfnt bytes"));
  ASSERT_TRUE(base::WriteFile(cache_dir.AppendASCII("FontC.1234.5678.ttf"),
                              "stale sfnt bytes"));
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(cache_dir, &temp_file));

  // Non-WOFF2 fonts are ignored.
  base::FilePath ttf_file = CreateTestFile("FontD.ttf", "mock_ttf_data");
  ScheduleCacheCleanup({woff2_a.value(), woff2_b.value(), ttf_file.value()});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(ListFontCacheDir(),
            (std::vector<base::FilePath>{cache_file_a, cache_file_b}));
}

TEST_F(SkWoff2FontCacheCobaltTest, CleanupWithoutCacheDirectoryIsNoOp) {
  base::FilePath woff2 = CreateTestFile("font.woff2", "woff2 bytes");
  ScheduleCacheCleanup({woff2.value()});
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(font_cache_dir()));
}

// Cache file writes and cleanups run one at a time, in the order in which
// they were scheduled, so that a cleanup never runs concurrently with (and
// deletes the temporary file of) a write.
TEST_F(SkWoff2FontCacheCobaltTest, CacheWorkRunsInScheduledOrder) {
  base::FilePath woff2 = CreateTestFile("font.woff2", "woff2 bytes");
  const SkString font_path(woff2.value().c_str());

  // A cleanup that does not keep the font deletes the cache file written
  // before it...
  ScheduleCacheFileWrite(font_path, MakeData("sfnt bytes"));
  ScheduleCacheCleanup({});
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetCachedSfntPath(font_path).isEmpty());
  EXPECT_TRUE(ListFontCacheDir().empty());

  // ...but not the one written after it.
  ScheduleCacheCleanup({});
  ScheduleCacheFileWrite(font_path, MakeData("sfnt bytes"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ListFontCacheDir(),
            std::vector<base::FilePath>{ExpectedCacheFile(woff2)});
}

// Without a ThreadPool (e.g. in tests that create typefaces without a
// TaskEnvironment), scheduling must be a no-op rather than a crash.
TEST(SkWoff2FontCacheCobaltNoThreadPoolTest, ScheduleIsNoOp) {
  if (base::ThreadPoolInstance::Get()) {
    GTEST_SKIP() << "A ThreadPool already exists.";
  }
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::ScopedPathOverride cache_override(base::DIR_CACHE, temp_dir.GetPath());
  base::FilePath woff2 = temp_dir.GetPath().AppendASCII("font.woff2");
  ASSERT_TRUE(base::WriteFile(woff2, "not really woff2"));

  ScheduleCacheFileWrite(SkString(woff2.value().c_str()),
                         MakeData("sfnt bytes"));
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath().AppendASCII("font_cache")));

  ScheduleCacheCleanup({woff2.value()});
}

}  // namespace
}  // namespace sk_woff2_cache_cobalt
