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

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "media/base/media_util.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_video_renderer_sink.h"
#include "media/base/pipeline_status.h"
#include "media/base/starboard/starboard_renderer_config.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/starboard/starboard_renderer_client.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "media/mojo/services/starboard/starboard_renderer_wrapper.h"
#include "media/renderers/video_overlay_factory.h"
#include "media/starboard/mock_sbplayer_interface.h"
#include "media/starboard/sbplayer_interface.h"
#include "media/starboard/url_player_demuxer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "starboard/player.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace media {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

// Connects StarboardRendererClient to a StarboardRendererWrapper hosted by
// MojoRendererService over mojo pipes, as the renderer and GPU processes do.
// The wrapper's StarboardRenderer plays through a mock SbPlayer.
class UrlPlayerMetadataFlowTest : public testing::Test {
 protected:
  UrlPlayerMetadataFlowTest() {
    const auto task_runner = task_environment_.GetMainThreadTaskRunner();

    mojo::PendingRemote<mojom::Renderer> renderer_remote;
    mojo::PendingRemote<mojom::StarboardRendererExtension>
        renderer_extension_remote;
    mojo::PendingRemote<mojom::StarboardRendererClientExtension>
        client_extension_remote;
    auto client_extension_receiver =
        client_extension_remote.InitWithNewPipeAndPassReceiver();

    StarboardRendererTraits traits(
        task_runner, task_runner, media_log_.InitWithNewPipeAndPassRemote(),
        &video_geometry_setter_service_, base::UnguessableToken::Create(),
        base::Seconds(1), base::Seconds(1), std::string(), std::string(),
        StarboardRendererConfig::ExperimentalFeatures{}, gfx::Size(),
        renderer_extension_remote.InitWithNewPipeAndPassReceiver(),
        std::move(client_extension_remote), base::NullCallback());
    MojoRendererService::Create(
        /*mojo_cdm_service_context=*/nullptr,
        std::make_unique<StarboardRendererWrapper>(std::move(traits)),
        renderer_remote.InitWithNewPipeAndPassReceiver());

    client_ = std::make_unique<StarboardRendererClient>(
        task_runner, std::make_unique<NullMediaLog>(),
        std::make_unique<MojoRenderer>(task_runner,
                                       /*video_overlay_factory=*/nullptr,
                                       /*video_renderer_sink=*/nullptr,
                                       std::move(renderer_remote)),
        std::make_unique<VideoOverlayFactory>(), &video_renderer_sink_,
        std::move(renderer_extension_remote),
        std::move(client_extension_receiver),
        /*get_sb_window_handle_callback=*/
        base::BindRepeating([]() -> uint64_t { return 0; }),
        /*gpu_factories=*/nullptr);

    demuxer_.Initialize(&demuxer_host_, base::DoNothing());
  }

  ~UrlPlayerMetadataFlowTest() override {
    // Closes the pipes so that MojoRendererService destroys the wrapper, and
    // with it the SbPlayer, while the mock SbPlayer interface is alive.
    client_.reset();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockSbPlayerInterface> sbplayer_interface_;
  ScopedSbPlayerInterfaceForTesting scoped_sbplayer_interface_{
      &sbplayer_interface_};
  cobalt::media::VideoGeometrySetterService video_geometry_setter_service_;
  mojo::PendingReceiver<mojom::MediaLog> media_log_;
  StrictMock<MockDemuxerHost> demuxer_host_;
  UrlPlayerDemuxer demuxer_{task_environment_.GetMainThreadTaskRunner(),
                            GURL("https://example.com/master.m3u8")};
  NiceMock<MockVideoRendererSink> video_renderer_sink_;
  NiceMock<MockRendererClient> renderer_client_;
  StrictMock<base::MockOnceCallback<void(PipelineStatus)>> init_cb_;
  std::unique_ptr<StarboardRendererClient> client_;
};

TEST_F(UrlPlayerMetadataFlowTest, DemuxerHasDurationWhenInitCbRuns) {
  SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  SbPlayerStatusFunc player_status_cb = nullptr;
  void* context = nullptr;
  EXPECT_CALL(sbplayer_interface_, CreateUrlPlayer(_, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&player_status_cb), SaveArg<5>(&context),
                      Return(player)));

  client_->Initialize(&demuxer_, &renderer_client_, init_cb_.Get());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(player_status_cb);

  SbPlayerInfo info = {};
  info.duration = 0;
  EXPECT_CALL(sbplayer_interface_, GetInfo(player, _))
      .WillOnce(SetArgPointee<1>(info));
  {
    InSequence sequence;
    EXPECT_CALL(demuxer_host_, SetDuration(kInfiniteDuration));
    EXPECT_CALL(init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  }
  player_status_cb(player, context, kSbPlayerStateInitialized,
                   SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace media
