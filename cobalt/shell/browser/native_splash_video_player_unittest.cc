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

#include "cobalt/shell/browser/native_splash_video_player.h"

#include "build/build_config.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "media/starboard/mock_sbplayer_interface.h"
#include "media/starboard/sbplayer_interface.h"
#include "starboard/player.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::SaveArg;
using Error = NativeSplashVideoPlayer::Error;

constexpr gfx::Rect kBounds(0, 0, 1920, 1080);

// An SbPlayer that the mock created, and the callbacks that Starboard would
// call it back with.
struct CreatedPlayer {
  void ReportState(SbPlayerState state, int ticket) const {
    player_status_func(player, context, state, ticket);
  }

  void ReportError() const {
    player_error_func(player, context, kSbPlayerErrorDecode, "Decode error");
  }

  SbPlayer player = kSbPlayerInvalid;
  SbPlayerStatusFunc player_status_func = nullptr;
  SbPlayerErrorFunc player_error_func = nullptr;
  void* context = nullptr;
};

// An action of SbPlayerInterface::Create() that creates a player, which the
// mock's Destroy() deletes, as `created`.
auto CreatesPlayer(CreatedPlayer* created) {
  return [created](SbWindow, const SbPlayerCreationParam*,
                   SbPlayerDeallocateSampleFunc, SbPlayerDecoderStatusFunc,
                   SbPlayerStatusFunc player_status_func,
                   SbPlayerErrorFunc player_error_func, void* context,
                   SbDecodeTargetGraphicsContextProvider*) {
    created->player = reinterpret_cast<SbPlayer>(new media::MockSbPlayer());
    created->player_status_func = player_status_func;
    created->player_error_func = player_error_func;
    created->context = context;
    return created->player;
  };
}

// A video that the WebApp may cache: any valid one.
std::string CachedVideo() {
  const base::span<const uint8_t> webm =
      H5vccSchemeURLLoaderFactory::GetBuiltInResource("music_splash.webm");
  return std::string(webm.begin(), webm.end());
}

class NativeSplashVideoPlayerTest : public testing::Test {
 protected:
  NativeSplashVideoPlayerTest() {
    sbplayer_.SetupDefaultExpectations();
    EXPECT_CALL(sbplayer_, Seek(_, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<2>(&ticket_));
    EXPECT_CALL(sbplayer_, SetBounds(_, _, _, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(sbplayer_, WriteSamples(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(sbplayer_, WriteEndOfStream(_, _)).Times(AnyNumber());

    // The tests that play the cached video expect the platform to play it.
    callbacks_.on_cached_video_rejected = base::BindOnce(
        [] { ADD_FAILURE() << "The cached video was rejected."; });
  }

  ~NativeSplashVideoPlayerTest() override {
    // The SbPlayer is destroyed on the player's thread, before the mock.
    player_.reset();
    task_environment_.RunUntilIdle();
  }

  // Plays `cached_webm`, or the built-in video if it's empty, with
  // `callbacks_`, until its SbPlayer is created.
  void CreatePlayer(std::string cached_webm) {
    player_ = NativeSplashVideoPlayer::CreateForSplashScreen(
        /*topic=*/"", std::move(cached_webm), kBounds, std::move(callbacks_));
    ASSERT_TRUE(player_);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<media::MockSbPlayerInterface> sbplayer_;
  media::ScopedSbPlayerInterfaceForTesting scoped_sbplayer_{&sbplayer_};
  NativeSplashVideoPlayer::Callbacks callbacks_;
  std::unique_ptr<NativeSplashVideoPlayer> player_;
  // Of the last seek, i.e., of the current player.
  int ticket_ = 0;
};

// SbPlayerCreate() may report its error before it returns, e.g., Android's
// when the video surface isn't available: it's reported once, as
// kCreateFailed, and the built-in video isn't tried.
TEST_F(NativeSplashVideoPlayerTest, CreateErrorIsReportedOnceWithoutRetrying) {
  EXPECT_CALL(sbplayer_, Create(_, _, _, _, _, _, _, _))
      .WillOnce([](SbWindow, const SbPlayerCreationParam*,
                   SbPlayerDeallocateSampleFunc, SbPlayerDecoderStatusFunc,
                   SbPlayerStatusFunc, SbPlayerErrorFunc player_error_func,
                   void* context, SbDecodeTargetGraphicsContextProvider*) {
        player_error_func(kSbPlayerInvalid, context, kSbPlayerErrorDecode,
                          "Video surface is not available now");
        return kSbPlayerInvalid;
      });
  std::optional<Error> error;
  callbacks_.on_error =
      base::BindLambdaForTesting([&](Error reported) { error = reported; });
  callbacks_.on_cached_video_failed =
      base::BindOnce([] { ADD_FAILURE() << "The built-in video was tried."; });

  ASSERT_NO_FATAL_FAILURE(CreatePlayer(CachedVideo()));
  EXPECT_EQ(error, Error::kCreateFailed);
}

// The video that the WebApp cached fails before its first frame: the built-in
// video plays instead, without an error.
TEST_F(NativeSplashVideoPlayerTest, CachedVideoErrorPlaysTheBuiltInVideo) {
  CreatedPlayer cached;
  CreatedPlayer built_in;
  EXPECT_CALL(sbplayer_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(CreatesPlayer(&cached))
      .WillOnce(CreatesPlayer(&built_in));
  bool cached_video_failed = false;
  callbacks_.on_cached_video_failed =
      base::BindLambdaForTesting([&] { cached_video_failed = true; });
  callbacks_.on_error = base::BindOnce([](Error error) {
    ADD_FAILURE() << "Error " << static_cast<int>(error);
  });

  ASSERT_NO_FATAL_FAILURE(CreatePlayer(CachedVideo()));
  cached.ReportError();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(cached_video_failed);
  EXPECT_NE(built_in.player, kSbPlayerInvalid);
}

// After the first frame, which stays on screen, an error is reported as such,
// even for the cached video, which isn't retried.
TEST_F(NativeSplashVideoPlayerTest, ErrorAfterFirstFrameIsReportedAsSuch) {
  CreatedPlayer created;
  EXPECT_CALL(sbplayer_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(CreatesPlayer(&created));
  bool first_frame = false;
  std::optional<Error> error;
  callbacks_.on_first_frame =
      base::BindLambdaForTesting([&] { first_frame = true; });
  callbacks_.on_error =
      base::BindLambdaForTesting([&](Error reported) { error = reported; });
  callbacks_.on_cached_video_failed =
      base::BindOnce([] { ADD_FAILURE() << "The built-in video was tried."; });

  ASSERT_NO_FATAL_FAILURE(CreatePlayer(CachedVideo()));
  created.ReportState(kSbPlayerStatePresenting, ticket_);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first_frame);

  created.ReportError();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(error, Error::kPlaybackErrorAfterFirstFrame);
}

// Without a cached video, an error before the first frame is a playback error.
TEST_F(NativeSplashVideoPlayerTest, BuiltInVideoErrorIsAPlaybackError) {
  CreatedPlayer created;
  EXPECT_CALL(sbplayer_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(CreatesPlayer(&created));
  std::optional<Error> error;
  callbacks_.on_error =
      base::BindLambdaForTesting([&](Error reported) { error = reported; });

  ASSERT_NO_FATAL_FAILURE(CreatePlayer(/*cached_webm=*/""));
  created.ReportError();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(error, Error::kPlaybackError);
}

// The player is destroyed while its thread starts it, e.g., when the app is
// concealed as the splash screen starts: no SbPlayer is created afterwards.
TEST_F(NativeSplashVideoPlayerTest, NoPlayerIsCreatedOnceDestroyed) {
  base::WaitableEvent starting;
  base::WaitableEvent destroyed;
  EXPECT_CALL(sbplayer_, GetPreferredOutputMode(_))
      .WillOnce([&](const SbPlayerCreationParam*) {
        starting.Signal();
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        destroyed.Wait();
        return kSbPlayerOutputModePunchOut;
      });
  EXPECT_CALL(sbplayer_, Create(_, _, _, _, _, _, _, _)).Times(0);

  player_ = NativeSplashVideoPlayer::CreateForSplashScreen(
      /*topic=*/"", /*cached_webm=*/"", kBounds, std::move(callbacks_));
  ASSERT_TRUE(player_);
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    starting.Wait();
  }
  player_.reset();
  destroyed.Signal();
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace content

#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
