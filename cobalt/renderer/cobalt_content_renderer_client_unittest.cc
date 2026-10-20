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

#include "cobalt/renderer/cobalt_content_renderer_client.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cobalt/media/service/mojom/platform_window_provider.mojom.h"
#include "media/base/audio_codecs.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)
#include "cobalt/internal/cobalt/components/cdm/renderer/starboard/platform_drm_key_system_info.h"
#endif  // BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)

namespace cobalt {
namespace {

TEST(CobaltContentRendererClientTest, MediaQueriesCallableFromWorkerThread) {
  base::test::TaskEnvironment task_environment;
  CobaltContentRendererClient client;

  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](CobaltContentRendererClient* client,
             base::OnceClosure quit_closure) {
            client->IsDecoderSupportedAudioType({::media::AudioCodec::kAAC});
            client->IsDecoderSupportedVideoType({::media::VideoCodec::kH264});
            client->GetSupportedKeySystems(
                /*render_frame=*/nullptr,
                base::BindRepeating([](::media::KeySystemInfos key_systems) {
                  EXPECT_FALSE(key_systems.empty());
                }));
            std::move(quit_closure).Run();
          },
          base::Unretained(&client), run_loop.QuitClosure()));
  run_loop.Run();
}

#if BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)

// Returns the base key system name the client registers for `key_system`, or
// an empty string if no registered key system claims that name.
std::string RegisteredBaseKeySystemName(const std::string& key_system) {
  CobaltContentRendererClient client;
  std::string base_name;
  client.GetSupportedKeySystems(
      /*render_frame=*/nullptr,
      base::BindRepeating(
          [](std::string* base_name, const std::string& key_system,
             ::media::KeySystemInfos key_systems) {
            for (const auto& info : key_systems) {
              if (info->IsSupportedKeySystem(key_system)) {
                *base_name = info->GetBaseKeySystemName();
                return;
              }
            }
          },
          base::Unretained(&base_name), key_system));
  return base_name;
}

TEST(CobaltContentRendererClientTest, PlatformDrmKeySystemIsRegistered) {
  base::test::TaskEnvironment task_environment;

  const cdm::PlatformDrmKeySystemInfo info(
      ::media::EME_CODEC_NONE, base::flat_set<::media::EncryptionScheme>{},
      ::media::EME_CODEC_NONE, ::media::EmeFeatureSupport::ALWAYS_ENABLED,
      ::media::EmeFeatureSupport::ALWAYS_ENABLED);
  const std::string key_system = info.GetBaseKeySystemName();

  EXPECT_EQ(RegisteredBaseKeySystemName(key_system), key_system);
}

TEST(CobaltContentRendererClientTest, PlatformDrmSbdlKeySystemIsRegistered) {
  base::test::TaskEnvironment task_environment;

  const cdm::PlatformDrmSbdlKeySystemInfo sbdl_info(
      base::flat_set<::media::EncryptionScheme>{},
      ::media::EmeFeatureSupport::ALWAYS_ENABLED,
      ::media::EmeFeatureSupport::ALWAYS_ENABLED);
  const std::string sbdl_key_system = sbdl_info.GetBaseKeySystemName();

  EXPECT_EQ(RegisteredBaseKeySystemName(sbdl_key_system), sbdl_key_system);
}

#endif  // BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)

}  // namespace

class CobaltContentRendererClientWindowHandleTest : public ::testing::Test {
 protected:
  class FakePlatformWindowProvider
      : public media::mojom::PlatformWindowProvider {
   public:
    void Bind(
        mojo::PendingReceiver<media::mojom::PlatformWindowProvider> receiver) {
      receivers_.Add(this, std::move(receiver));
    }

    void GetSbWindow(GetSbWindowCallback callback) override {
      pending_callbacks_.push_back(std::move(callback));
    }

    size_t pending_request_count() const { return pending_callbacks_.size(); }

    void ResolveNextRequest(uint64_t handle) {
      ASSERT_FALSE(pending_callbacks_.empty());
      auto cb = std::move(pending_callbacks_.front());
      pending_callbacks_.pop_front();
      std::move(cb).Run(handle);
    }

   private:
    mojo::ReceiverSet<media::mojom::PlatformWindowProvider> receivers_;
    std::deque<GetSbWindowCallback> pending_callbacks_;
  };

  class TestCobaltContentRendererClient : public CobaltContentRendererClient {
   public:
    explicit TestCobaltContentRendererClient(
        FakePlatformWindowProvider* provider)
        : provider_(provider) {}

   private:
    void BindPlatformWindowProvider(
        content::RenderFrame* /*render_frame*/,
        mojo::PendingReceiver<media::mojom::PlatformWindowProvider> receiver)
        override {
      provider_->Bind(std::move(receiver));
    }

    FakePlatformWindowProvider* provider_;
  };

  void SetUp() override {
    client_ = std::make_unique<TestCobaltContentRendererClient>(&provider_);
  }

  static content::RenderFrame* FakeFrameId(uintptr_t id) {
    return reinterpret_cast<content::RenderFrame*>(id * 0x100);
  }

  void OnMainFrameShown(content::RenderFrame* frame) {
    client_->OnMainFrameShown(frame);
  }

  void OnMainFrameHiddenOrDestroyed(content::RenderFrame* frame) {
    client_->OnMainFrameHiddenOrDestroyed(frame);
  }

  base::test::TaskEnvironment task_environment_;
  FakePlatformWindowProvider provider_;
  std::unique_ptr<TestCobaltContentRendererClient> client_;
};

// Verifies the user journey where the splash screen finishes and switches to
// the main WebContents (Shell::SwitchToMainWebContents): if the splash frame's
// WasHidden() arrives on its Widget Mojo pipe after the main frame's
// WasShown(), the cached SbWindow handle must remain valid so subsequent video
// playback does not hang waiting for a new handle.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       SplashFrameHiddenAfterMainFrameShownPreservesSbWindowHandle) {
  content::RenderFrame* splash_frame = FakeFrameId(1);
  content::RenderFrame* main_frame = FakeFrameId(2);

  OnMainFrameShown(splash_frame);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0x1111);
  task_environment_.RunUntilIdle();

  // Main frame becomes visible, then splash frame hides/destroys afterwards.
  OnMainFrameShown(main_frame);
  OnMainFrameHiddenOrDestroyed(splash_frame);
  task_environment_.RunUntilIdle();

  uint64_t received_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { received_handle = handle; }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(received_handle, 0x1111u);
}

// Verifies the user journey at startup when LoadSplashScreenContents()
// detaches the initial main WebContents while showing the splash WebContents:
// the main frame's WasHidden() must not wipe the shared SbWindow handle while
// the splash frame is still visible for video splash playback.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       MainFrameHiddenWhileSplashVisiblePreservesSbWindowHandle) {
  content::RenderFrame* main_frame = FakeFrameId(1);
  content::RenderFrame* splash_frame = FakeFrameId(2);

  OnMainFrameShown(main_frame);
  OnMainFrameShown(splash_frame);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0x1111);
  task_environment_.RunUntilIdle();

  OnMainFrameHiddenOrDestroyed(main_frame);
  task_environment_.RunUntilIdle();

  uint64_t received_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { received_handle = handle; }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(received_handle, 0x1111u);
}

// Verifies the user journey where the user backgrounds the app (hiding all
// visible main frames) while a window request is in flight and later returns:
// the pre-conceal request is invalidated so its stale reply is ignored, and
// queued GetSbWindowHandle() callers receive the fresh post-reveal SbWindow.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       AllMainFramesHiddenResetsAndRefetchesNewSbWindowOnReveal) {
  content::RenderFrame* main_frame = FakeFrameId(1);

  OnMainFrameShown(main_frame);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);

  // Conceal before the initial GetSbWindow() reply arrives.
  OnMainFrameHiddenOrDestroyed(main_frame);
  provider_.ResolveNextRequest(0x1111);
  task_environment_.RunUntilIdle();

  uint64_t received_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { received_handle = handle; }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(received_handle, 0u);

  // Reveal requests a fresh handle and fulfills the queued media callback.
  OnMainFrameShown(main_frame);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0x2222);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(received_handle, 0x2222u);
}

// Verifies the recovery journey where an initial GetSbWindow() call returned
// an invalid handle (0) while a main frame remained visible: a subsequent
// media GetSbWindowHandle() call re-requests the handle from the browser
// instead of waiting indefinitely.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleSelfHealsWhenVisibleFrameExistsWithoutCachedHandle) {
  content::RenderFrame* main_frame = FakeFrameId(1);

  OnMainFrameShown(main_frame);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0);
  task_environment_.RunUntilIdle();

  uint64_t received_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { received_handle = handle; }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);

  provider_.ResolveNextRequest(0x3333);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(received_handle, 0x3333u);
}

}  // namespace cobalt
