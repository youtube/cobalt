// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/script_object_owner.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/wrappable.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"
#include "base/run_loop.h"

using ::cobalt::script::testing::FakeScriptValue;
using ::cobalt::script::CallbackResult;
using ::cobalt::script::ScriptValue;
using ::cobalt::script::Wrappable;

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::_;

namespace cobalt {
namespace media_session {
namespace {

class MockCallbackFunction : public MediaSession::MediaSessionActionHandler {
 public:
  MOCK_CONST_METHOD1(Run, ReturnValue(
      const scoped_refptr<MediaSessionActionDetails>& action_details));
};

class MockMediaSessionClient : public MediaSessionClient {
 public:
  MOCK_METHOD0(OnMediaSessionChanged, void());
};

MATCHER_P(SeekTime, time, "") {
  return arg->action() ==  kMediaSessionActionSeek
      && arg->seek_time() == time;
}

MATCHER_P2(SeekOffset, action, offset, "") {
  return arg->action() ==  action
      && arg->seek_offset() == offset;
}

TEST(MediaSessionTest, MediaSessionTest) {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  MockMediaSessionClient client;

  ON_CALL(client, OnMediaSessionChanged())
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(client, OnMediaSessionChanged()).Times(1);

  scoped_refptr<MediaSession> session = client.GetMediaSession();

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());

  run_loop.Run();
}

TEST(MediaSessionTest, GetActualPlaybackState) {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  MockMediaSessionClient client;

  ON_CALL(client, OnMediaSessionChanged())
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(client, OnMediaSessionChanged()).Times(AtLeast(2));

  scoped_refptr<MediaSession> session = client.GetMediaSession();

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePaused, client.GetActualPlaybackState());

  session->set_playback_state(kMediaSessionPlaybackStateNone);

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStateNone);

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());

  run_loop.Run();
}

TEST(MediaSessionTest, NullActionClears) {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  MockMediaSessionClient client;

  ON_CALL(client, OnMediaSessionChanged())
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(client, OnMediaSessionChanged()).Times(AtLeast(0));

  scoped_refptr<MediaSession> session = client.GetMediaSession();

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());
  EXPECT_EQ(0, client.GetAvailableActions().to_ulong());

  MockCallbackFunction cf;
  EXPECT_CALL(cf, Run(_))
      .Times(1)
      .WillRepeatedly(Return(CallbackResult<void>()));
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);

  FakeScriptValue<MediaSession::MediaSessionActionHandler> null_holder(NULL);

  session->SetActionHandler(kMediaSessionActionPlay, holder);
  EXPECT_EQ(1, client.GetAvailableActions().to_ulong());
  client.InvokeAction(kMediaSessionActionPlay);

  session->SetActionHandler(kMediaSessionActionPlay, null_holder);
  EXPECT_EQ(0, client.GetAvailableActions().to_ulong());
  client.InvokeAction(kMediaSessionActionPlay);
}

TEST(MediaSessionTest, GetAvailableActions) {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  MockMediaSessionClient client;

  ON_CALL(client, OnMediaSessionChanged())
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(client, OnMediaSessionChanged()).Times(AtLeast(0));

  scoped_refptr<MediaSession> session = client.GetMediaSession();

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());
  EXPECT_EQ(0, client.GetAvailableActions().to_ulong());

  MockCallbackFunction cf;
  EXPECT_CALL(cf, Run(_)).Times(0);
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);

  session->SetActionHandler(kMediaSessionActionPlay, holder);

  EXPECT_EQ(1, client.GetAvailableActions().to_ulong());

  session->SetActionHandler(kMediaSessionActionPause, holder);

  EXPECT_EQ(1, client.GetAvailableActions().to_ulong());

  session->SetActionHandler(kMediaSessionActionSeek, holder);

  EXPECT_EQ(5, client.GetAvailableActions().to_ulong());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(6, client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(6, client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(6, client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(6, client.GetAvailableActions().to_ulong());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(6, client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStateNone);

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());
  EXPECT_EQ(5, client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePaused, client.GetActualPlaybackState());
  EXPECT_EQ(5, client.GetAvailableActions().to_ulong());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStateNone);

  EXPECT_EQ(kMediaSessionPlaybackStatePaused, client.GetActualPlaybackState());
  EXPECT_EQ(5, client.GetAvailableActions().to_ulong());

  run_loop.Run();
}

TEST(MediaSessionTest, SeekDetails) {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  MockMediaSessionClient client;

  ON_CALL(client, OnMediaSessionChanged())
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(client, OnMediaSessionChanged()).Times(AtLeast(0));

  scoped_refptr<MediaSession> session = client.GetMediaSession();

  MockCallbackFunction cf;
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);

  session->SetActionHandler(kMediaSessionActionSeek, holder);
  session->SetActionHandler(kMediaSessionActionSeekforward, holder);
  session->SetActionHandler(kMediaSessionActionSeekbackward, holder);

  EXPECT_CALL(cf, Run(SeekTime(0.0)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(kMediaSessionActionSeek);

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekforward, 0.0)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(kMediaSessionActionSeekforward);

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekbackward, 0.0)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(kMediaSessionActionSeekbackward);

  EXPECT_CALL(cf, Run(SeekTime(1.2)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(scoped_ptr<MediaSessionActionDetails::Data>(
      new MediaSessionActionDetails::Data(kMediaSessionActionSeek, 1.2)));

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekforward, 3.4)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(scoped_ptr<MediaSessionActionDetails::Data>(
      new MediaSessionActionDetails::Data(
          kMediaSessionActionSeekforward, 3.4)));

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekbackward, 5.6)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(scoped_ptr<MediaSessionActionDetails::Data>(
      new MediaSessionActionDetails::Data(
          kMediaSessionActionSeekbackward, 5.6)));
}

}  // namespace
}  // namespace media_session
}  // namespace cobalt
