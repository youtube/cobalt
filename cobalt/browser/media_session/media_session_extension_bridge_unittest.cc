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

#include "cobalt/browser/media_session/media_session_extension_bridge.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "content/public/test/mock_media_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionImageType;
using media_session::mojom::MediaSessionInfo;
using SuspendType = media_session::mojom::MediaSession::SuspendType;
using ::testing::_;
using ::testing::NiceMock;

// Deep copy of the last CobaltExtensionMediaSessionState seen by the fake
// platform, since the extension's pointers are only valid during the call.
struct RecordedState {
  CobaltExtensionMediaSessionPlaybackState playback_state =
      kCobaltExtensionMediaSessionNone;
  bool available_actions[kCobaltExtensionMediaSessionActionNumActions] = {};
  std::string title;
  std::string artist;
  std::string album;
  std::vector<std::string> artwork_src;
  std::vector<std::string> artwork_sizes;
  bool has_position_state = false;
  int64_t duration = 0;
  int64_t position = 0;
  double playback_rate = 0;
};

// State of the fake platform. Starboard extensions are plain C structs of
// function pointers, so the fake records into globals.
struct FakePlatform {
  int state_changed_count = 0;
  RecordedState last_state;
  void* context = nullptr;
  CobaltExtensionMediaSessionInvokeActionCallback invoke_action = nullptr;
  CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
      update_playback_state = nullptr;
  int destroy_count = 0;
};
FakePlatform* g_platform = nullptr;

void FakeOnMediaSessionStateChanged(CobaltExtensionMediaSessionState state) {
  ++g_platform->state_changed_count;
  RecordedState& out = g_platform->last_state;
  out = RecordedState();
  out.playback_state = state.actual_playback_state;
  for (int i = 0; i < kCobaltExtensionMediaSessionActionNumActions; ++i) {
    out.available_actions[i] = state.available_actions[i];
  }
  if (state.metadata) {
    out.title = state.metadata->title;
    out.artist = state.metadata->artist;
    out.album = state.metadata->album;
    for (size_t i = 0; i < state.metadata->artwork_count; ++i) {
      out.artwork_src.push_back(state.metadata->artwork[i].src);
      out.artwork_sizes.push_back(state.metadata->artwork[i].size);
    }
  }
  out.has_position_state = state.has_position_state;
  out.duration = state.duration;
  out.position = state.current_playback_position;
  out.playback_rate = state.actual_playback_rate;
}

void FakeRegisterMediaSessionCallbacks(
    void* context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
        update_playback_state) {
  g_platform->context = context;
  g_platform->invoke_action = invoke_action;
  g_platform->update_playback_state = update_playback_state;
}

void FakeDestroyMediaSessionClientCallback() {
  ++g_platform->destroy_count;
  g_platform->context = nullptr;
  g_platform->invoke_action = nullptr;
  g_platform->update_playback_state = nullptr;
}

void FakeUpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState) {}

const CobaltExtensionMediaSessionApi kFakeExtension = {
    kCobaltExtensionMediaSessionName,
    1,
    &FakeOnMediaSessionStateChanged,
    &FakeRegisterMediaSessionCallbacks,
    &FakeDestroyMediaSessionClientCallback,
    &FakeUpdateActiveSessionPlatformPlaybackState,
};

// Bridge that controls a mock MediaSession instead of a WebContents' one.
class TestBridge : public MediaSessionExtensionBridge {
 public:
  explicit TestBridge(content::MediaSession* media_session)
      : MediaSessionExtensionBridge(&kFakeExtension, nullptr),
        media_session_(media_session) {
    Initialize();
  }

  using MediaSessionExtensionBridge::HandleAction;

 protected:
  content::MediaSession* GetMediaSession() override { return media_session_; }

 private:
  raw_ptr<content::MediaSession> media_session_;
};

CobaltExtensionMediaSessionActionDetails MakeAction(
    CobaltExtensionMediaSessionAction action) {
  CobaltExtensionMediaSessionActionDetails details;
  CobaltExtensionMediaSessionActionDetailsInit(&details, action);
  return details;
}

media_session::mojom::MediaSessionInfoPtr MakeInfo(
    MediaSessionInfo::SessionState state,
    MediaPlaybackState playback_state) {
  auto info = MediaSessionInfo::New();
  info->state = state;
  info->playback_state = playback_state;
  return info;
}

class MediaSessionExtensionBridgeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_platform = &platform_;
    ON_CALL(media_session_, AddObserver(_))
        .WillByDefault(
            [this](
                mojo::PendingRemote<media_session::mojom::MediaSessionObserver>
                    observer) { observer_ = std::move(observer); });
    bridge_ = std::make_unique<TestBridge>(&media_session_);
  }

  void TearDown() override {
    bridge_.reset();
    g_platform = nullptr;
  }

  // Simulates the platform invoking an action from its own thread context and
  // lets the posted task run.
  void InvokeFromPlatform(CobaltExtensionMediaSessionAction action) {
    ASSERT_TRUE(platform_.invoke_action);
    platform_.invoke_action(MakeAction(action), platform_.context);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  FakePlatform platform_;
  NiceMock<content::MockMediaSession> media_session_;
  mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer_;
  std::unique_ptr<TestBridge> bridge_;
};

TEST_F(MediaSessionExtensionBridgeTest, RegistersAndUnregistersWithPlatform) {
  EXPECT_EQ(platform_.context, bridge_.get());
  EXPECT_NE(platform_.invoke_action, nullptr);
  EXPECT_NE(platform_.update_playback_state, nullptr);

  bridge_.reset();
  EXPECT_EQ(platform_.destroy_count, 1);
  EXPECT_EQ(platform_.context, nullptr);
}

TEST_F(MediaSessionExtensionBridgeTest, NewBridgeTakesOverRegistration) {
  NiceMock<content::MockMediaSession> other_session;
  auto new_bridge = std::make_unique<TestBridge>(&other_session);
  EXPECT_EQ(platform_.context, new_bridge.get());

  // Destroying the old bridge must not tear down the new registration.
  bridge_.reset();
  EXPECT_EQ(platform_.destroy_count, 0);
  EXPECT_EQ(platform_.context, new_bridge.get());

  EXPECT_CALL(other_session, Suspend(SuspendType::kUI));
  EXPECT_CALL(media_session_, Suspend(_)).Times(0);
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionPause);

  new_bridge.reset();
  EXPECT_EQ(platform_.destroy_count, 1);
}

TEST_F(MediaSessionExtensionBridgeTest, ForwardsPlaybackState) {
  bridge_->MediaSessionInfoChanged(MakeInfo(
      MediaSessionInfo::SessionState::kActive, MediaPlaybackState::kPlaying));
  EXPECT_EQ(platform_.last_state.playback_state,
            kCobaltExtensionMediaSessionPlaying);
  EXPECT_FALSE(platform_.last_state.has_position_state);
  EXPECT_EQ(platform_.last_state.playback_rate, 1.0);

  bridge_->MediaSessionInfoChanged(MakeInfo(
      MediaSessionInfo::SessionState::kSuspended, MediaPlaybackState::kPaused));
  EXPECT_EQ(platform_.last_state.playback_state,
            kCobaltExtensionMediaSessionPaused);

  bridge_->MediaSessionInfoChanged(MakeInfo(
      MediaSessionInfo::SessionState::kInactive, MediaPlaybackState::kPaused));
  EXPECT_EQ(platform_.last_state.playback_state,
            kCobaltExtensionMediaSessionNone);
}

TEST_F(MediaSessionExtensionBridgeTest, ForwardsMetadataAndArtwork) {
  media_session::MediaMetadata metadata;
  metadata.title = u"Title";
  metadata.artist = u"Artist";
  metadata.album = u"Album";
  bridge_->MediaSessionMetadataChanged(metadata);

  media_session::MediaImage image;
  image.src = GURL("https://example.com/art.png");
  image.sizes = {gfx::Size(96, 96), gfx::Size(256, 256)};
  base::flat_map<MediaSessionImageType, std::vector<media_session::MediaImage>>
      images;
  images[MediaSessionImageType::kArtwork] = {image};
  bridge_->MediaSessionImagesChanged(images);

  EXPECT_EQ(platform_.last_state.title, "Title");
  EXPECT_EQ(platform_.last_state.artist, "Artist");
  EXPECT_EQ(platform_.last_state.album, "Album");
  ASSERT_EQ(platform_.last_state.artwork_src.size(), 1u);
  EXPECT_EQ(platform_.last_state.artwork_src[0], "https://example.com/art.png");
  EXPECT_EQ(platform_.last_state.artwork_sizes[0], "96x96 256x256");

  bridge_->MediaSessionMetadataChanged(std::nullopt);
  EXPECT_EQ(platform_.last_state.title, "");
}

TEST_F(MediaSessionExtensionBridgeTest, ForwardsAvailableActions) {
  bridge_->MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kSeekTo,
       MediaSessionAction::kEnterPictureInPicture});
  const auto& actions = platform_.last_state.available_actions;
  EXPECT_TRUE(actions[kCobaltExtensionMediaSessionActionPlay]);
  EXPECT_TRUE(actions[kCobaltExtensionMediaSessionActionSeekto]);
  EXPECT_FALSE(actions[kCobaltExtensionMediaSessionActionPause]);
  EXPECT_FALSE(actions[kCobaltExtensionMediaSessionActionNexttrack]);
}

TEST_F(MediaSessionExtensionBridgeTest, ForwardsPositionState) {
  bridge_->MediaSessionPositionChanged(media_session::MediaPosition(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(100),
      /*position=*/base::Seconds(42), /*end_of_media=*/false));
  EXPECT_TRUE(platform_.last_state.has_position_state);
  EXPECT_EQ(platform_.last_state.duration, base::Seconds(100).InMicroseconds());
  EXPECT_EQ(platform_.last_state.position, base::Seconds(42).InMicroseconds());
  EXPECT_EQ(platform_.last_state.playback_rate, 0.0);
}

TEST_F(MediaSessionExtensionBridgeTest, PlatformPlayPauseStop) {
  EXPECT_CALL(media_session_, Resume(SuspendType::kUI));
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionPlay);

  EXPECT_CALL(media_session_, Suspend(SuspendType::kUI));
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionPause);

  EXPECT_CALL(media_session_, DidReceiveAction(MediaSessionAction::kStop));
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionStop);
}

TEST_F(MediaSessionExtensionBridgeTest, PlatformTrackActions) {
  EXPECT_CALL(media_session_, NextTrack());
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionNexttrack);

  EXPECT_CALL(media_session_, PreviousTrack());
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionPrevioustrack);

  EXPECT_CALL(media_session_, SkipAd());
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionSkipad);
}

TEST_F(MediaSessionExtensionBridgeTest, SeekWithoutOffsetUsesDefault) {
  EXPECT_CALL(media_session_,
              Seek(MediaSessionExtensionBridge::kDefaultSeekOffset));
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionSeekforward);

  EXPECT_CALL(media_session_,
              Seek(-MediaSessionExtensionBridge::kDefaultSeekOffset));
  InvokeFromPlatform(kCobaltExtensionMediaSessionActionSeekbackward);
}

TEST_F(MediaSessionExtensionBridgeTest, SeekOffsetWithoutPositionUsesSeek) {
  auto details = MakeAction(kCobaltExtensionMediaSessionActionSeekforward);
  details.seek_offset = 30;
  EXPECT_CALL(media_session_, Seek(base::Seconds(30)));
  bridge_->HandleAction(details);
}

TEST_F(MediaSessionExtensionBridgeTest, SeekOffsetWithPositionSeeksTo) {
  bridge_->MediaSessionPositionChanged(media_session::MediaPosition(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(100),
      /*position=*/base::Seconds(50), /*end_of_media=*/false));

  auto forward = MakeAction(kCobaltExtensionMediaSessionActionSeekforward);
  forward.seek_offset = 30;
  EXPECT_CALL(media_session_, SeekTo(base::Seconds(80)));
  bridge_->HandleAction(forward);

  // Clamped to the duration.
  forward.seek_offset = 90;
  EXPECT_CALL(media_session_, SeekTo(base::Seconds(100)));
  bridge_->HandleAction(forward);

  // Clamped to zero.
  auto backward = MakeAction(kCobaltExtensionMediaSessionActionSeekbackward);
  backward.seek_offset = 90;
  EXPECT_CALL(media_session_, SeekTo(base::TimeDelta()));
  bridge_->HandleAction(backward);
}

TEST_F(MediaSessionExtensionBridgeTest, SeekTo) {
  auto details = MakeAction(kCobaltExtensionMediaSessionActionSeekto);
  details.seek_time = 12.5;
  EXPECT_CALL(media_session_, SeekTo(base::Seconds(12.5)));
  bridge_->HandleAction(details);

  details.fast_seek = true;
  EXPECT_CALL(media_session_, ScrubTo(base::Seconds(12.5)));
  bridge_->HandleAction(details);

  // A seekto without a time is ignored.
  details.seek_time = -1;
  EXPECT_CALL(media_session_, SeekTo(_)).Times(0);
  EXPECT_CALL(media_session_, ScrubTo(_)).Times(0);
  bridge_->HandleAction(details);
}

TEST_F(MediaSessionExtensionBridgeTest, ActionsAfterDestructionAreDropped) {
  auto invoke_action = platform_.invoke_action;
  void* context = platform_.context;
  bridge_.reset();

  // A platform that ignores DestroyMediaSessionClientCallback and keeps
  // invoking the old callback must not crash.
  invoke_action(MakeAction(kCobaltExtensionMediaSessionActionPause), context);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSessionExtensionBridgeTest, UpdatePlatformPlaybackStateIsNoOp) {
  ASSERT_TRUE(platform_.update_playback_state);
  int count = platform_.state_changed_count;
  platform_.update_playback_state(kCobaltExtensionMediaSessionPlaying,
                                  platform_.context);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(platform_.state_changed_count, count);
}

}  // namespace
}  // namespace cobalt
