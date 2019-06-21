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

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/bindings/testing/script_object_owner.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/wrappable.h"
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

class MockCallbackFunction : public MediaSession::MediaSessionActionHandler {
 public:
  MOCK_CONST_METHOD1(
      Run, ReturnValue(const MediaSessionActionDetails& action_details));
};

class MockMediaSessionClient : public MediaSessionClient {
 public:
  MOCK_METHOD0(OnMediaSessionChanged, void());
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
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
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
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
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
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
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

  EXPECT_EQ(1 << kMediaSessionActionPlay,
            client.GetAvailableActions().to_ulong());

  session->SetActionHandler(kMediaSessionActionPause, holder);

  EXPECT_EQ(1 << kMediaSessionActionPlay,
            client.GetAvailableActions().to_ulong());

  session->SetActionHandler(kMediaSessionActionSeekto, holder);

  EXPECT_EQ(1 << kMediaSessionActionPlay | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePlaying);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePlaying, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPause | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStateNone);

  EXPECT_EQ(kMediaSessionPlaybackStateNone, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPlay | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  session->set_playback_state(kMediaSessionPlaybackStatePaused);

  EXPECT_EQ(kMediaSessionPlaybackStatePaused, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPlay | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  client.UpdatePlatformPlaybackState(kMediaSessionPlaybackStateNone);

  EXPECT_EQ(kMediaSessionPlaybackStatePaused, client.GetActualPlaybackState());
  EXPECT_EQ(1 << kMediaSessionActionPlay | 1 << kMediaSessionActionSeekto,
            client.GetAvailableActions().to_ulong());

  run_loop.Run();
}

TEST(MediaSessionTest, SeekDetails) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  MockMediaSessionClient client;

  ON_CALL(client, OnMediaSessionChanged())
      .WillByDefault(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(client, OnMediaSessionChanged()).Times(AtLeast(0));

  scoped_refptr<MediaSession> session = client.GetMediaSession();

  MockCallbackFunction cf;
  FakeScriptValue<MediaSession::MediaSessionActionHandler> holder(&cf);
  std::unique_ptr<MediaSessionActionDetails> details;

  session->SetActionHandler(kMediaSessionActionSeekto, holder);
  session->SetActionHandler(kMediaSessionActionSeekforward, holder);
  session->SetActionHandler(kMediaSessionActionSeekbackward, holder);

  EXPECT_CALL(cf, Run(SeekNoOffset(kMediaSessionActionSeekforward)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(kMediaSessionActionSeekforward);

  EXPECT_CALL(cf, Run(SeekNoOffset(kMediaSessionActionSeekbackward)))
      .WillOnce(Return(CallbackResult<void>()));
  client.InvokeAction(kMediaSessionActionSeekbackward);

  EXPECT_CALL(cf, Run(SeekTime(1.2))).WillOnce(Return(CallbackResult<void>()));
  details.reset(new MediaSessionActionDetails());
  details->set_action(kMediaSessionActionSeekto);
  details->set_seek_time(1.2);
  client.InvokeAction(std::move(details));

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekforward, 3.4)))
      .WillOnce(Return(CallbackResult<void>()));
  details.reset(new MediaSessionActionDetails());
  details->set_action(kMediaSessionActionSeekforward);
  details->set_seek_offset(3.4);
  client.InvokeAction(std::move(details));

  EXPECT_CALL(cf, Run(SeekOffset(kMediaSessionActionSeekbackward, 5.6)))
      .WillOnce(Return(CallbackResult<void>()));
  details.reset(new MediaSessionActionDetails());
  details->set_action(kMediaSessionActionSeekbackward);
  details->set_seek_offset(5.6);
  client.InvokeAction(std::move(details));
}

}  // namespace
}  // namespace media_session
}  // namespace cobalt
