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

#include <optional>
#include <string>
#include <variant>

#include "base/base_paths.h"
#include "base/features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/shell/common/shell_paths.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/public/browser/overlay_window.h"
#include "gpu/config/gpu_finch_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "sql/database.h"
#include "starboard/configuration_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

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

#if BUILDFLAG(IS_ANDROID)
TEST_F(CobaltContentBrowserClientTest, GrShaderDiskCacheDirectory) {
  // The one-argument override creates a temp dir and registers its canonical
  // path, so the expectation below must be read back from PathService (a
  // ScopedTempDir path would differ on Android: /data/user/0 vs /data/data).
  base::ScopedPathOverride cache_override(base::DIR_CACHE);
  const base::FilePath cache_dir =
      base::PathService::CheckedGet(base::DIR_CACHE);
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);

  // Feature off (the default): no directory, so no GPU disk cache can be
  // created even without --disable-gpu-shader-disk-cache.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(::features::kCobaltGpuShaderDiskCache);
    EXPECT_TRUE(client.GetGrShaderDiskCacheDirectory().empty());
  }

  // Feature on: under base::DIR_CACHE, never under the user data directory.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(::features::kCobaltGpuShaderDiskCache);
    EXPECT_EQ(client.GetGrShaderDiskCacheDirectory(),
              cache_dir.Append(FILE_PATH_LITERAL("GrShaderCache")));
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

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
  base::ScopedPathOverride user_data_override(content::SHELL_DIR_USER_DATA);
  base::ScopedPathOverride cache_override(base::DIR_CACHE);
  const base::FilePath user_data_dir =
      base::PathService::CheckedGet(content::SHELL_DIR_USER_DATA);
  const base::FilePath cache_dir =
      base::PathService::CheckedGet(base::DIR_CACHE);

  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  // By default (kCobaltStorageCleanup disabled), the shader disk cache
  // directory remains in SHELL_DIR_USER_DATA. GetGrShaderDiskCacheDirectory()
  // is gated on kCobaltGpuShaderDiskCache instead; see the
  // GrShaderDiskCacheDirectory test.
  EXPECT_EQ(client.GetShaderDiskCacheDirectory(),
            user_data_dir.Append(FILE_PATH_LITERAL("ShaderCache")));

  // When kCobaltStorageCleanup is enabled, the shader disk cache directory uses
  // base::DIR_CACHE.
  base::test::ScopedFeatureList feature_list(
      base::features::kCobaltStorageCleanup);
  EXPECT_EQ(client.GetShaderDiskCacheDirectory(),
            cache_dir.Append(FILE_PATH_LITERAL("ShaderCache")));
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

TEST_F(CobaltContentBrowserClientTest,
       AllowCompressionDictionaryTransportIsDisabled) {
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  EXPECT_TRUE(client.AllowCompressionDictionaryTransport(/*context=*/nullptr));

  base::test::ScopedFeatureList feature_list(
      base::features::kCobaltStorageCleanup);
  EXPECT_FALSE(client.AllowCompressionDictionaryTransport(/*context=*/nullptr));
}

TEST_F(CobaltContentBrowserClientTest, DeleteUnusedChromiumStoragePaths) {
  // Safe no-op on empty or non-existent directories.
  DeleteUnusedChromiumStoragePaths(base::FilePath());

  base::ScopedTempDir temp_user_data_dir;
  ASSERT_TRUE(temp_user_data_dir.CreateUniqueTempDir());
  const base::FilePath& user_data = temp_user_data_dir.GetPath();
  DeleteUnusedChromiumStoragePaths(user_data.AppendASCII("non_existent"));

  const base::FilePath trust_tokens_db = user_data.Append(kTrustTokenFilename);
  const base::FilePath shared_storage_db =
      user_data.Append(::storage::kSharedStoragePath);
  const base::FilePath unused_files[] = {
      user_data.Append(kSCTAuditingPendingReportsFileName),
      trust_tokens_db,
      sql::Database::JournalPath(trust_tokens_db),
      sql::Database::SharedMemoryFilePath(trust_tokens_db),
      sql::Database::WriteAheadLogPath(trust_tokens_db),
      shared_storage_db,
      sql::Database::JournalPath(shared_storage_db),
      sql::Database::SharedMemoryFilePath(shared_storage_db),
      sql::Database::WriteAheadLogPath(shared_storage_db),
  };

  // Populate legacy unused files and directories alongside active persistent
  // storage files and directories.
  for (const base::FilePath& file_path : unused_files) {
    ASSERT_TRUE(base::WriteFile(file_path, "legacy"));
  }
  for (const char* dir_name : {
           "Shared Dictionary",
           ::storage::kSessionStorageDirectory,
           "VideoDecodeStats",
           "shared_proto_db",
       }) {
    base::FilePath dir_path = user_data.AppendASCII(dir_name);
    ASSERT_TRUE(base::CreateDirectory(dir_path));
    ASSERT_TRUE(base::WriteFile(dir_path.AppendASCII("LOCK"), "legacy"));
  }

  constexpr auto kLocalStoragePath = FILE_PATH_LITERAL("Local Storage");

  ASSERT_TRUE(base::WriteFile(user_data.AppendASCII("Cookies"), "active"));
  ASSERT_TRUE(
      base::WriteFile(user_data.AppendASCII("TransportSecurity"), "active"));
  ASSERT_TRUE(base::CreateDirectory(user_data.Append(kLocalStoragePath)));
  ASSERT_TRUE(base::WriteFile(
      user_data.Append(kLocalStoragePath).AppendASCII("CURRENT"), "active"));
  ASSERT_TRUE(
      base::CreateDirectory(user_data.Append(::storage::kWebStorageDirectory)));

  DeleteUnusedChromiumStoragePaths(user_data);

  for (const base::FilePath& file_path : unused_files) {
    EXPECT_FALSE(base::PathExists(file_path))
        << "Expected unused file to be deleted: " << file_path;
  }
  for (const char* dir_name : {
           "Shared Dictionary",
           ::storage::kSessionStorageDirectory,
           "VideoDecodeStats",
           "shared_proto_db",
       }) {
    EXPECT_FALSE(base::PathExists(user_data.AppendASCII(dir_name)))
        << "Expected unused directory to be deleted: " << dir_name;
  }

  // Active persistent files and directories must remain untouched.
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("Cookies")));
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("TransportSecurity")));
  EXPECT_TRUE(base::PathExists(
      user_data.Append(kLocalStoragePath).AppendASCII("CURRENT")));
  EXPECT_TRUE(
      base::DirectoryExists(user_data.Append(::storage::kWebStorageDirectory)));

  // Subsequent runs when unused paths are already absent must be a safe no-op.
  DeleteUnusedChromiumStoragePaths(user_data);
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("Cookies")));
}

class CobaltContentBrowserClientHeaderTest
    : public CobaltContentBrowserClientTest {
 protected:
  void SetUp() override {
    browser::CobaltHeaderValueProvider::GetInstance()
        ->ClearHeaderValuesForTesting();
  }

  void TearDown() override {
    browser::CobaltHeaderValueProvider::GetInstance()
        ->ClearHeaderValuesForTesting();
  }

  network::mojom::NetworkContextParamsPtr ConfigureNetworkContextParams() {
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::PopulateCobaltExtraRequestHeaders(params.get());
    return params;
  }

  // Returns the header client that WillCreateURLLoaderFactory() installs, if
  // any.
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
  GetInstalledHeaderClient() {
    network::URLLoaderFactoryBuilder factory_builder;
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        header_client;
    client_.WillCreateURLLoaderFactory(
        /*browser_context=*/nullptr, /*frame=*/nullptr,
        /*render_process_id=*/0,
        CobaltContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
        url::Origin(), net::IsolationInfo(), /*navigation_id=*/std::nullopt,
        ukm::kInvalidSourceIdObj, factory_builder, &header_client,
        /*bypass_redirect_checks=*/nullptr, /*disable_secure_dns=*/nullptr,
        /*factory_override=*/nullptr,
        /*navigation_response_task_runner=*/nullptr);
    return header_client;
  }

  CobaltContentBrowserClient client_{/*startup_timestamp=*/std::nullopt,
                                     /*deep_link=*/""};
};

TEST_F(CobaltContentBrowserClientHeaderTest,
       NetworkContextParamsHaveNoHeadersByDefault) {
  browser::CobaltHeaderValueProvider::GetInstance()->SetHeaderValue(
      browser::kAndroidOSExperienceHeaderName, "Amati");

  EXPECT_TRUE(
      ConfigureNetworkContextParams()->cobalt_extra_request_headers.empty());
}

TEST_F(CobaltContentBrowserClientHeaderTest,
       NetworkContextParamsHaveHeadersWhenSkippingHeaderClient) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltSkipTrustedHeaderClient);
  auto* provider = browser::CobaltHeaderValueProvider::GetInstance();
  provider->SetHeaderValue(browser::kAndroidOSExperienceHeaderName, "Amati");
  provider->SetHeaderValue(browser::kBuildFingerprintHeaderName,
                           "test/fingerprint");

  EXPECT_THAT(
      ConfigureNetworkContextParams()->cobalt_extra_request_headers,
      testing::UnorderedElementsAre(
          testing::Pair(browser::kAndroidOSExperienceHeaderName, "Amati"),
          testing::Pair(browser::kBuildFingerprintHeaderName,
                        "test/fingerprint")));
}

TEST_F(CobaltContentBrowserClientHeaderTest, InstallsHeaderClientByDefault) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client = GetInstalledHeaderClient();
  EXPECT_TRUE(header_client.is_valid());

  // Let the self-owned header client see the disconnect and delete itself.
  header_client.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(CobaltContentBrowserClientHeaderTest,
       DoesNotInstallHeaderClientWhenSkippingHeaderClient) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltSkipTrustedHeaderClient);

  EXPECT_FALSE(GetInstalledHeaderClient().is_valid());
}

}  // namespace
}  // namespace cobalt
