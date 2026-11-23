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

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/experiments/experiment_config_manager.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/h5vcc_platform_service/h5vcc_platform_service_manager_impl.h"
#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"
#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "cobalt/shell/common/shell_paths.h"
#include "components/prefs/pref_service.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/site_instance.h"
#include "content/test/test_web_contents.h"
#include "gpu/config/gpu_finch_features.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"
#include "sql/database.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/platform_service.h"
#include "storage/browser/quota/quota_device_info_helper.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom.h"
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

#if BUILDFLAG(IS_STARBOARD)
TEST_F(CobaltContentBrowserClientTest,
       GrShaderDiskCacheDirectoryIsAlwaysDirCache) {
  base::ScopedPathOverride user_data_override(content::SHELL_DIR_USER_DATA);
  base::ScopedPathOverride cache_override(base::DIR_CACHE);
  const base::FilePath user_data_dir =
      base::PathService::CheckedGet(content::SHELL_DIR_USER_DATA);
  const base::FilePath cache_dir =
      base::PathService::CheckedGet(base::DIR_CACHE);
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);

  // DIR_CACHE regardless of kCobaltStorageCleanup; never under user data.
  for (const bool storage_cleanup_enabled : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(base::features::kCobaltStorageCleanup,
                                      storage_cleanup_enabled);
    const base::FilePath dir = client.GetGrShaderDiskCacheDirectory();
    EXPECT_EQ(dir, cache_dir.Append(FILE_PATH_LITERAL("GrShaderCache")))
        << "kCobaltStorageCleanup=" << storage_cleanup_enabled;
    EXPECT_FALSE(user_data_dir.IsParent(dir));
  }
}
#endif  // BUILDFLAG(IS_STARBOARD)

TEST_F(CobaltContentBrowserClientTest, ComputeDefaultHttpCacheSize) {
  // 1. Nominal Starboard budget (24 MiB -> 11 MiB HTTP cache):
  EXPECT_EQ(
      CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(24 * 1024 * 1024),
      11u * 1024 * 1024);

  // 2. Current platform constant equals budget minus the 13 MiB reserve:
  EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(
                kSbMaxSystemPathCacheDirectorySize),
            kSbMaxSystemPathCacheDirectorySize - 13u * 1024 * 1024);

  // 3. Zero / unconfigured budget remains 0 regardless of feature flags:
  EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(0), 0u);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltCodeCacheOptimization);
    EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(0), 0u);
  }

  // 4. When kCobaltCodeCacheOptimization raises JS code cache from 5 MiB to
  // 10 MiB, Starboard's dynamic non-HTTP reserve grows from 13 MiB to 18 MiB
  // (leaving 6 MiB on a 24 MiB budget so DIR_CACHE never exceeds 24 MiB):
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltCodeCacheOptimization);
    EXPECT_EQ(CobaltContentBrowserClient::ComputeDefaultHttpCacheSize(
                  24 * 1024 * 1024),
              6u * 1024 * 1024);
  }
}

TEST_F(CobaltContentBrowserClientTest, GetGeneratedCodeCacheSettings) {
  base::ScopedPathOverride cache_override(base::DIR_CACHE);
  const base::FilePath cache_dir =
      base::PathService::CheckedGet(base::DIR_CACHE);
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);

  // 1. Feature disabled (default): 5242880 bytes (5 MiB).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kCobaltCodeCacheOptimization);
    content::GeneratedCodeCacheSettings settings =
        client.GetGeneratedCodeCacheSettings(/*context=*/nullptr);
    EXPECT_TRUE(settings.enabled());
    EXPECT_EQ(settings.size_in_bytes(), 5242880u);
    EXPECT_EQ(settings.path(), cache_dir);
  }

  // 2. Feature enabled (default parameter): 10485760 bytes (10 MiB).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltCodeCacheOptimization);
    content::GeneratedCodeCacheSettings settings =
        client.GetGeneratedCodeCacheSettings(/*context=*/nullptr);
    EXPECT_TRUE(settings.enabled());
    EXPECT_EQ(settings.size_in_bytes(), 10485760u);
    EXPECT_EQ(settings.path(), cache_dir);
  }

  // 3. Custom byte parameter & [1048576, 1073741824] clamping.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltCodeCacheOptimization,
        {{features::kCobaltGeneratedCodeCacheSizeBytes.name, "8388608"}});
    content::GeneratedCodeCacheSettings settings =
        client.GetGeneratedCodeCacheSettings(/*context=*/nullptr);
    EXPECT_EQ(settings.size_in_bytes(), 8388608u);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltCodeCacheOptimization,
        {{features::kCobaltGeneratedCodeCacheSizeBytes.name, "0"}});
    content::GeneratedCodeCacheSettings settings =
        client.GetGeneratedCodeCacheSettings(/*context=*/nullptr);
    EXPECT_EQ(settings.size_in_bytes(), 1048576u);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltCodeCacheOptimization,
        {{features::kCobaltGeneratedCodeCacheSizeBytes.name, "2147483647"}});
    content::GeneratedCodeCacheSettings settings =
        client.GetGeneratedCodeCacheSettings(/*context=*/nullptr);
    EXPECT_EQ(settings.size_in_bytes(), 1073741824u);
  }
}

class CobaltWebPrefsTest : public content::ShellTestBase {};

TEST_F(CobaltWebPrefsTest, OverrideWebPreferencesV8CacheOptions) {
  content::WebContents::CreateParams create_params(browser_context());
  std::unique_ptr<content::WebContents> web_contents(
      content::TestWebContents::Create(create_params));
  ASSERT_NE(web_contents->GetSiteInstance(), nullptr);

  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);

  // 1. Disabled by default: keeps upstream V8CacheOptions::kDefault.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kCobaltCodeCacheOptimization);
    blink::web_pref::WebPreferences prefs;
    client.OverrideWebPreferences(web_contents.get(),
                                  *web_contents->GetSiteInstance(), &prefs);
    EXPECT_EQ(prefs.v8_cache_options, blink::mojom::V8CacheOptions::kDefault);
  }

  // 2. Enabled (default bypass_heat_check = true): sets kCodeWithoutHeatCheck.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltCodeCacheOptimization);
    blink::web_pref::WebPreferences prefs;
    client.OverrideWebPreferences(web_contents.get(),
                                  *web_contents->GetSiteInstance(), &prefs);
    EXPECT_EQ(prefs.v8_cache_options,
              blink::mojom::V8CacheOptions::kCodeWithoutHeatCheck);
  }

  // 3. Enabled with CobaltCodeCacheOptimization_bypass_heat_check = false:
  // keeps kDefault.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltCodeCacheOptimization,
        {{features::kCobaltV8CodeCacheBypassHeatCheck.name, "false"}});
    blink::web_pref::WebPreferences prefs;
    client.OverrideWebPreferences(web_contents.get(),
                                  *web_contents->GetSiteInstance(), &prefs);
    EXPECT_EQ(prefs.v8_cache_options, blink::mojom::V8CacheOptions::kDefault);
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(CobaltContentBrowserClientTest,
       ConfigureHttpCacheMaxSizeStarboardQuota) {
  constexpr uint32_t kBudget24MiB = 24u * 1024 * 1024;
  constexpr int kExpectedDefault11MiB = 11 * 1024 * 1024;  // 11534336
  constexpr int kExpectedCombined6MiB = 6 * 1024 * 1024;   // 6291456

  // 1. Budgeted Starboard platform (24 MiB -> 11 MiB default):
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, kExpectedDefault11MiB);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, kExpectedDefault11MiB);
  }
  // Combined kCobaltCodeCacheOptimization (10 MiB JS code cache -> 18 MiB
  // non-HTTP reserve) + kCobaltHttpCacheQuota (12 MiB cap) caps HTTP cache at
  // min(6 MiB, 12 MiB) = 6 MiB so DIR_CACHE stays within 24 MiB:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kCobaltCodeCacheOptimization,
                                   features::kCobaltHttpCacheQuota},
                                  {});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, kExpectedCombined6MiB);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltHttpCacheQuota,
        {{"cobalt_http_cache_size_bytes", "8388608"}});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, 8388608);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltHttpCacheQuota,
        {{"cobalt_http_cache_size_bytes", "0"}});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, 1048576);
  }

  // 2. Unbudgeted / auto-sized Starboard platform (budget == 0):
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, /*total_dir_budget_bytes=*/0, params.get());
    EXPECT_EQ(params->http_cache_max_size, 0);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, /*total_dir_budget_bytes=*/0, params.get());
    EXPECT_EQ(params->http_cache_max_size, 12582912);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltHttpCacheQuota,
        {{"cobalt_http_cache_size_bytes", "2147483647"}});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, /*total_dir_budget_bytes=*/0, params.get());
    EXPECT_EQ(params->http_cache_max_size, 1073741824);
  }

  // 3. In-memory profile and explicit --max-http-cache-size CLI override:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/true, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, 0);
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kMaxHttpCacheSize, "16777216");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kBudget24MiB, params.get());
    EXPECT_EQ(params->http_cache_max_size, 16777216);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
TEST_F(CobaltContentBrowserClientTest, ConfigureHttpCacheMaxSizeAndroidQuota) {
  // 1. Disabled by default: Android baseline is 26214400 bytes (25 MiB).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 26214400);
  }

  // 2. Enabling kCobaltHttpCacheQuota configures 12582912 bytes (12 MiB).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 12582912);
  }

  // 3. Custom cobalt_http_cache_size_bytes & [1048576, 1073741824] clamping.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltHttpCacheQuota,
        {{"cobalt_http_cache_size_bytes", "10485760"}});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 10485760);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltHttpCacheQuota,
        {{"cobalt_http_cache_size_bytes", "0"}});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 1048576);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltHttpCacheQuota,
        {{"cobalt_http_cache_size_bytes", "2147483647"}});
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 1073741824);
  }

  // 4. In-memory profile and explicit --max-http-cache-size CLI override.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/true, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 0);
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kMaxHttpCacheSize, "16777216");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCobaltHttpCacheQuota);
    auto params = network::mojom::NetworkContextParams::New();
    CobaltContentBrowserClient::ConfigureHttpCacheMaxSize(
        /*in_memory=*/false, kSbMaxSystemPathCacheDirectorySize, params.get());
    EXPECT_EQ(params->http_cache_max_size, 16777216);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

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

  ASSERT_TRUE(base::WriteFile(user_data.AppendASCII("Cookies"), "active"));
  ASSERT_TRUE(
      base::WriteFile(user_data.AppendASCII("TransportSecurity"), "active"));
  ASSERT_TRUE(
      base::CreateDirectory(user_data.Append(::storage::kLocalStoragePath)));
  ASSERT_TRUE(base::WriteFile(
      user_data.Append(::storage::kLocalStoragePath).AppendASCII("CURRENT"),
      "active"));
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
      user_data.Append(::storage::kLocalStoragePath).AppendASCII("CURRENT")));
  EXPECT_TRUE(
      base::DirectoryExists(user_data.Append(::storage::kWebStorageDirectory)));

  // Subsequent runs when unused paths are already absent must be a safe no-op.
  DeleteUnusedChromiumStoragePaths(user_data);
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("Cookies")));
}

class MockQuotaDeviceInfoHelper : public storage::QuotaDeviceInfoHelper {
 public:
  MockQuotaDeviceInfoHelper() = default;
  MOCK_METHOD(int64_t,
              AmountOfTotalDiskSpace,
              (const base::FilePath&),
              (const, override));
  MOCK_METHOD(uint64_t, AmountOfPhysicalMemory, (), (const, override));
};

// Per-origin CacheStorage quota is capped at 6 MiB (matching the reserve
// deducted from the HTTP cache budget in ComputeDefaultHttpCacheSize), and
// pool_size is 2x the per-origin quota (12 MiB) so single-origin usage never
// exceeds QuotaTemporaryStorageEvictor's kUsageRatioToStartEviction (0.7).
constexpr int64_t kExpectedCacheStorageQuota =
    CobaltContentBrowserClient::kCacheStorageQuotaBytes;
constexpr int64_t kExpectedCacheStoragePoolSize =
    CobaltContentBrowserClient::kCacheStoragePoolSizeBytes;
constexpr double kUsageRatioToStartEviction = 0.7;

TEST_F(CobaltContentBrowserClientTest, GetCacheQuotaSettings_LargeDiskSpace) {
  MockQuotaDeviceInfoHelper device_info_helper;
  const int64_t kTotalDiskSpace = 500LL * 1024 * 1024 * 1024;  // 500 GB
  EXPECT_CALL(device_info_helper, AmountOfTotalDiskSpace(testing::_))
      .WillRepeatedly(testing::Return(kTotalDiskSpace));

  auto settings =
      CobaltContentBrowserClient::CalculateCacheQuotaSettingsForTesting(
          base::FilePath(FILE_PATH_LITERAL("/dummy/cache")),
          &device_info_helper);

  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(settings->pool_size, kExpectedCacheStoragePoolSize);
  EXPECT_EQ(settings->per_storage_key_quota, kExpectedCacheStorageQuota);
  EXPECT_EQ(settings->session_only_per_storage_key_quota,
            kExpectedCacheStorageQuota);
  EXPECT_LE(
      settings->per_storage_key_quota,
      static_cast<int64_t>(settings->pool_size * kUsageRatioToStartEviction));
  // Must remain available should be min(1GB, 500GB * 0.01 = 5GB) = 1GB.
  EXPECT_EQ(settings->must_remain_available, 1024LL * 1024 * 1024);
  // Disk-pressure eviction is disabled.
  EXPECT_EQ(settings->should_remain_available, 0);
}

TEST_F(CobaltContentBrowserClientTest, GetCacheQuotaSettings_SmallDiskSpace) {
  MockQuotaDeviceInfoHelper device_info_helper;
  const int64_t kTotalDiskSpace = 10LL * 1024 * 1024;  // 10 MB (< 24 MiB)
  EXPECT_CALL(device_info_helper, AmountOfTotalDiskSpace(testing::_))
      .WillRepeatedly(testing::Return(kTotalDiskSpace));

  auto settings =
      CobaltContentBrowserClient::CalculateCacheQuotaSettingsForTesting(
          base::FilePath(FILE_PATH_LITERAL("/dummy/cache")),
          &device_info_helper);

  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(settings->pool_size, kExpectedCacheStoragePoolSize);
  EXPECT_EQ(settings->per_storage_key_quota, kExpectedCacheStorageQuota);
  EXPECT_EQ(settings->session_only_per_storage_key_quota,
            kExpectedCacheStorageQuota);
  EXPECT_LE(
      settings->per_storage_key_quota,
      static_cast<int64_t>(settings->pool_size * kUsageRatioToStartEviction));
  // Must remain available should be min(1GB, 10MB * 0.01) = 104857 bytes
  // (~100 KB).
  EXPECT_EQ(settings->must_remain_available,
            static_cast<int64_t>(kTotalDiskSpace * 0.01));
  // Disk-pressure eviction is disabled.
  EXPECT_EQ(settings->should_remain_available, 0);
}

TEST_F(CobaltContentBrowserClientTest, GetCacheQuotaSettings_TinyDiskSpace) {
  MockQuotaDeviceInfoHelper device_info_helper;
  const int64_t kTotalDiskSpace = 4LL * 1024 * 1024;  // 4 MiB (< 6 MiB)
  EXPECT_CALL(device_info_helper, AmountOfTotalDiskSpace(testing::_))
      .WillRepeatedly(testing::Return(kTotalDiskSpace));

  auto settings =
      CobaltContentBrowserClient::CalculateCacheQuotaSettingsForTesting(
          base::FilePath(FILE_PATH_LITERAL("/dummy/cache")),
          &device_info_helper);

  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(settings->pool_size, kTotalDiskSpace * 2);
  EXPECT_EQ(settings->per_storage_key_quota, kTotalDiskSpace);
  EXPECT_EQ(settings->session_only_per_storage_key_quota, kTotalDiskSpace);
  EXPECT_LE(
      settings->per_storage_key_quota,
      static_cast<int64_t>(settings->pool_size * kUsageRatioToStartEviction));
}

TEST_F(CobaltContentBrowserClientTest, GetCacheQuotaSettings_DiskSpaceError) {
  MockQuotaDeviceInfoHelper device_info_helper;
  EXPECT_CALL(device_info_helper, AmountOfTotalDiskSpace(testing::_))
      .WillRepeatedly(testing::Return(-1));

  auto settings =
      CobaltContentBrowserClient::CalculateCacheQuotaSettingsForTesting(
          base::FilePath(FILE_PATH_LITERAL("/dummy/cache")),
          &device_info_helper);

  EXPECT_FALSE(settings.has_value());
}

TEST_F(CobaltContentBrowserClientTest, GetCacheQuotaSettings_AsyncDispatch) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  base::test::TestFuture<std::optional<storage::QuotaSettings>> future;
  client.GetCacheQuotaSettings(/*browser_context=*/nullptr, temp_dir.GetPath(),
                               future.GetCallback());
  task_environment_.RunUntilIdle();
  auto settings = future.Take();
  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(settings->pool_size, kExpectedCacheStoragePoolSize);
  EXPECT_EQ(settings->per_storage_key_quota, kExpectedCacheStorageQuota);
  EXPECT_LE(
      settings->per_storage_key_quota,
      static_cast<int64_t>(settings->pool_size * kUsageRatioToStartEviction));
}

// Verifies that when Cobalt initializes its NetworkContext for HTTPS web app
// navigation and media playback, ConfigureSSLConfig() populates
// disabled_cipher_suites so the TLS ClientHello never advertises obsolete
// static-RSA or CBC-mode cipher suites while keeping modern ECDHE AEAD suites
// enabled.
TEST_F(CobaltContentBrowserClientTest,
       ConfigureNetworkContextParamsDisablesObsoleteCipherSuites) {
  auto params = network::mojom::NetworkContextParams::New();
  CobaltContentBrowserClient::ConfigureSSLConfig(params.get());

  ASSERT_TRUE(params->initial_ssl_config);
  const auto& disabled = params->initial_ssl_config->disabled_cipher_suites;
  EXPECT_FALSE(disabled.empty());
  for (uint16_t cipher_suite : disabled) {
    EXPECT_FALSE(net::IsTLSCipherSuiteAllowedByHTTP2(cipher_suite));
  }

  // Static RSA (kRSA) and non-AEAD CBC-mode cipher suites must be disabled.
  EXPECT_THAT(disabled, testing::IsSupersetOf(
                            {0x002f, 0x0035, 0x009c, 0x009d, 0xc013, 0xc014}));

  // Modern ECDHE AEAD cipher suites must not be disabled.
  EXPECT_THAT(disabled, testing::Not(testing::Contains(0xc02b)));
  EXPECT_THAT(disabled, testing::Not(testing::Contains(0xc02f)));
  EXPECT_THAT(disabled, testing::Not(testing::Contains(0xcca8)));
  EXPECT_THAT(disabled, testing::Not(testing::Contains(0xcca9)));
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
       NetworkContextParamsHaveHeadersByDefault) {
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

TEST_F(CobaltContentBrowserClientHeaderTest,
       NetworkContextParamsHaveNoHeadersWhenNotSkippingHeaderClient) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kCobaltSkipTrustedHeaderClient);
  browser::CobaltHeaderValueProvider::GetInstance()->SetHeaderValue(
      browser::kAndroidOSExperienceHeaderName, "Amati");

  EXPECT_TRUE(
      ConfigureNetworkContextParams()->cobalt_extra_request_headers.empty());
}

TEST_F(CobaltContentBrowserClientHeaderTest,
       DoesNotInstallHeaderClientByDefault) {
  EXPECT_FALSE(GetInstalledHeaderClient().is_valid());
}

TEST_F(CobaltContentBrowserClientHeaderTest,
       InstallsHeaderClientWhenNotSkippingHeaderClient) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kCobaltSkipTrustedHeaderClient);

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client = GetInstalledHeaderClient();
  EXPECT_TRUE(header_client.is_valid());

  // Let the self-owned header client see the disconnect and delete itself.
  header_client.reset();
  task_environment_.RunUntilIdle();
}

// A helper observer implementation for PlatformServiceObserver that captures
// received data and signals a closure when data is received.
//
// Lifetime: Owned by the test method, typically matching the duration of the
// test.
// Threading: Affine to the thread where it is constructed (the UI thread in
// these tests).
class TestPlatformServiceObserver
    : public h5vcc_platform_service::mojom::PlatformServiceObserver {
 public:
  explicit TestPlatformServiceObserver(base::OnceClosure on_received)
      : on_received_(std::move(on_received)) {}

  void OnDataReceived(base::span<const uint8_t> data) override {
    received_data_.assign(data.begin(), data.end());
    if (on_received_) {
      std::move(on_received_).Run();
    }
  }

  const std::vector<uint8_t>& received_data() const { return received_data_; }

 private:
  std::vector<uint8_t> received_data_;
  base::OnceClosure on_received_;
};

// Test fixture for testing H5vccPlatformService binding and execution,
// verifying both UI-thread and off-UI-thread configurations.
//
// Lifetime: Created and destroyed by the gtest framework for each test case.
// Threading: Affine to the main test thread (UI thread).
class CobaltPlatformServiceBinderTest : public content::ShellTestBase {
 protected:
  struct FakeServiceState {
    void* context = nullptr;
    ReceiveMessageCallback receive_callback = nullptr;
    bool has_on_ui_thread = false;
    bool has_on_service_thread = false;
    bool open_on_ui_thread = false;
    bool open_on_service_thread = false;
    bool send_on_ui_thread = false;
    bool send_on_service_thread = false;
    bool close_on_ui_thread = false;
    bool close_on_service_thread = false;
    bool wait_for_ui_thread_in_send = false;
    bool ui_thread_task_ran_during_send = false;
    bool fail_open = false;
    bool fail_send_invalid_state = false;
    std::vector<uint8_t> sync_message_on_open;
    base::OnceClosure on_close;
  };

  static FakeServiceState*& CurrentState() {
    static FakeServiceState* s_state = nullptr;
    return s_state;
  }

  static bool FakeHas(const char* name) {
    auto* state = CurrentState();
    state->has_on_ui_thread =
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
    state->has_on_service_thread =
        h5vcc_platform_service::GetPlatformServiceTaskRunner()
            ->BelongsToCurrentThread();
    return std::strcmp(name, "test.Service") == 0;
  }

  static CobaltExtensionPlatformService FakeOpen(
      void* context,
      const char* /*name*/,
      ReceiveMessageCallback receive_callback) {
    auto* state = CurrentState();
    state->context = context;
    state->receive_callback = receive_callback;
    state->open_on_ui_thread =
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
    state->open_on_service_thread =
        h5vcc_platform_service::GetPlatformServiceTaskRunner()
            ->BelongsToCurrentThread();
    if (state->fail_open) {
      return kCobaltExtensionPlatformServiceInvalid;
    }
    if (!state->sync_message_on_open.empty() && receive_callback) {
      receive_callback(context, state->sync_message_on_open.data(),
                       state->sync_message_on_open.size());
    }
    return reinterpret_cast<CobaltExtensionPlatformService>(state);
  }

  static void FakeClose(CobaltExtensionPlatformService /*service*/) {
    auto* state = CurrentState();
    state->close_on_ui_thread =
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
    state->close_on_service_thread =
        h5vcc_platform_service::GetPlatformServiceTaskRunner()
            ->BelongsToCurrentThread();
    if (state->on_close) {
      std::move(state->on_close).Run();
    }
  }

  static void* FakeSend(CobaltExtensionPlatformService /*service*/,
                        const void* data,
                        uint64_t length,
                        uint64_t* output_length,
                        bool* invalid_state) {
    auto* state = CurrentState();
    state->send_on_ui_thread =
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
    state->send_on_service_thread =
        h5vcc_platform_service::GetPlatformServiceTaskRunner()
            ->BelongsToCurrentThread();
    if (state->fail_send_invalid_state) {
      *invalid_state = true;
      const char kErr[] = "custom invalid state";
      *output_length = sizeof(kErr) - 1;
      void* out = malloc(*output_length);
      std::memcpy(out, kErr, *output_length);
      return out;
    }
    if (state->wait_for_ui_thread_in_send && !state->send_on_ui_thread) {
      base::WaitableEvent ui_event;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindLambdaForTesting([&] {
            state->ui_thread_task_ran_during_send = true;
            ui_event.Signal();
          }));
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
      ui_event.Wait();
    }
    if (state->receive_callback && length > 0) {
      state->receive_callback(state->context, data, length);
    }
    *invalid_state = false;
    *output_length = length;
    if (length == 0) {
      return nullptr;
    }
    void* out = malloc(length);
    std::memcpy(out, data, length);
    return out;
  }

  void SetUp() override {
    content::ShellTestBase::SetUp();
    CurrentState() = &state_;
    h5vcc_platform_service::PlatformServiceImpl::
        SetPlatformServiceApiForTesting(&kFakeApi);
    content::WebContents::CreateParams create_params(browser_context());
    web_contents_.reset(content::TestWebContents::Create(create_params));
  }

  void TearDown() override {
    web_contents_.reset();
    base::RunLoop flush_loop;
    h5vcc_platform_service::GetPlatformServiceTaskRunner()->PostTask(
        FROM_HERE, flush_loop.QuitClosure());
    flush_loop.Run();
    h5vcc_platform_service::PlatformServiceImpl::
        SetPlatformServiceApiForTesting(nullptr);
    h5vcc_platform_service::ResetPlatformServiceTaskRunnerForTesting();
    CurrentState() = nullptr;
    content::ShellTestBase::TearDown();
  }

  mojo::Remote<h5vcc_platform_service::mojom::H5vccPlatformServiceManager>
  BindManager() {
    content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
    mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
    PopulateCobaltFrameBinders(std::nullopt, rfh, &binder_map);

    mojo::Remote<h5vcc_platform_service::mojom::H5vccPlatformServiceManager>
        manager;
    mojo::GenericPendingReceiver generic_receiver(
        manager.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(binder_map.TryBind(rfh, &generic_receiver));
    return manager;
  }

  void ExercisePlatformService(bool wait_for_ui_thread_in_send) {
    base::HistogramTester histogram_tester;
    auto manager = BindManager();

    base::RunLoop has_loop;
    bool exists = false;
    manager->Has("test.Service", base::BindLambdaForTesting([&](bool result) {
                   exists = result;
                   has_loop.Quit();
                 }));
    has_loop.Run();
    EXPECT_TRUE(exists);
    histogram_tester.ExpectTotalCount("Cobalt.PlatformService.Has.DurationUs",
                                      1);

    base::RunLoop receive_loop;
    TestPlatformServiceObserver observer_impl(receive_loop.QuitClosure());
    mojo::Receiver<h5vcc_platform_service::mojom::PlatformServiceObserver>
        observer_receiver(&observer_impl);
    mojo::Remote<h5vcc_platform_service::mojom::PlatformService> service;

    base::RunLoop open_loop;
    manager->Open("test.Service", observer_receiver.BindNewPipeAndPassRemote(),
                  service.BindNewPipeAndPassReceiver(),
                  open_loop.QuitClosure());
    open_loop.Run();
    histogram_tester.ExpectUniqueSample("Cobalt.PlatformService.Open.Success",
                                        true, 1);
    histogram_tester.ExpectTotalCount("Cobalt.PlatformService.Open.DurationUs",
                                      1);

    state_.wait_for_ui_thread_in_send = wait_for_ui_thread_in_send;
    const std::vector<uint8_t> payload = {1, 2, 3};
    base::RunLoop send_loop;
    service->Send(payload, base::BindLambdaForTesting(
                               [&](std::optional<mojo_base::BigBuffer> response,
                                   const std::optional<std::string>& error) {
                                 EXPECT_TRUE(response.has_value());
                                 EXPECT_FALSE(error.has_value());
                                 send_loop.Quit();
                               }));
    send_loop.Run();
    receive_loop.Run();
    EXPECT_EQ(observer_impl.received_data(), payload);
    histogram_tester.ExpectUniqueSample(
        "Cobalt.PlatformService.Send.Status",
        h5vcc_platform_service::PlatformServiceSendStatus::kSuccess, 1);
    histogram_tester.ExpectTotalCount("Cobalt.PlatformService.Send.DurationUs",
                                      1);

    base::RunLoop close_loop;
    state_.on_close = close_loop.QuitClosure();
    service.reset();
    close_loop.Run();
  }

  static constexpr CobaltExtensionPlatformServiceApi kFakeApi = {
      kCobaltExtensionPlatformServiceName,
      1,
      &FakeHas,
      &FakeOpen,
      &FakeClose,
      &FakeSend,
  };

  FakeServiceState state_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(CobaltPlatformServiceBinderTest, RunsOnUiThreadByDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kCobaltPlatformServicesOffUiThread);

  ExercisePlatformService(/*wait_for_ui_thread_in_send=*/false);

  EXPECT_TRUE(state_.has_on_ui_thread);
  EXPECT_FALSE(state_.has_on_service_thread);
  EXPECT_TRUE(state_.open_on_ui_thread);
  EXPECT_FALSE(state_.open_on_service_thread);
  EXPECT_TRUE(state_.send_on_ui_thread);
  EXPECT_FALSE(state_.send_on_service_thread);
  EXPECT_TRUE(state_.close_on_ui_thread);
  EXPECT_FALSE(state_.close_on_service_thread);
}

TEST_F(CobaltPlatformServiceBinderTest, RunsOnServiceThreadWhenEnabled) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltPlatformServicesOffUiThread);

  ExercisePlatformService(/*wait_for_ui_thread_in_send=*/true);

  EXPECT_FALSE(state_.has_on_ui_thread);
  EXPECT_TRUE(state_.has_on_service_thread);
  EXPECT_FALSE(state_.open_on_ui_thread);
  EXPECT_TRUE(state_.open_on_service_thread);
  EXPECT_FALSE(state_.send_on_ui_thread);
  EXPECT_TRUE(state_.send_on_service_thread);
  EXPECT_TRUE(state_.ui_thread_task_ran_during_send);
  EXPECT_FALSE(state_.close_on_ui_thread);
  EXPECT_TRUE(state_.close_on_service_thread);
}

TEST_F(CobaltPlatformServiceBinderTest,
       ClosesOpenServiceWhenWebContentsDestroyedOffUiThread) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltPlatformServicesOffUiThread);

  auto manager = BindManager();
  TestPlatformServiceObserver observer_impl{base::DoNothing()};
  mojo::Receiver<h5vcc_platform_service::mojom::PlatformServiceObserver>
      observer_receiver(&observer_impl);
  mojo::Remote<h5vcc_platform_service::mojom::PlatformService> service;

  base::RunLoop open_loop;
  manager->Open("test.Service", observer_receiver.BindNewPipeAndPassRemote(),
                service.BindNewPipeAndPassReceiver(), open_loop.QuitClosure());
  open_loop.Run();
  EXPECT_TRUE(state_.open_on_service_thread);

  base::RunLoop close_loop;
  state_.on_close = close_loop.QuitClosure();
  // Destroy the WebContents while |service| and |manager| remotes remain bound.
  web_contents_.reset();
  close_loop.Run();

  EXPECT_FALSE(state_.close_on_ui_thread);
  EXPECT_TRUE(state_.close_on_service_thread);
}

TEST_F(CobaltPlatformServiceBinderTest,
       DisconnectsPipeWhenOpenStarboardServiceFails) {
  for (bool off_ui_thread : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (off_ui_thread) {
      feature_list.InitAndEnableFeature(
          features::kCobaltPlatformServicesOffUiThread);
    } else {
      feature_list.InitAndDisableFeature(
          features::kCobaltPlatformServicesOffUiThread);
    }

    content::WebContents::CreateParams create_params(browser_context());
    web_contents_.reset(content::TestWebContents::Create(create_params));
    state_ = FakeServiceState{};
    state_.fail_open = true;
    base::HistogramTester histogram_tester;

    auto manager = BindManager();
    TestPlatformServiceObserver observer_impl{base::DoNothing()};
    mojo::Receiver<h5vcc_platform_service::mojom::PlatformServiceObserver>
        observer_receiver(&observer_impl);
    mojo::Remote<h5vcc_platform_service::mojom::PlatformService> service;
    auto service_receiver = service.BindNewPipeAndPassReceiver();

    base::RunLoop disconnect_loop;
    service.set_disconnect_handler(disconnect_loop.QuitClosure());

    base::RunLoop open_loop;
    manager->Open("test.Service", observer_receiver.BindNewPipeAndPassRemote(),
                  std::move(service_receiver), open_loop.QuitClosure());
    open_loop.Run();
    disconnect_loop.Run();

    EXPECT_FALSE(service.is_connected());
    histogram_tester.ExpectUniqueSample("Cobalt.PlatformService.Open.Success",
                                        false, 1);
  }
}

TEST_F(CobaltPlatformServiceBinderTest, RejectsNullRenderFrameHostBinding) {
  for (bool off_ui_thread : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (off_ui_thread) {
      feature_list.InitAndEnableFeature(
          features::kCobaltPlatformServicesOffUiThread);
    } else {
      feature_list.InitAndDisableFeature(
          features::kCobaltPlatformServicesOffUiThread);
    }

    mojo::Remote<h5vcc_platform_service::mojom::H5vccPlatformServiceManager>
        manager;
    auto manager_receiver = manager.BindNewPipeAndPassReceiver();
    base::RunLoop disconnect_loop;
    manager.set_disconnect_handler(disconnect_loop.QuitClosure());

    h5vcc_platform_service::H5vccPlatformServiceManagerImpl::GetOrCreate(
        /*render_frame_host=*/nullptr, std::move(manager_receiver));
    disconnect_loop.Run();
    EXPECT_FALSE(manager.is_connected());
  }
}

TEST_F(CobaltPlatformServiceBinderTest,
       HandlesSendEdgeCasesAndSynchronousReceiveInOpen) {
  base::test::ScopedFeatureList feature_list(
      features::kCobaltPlatformServicesOffUiThread);
  base::HistogramTester histogram_tester;

  state_.sync_message_on_open = {9, 8, 7};
  base::RunLoop sync_receive_loop;
  TestPlatformServiceObserver observer_impl(sync_receive_loop.QuitClosure());
  mojo::Receiver<h5vcc_platform_service::mojom::PlatformServiceObserver>
      observer_receiver(&observer_impl);
  mojo::Remote<h5vcc_platform_service::mojom::PlatformService> service;

  auto manager = BindManager();
  base::RunLoop open_loop;
  manager->Open("test.Service", observer_receiver.BindNewPipeAndPassRemote(),
                service.BindNewPipeAndPassReceiver(), open_loop.QuitClosure());
  open_loop.Run();
  sync_receive_loop.Run();
  EXPECT_EQ(observer_impl.received_data(), state_.sync_message_on_open);

  // 1. Zero-length Send payload / response returns an empty BigBuffer.
  {
    base::RunLoop send_loop;
    service->Send({}, base::BindLambdaForTesting(
                          [&](std::optional<mojo_base::BigBuffer> response,
                              const std::optional<std::string>& error) {
                            ASSERT_TRUE(response.has_value());
                            EXPECT_EQ(response->size(), 0u);
                            EXPECT_FALSE(error.has_value());
                            send_loop.Quit();
                          }));
    send_loop.Run();
  }

  // 2. Payload exceeding kCobaltExtensionPlatformServiceMaxMessageLength is
  // rejected with "Message too large".
  {
    std::vector<uint8_t> oversized(
        kCobaltExtensionPlatformServiceMaxMessageLength + 1, 0x42);
    base::RunLoop send_loop;
    service->Send(
        oversized,
        base::BindLambdaForTesting(
            [&](std::optional<mojo_base::BigBuffer> response,
                const std::optional<std::string>& error) {
              EXPECT_FALSE(response.has_value());
              EXPECT_EQ(error, std::optional<std::string>("Message too large"));
              send_loop.Quit();
            }));
    send_loop.Run();
  }

  // 3. Starboard invalid_state propagates custom error message.
  {
    state_.fail_send_invalid_state = true;
    base::RunLoop send_loop;
    service->Send(std::vector<uint8_t>{1},
                  base::BindLambdaForTesting(
                      [&](std::optional<mojo_base::BigBuffer> response,
                          const std::optional<std::string>& error) {
                        EXPECT_FALSE(response.has_value());
                        EXPECT_EQ(error, std::optional<std::string>(
                                             "custom invalid state"));
                        send_loop.Quit();
                      }));
    send_loop.Run();
  }

  histogram_tester.ExpectBucketCount(
      "Cobalt.PlatformService.Send.Status",
      h5vcc_platform_service::PlatformServiceSendStatus::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Cobalt.PlatformService.Send.Status",
      h5vcc_platform_service::PlatformServiceSendStatus::kMessageTooLarge, 1);
  histogram_tester.ExpectBucketCount(
      "Cobalt.PlatformService.Send.Status",
      h5vcc_platform_service::PlatformServiceSendStatus::kStarboardInvalidState,
      1);

  base::RunLoop close_loop;
  state_.on_close = close_loop.QuitClosure();
  service.reset();
  close_loop.Run();
}

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
class CobaltContentBrowserClientFieldTrialConfigTest
    : public CobaltContentBrowserClientTest {
 protected:
  void SetUp() override { ResetGlobalState(); }

  void TearDown() override { ResetGlobalState(); }

  void ResetGlobalState() {
    auto* global_features = GlobalFeatures::GetInstance();
    ASSERT_NE(global_features, nullptr);
    global_features->experiment_config()->ClearPref(kExperimentConfigFeatures);
    global_features->experiment_config()->ClearPref(
        kExperimentConfigFeatureParams);
    global_features->experiment_config()->ClearPref(
        kExperimentConfigActiveConfigData);
    global_features->InitializeActiveConfigData(
        ExperimentConfigType::kEmptyConfig);
    global_features->experiment_config_manager()->ResetForTesting();
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  void PopulateCachedExperimentConfig() {
    auto* experiment_config =
        GlobalFeatures::GetInstance()->experiment_config();
    base::DictValue feature_map;
    feature_map.Set(features::kTestFinchFeature.name, true);
    experiment_config->SetDict(kExperimentConfigFeatures,
                               std::move(feature_map));
    base::DictValue param_map;
    param_map.Set(features::kTestFinchFeatureParam.name, "CachedConfigValue");
    experiment_config->SetDict(kExperimentConfigFeatureParams,
                               std::move(param_map));
    experiment_config->SetString(kExperimentConfigActiveConfigData,
                                 "cached_active_config_data");
  }

  base::test::ScopedCommandLine scoped_command_line_;
  CobaltContentBrowserClient client_{/*startup_timestamp=*/std::nullopt,
                                     /*deep_link=*/""};
};

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsAppliesFieldTrialTestingConfig) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      variations::switches::kEnableFieldTrialTestingConfig);
  PopulateCachedExperimentConfig();

  auto feature_list = std::make_unique<base::FeatureList>();
  client_.SetUpCobaltFeaturesAndParams(feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_TRUE(GlobalFeatures::GetInstance()->active_config_data().empty());
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
  EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "1");
}

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsDisableFieldTrialTestingConfigUsesCache) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      variations::switches::kDisableFieldTrialTestingConfig);
  PopulateCachedExperimentConfig();

  auto feature_list = std::make_unique<base::FeatureList>();
  client_.SetUpCobaltFeaturesAndParams(feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  auto* global_features = GlobalFeatures::GetInstance();
  EXPECT_EQ(
      global_features->experiment_config_manager()->GetExperimentConfigType(),
      ExperimentConfigType::kRegularConfig);
  EXPECT_EQ(global_features->active_config_data(), "cached_active_config_data");
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
  EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "CachedConfigValue");
}

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsDefaultBehaviorWithoutSwitches) {
  PopulateCachedExperimentConfig();

  auto feature_list = std::make_unique<base::FeatureList>();
  client_.SetUpCobaltFeaturesAndParams(feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  auto* global_features = GlobalFeatures::GetInstance();
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
#if defined(OFFICIAL_BUILD)
  // Official builds only apply the testing config when explicitly enabled, so
  // the cached experiment config is used.
  EXPECT_EQ(
      global_features->experiment_config_manager()->GetExperimentConfigType(),
      ExperimentConfigType::kRegularConfig);
  EXPECT_EQ(global_features->active_config_data(), "cached_active_config_data");
  EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "CachedConfigValue");
#else
  // Non-official builds apply the testing config by default.
  EXPECT_TRUE(global_features->active_config_data().empty());
  EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "1");
#endif  // defined(OFFICIAL_BUILD)
}

TEST_F(CobaltContentBrowserClientFieldTrialConfigTest,
       SetUpCobaltFeaturesAndParamsRespectsFeatureOverridePrecedence) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      variations::switches::kEnableFieldTrialTestingConfig);

  // 1. Command-line overrides (1st) should take precedence over extra feature
  // overrides (2nd) and FieldTrialTestingConfig (3rd).
  {
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->InitFromCommandLine(
        /*enable_features=*/"",
        /*disable_features=*/features::kTestFinchFeature.name);
    feature_list->RegisterExtraFeatureOverrides(
        {{std::cref(features::kTestFinchFeature),
          base::FeatureList::OVERRIDE_ENABLE_FEATURE}});
    client_.SetUpCobaltFeaturesAndParams(feature_list.get());

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
    EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "");
  }

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

  // 2. Extra feature overrides (2nd) should take precedence over
  // FieldTrialTestingConfig (3rd).
  {
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->InitFromCommandLine(/*enable_features=*/"",
                                      /*disable_features=*/"");
    feature_list->RegisterExtraFeatureOverrides(
        {{std::cref(features::kTestFinchFeature),
          base::FeatureList::OVERRIDE_DISABLE_FEATURE}});
    client_.SetUpCobaltFeaturesAndParams(feature_list.get());

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_FALSE(base::FeatureList::IsEnabled(features::kTestFinchFeature));
    EXPECT_EQ(features::kTestFinchFeatureParam.Get(), "");
  }
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

}  // namespace
}  // namespace cobalt
