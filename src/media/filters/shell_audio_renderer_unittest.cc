/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_audio_renderer.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace media {

class ShellAudioRendererTest : public ::testing::Test {
 public:
  ShellAudioRendererTest()
      : audio_decoder_config_(kCodecVorbis,
                              32,
                              CHANNEL_LAYOUT_MONO,
                              44100,
                              NULL,
                              0,
                              false),
        sink_(new NiceMock<MockAudioRendererSink>),
        demuxer_stream_(new NiceMock<MockDemuxerStream>),
        decoder_(new NiceMock<MockAudioDecoder>) {
    renderer_ = ShellAudioRenderer::Create(sink_, SetDecryptorReadyCB(),
                                           base::MessageLoopProxy::current());

    ON_CALL(*demuxer_stream_, type())
        .WillByDefault(Return(DemuxerStream::AUDIO));
    ON_CALL(*demuxer_stream_, audio_decoder_config())
        .WillByDefault(ReturnRef(audio_decoder_config_));

    ON_CALL(*decoder_, bits_per_channel()).WillByDefault(Return(32));
    ON_CALL(*decoder_, channel_layout())
        .WillByDefault(Return(CHANNEL_LAYOUT_MONO));
    ON_CALL(*decoder_, samples_per_second()).WillByDefault(Return(44100));
    ON_CALL(*decoder_, Reset(_)).WillByDefault(RunCallback<0>());
  }

  MOCK_METHOD1(OnPipelineStatus, void(PipelineStatus));
  MOCK_METHOD1(OnPipelineStatistics, void(const PipelineStatistics&));
  MOCK_METHOD0(OnUnderflow, void());
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD0(OnDisabled, void());
  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD0(OnStopped, void());

  void OnAudioTimeCallback(base::TimeDelta current_time,
                           base::TimeDelta max_time) {
    CHECK(current_time <= max_time);
  }

  void InitializeWithoutWaitForPendingTasks() { Initialize(false); }

  void InitializeAndWaitForPendingTasks() { Initialize(true); }

  void Preroll() {
    renderer_->Preroll(base::TimeDelta(),
                       base::Bind(&ShellAudioRendererTest::OnPipelineStatus,
                                  base::Unretained(this)));
  }

  void StopAndWaitForPendingTasks() {
    EXPECT_CALL(*this, OnStopped()).Times(1);
    renderer_->Stop(
        base::Bind(&ShellAudioRendererTest::OnStopped, base::Unretained(this)));
#if !defined(__LB_PS3__)
    // The extra Render call is required by the ShellAudioRendererImpl when the
    // renderer starts to running.
    scoped_ptr<AudioBus> audio_bus = AudioBus::Create(1, 1024, 4, false);
    renderer_->Render(audio_bus.get(), 0);
#endif  // !defined(__LB_PS3__)
    FinishPendingTasks();
  }

  void FinishPendingTasks() { message_loop_.RunUntilIdle(); }

 private:
  void Initialize(bool wait_until_finished) {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    if (wait_until_finished) {
      EXPECT_CALL(*this, OnPipelineStatus(_)).Times(1);
    } else {
      EXPECT_CALL(*this, OnPipelineStatus(_)).Times(AtMost(1));
    }
    AudioRenderer::AudioDecoderList decoders;
    decoders.push_back(decoder_);
    renderer_->Initialize(
        demuxer_stream_, decoders,
        base::Bind(&ShellAudioRendererTest::OnPipelineStatus,
                   base::Unretained(this)),
        base::Bind(&ShellAudioRendererTest::OnPipelineStatistics,
                   base::Unretained(this)),
        base::Bind(&ShellAudioRendererTest::OnUnderflow,
                   base::Unretained(this)),
        base::Bind(&ShellAudioRendererTest::OnAudioTimeCallback,
                   base::Unretained(this)),
        base::Bind(&ShellAudioRendererTest::OnEnded, base::Unretained(this)),
        base::Bind(&ShellAudioRendererTest::OnDisabled, base::Unretained(this)),
        base::Bind(&ShellAudioRendererTest::OnError, base::Unretained(this)));
    if (wait_until_finished) {
      FinishPendingTasks();
    }
  }

  AudioDecoderConfig audio_decoder_config_;
  scoped_refptr<NiceMock<MockAudioRendererSink> > sink_;
  scoped_refptr<NiceMock<MockDemuxerStream> > demuxer_stream_;
  scoped_refptr<NiceMock<MockAudioDecoder> > decoder_;
  scoped_refptr<ShellAudioRenderer> renderer_;
  MessageLoop message_loop_;
};

TEST_F(ShellAudioRendererTest, Initialize) {
  InSequence in_sequence;
  InitializeAndWaitForPendingTasks();
  StopAndWaitForPendingTasks();
}

TEST_F(ShellAudioRendererTest, StopImmediatelyAfterInitialize) {
  // Not using InSequence here as the audio renderer may call init_cb after
  // stop_cb is called.
  InitializeWithoutWaitForPendingTasks();
  StopAndWaitForPendingTasks();
}

TEST_F(ShellAudioRendererTest, Preroll) {
  InSequence in_sequence;
  InitializeAndWaitForPendingTasks();
  Preroll();
  FinishPendingTasks();
  StopAndWaitForPendingTasks();
}

TEST_F(ShellAudioRendererTest, StopImmediatelyAfterPreroll) {
  InSequence in_sequence;
  InitializeAndWaitForPendingTasks();
  Preroll();
  StopAndWaitForPendingTasks();
}

}  // namespace media
