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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"
#include "base/run_loop.h"

using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;

namespace cobalt {
namespace media_session {

class MockMediaSessionClient : public MediaSessionClient {
 public:
  MOCK_METHOD0(OnMediaSessionChanged, void());
};

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

}  // namespace media_session
}  // namespace cobalt
