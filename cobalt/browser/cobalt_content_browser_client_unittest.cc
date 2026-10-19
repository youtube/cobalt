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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/shell/common/shell_paths.h"
#include "content/browser/gpu/gpu_disk_cache_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "gpu/ipc/host/gpu_disk_cache.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/isolation_info.h"
#include "net/base/test_completion_callback.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "starboard/configuration_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

TEST_F(CobaltContentBrowserClientTest,
       ShaderDiskCacheDirectoriesUseCacheDirectory) {
  base::ScopedTempDir temp_cache_dir;
  base::ScopedTempDir temp_user_data_dir;
  ASSERT_TRUE(temp_cache_dir.CreateUniqueTempDir());
  ASSERT_TRUE(temp_user_data_dir.CreateUniqueTempDir());

  base::ScopedPathOverride cache_override(
      base::DIR_CACHE, temp_cache_dir.GetPath(),
      /*is_absolute=*/true, /*create=*/false);
  base::ScopedPathOverride user_data_override(
      content::SHELL_DIR_USER_DATA, temp_user_data_dir.GetPath(),
      /*is_absolute=*/true, /*create=*/false);

  CobaltContentBrowserClient client(/*startup_timestamp=*/std::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);

  base::FilePath shader_cache_dir = client.GetShaderDiskCacheDirectory();
  EXPECT_EQ(shader_cache_dir,
            temp_cache_dir.GetPath().Append(FILE_PATH_LITERAL("ShaderCache")));
  EXPECT_TRUE(temp_cache_dir.GetPath().IsParent(shader_cache_dir));
  EXPECT_FALSE(temp_user_data_dir.GetPath().IsParent(shader_cache_dir));

  base::FilePath gr_shader_cache_dir = client.GetGrShaderDiskCacheDirectory();
  EXPECT_EQ(gr_shader_cache_dir, temp_cache_dir.GetPath().Append(
                                     FILE_PATH_LITERAL("GrShaderCache")));
  EXPECT_TRUE(temp_cache_dir.GetPath().IsParent(gr_shader_cache_dir));
  EXPECT_FALSE(temp_user_data_dir.GetPath().IsParent(gr_shader_cache_dir));

  base::FilePath graphite_dawn_cache_dir =
      client.GetGraphiteDawnDiskCacheDirectory();
  EXPECT_EQ(
      graphite_dawn_cache_dir,
      temp_cache_dir.GetPath().Append(FILE_PATH_LITERAL("GraphiteDawnCache")));
  EXPECT_TRUE(temp_cache_dir.GetPath().IsParent(graphite_dawn_cache_dir));
  EXPECT_FALSE(temp_user_data_dir.GetPath().IsParent(graphite_dawn_cache_dir));
}

TEST_F(CobaltContentBrowserClientTest,
       RendererGpuDiskCachePathsUseCacheDirectory) {
  base::ScopedTempDir temp_cache_dir;
  base::ScopedTempDir temp_user_data_dir;
  ASSERT_TRUE(temp_cache_dir.CreateUniqueTempDir());
  ASSERT_TRUE(temp_user_data_dir.CreateUniqueTempDir());

  base::ScopedPathOverride cache_override(
      base::DIR_CACHE, temp_cache_dir.GetPath(),
      /*is_absolute=*/true, /*create=*/false);
  base::ScopedPathOverride user_data_override(
      content::SHELL_DIR_USER_DATA, temp_user_data_dir.GetPath(),
      /*is_absolute=*/true, /*create=*/false);

  CobaltContentBrowserClient client(/*startup_timestamp=*/std::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  const base::FilePath shader_cache_dir = client.GetShaderDiskCacheDirectory();
  const base::FilePath gr_shader_cache_dir =
      client.GetGrShaderDiskCacheDirectory();
  const base::FilePath graphite_dawn_cache_dir =
      client.GetGraphiteDawnDiskCacheDirectory();

  for (const gpu::GpuDiskCacheType type : gpu::kGpuDiskCacheTypes) {
    base::FilePath gpu_cache_path = content::GetCobaltGpuDiskCachePath(type);
    EXPECT_EQ(gpu_cache_path, temp_cache_dir.GetPath().Append(
                                  gpu::GetGpuDiskCacheSubdir(type)));
    EXPECT_TRUE(temp_cache_dir.GetPath().IsParent(gpu_cache_path));
    EXPECT_FALSE(temp_user_data_dir.GetPath().IsParent(gpu_cache_path));

    // Renderer GPU cache paths must remain distinct from reserved host shader
    // cache paths since both share base::DIR_CACHE on Cobalt.
    EXPECT_NE(gpu_cache_path, shader_cache_dir);
    EXPECT_NE(gpu_cache_path, gr_shader_cache_dir);
    EXPECT_NE(gpu_cache_path, graphite_dawn_cache_dir);
  }
}

class CobaltContentBrowserClientGpuDiskCacheTest : public testing::Test {
 protected:
  CobaltContentBrowserClientGpuDiskCacheTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    scoped_feature_list_.InitAndDisableFeature(
        content::kNetworkServiceDedicatedThread);
    content::ForceInProcessNetworkService();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_cache_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_cache_dir_.GetPath(),
        /*is_absolute=*/true, /*create=*/false);

    old_content_client_ = content::GetContentClientForTesting();
    content::SetContentClient(&test_content_client_);
    old_browser_client_ =
        content::SetBrowserClientForTesting(&test_browser_client_);
    clients_installed_ = true;

    if (!content::GetGpuDiskCacheFactorySingleton()) {
      content::InitGpuDiskCacheFactorySingleton();
    }
    factory_ = content::GetGpuDiskCacheFactorySingleton();
    ASSERT_NE(factory_, nullptr);

    browser_context_ = std::make_unique<content::TestBrowserContext>();
  }

  void TearDown() override {
    if (factory_) {
      for (auto& cache : dir_cache_caches_) {
        if (cache) {
          factory_->ReleaseCacheHandle(cache.get());
        }
      }
      if (user_data_cache_) {
        factory_->ReleaseCacheHandle(user_data_cache_.get());
      }
    }
    dir_cache_caches_.clear();
    user_data_cache_.reset();
    browser_context_.reset();
    task_environment_.RunUntilIdle();

    if (clients_installed_) {
      content::SetBrowserClientForTesting(old_browser_client_);
      content::SetContentClient(old_content_client_);
    }
    cache_override_.reset();
  }

  // Declared before `task_environment_` so the temporary directory outlives
  // any asynchronous cache backend cleanup tasks on the ThreadPool.
  base::ScopedTempDir temp_cache_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;

  content::TestContentClient test_content_client_;
  content::TestContentBrowserClient test_browser_client_;
  content::ContentClient* old_content_client_ = nullptr;
  content::ContentBrowserClient* old_browser_client_ = nullptr;
  bool clients_installed_ = false;
  gpu::GpuDiskCacheFactory* factory_ = nullptr;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::vector<scoped_refptr<gpu::GpuDiskCache>> dir_cache_caches_;
  scoped_refptr<gpu::GpuDiskCache> user_data_cache_;
};

TEST_F(CobaltContentBrowserClientGpuDiskCacheTest,
       StoragePartitionShaderCacheClearsCacheDirectory) {
  content::StoragePartition* partition =
      browser_context_->GetDefaultStoragePartition();
  ASSERT_NE(partition->GetPath(), temp_cache_dir_.GetPath());

  // Populate GPU disk caches under base::DIR_CACHE (where Cobalt registers
  // renderer GPU disk caches via GetCobaltGpuDiskCachePath) and under the
  // StoragePartition path (where non-Cobalt builds would place them).
  for (const gpu::GpuDiskCacheType type : gpu::kGpuDiskCacheTypes) {
    auto handle = factory_->GetCacheHandle(
        type, content::GetCobaltGpuDiskCachePath(type));
    scoped_refptr<gpu::GpuDiskCache> cache =
        factory_->Create(handle, base::DoNothing());
    ASSERT_NE(cache, nullptr);

    net::TestCompletionCallback available_cb;
    int rv = cache->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));

    cache->Cache("shader_key", "shader_blob");
    net::TestCompletionCallback complete_cb;
    rv = cache->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
    EXPECT_EQ(1, cache->Size());
    dir_cache_caches_.push_back(std::move(cache));
  }

  auto user_data_handle = factory_->GetCacheHandle(
      gpu::GpuDiskCacheType::kGlShaders,
      partition->GetPath().Append(
          gpu::GetGpuDiskCacheSubdir(gpu::GpuDiskCacheType::kGlShaders)));
  user_data_cache_ = factory_->Create(user_data_handle, base::DoNothing());
  ASSERT_NE(user_data_cache_, nullptr);
  {
    net::TestCompletionCallback available_cb;
    int rv = user_data_cache_->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
    user_data_cache_->Cache("shader_key", "shader_blob");
    net::TestCompletionCallback complete_cb;
    rv = user_data_cache_->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
    EXPECT_EQ(1, user_data_cache_->Size());
  }

  base::RunLoop run_loop;
  partition->ClearData(
      content::StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      blink::StorageKey(), base::Time(), base::Time::Max(),
      run_loop.QuitClosure());
  run_loop.Run();

  for (const auto& cache : dir_cache_caches_) {
    EXPECT_EQ(0, cache->Size());
  }
  EXPECT_EQ(1, user_data_cache_->Size());
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
