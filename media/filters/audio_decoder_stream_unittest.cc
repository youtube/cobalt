// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/filters/decoder_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace media {

MATCHER(IsRegularDecoderBuffer, "") {
  return !arg->end_of_stream();
}

MATCHER(IsEOSDecoderBuffer, "") {
  return arg->end_of_stream();
}

static void OnAudioDecoderStreamInitialized(base::OnceClosure closure,
                                            bool success) {
  ASSERT_TRUE(success);
  std::move(closure).Run();
}

class AudioDecoderStreamTest : public testing::Test {
 public:
  AudioDecoderStreamTest()
      : audio_decoder_stream_(
            std::make_unique<AudioDecoderStream::StreamTraits>(
                &media_log_,
                CHANNEL_LAYOUT_STEREO,
                kSampleFormatPlanarF32),
            task_environment_.GetMainThreadTaskRunner(),
            base::BindRepeating(&AudioDecoderStreamTest::CreateMockAudioDecoder,
                                base::Unretained(this)),
            &media_log_) {
    // Any valid config will do.
    demuxer_stream_.set_audio_decoder_config({AudioCodec::kAAC,
                                              kSampleFormatS16,
                                              CHANNEL_LAYOUT_STEREO,
                                              44100,
                                              {},
                                              {}});
    EXPECT_CALL(demuxer_stream_, SupportsConfigChanges())
        .WillRepeatedly(Return(true));

    base::RunLoop run_loop;
    audio_decoder_stream_.Initialize(
        &demuxer_stream_,
        base::BindOnce(&OnAudioDecoderStreamInitialized,
                       run_loop.QuitClosure()),
        nullptr, base::DoNothing(), base::DoNothing());
    run_loop.Run();
  }

  AudioDecoderStreamTest(const AudioDecoderStreamTest&) = delete;
  AudioDecoderStreamTest& operator=(const AudioDecoderStreamTest&) = delete;

  MockDemuxerStream* demuxer_stream() { return &demuxer_stream_; }
  MockAudioDecoder* decoder() { return decoder_; }

  void ReadAudioBuffer(base::OnceClosure closure) {
    audio_decoder_stream_.Read(
        base::BindOnce(&AudioDecoderStreamTest::OnAudioBufferReadDone,
                       base::Unretained(this), std::move(closure)));
  }

  void ProduceDecoderOutput(scoped_refptr<DecoderBuffer> buffer,
                            AudioDecoder::DecodeCB decode_cb) {
    // Make sure successive AudioBuffers have increasing timestamps.
    last_timestamp_ += base::Milliseconds(27);
    const auto& config = demuxer_stream_.audio_decoder_config();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            decoder_output_cb_,
            AudioBuffer::CreateEmptyBuffer(
                config.channel_layout(), config.channels(),
                config.samples_per_second(), 1221, last_timestamp_)));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kOk));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  std::vector<std::unique_ptr<AudioDecoder>> CreateMockAudioDecoder() {
    auto decoder = std::make_unique<MockAudioDecoder>();
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(DoAll(SaveArg<3>(&decoder_output_cb_),
                              RunOnceCallback<2>(DecoderStatus::Codes::kOk)));
    decoder_ = decoder.get();

    std::vector<std::unique_ptr<AudioDecoder>> result;
    result.push_back(std::move(decoder));
    return result;
  }

  void OnAudioBufferReadDone(base::OnceClosure closure,
                             AudioDecoderStream::ReadResult result) {
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;
  testing::NiceMock<MockDemuxerStream> demuxer_stream_{DemuxerStream::AUDIO};
  AudioDecoderStream audio_decoder_stream_;

  raw_ptr<MockAudioDecoder> decoder_ = nullptr;
  AudioDecoder::OutputCB decoder_output_cb_;
  base::TimeDelta last_timestamp_;
};

TEST_F(AudioDecoderStreamTest, FlushOnConfigChange) {
  MockAudioDecoder* first_decoder = decoder();
  ASSERT_NE(first_decoder, nullptr);

  // Make a regular DemuxerStream::Read().
  scoped_refptr<DecoderBuffer> buffer = base::MakeRefCounted<DecoderBuffer>(12);
  DemuxerStream::DecoderBufferVector buffers;
  buffers.emplace_back(buffer);
  EXPECT_CALL(*demuxer_stream(), OnRead(_))
      .WillOnce(RunOnceCallback<0>(DemuxerStream::kOk, buffers));
  EXPECT_CALL(*decoder(), Decode(IsRegularDecoderBuffer(), _))
      .WillOnce(Invoke(this, &AudioDecoderStreamTest::ProduceDecoderOutput));
  base::RunLoop run_loop0;
  ReadAudioBuffer(run_loop0.QuitClosure());
  run_loop0.Run();

  // Make a config-change DemuxerStream::Read().
  // Expect the decoder to be flushed.  Upon flushing, the decoder releases
  // internally buffered output.
  EXPECT_CALL(*demuxer_stream(), OnRead(_))
      .WillOnce(RunOnceCallback<0>(DemuxerStream::kConfigChanged,
                                   DemuxerStream::DecoderBufferVector()));
  EXPECT_CALL(*decoder(), Decode(IsEOSDecoderBuffer(), _))
      .WillOnce(Invoke(this, &AudioDecoderStreamTest::ProduceDecoderOutput));
  base::RunLoop run_loop1;
  ReadAudioBuffer(run_loop1.QuitClosure());
  run_loop1.Run();

  // Expect the decoder to be re-initialized when AudioDecoderStream finishes
  // processing the last decode.
  EXPECT_CALL(*decoder(), Initialize_(_, _, _, _, _));
  RunUntilIdle();
}

}  // namespace media
