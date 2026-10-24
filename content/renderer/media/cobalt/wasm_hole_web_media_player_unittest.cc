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

#include "content/renderer/media/cobalt/wasm_hole_web_media_player.h"

#include <limits>
#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "cc/layers/layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"

// media_player_client.h uses KURL without including it.
#include "third_party/blink/renderer/platform/media/media_player_client.h"  // nogncheck
#include "third_party/blink/renderer/platform/weborigin/kurl.h"  // nogncheck
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"  // nogncheck

namespace content {
namespace {

using ::testing::_;
using ::testing::IsNull;
using ::testing::NotNull;

class MockMediaPlayerClient : public blink::MediaPlayerClient {
 public:
  MOCK_METHOD0(NetworkStateChanged, void());
  MOCK_METHOD0(ReadyStateChanged, void());
  MOCK_METHOD0(TimeChanged, void());
  MOCK_METHOD0(Repaint, void());
  MOCK_METHOD0(DurationChanged, void());
  MOCK_METHOD0(SizeChanged, void());
  MOCK_METHOD1(SetCcLayer, void(cc::Layer*));
  MOCK_METHOD1(AddMediaTrack, void(const media::MediaTrack&));
  MOCK_METHOD1(RemoveMediaTrack, void(const media::MediaTrack&));
  MOCK_METHOD1(MediaSourceOpened, void(std::unique_ptr<blink::WebMediaSource>));
  MOCK_METHOD2(RemotePlaybackCompatibilityChanged,
               void(const blink::KURL&, bool));
  MOCK_METHOD0(WasAlwaysMuted, bool());
  MOCK_METHOD0(HasSelectedVideoTrack, bool());
  MOCK_METHOD0(GetSelectedVideoTrackId, blink::WebMediaPlayer::TrackId());
  MOCK_METHOD0(HasNativeControls, bool());
  MOCK_METHOD0(IsAudioElement, bool());
  MOCK_CONST_METHOD0(GetDisplayType, blink::WebMediaPlayer::DisplayType());
  MOCK_CONST_METHOD0(IsInAutoPIP, bool());
  MOCK_METHOD1(MediaRemotingStarted, void(const blink::WebString&));
  MOCK_METHOD1(MediaRemotingStopped, void(int));
  MOCK_METHOD0(OnPictureInPictureStateChange, void());
  MOCK_METHOD1(OnRemotePlaybackDisabled, void(bool));
  MOCK_CONST_METHOD0(CouldPlayIfEnoughData, bool());
  MOCK_METHOD0(ResumePlayback, void());
  MOCK_METHOD1(PausePlayback, void(blink::WebMediaPlayer::PauseReason));
  MOCK_METHOD0(DidPlayerStartPlaying, void());
  MOCK_METHOD1(DidPlayerPaused, void(bool));
  MOCK_METHOD1(DidPlayerMutedStatusChange, void(bool));
  MOCK_METHOD6(DidMediaMetadataChange,
               void(bool,
                    bool,
                    media::AudioCodec,
                    media::VideoCodec,
                    media::MediaContentType,
                    bool));
  MOCK_METHOD4(DidPlayerMediaPositionStateChange,
               void(double, base::TimeDelta, base::TimeDelta, bool));
  MOCK_METHOD0(DidDisableAudioOutputSinkChanges, void());
  MOCK_METHOD1(DidUseAudioServiceChange, void(bool));
  MOCK_METHOD1(DidPlayerSizeChange, void(const gfx::Size&));
  MOCK_METHOD2(OnFirstFrame, void(base::TimeTicks, size_t));
  MOCK_METHOD0(GetElementId, int());
};

class WasmHoleWebMediaPlayerTest : public testing::Test {
 protected:
  // blink::WebURL allocates WTF strings; cobalt_unittests doesn't start Blink.
  static void SetUpTestSuite() { WTF::Partitions::Initialize(); }

  std::unique_ptr<blink::WebMediaPlayer> CreatePlayer(const std::string& url) {
    return MaybeCreateWasmHoleWebMediaPlayer(
        blink::WebURL(GURL(url)), &client_,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*compositor_task_runner=*/nullptr);
  }

  std::unique_ptr<blink::WebMediaPlayer> CreateLoadedPlayer() {
    std::unique_ptr<blink::WebMediaPlayer> player = CreatePlayer(url_);
    player->Load(blink::WebMediaPlayer::kLoadTypeURL,
                 blink::WebMediaPlayerSource(blink::WebURL(GURL(url_))),
                 blink::WebMediaPlayer::kCorsModeUnspecified,
                 /*is_cache_disabled=*/false);
    task_environment_.RunUntilIdle();
    return player;
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockMediaPlayerClient> client_;
  const std::string url_ = "blob:https://example.com/id#sb-surface=" +
                           base::UnguessableToken::Create().ToString();
};

TEST_F(WasmHoleWebMediaPlayerTest, ParsesSurfaceFromBlobUrl) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  const std::string fragment = "#sb-surface=" + token.ToString();
  EXPECT_EQ(WasmSurfaceFromUrl(GURL("blob:https://example.com/id" + fragment)),
            token);
  EXPECT_FALSE(WasmSurfaceFromUrl(GURL("https://example.com/id" + fragment)));
  EXPECT_FALSE(WasmSurfaceFromUrl(GURL("blob:https://example.com/id")));
  EXPECT_FALSE(
      WasmSurfaceFromUrl(GURL("blob:https://example.com/id#sb-surface=xyz")));
  EXPECT_FALSE(WasmSurfaceFromUrl(
      GURL("blob:https://example.com/"
           "id#sb-surface=00000000000000000000000000000000")));
}

TEST_F(WasmHoleWebMediaPlayerTest, CreatesPlayerOnlyForSurfaceUrls) {
  EXPECT_FALSE(CreatePlayer("blob:https://example.com/id"));
  EXPECT_FALSE(CreatePlayer("https://example.com/id#sb-surface=" +
                            base::UnguessableToken::Create().ToString()));
  EXPECT_TRUE(CreatePlayer(url_));
}

TEST_F(WasmHoleWebMediaPlayerTest, LoadReachesHaveEnoughDataOnce) {
  std::unique_ptr<blink::WebMediaPlayer> player = CreatePlayer(url_);
  EXPECT_CALL(client_, SetCcLayer(NotNull()));
  EXPECT_CALL(client_, ReadyStateChanged());
  EXPECT_CALL(client_, NetworkStateChanged());
  EXPECT_EQ(player->Load(blink::WebMediaPlayer::kLoadTypeURL,
                         blink::WebMediaPlayerSource(blink::WebURL(GURL(url_))),
                         blink::WebMediaPlayer::kCorsModeUnspecified,
                         /*is_cache_disabled=*/false),
            blink::WebMediaPlayer::LoadTiming::kImmediate);
  EXPECT_EQ(player->GetNetworkState(),
            blink::WebMediaPlayer::kNetworkStateLoading);
  EXPECT_EQ(player->GetReadyState(),
            blink::WebMediaPlayer::kReadyStateHaveNothing);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(player->GetReadyState(),
            blink::WebMediaPlayer::kReadyStateHaveEnoughData);
  EXPECT_EQ(player->GetNetworkState(),
            blink::WebMediaPlayer::kNetworkStateIdle);
  EXPECT_TRUE(player->HasVideo());
  EXPECT_FALSE(player->HasAudio());
  EXPECT_TRUE(player->HasAvailableVideoFrame());
  testing::Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(WasmHoleWebMediaPlayerTest, ReportsFixedMediaState) {
  std::unique_ptr<blink::WebMediaPlayer> player = CreateLoadedPlayer();
  EXPECT_EQ(player->NaturalSize(), gfx::Size(1920, 1080));
  EXPECT_EQ(player->Duration(), std::numeric_limits<double>::infinity());
  EXPECT_TRUE(player->Seekable().empty());
  EXPECT_EQ(player->CurrentTime(), 0);
  EXPECT_TRUE(player->Paused());

  player->Play();
  player->Seek(5);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(player->Paused());
  EXPECT_EQ(player->CurrentTime(), 0);
  EXPECT_EQ(player->NaturalSize(), gfx::Size(1920, 1080));
}

TEST_F(WasmHoleWebMediaPlayerTest, DestructionDetachesLayer) {
  std::unique_ptr<blink::WebMediaPlayer> player = CreateLoadedPlayer();
  EXPECT_CALL(client_, SetCcLayer(IsNull()));
  player.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(WasmHoleWebMediaPlayerTest, DestructionBeforeLoadFinishes) {
  std::unique_ptr<blink::WebMediaPlayer> player = CreatePlayer(url_);
  player->Load(blink::WebMediaPlayer::kLoadTypeURL,
               blink::WebMediaPlayerSource(blink::WebURL(GURL(url_))),
               blink::WebMediaPlayer::kCorsModeUnspecified,
               /*is_cache_disabled=*/false);
  EXPECT_CALL(client_, SetCcLayer(IsNull()));
  EXPECT_CALL(client_, SetCcLayer(NotNull())).Times(0);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(0);
  player.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace content
