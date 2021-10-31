// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media_session/media_session.h"

#include <limits>
#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/bindings/testing/script_object_owner.h"
#include "cobalt/extension/media_session.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/wrappable.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::cobalt::script::CallbackResult;
using ::cobalt::script::ScriptValue;
using ::cobalt::script::Wrappable;
using ::cobalt::script::testing::FakeScriptValue;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace cobalt {
namespace media_session {
namespace {

class MockMediaSessionClient;

class MockCallbackFunction : public MediaSession::MediaSessionActionHandler {
 public:
  MOCK_CONST_METHOD1(
      Run, ReturnValue(const MediaSessionActionDetails& action_details));
};

class MockMediaSessionClient : public MediaSessionClient {
 public:
  explicit MockMediaSessionClient(MediaSession* media_session) :
      MediaSessionClient(media_session) {
  }

  void OnMediaSessionStateChanged(const MediaSessionState& session_state)
      override {
    session_state_ = session_state;
    ++session_change_count_;
    MediaSessionClient::OnMediaSessionStateChanged(session_state);
  }
  void WaitForSessionStateChange() {
    size_t current_change_count = session_change_count_;
    while (GetMediaSession()->IsChangeTaskQueuedForTesting()) {
      base::RunLoop().RunUntilIdle();
      if (current_change_count != session_change_count_) {
        break;
      }
      SbThreadSleep(kSbTimeMillisecond);
    }
  }
  MediaSessionState GetMediaSessionState() const { return session_state_; }
  size_t GetMediaSessionChangeCount() const { return session_change_count_; }
  MediaSessionState session_state_;
  size_t session_change_count_ = 0;
};

class MockMediaSession : public MediaSession {
 public:
  explicit MockMediaSession(MockMediaSessionClient* client)
      : MediaSession(client) {}

  MockMediaSessionClient* mock_session_client() {
    return static_cast<MockMediaSessionClient*>(media_session_client());
  }

  MOCK_CONST_METHOD0(GetMonotonicNow, SbTimeMonotonic());
};

MATCHER_P(SeekTime, time, "") {
  return arg.action() == kMediaSessionActionSeekto && arg.seek_time() == time;
}

MATCHER_P2(SeekOffset, action, offset, "") {
  return arg.action() == action && arg.seek_offset() == offset;
}

MATCHER_P(SeekNoOffset, action, "") {
  return arg.action() == action && !arg.has_seek_offset();
}

TEST(MediaSessionTest, MediaSessionTest) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  EXPECT_EQ(kMediaSessionPlaybackStateNone, session->playback_state());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  EXPECT_EQ(session->mock_session_client()->GetMediaSessionChangeCount(), 1);
}

TEST(MediaSessionTest, ActualPlaybackState) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  // Trigger a session state change without impacting playback state.
  session->set_metadata(new MediaMetadata);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(session->mock_session_client()->GetMediaSessionChangeCount(), 1);

  EXPECT_EQ(kMediaSessionPlaybackStateNone, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  session->mock_session_client()->UpdatePlatformPlaybackState(
      kCobaltExtensionMediaSessionPlaying);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  session->mock_session_client()->UpdatePlatformPlaybackState(
      kCobaltExtensionMediaSessionPaused);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStatePaused, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  session->set_playback_state(kMediaSessionPlaybackStateNone);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStateNone, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  session->mock_session_client()->UpdatePlatformPlaybackState(
      kCobaltExtensionMediaSessionNone);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(kMediaSessionPlaybackStateNone, session->mock_session_client()
      ->GetMediaSessionState().actual_playback_state());

  EXPECT_GE(session->mock_session_client()->GetMediaSessionChangeCount(), 2);
}

TEST(MediaSessionTest, NullActionClears) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  // Trigger a session state change without impacting playback state.
  session->set_metadata(new MediaMetadata);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(session->mock_session_client()->GetMediaSessionChangeCount(), 1);

  MediaSessionState state =
      session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStateNone, state.actual_playback_state());
  EXPECT_EQ(0, state.available_actions().to_ulong());

  MockCallbackFunction cf;
  EXPECT_CALL(cf, Run(_))
      .Times(1)
      .WillRepeatedly(Return(CallbackResult<void>()));
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);

  FakeScriptValue<MediaSession::MediaSessionActionHandler> null_holder(NULL);

  session->SetActionHandler(kMediaSessionActionPlay, holder);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(1, session->mock_session_client()
      ->GetMediaSessionState().available_actions().to_ulong());
  session->mock_session_client()->InvokeAction(
      kCobaltExtensionMediaSessionActionPlay);

  session->SetActionHandler(kMediaSessionActionPlay, null_holder);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(0, session->mock_session_client()
      ->GetMediaSessionState().available_actions().to_ulong());
  session->mock_session_client()->InvokeAction(
      kCobaltExtensionMediaSessionActionPlay);

  EXPECT_GE(session->mock_session_client()->GetMediaSessionChangeCount(), 3);
}

TEST(MediaSessionTest, AvailableActions) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  MediaSessionState state;

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  // Trigger a session state change without impacting playback state.
  session->set_metadata(new MediaMetadata);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(session->mock_session_client()->GetMediaSessionChangeCount(), 1);

  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStateNone, state.actual_playback_state());
  EXPECT_EQ(0, state.available_actions().to_ulong());

  MockCallbackFunction cf;
  EXPECT_CALL(cf, Run(_)).Times(0);
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);

  session->SetActionHandler(kMediaSessionActionPlay, holder);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(1 << kMediaSessionActionPlay,
            state.available_actions().to_ulong());

  session->SetActionHandler(kMediaSessionActionPause, holder);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(1 << kMediaSessionActionPlay,
            state.available_actions().to_ulong());

  session->SetActionHandler(kMediaSessionActionSeekto, holder);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(1 << kMediaSessionActionPlay, state.available_actions().to_ulong());

  session->mock_session_client()->UpdatePlatformPlaybackState(
      kCobaltExtensionMediaSessionPlaying);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());

  session->mock_session_client()->UpdatePlatformPlaybackState(
      kCobaltExtensionMediaSessionPaused);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStateNone);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStateNone, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPlay, state.available_actions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePaused, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPlay | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());

  session->mock_session_client()->UpdatePlatformPlaybackState(
      kCobaltExtensionMediaSessionNone);

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(kMediaSessionPlaybackStatePaused, state.actual_playback_state());
  EXPECT_EQ(1 << kMediaSessionActionPlay | 1 << kMediaSessionActionSeekto,
            state.available_actions().to_ulong());
}

TEST(MediaSessionTest, InvokeAction) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  MockCallbackFunction cf;
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);
  CobaltExtensionMediaSessionActionDetails details;

  session->SetActionHandler(kMediaSessionActionSeekto, holder);
  EXPECT_CALL(cf, Run(SeekTime(1.2))).WillOnce(Return(CallbackResult<void>()));

  CobaltExtensionMediaSessionActionDetailsInit(
      &details, kCobaltExtensionMediaSessionActionSeekto);
  details.seek_time = 1.2;
  session->mock_session_client()->InvokeAction(details);
}

TEST(MediaSessionTest, SeekDetails) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  MockCallbackFunction cf;
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);
  CobaltExtensionMediaSessionActionDetails details;

  session->SetActionHandler(kMediaSessionActionSeekto, holder);
  session->SetActionHandler(kMediaSessionActionSeekforward, holder);
  session->SetActionHandler(kMediaSessionActionSeekbackward, holder);

  EXPECT_CALL(cf, Run(SeekNoOffset(kMediaSessionActionSeekforward)))
      .WillOnce(Return(CallbackResult<void>()));
  session->mock_session_client()->InvokeAction(
      kCobaltExtensionMediaSessionActionSeekforward);

  EXPECT_CALL(cf, Run(SeekNoOffset(kMediaSessionActionSeekbackward)))
      .WillOnce(Return(CallbackResult<void>()));
  session->mock_session_client()->InvokeAction(
      kCobaltExtensionMediaSessionActionSeekbackward);

  EXPECT_CALL(cf, Run(SeekTime(1.2))).WillOnce(Return(CallbackResult<void>()));
  CobaltExtensionMediaSessionActionDetailsInit(
      &details, kCobaltExtensionMediaSessionActionSeekto);
  details.seek_time = 1.2;
  session->mock_session_client()->InvokeAction(details);

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekforward, 3.4)))
      .WillOnce(Return(CallbackResult<void>()));
  CobaltExtensionMediaSessionActionDetailsInit(
      &details, kCobaltExtensionMediaSessionActionSeekforward);
  details.seek_offset = 3.4;
  session->mock_session_client()->InvokeAction(details);

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekbackward, 5.6)))
      .WillOnce(Return(CallbackResult<void>()));
  CobaltExtensionMediaSessionActionDetailsInit(
      &details, kCobaltExtensionMediaSessionActionSeekbackward);
  details.seek_offset = 5.6;
  session->mock_session_client()->InvokeAction(details);

  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_GE(session->mock_session_client()->GetMediaSessionChangeCount(), 0);
}

TEST(MediaSessionTest, PositionState) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);

  MediaSessionState state;

  SbTimeMonotonic start_time = 1111111111;

  base::Optional<MediaPositionState> position_state;
  position_state.emplace();
  position_state->set_duration(100.0);
  position_state->set_position(10.0);

  // Trigger a session state change without impacting playback state.
  session->set_metadata(new MediaMetadata);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(session->mock_session_client()->GetMediaSessionChangeCount(), 1);

  // Position state not yet reported
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(0,
            state.GetCurrentPlaybackPosition(start_time + 999 * kSbTimeSecond));
  EXPECT_EQ(0, state.duration());
  EXPECT_EQ(0.0, state.actual_playback_rate());

  // Forward playback
  EXPECT_CALL(*session, GetMonotonicNow()).WillOnce(Return(start_time));
  position_state->set_playback_rate(1.0);
  session->SetPositionState(position_state);
  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ((10 + 50) * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 50 * kSbTimeSecond));
  EXPECT_EQ(100 * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 150 * kSbTimeSecond));
  EXPECT_EQ(100 * kSbTimeSecond, state.duration());
  EXPECT_EQ(1.0, state.actual_playback_rate());

  // Fast playback
  EXPECT_CALL(*session, GetMonotonicNow()).WillOnce(Return(start_time));
  position_state->set_playback_rate(2.0);
  session->SetPositionState(position_state);
  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ((10 + 2 * 20) * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 20 * kSbTimeSecond));
  EXPECT_EQ(100 * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 50 * kSbTimeSecond));
  EXPECT_EQ(100 * kSbTimeSecond, state.duration());
  EXPECT_EQ(2.0, state.actual_playback_rate());

  // Reverse playback
  EXPECT_CALL(*session, GetMonotonicNow()).WillOnce(Return(start_time));
  position_state->set_playback_rate(-1.0);
  session->SetPositionState(position_state);
  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(0 * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 20 * kSbTimeSecond));
  EXPECT_EQ((10 - 3) * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 3 * kSbTimeSecond));
  EXPECT_EQ(100 * kSbTimeSecond, state.duration());
  EXPECT_EQ(-1.0, state.actual_playback_rate());

  // Indefinite duration (live) playback
  EXPECT_CALL(*session, GetMonotonicNow()).WillOnce(Return(start_time));
  position_state->set_duration(std::numeric_limits<double>::infinity());
  position_state->set_playback_rate(1.0);
  session->SetPositionState(position_state);
  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(10 * kSbTimeSecond + 1 * kSbTimeDay,
            state.GetCurrentPlaybackPosition(start_time + 1 * kSbTimeDay));
  EXPECT_EQ(kSbTimeMax, state.duration());
  EXPECT_EQ(1.0, state.actual_playback_rate());

  // Paused playback
  // (Actual playback rate is 0.0, so position is the last reported position.
  //  The web app should update position and playback states together.)
  session->set_playback_state(kMediaSessionPlaybackStatePaused);
  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(10 * kSbTimeSecond,
            state.GetCurrentPlaybackPosition(start_time + 999 * kSbTimeSecond));
  EXPECT_EQ(kSbTimeMax, state.duration());
  EXPECT_EQ(0.0, state.actual_playback_rate());
  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  // Position state cleared
  EXPECT_CALL(*session, GetMonotonicNow()).WillOnce(Return(start_time));
  session->SetPositionState(base::nullopt);
  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  EXPECT_EQ(0,
            state.GetCurrentPlaybackPosition(start_time + 999 * kSbTimeSecond));
  EXPECT_EQ(0, state.duration());
  EXPECT_EQ(0.0, state.actual_playback_rate());

  EXPECT_GE(session->mock_session_client()->GetMediaSessionChangeCount(), 3);
}

TEST(MediaSessionTest, Metadata) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  scoped_refptr<MockMediaSession> session =
      scoped_refptr<MockMediaSession>(new MockMediaSession(
          new MockMediaSessionClient(nullptr)));
  session->media_session_client()->set_media_session(session);
  MediaSessionState state;

  MediaMetadataInit init_metadata;
  base::Optional<MediaMetadataInit> state_metadata;

  // Trigger a session state change without impacting metadata.
  session->set_playback_state(kMediaSessionPlaybackStateNone);
  session->mock_session_client()->WaitForSessionStateChange();
  EXPECT_EQ(session->mock_session_client()->GetMediaSessionChangeCount(), 1);

  // Metadata not yet set
  state = session->mock_session_client()->GetMediaSessionState();
  state_metadata = state.metadata();
  EXPECT_FALSE(state.has_metadata());
  EXPECT_FALSE(state_metadata.has_value());

  // Set metadata and make sure it gets into the MediaSessionState
  init_metadata = MediaMetadataInit();
  init_metadata.set_title("title");
  init_metadata.set_artist("artist");
  init_metadata.set_album("album");
  MediaImage art_image;
  art_image.set_src("http://art.image");
  script::Sequence<MediaImage> artwork;
  artwork.push_back(art_image);
  init_metadata.set_artwork(artwork);
  session->set_metadata(
      scoped_refptr<MediaMetadata>(new MediaMetadata(init_metadata)));

  session->mock_session_client()->WaitForSessionStateChange();
  state = session->mock_session_client()->GetMediaSessionState();
  state_metadata = state.metadata();
  EXPECT_TRUE(state.has_metadata());
  EXPECT_TRUE(state_metadata.has_value());
  EXPECT_EQ("title", state_metadata->title());
  EXPECT_EQ("artist", state_metadata->artist());
  EXPECT_EQ("album", state_metadata->album());
  EXPECT_EQ(1, state_metadata->artwork().size());
  EXPECT_EQ("http://art.image", state_metadata->artwork().at(0).src());

  EXPECT_GE(session->mock_session_client()->GetMediaSessionChangeCount(), 2);
}

}  // namespace
}  // namespace media_session
}  // namespace cobalt
