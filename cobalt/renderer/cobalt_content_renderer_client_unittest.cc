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

// Test fixture verifying asynchronous SbWindow handle retrieval through
// CobaltContentRendererClient's PlatformWindowProvider Mojo binding on the
// renderer main thread and caller-sequence reply posting.
class CobaltContentRendererClientWindowHandleTest : public ::testing::Test {
 protected:
  // In-memory fake browser PlatformWindowProvider that queues incoming
  // GetSbWindow() requests so tests can resolve or drop them deterministically.
  class FakePlatformWindowProvider
      : public media::mojom::PlatformWindowProvider {
   public:
    void Bind(
        mojo::PendingReceiver<media::mojom::PlatformWindowProvider> receiver) {
      ++bind_count_;
      receivers_.Add(this, std::move(receiver));
    }

    void GetSbWindow(GetSbWindowCallback callback) override {
      pending_callbacks_.push_back(std::move(callback));
    }

    size_t bind_count() const { return bind_count_; }
    size_t pending_request_count() const { return pending_callbacks_.size(); }

    void ResolveNextRequest(uint64_t handle) {
      ASSERT_FALSE(pending_callbacks_.empty());
      auto cb = std::move(pending_callbacks_.front());
      pending_callbacks_.pop_front();
      std::move(cb).Run(handle);
    }

    void DropAllReceiversAndPendingRequests() {
      receivers_.Clear();
      pending_callbacks_.clear();
    }

   private:
    size_t bind_count_ = 0;
    mojo::ReceiverSet<media::mojom::PlatformWindowProvider> receivers_;
    std::deque<GetSbWindowCallback> pending_callbacks_;
  };

  void SetUp() override {
    client_ = std::make_unique<CobaltContentRendererClient>();
    client_->main_task_runner_ = task_environment_.GetMainThreadTaskRunner();
    client_->SetPlatformWindowProviderBinderForTesting(base::BindRepeating(
        &FakePlatformWindowProvider::Bind, base::Unretained(&provider_)));
  }

  void ClearPlatformWindowProviderBinder() {
    client_->SetPlatformWindowProviderBinderForTesting({});
  }

  base::test::TaskEnvironment task_environment_;
  FakePlatformWindowProvider provider_;
  std::unique_ptr<CobaltContentRendererClient> client_;
};

#if BUILDFLAG(IS_STARBOARD)
// Verifies the user journey where playback starts before conceal, the app is
// backgrounded (destroying the old SbWindow), and playback resumes on reveal:
// each GetSbWindowHandle() fetches the current live handle from the browser so
// resumed players never receive a destroyed pre-conceal handle.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleFetchesFreshWindowAcrossConcealAndReveal) {
  uint64_t first_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { first_handle = handle; }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0x1111);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(first_handle, 0x1111u);

  // Subsequent media initialization after conceal/reveal issues a new browser
  // request and receives the refreshed handle instead of a stale cached value.
  uint64_t second_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { second_handle = handle; }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0x2222);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(second_handle, 0x2222u);
}

// Verifies the user journey where a media load requests the window handle while
// no native SbWindow exists yet (e.g., preloading or mid-reveal) and the
// browser queues the request until OnSbWindowCreated() runs.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleWaitsForDeferredBrowserWindowReply) {
  bool callback_invoked = false;
  uint64_t received_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting([&](uint64_t handle) {
    callback_invoked = true;
    received_handle = handle;
  }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  EXPECT_FALSE(callback_invoked);

  provider_.ResolveNextRequest(0x3333);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_handle, 0x3333u);
}

// Verifies the failure-recovery journey where the browser drops the
// PlatformWindowProvider Mojo pipe without replying (e.g., during shutdown or
// host disconnect): the callback still runs with 0 instead of hanging media
// initialization indefinitely, and later requests continue to succeed.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleReturnsZeroWhenProviderDisconnects) {
  bool first_called = false;
  uint64_t first_handle = 0xdead;
  client_->GetSbWindowHandle(base::BindLambdaForTesting([&](uint64_t handle) {
    first_called = true;
    first_handle = handle;
  }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);

  provider_.DropAllReceiversAndPendingRequests();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first_called);
  EXPECT_EQ(first_handle, 0u);

  uint64_t second_handle = 0;
  client_->GetSbWindowHandle(base::BindLambdaForTesting(
      [&](uint64_t handle) { second_handle = handle; }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);
  provider_.ResolveNextRequest(0x4444);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(second_handle, 0x4444u);
}

// Verifies the cross-thread media pipeline journey where
// StarboardRendererClient calls GetSbWindowHandle() from the media sequence:
// the request hops to the renderer main thread for Mojo binding and delivers
// the handle back on the caller's sequence.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleRepliesOnCallerSequence) {
  scoped_refptr<base::SequencedTaskRunner> caller_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop request_sent;
  base::RunLoop reply_received;
  bool replied_on_caller_sequence = false;
  uint64_t received_handle = 0;

  caller_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        client_->GetSbWindowHandle(
            base::BindLambdaForTesting([&](uint64_t handle) {
              replied_on_caller_sequence =
                  caller_runner->RunsTasksInCurrentSequence();
              received_handle = handle;
              reply_received.Quit();
            }));
        request_sent.Quit();
      }));

  request_sent.Run();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider_.pending_request_count(), 1u);

  provider_.ResolveNextRequest(0x5555);
  reply_received.Run();
  EXPECT_TRUE(replied_on_caller_sequence);
  EXPECT_EQ(received_handle, 0x5555u);
}

// Verifies the teardown / test-environment journey where a queued window handle
// request runs on the main thread when content::RenderThread::Get() is null:
// the callback completes safely with 0 instead of dereferencing a null pointer.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleReturnsZeroWhenRenderThreadIsNull) {
  ClearPlatformWindowProviderBinder();

  bool called = false;
  uint64_t received_handle = 0xdead;
  client_->GetSbWindowHandle(base::BindLambdaForTesting([&](uint64_t handle) {
    called = true;
    received_handle = handle;
  }));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_EQ(received_handle, 0u);
  EXPECT_EQ(provider_.bind_count(), 0u);
}
#else
// Verifies that non-Starboard builds (e.g., Android and tvOS) return 0
// immediately without binding PlatformWindowProvider.
TEST_F(CobaltContentRendererClientWindowHandleTest,
       GetSbWindowHandleReturnsZeroWithoutBindingOnNonStarboard) {
  bool called = false;
  uint64_t received_handle = 0xdead;
  client_->GetSbWindowHandle(base::BindLambdaForTesting([&](uint64_t handle) {
    called = true;
    received_handle = handle;
  }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(received_handle, 0u);
  EXPECT_EQ(provider_.bind_count(), 0u);
}
#endif  // BUILDFLAG(IS_STARBOARD)

}  // namespace cobalt
