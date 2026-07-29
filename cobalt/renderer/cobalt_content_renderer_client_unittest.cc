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

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace
}  // namespace cobalt
