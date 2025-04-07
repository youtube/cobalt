// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.  // You may
// obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "media/starboard/starboard_renderer.h"

#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "media/base/mock_media_log.h"
#include "media/base/mock_video_renderer_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using ::testing::StrictMock;

TEST(StarboardRenderer, Create) {
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner(
      new base::TestSimpleTaskRunner());
  StrictMock<MockVideoRendererSink> video_sink;
  StrictMock<MockMediaLog> media_log;

  BindHostReceiverCallback cb =
      base::BindLambdaForTesting([](mojo::GenericPendingReceiver receiver) {});

  StarboardRenderer render(media_task_runner, &video_sink, &media_log,
                           std::make_unique<VideoOverlayFactory>(),
                           /*audio_write_duration_local=*/base::TimeDelta(),
                           /*audio_write_duration_remote=*/base::TimeDelta(),
                           /*bind_host_receiver_callback=*/cb);
}

}  // namespace
}  // namespace media
