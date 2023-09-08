// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/player/web_media_player_impl.h"

#include "base/test/scoped_task_environment.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockWebMediaPlayerClient : public cobalt::media::WebMediaPlayerClient {
 public:
  MockWebMediaPlayerClient() = default;

  MockWebMediaPlayerClient(const MockWebMediaPlayerClient&) = delete;
  MockWebMediaPlayerClient(const MockWebMediaPlayerClient&&) = delete;
  MockWebMediaPlayerClient& operator=(const MockWebMediaPlayerClient&) = delete;
  MockWebMediaPlayerClient& operator=(const MockWebMediaPlayerClient&&) =
      delete;

  MOCK_METHOD0(NetworkStateChanged, void());
  MOCK_METHOD1(NetworkError, void(const std::string& message));
};

class WebMediaPlayerImplTest : public ::testing::Test {
 public:
  WebMediaPlayerImplTest() : media_thread_("MediaThreadForTest") {
    media_thread_.StartAndWaitForTesting();
  }

  typedef ::cobalt::media::WebMediaPlayerImpl WebMediaPlayerImpl;

 protected:
  void InitializeWebMediaPlayerImpl() {}
  // bool IsSuspended() { return wmpi_->pipeline_controller_->IsSuspended(); }
  bool IsSuspended() { return false; }

 protected:
  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
}
