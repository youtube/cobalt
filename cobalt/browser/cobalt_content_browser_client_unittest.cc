// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/cobalt_content_browser_client.h"

#include <string>
#include <variant>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/global_features.h"
#include "content/public/browser/overlay_window.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {
namespace {

class CobaltContentBrowserClientTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(CobaltContentBrowserClientTest, ParseAndApplyH5vccSettingsForTesting) {
  auto* instance = GlobalFeatures::GetInstance();
  ASSERT_NE(instance, nullptr);

  ParseAndApplyH5vccSettingsForTesting("Foo=1234;Bar=Baz", instance);

  const auto& settings = instance->GetSettings();
  auto it1 = settings.find("Foo");
  ASSERT_NE(it1, settings.end());
  EXPECT_EQ(std::get<int64_t>(it1->second), 1234);

  auto it2 = settings.find("Bar");
  ASSERT_NE(it2, settings.end());
  EXPECT_EQ(std::get<std::string>(it2->second), "Baz");
}

TEST_F(CobaltContentBrowserClientTest,
       CreateWindowForVideoPictureInPicturePlatformBehavior) {
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  std::unique_ptr<content::VideoOverlayWindow> window =
      client.CreateWindowForVideoPictureInPicture(/*controller=*/nullptr);
// TODO: b/532158001 - Support PiP on Linux.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE(window, nullptr);
#else
  EXPECT_EQ(window, nullptr);
#endif
}

TEST_F(CobaltContentBrowserClientTest, ComputeDefaultHttpCacheSize) {
  // 1. Nominal Starboard budget (24 MiB -> 12 MiB HTTP cache):
  EXPECT_EQ(
      CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(24 * 1024 * 1024),
      12u * 1024 * 1024);

  // 2. Current platform constant equals budget minus the 12 MiB reserve:
  EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(
                kSbMaxSystemPathCacheDirectorySize),
            kSbMaxSystemPathCacheDirectorySize - 12u * 1024 * 1024);

  // 3. Zero / unconfigured budget:
  EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(0), 0u);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(CobaltContentBrowserClientTest, ShaderDiskCacheDirectoriesUseDirCache) {
  base::ScopedTempDir temp_cache_dir;
  ASSERT_TRUE(temp_cache_dir.CreateUniqueTempDir());
  base::ScopedPathOverride cache_override(base::DIR_CACHE,
                                          temp_cache_dir.GetPath());

  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  EXPECT_EQ(client.GetShaderDiskCacheDirectory(),
            temp_cache_dir.GetPath().Append(FILE_PATH_LITERAL("ShaderCache")));
  EXPECT_EQ(
      client.GetGrShaderDiskCacheDirectory(),
      temp_cache_dir.GetPath().Append(FILE_PATH_LITERAL("GrShaderCache")));
}
#endif

TEST_F(CobaltContentBrowserClientTest, DeleteOrphanedUserDataCacheDirectories) {
  base::ScopedTempDir temp_user_data_dir;
  base::ScopedTempDir temp_cache_dir;
  ASSERT_TRUE(temp_user_data_dir.CreateUniqueTempDir());
  ASSERT_TRUE(temp_cache_dir.CreateUniqueTempDir());

  const base::FilePath& user_data = temp_user_data_dir.GetPath();
  const base::FilePath& cache_dir = temp_cache_dir.GetPath();

  // Create orphaned cache directories and a persistent Local Storage directory
  // in user_data, plus live cache directories in cache_dir.
  for (const char* name :
       {"Cache", "Code Cache", "ShaderCache", "GrShaderCache"}) {
    ASSERT_TRUE(base::CreateDirectory(user_data.AppendASCII(name)));
    ASSERT_TRUE(base::WriteFile(
        user_data.AppendASCII(name).AppendASCII("entry"), "stale"));
    ASSERT_TRUE(base::CreateDirectory(cache_dir.AppendASCII(name)));
    ASSERT_TRUE(base::WriteFile(
        cache_dir.AppendASCII(name).AppendASCII("entry"), "live"));
  }
  ASSERT_TRUE(base::CreateDirectory(user_data.AppendASCII("Local Storage")));

  // Safety check: when user_data == cache_dir, nothing is deleted.
  DeleteOrphanedUserDataCacheDirectories(user_data, user_data);
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("Cache")));

  // Safety check: when cache_dir is nested inside user_data (e.g.,
  // user_data/Cache), user_data/Cache is preserved while other orphaned
  // directories (e.g., Code Cache) are cleaned up.
  DeleteOrphanedUserDataCacheDirectories(user_data,
                                         user_data.AppendASCII("Cache"));
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("Cache")));
  EXPECT_FALSE(base::PathExists(user_data.AppendASCII("Code Cache")));

  // Normal execution: all remaining orphaned cache directories in user_data are
  // renamed to old_<name>_000 and deleted via
  // disk_cache::CleanupDirectorySync(), while cache_dir and persistent
  // user_data subdirectories remain intact.
  DeleteOrphanedUserDataCacheDirectories(user_data, cache_dir);
  for (const char* name :
       {"Cache", "Code Cache", "ShaderCache", "GrShaderCache"}) {
    EXPECT_FALSE(base::PathExists(user_data.AppendASCII(name)));
    EXPECT_TRUE(base::PathExists(cache_dir.AppendASCII(name)));
  }

  // Run queued BEST_EFFORT tasks posted by disk_cache::CleanupDirectorySync()
  // and verify old_<name>_000 temporary directories are deleted.
  task_environment_.RunUntilIdle();
  for (const char* name :
       {"Cache", "Code Cache", "ShaderCache", "GrShaderCache"}) {
    EXPECT_FALSE(base::PathExists(
        user_data.AppendASCII(std::string("old_") + name + "_000")));
  }
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("Local Storage")));
}

}  // namespace
}  // namespace cobalt
