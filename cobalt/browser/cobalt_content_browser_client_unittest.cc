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
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/h5vcc_platform_service/h5vcc_platform_service_manager_impl.h"
#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"
#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "cobalt/shell/common/shell_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/overlay_window.h"
#include "content/test/test_web_contents.h"
#include "gpu/config/gpu_finch_features.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/platform_service.h"
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

}  // namespace
}  // namespace cobalt
