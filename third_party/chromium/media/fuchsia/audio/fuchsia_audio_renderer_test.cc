// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_renderer.h"

#include <fuchsia/media/audio/cpp/fidl_test_base.h>
#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include "base/containers/queue.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer_client.h"
#include "media/fuchsia/cdm/fuchsia_cdm_context.h"
#include "media/fuchsia/common/passthrough_sysmem_buffer_stream.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

namespace {

constexpr int kDefaultSampleRate = 48000;
constexpr base::TimeDelta kPacketDuration = base::Milliseconds(20);
constexpr base::TimeDelta kMinLeadTime = base::Milliseconds(100);
constexpr base::TimeDelta kMaxLeadTime = base::Milliseconds(500);
const base::TimeDelta kTimeStep = base::Milliseconds(2);

class TestDemuxerStream : public DemuxerStream {
 public:
  struct ReadResult {
    explicit ReadResult(scoped_refptr<DecoderBuffer> buffer) : buffer(buffer) {}
    explicit ReadResult(const AudioDecoderConfig& config) : config(config) {}

    Status status;
    absl::optional<AudioDecoderConfig> config;
    scoped_refptr<DecoderBuffer> buffer;
  };

  explicit TestDemuxerStream(const AudioDecoderConfig& config)
      : config_(config) {}

  ~TestDemuxerStream() override {}

  void QueueReadResult(ReadResult read_result) {
    CHECK(read_result.config.has_value() == !read_result.buffer);
    read_queue_.push(std::move(read_result));
    SatisfyRead();
  }

  void DiscardQueueAndAbortRead() {
    while (!read_queue_.empty())
      read_queue_.pop();
    if (read_cb_)
      std::move(read_cb_).Run(kAborted, nullptr);
  }

  bool is_read_pending() const { return !!read_cb_; }

  // DemuxerStream implementation.
  void Read(ReadCB read_cb) override {
    read_cb_ = std::move(read_cb);
    SatisfyRead();
  }
  AudioDecoderConfig audio_decoder_config() override { return config_; }
  VideoDecoderConfig video_decoder_config() override {
    NOTREACHED();
    return VideoDecoderConfig();
  }
  Type type() const override { return AUDIO; }
  Liveness liveness() const override { return LIVENESS_RECORDED; }
  bool SupportsConfigChanges() override { return true; }

 private:
  void SatisfyRead() {
    if (read_queue_.empty() || !read_cb_)
      return;

    auto result = std::move(read_queue_.front());
    read_queue_.pop();

    Status status;
    if (result.config) {
      config_ = result.config.value();
      status = kConfigChanged;
    } else {
      status = kOk;
    }

    std::move(read_cb_).Run(status, result.buffer);
  }

  AudioDecoderConfig config_;
  ReadCB read_cb_;

  base::queue<ReadResult> read_queue_;
};

class TestStreamSink : public fuchsia::media::testing::StreamSink_TestBase {
 public:
  TestStreamSink(
      std::vector<zx::vmo> buffers,
      fuchsia::media::AudioStreamType stream_type,
      std::unique_ptr<fuchsia::media::Compression> compression,
      fidl::InterfaceRequest<fuchsia::media::StreamSink> stream_sink_request)
      : binding_(this, std::move(stream_sink_request)),
        buffers_(std::move(buffers)),
        stream_type_(std::move(stream_type)),
        compression_(std::move(compression)) {}

  const std::vector<zx::vmo>& buffers() const { return buffers_; }
  const fuchsia::media::AudioStreamType& stream_type() const {
    return stream_type_;
  }
  const fuchsia::media::Compression* compression() const {
    return compression_.get();
  }

  std::vector<fuchsia::media::StreamPacket>* received_packets() {
    return &received_packets_;
  }

  std::vector<fuchsia::media::StreamPacket>* discarded_packets() {
    return &discarded_packets_;
  }

  bool received_end_of_stream() const { return received_end_of_stream_; }

  // fuchsia::media::StreamSink overrides.
  void SendPacket(fuchsia::media::StreamPacket packet,
                  SendPacketCallback callback) override {
    EXPECT_FALSE(received_end_of_stream_);
    received_packets_.push_back(std::move(packet));
    callback();
  }
  void EndOfStream() override { received_end_of_stream_ = true; }
  void DiscardAllPackets(DiscardAllPacketsCallback callback) override {
    DiscardAllPacketsNoReply();
    callback();
  }
  void DiscardAllPacketsNoReply() override {
    std::move(std::begin(received_packets_), std::end(received_packets_),
              std::back_inserter(discarded_packets_));
    received_packets_.clear();
  }

  // Other methods are not expected to be called.
  void NotImplemented_(const std::string& name) final {
    FAIL() << ": " << name;
  }

 private:
  fidl::Binding<fuchsia::media::StreamSink> binding_;

  std::vector<zx::vmo> buffers_;
  fuchsia::media::AudioStreamType stream_type_;
  std::unique_ptr<fuchsia::media::Compression> compression_;

  std::vector<fuchsia::media::StreamPacket> received_packets_;
  std::vector<fuchsia::media::StreamPacket> discarded_packets_;

  bool received_end_of_stream_ = false;
};

class TestAudioConsumer
    : public fuchsia::media::testing::AudioConsumer_TestBase,
      public fuchsia::media::audio::testing::VolumeControl_TestBase {
 public:
  explicit TestAudioConsumer(
      fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request)
      : binding_(this, std::move(request)), volume_control_binding_(this) {}

  std::unique_ptr<TestStreamSink> TakeStreamSink() {
    return std::move(stream_sink_);
  }

  std::unique_ptr<TestStreamSink> WaitStreamSinkConnected() {
    if (!stream_sink_) {
      base::RunLoop run_loop;
      wait_stream_sink_created_loop_ = &run_loop;
      run_loop.Run();
      wait_stream_sink_created_loop_ = nullptr;
    }
    EXPECT_TRUE(stream_sink_);
    return TakeStreamSink();
  }

  void WaitStarted() {
    if (started_)
      return;

    base::RunLoop run_loop;
    wait_started_loop_ = &run_loop;
    run_loop.Run();
    wait_started_loop_ = nullptr;
    EXPECT_TRUE(started_);
  }

  void UpdateStatus(absl::optional<base::TimeTicks> reference_time,
                    absl::optional<base::TimeDelta> media_time) {
    fuchsia::media::AudioConsumerStatus status;
    if (reference_time) {
      CHECK(media_time);

      fuchsia::media::TimelineFunction timeline;
      timeline.reference_time = reference_time->ToZxTime();
      timeline.subject_time = media_time->ToZxDuration();
      timeline.reference_delta = 1000;
      timeline.subject_delta = static_cast<int>(playback_rate_ * 1000.0);
      status.set_presentation_timeline(std::move(timeline));
    }

    status.set_min_lead_time(kMinLeadTime.ToZxDuration());
    status.set_max_lead_time(kMaxLeadTime.ToZxDuration());
    status_update_ = std::move(status);

    if (status_callback_)
      CallStatusCallback();
  }

  void SignalEndOfStream() { binding_.events().OnEndOfStream(); }

  bool started() const { return started_; }
  base::TimeDelta start_media_time() const { return start_media_time_; }
  float playback_rate() const { return playback_rate_; }
  float volume() const { return volume_; }

  // fuchsia::media::AudioConsumer overrides.
  void CreateStreamSink(
      std::vector<zx::vmo> buffers,
      fuchsia::media::AudioStreamType stream_type,
      std::unique_ptr<fuchsia::media::Compression> compression,
      fidl::InterfaceRequest<fuchsia::media::StreamSink> stream_sink_request)
      override {
    create_stream_sink_called_ = true;
    stream_sink_ = std::make_unique<TestStreamSink>(
        std::move(buffers), std::move(stream_type), std::move(compression),
        std::move(stream_sink_request));
    if (wait_stream_sink_created_loop_)
      wait_stream_sink_created_loop_->Quit();
  }

  void Start(fuchsia::media::AudioConsumerStartFlags flags,
             int64_t reference_time,
             int64_t media_time) override {
    EXPECT_TRUE(create_stream_sink_called_);
    EXPECT_FALSE(started_);
    EXPECT_EQ(reference_time, fuchsia::media::NO_TIMESTAMP);
    started_ = true;
    start_media_time_ = base::TimeDelta::FromZxDuration(media_time);
    if (wait_started_loop_)
      wait_started_loop_->Quit();
  }

  void Stop() override {
    EXPECT_TRUE(started_);
    started_ = false;
  }

  void SetRate(float rate) override { playback_rate_ = rate; }

  void BindVolumeControl(
      fidl::InterfaceRequest<fuchsia::media::audio::VolumeControl>
          volume_control_request) override {
    volume_control_binding_.Bind(std::move(volume_control_request));
  }

  void WatchStatus(WatchStatusCallback callback) override {
    EXPECT_FALSE(!!status_callback_);

    status_callback_ = std::move(callback);

    if (status_update_) {
      CallStatusCallback();
    }
  }

  // fuchsia::media::audio::VolumeControl overrides.
  void SetVolume(float volume) override { volume_ = volume; }

  // Other methods are not expected to be called.
  void NotImplemented_(const std::string& name) final {
    FAIL() << ": " << name;
  }

 private:
  void CallStatusCallback() {
    EXPECT_TRUE(status_callback_);
    EXPECT_TRUE(status_update_);

    std::move(status_callback_)(std::move(status_update_.value()));
    status_callback_ = {};
    status_update_ = absl::nullopt;
  }

  fidl::Binding<fuchsia::media::AudioConsumer> binding_;
  fidl::Binding<fuchsia::media::audio::VolumeControl> volume_control_binding_;
  std::unique_ptr<TestStreamSink> stream_sink_;

  base::RunLoop* wait_stream_sink_created_loop_ = nullptr;
  base::RunLoop* wait_started_loop_ = nullptr;

  bool create_stream_sink_called_ = false;

  WatchStatusCallback status_callback_;
  absl::optional<fuchsia::media::AudioConsumerStatus> status_update_;

  bool started_ = false;
  base::TimeDelta start_media_time_;

  float playback_rate_ = 1.0;

  float volume_ = 1.0;
};

class TestRendererClient : public RendererClient {
 public:
  TestRendererClient() = default;
  ~TestRendererClient() {
    EXPECT_EQ(expected_error_, PIPELINE_OK);
    EXPECT_FALSE(expect_eos_);
  }

  void ExpectError(PipelineStatus expected_error) {
    EXPECT_EQ(expected_error_, PIPELINE_OK);
    expected_error_ = expected_error;
  }

  void ExpectEos() {
    EXPECT_FALSE(expect_eos_);
    expect_eos_ = true;
  }

  BufferingState buffering_state() const { return buffering_state_; }

  absl::optional<AudioDecoderConfig> last_config_change() const {
    return last_config_change_;
  }

  // RendererClient implementation.
  void OnError(PipelineStatus status) override {
    EXPECT_EQ(status, expected_error_);
    EXPECT_FALSE(have_error_);
    have_error_ = true;
    expected_error_ = PIPELINE_OK;
  }

  void OnEnded() override {
    EXPECT_TRUE(expect_eos_);
    expect_eos_ = false;
  }

  void OnStatisticsUpdate(const PipelineStatistics& stats) override {
    bytes_decoded_ += stats.audio_bytes_decoded;
  }

  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override {
    buffering_state_ = state;
  }

  void OnWaiting(WaitingReason reason) override {}
  void OnAudioConfigChange(const AudioDecoderConfig& config) override {
    last_config_change_ = config;
  }
  void OnVideoConfigChange(const VideoDecoderConfig& config) override {
    FAIL();
  }
  void OnVideoNaturalSizeChange(const gfx::Size& size) override { FAIL(); }
  void OnVideoOpacityChange(bool opaque) override { FAIL(); }
  void OnVideoFrameRateChange(absl::optional<int> fps) override { FAIL(); }

 private:
  PipelineStatus expected_error_ = PIPELINE_OK;
  bool have_error_ = false;
  bool expect_eos_ = false;
  BufferingState buffering_state_ = BUFFERING_HAVE_NOTHING;
  size_t bytes_decoded_ = 0;
  absl::optional<AudioDecoderConfig> last_config_change_;
};

// SysmemBufferStream that asynchronously decouples buffer production from
// buffer consumption. Used to simulate stream decryptor.
class AsyncSysmemBufferStream : public SysmemBufferStream {
 public:
  AsyncSysmemBufferStream();
  ~AsyncSysmemBufferStream() override;

  AsyncSysmemBufferStream(const AsyncSysmemBufferStream&) = delete;
  AsyncSysmemBufferStream& operator=(const AsyncSysmemBufferStream&) = delete;

  // SysmemBufferStream implementation:
  void Initialize(Sink* sink,
                  size_t min_buffer_size,
                  size_t min_buffer_count) override;
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) override;
  void Reset() override;

 private:
  void DoEnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);

  SysmemAllocatorClient sysmem_allocator_;
  PassthroughSysmemBufferStream passthrough_stream_;

  bool is_at_end_of_stream_ = false;

  base::WeakPtrFactory<AsyncSysmemBufferStream> weak_factory_{this};
};

AsyncSysmemBufferStream::AsyncSysmemBufferStream()
    : sysmem_allocator_("AsyncSysmemBufferStream"),
      passthrough_stream_(&sysmem_allocator_) {}

AsyncSysmemBufferStream::~AsyncSysmemBufferStream() = default;

void AsyncSysmemBufferStream::Initialize(Sink* sink,
                                         size_t min_buffer_size,
                                         size_t min_buffer_count) {
  passthrough_stream_.Initialize(sink, min_buffer_size, min_buffer_count);
}

void AsyncSysmemBufferStream::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  if (buffer->end_of_stream()) {
    EXPECT_FALSE(is_at_end_of_stream_);
    is_at_end_of_stream_ = true;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AsyncSysmemBufferStream::DoEnqueueBuffer,
                                weak_factory_.GetWeakPtr(), std::move(buffer)));
}

void AsyncSysmemBufferStream::Reset() {
  passthrough_stream_.Reset();

  // Drop pending DoEnqueueBuffer tasks.
  weak_factory_.InvalidateWeakPtrs();

  is_at_end_of_stream_ = false;
}

void AsyncSysmemBufferStream::DoEnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  passthrough_stream_.EnqueueBuffer(std::move(buffer));
}

class TestFuchsiaCdmContext : public CdmContext, public FuchsiaCdmContext {
 public:
  // CdmContext overrides.
  FuchsiaCdmContext* GetFuchsiaCdmContext() override { return this; }

  // FuchsiaCdmContext implementation.
  std::unique_ptr<SysmemBufferStream> CreateStreamDecryptor(
      bool secure_mode) override {
    return std::make_unique<AsyncSysmemBufferStream>();
  }
};

}  // namespace

struct RendererTestConfig {
  bool simulate_fuchsia_cdm;
};

class FuchsiaAudioRendererTest
    : public testing::Test,
      public testing::WithParamInterface<RendererTestConfig> {
 public:
  FuchsiaAudioRendererTest() = default;
  ~FuchsiaAudioRendererTest() override = default;

  void CreateUninitializedRenderer();
  void CreateTestDemuxerStream();
  void InitializeRenderer();
  void CreateAndInitializeRenderer();
  void ProduceDemuxerPacket(base::TimeDelta duration);
  void FillDemuxerStream(base::TimeDelta end_pos);
  void FillBuffer();
  void StartPlayback(base::TimeDelta start_time = base::TimeDelta());
  void CheckGetWallClockTimes(absl::optional<base::TimeDelta> media_timestamp,
                              base::TimeTicks expected_wall_clock,
                              bool is_time_moving);

  void TestPcmStream(SampleFormat sample_format,
                     size_t bytes_per_sample_input,
                     fuchsia::media::AudioSampleFormat fuchsia_sample_format,
                     size_t bytes_per_sample_output);

  // Starts playback from |start_time| at the specified |playback_rate| and
  // verifies that the clock works correctly.
  void StartPlaybackAndVerifyClock(base::TimeDelta start_time,
                                   float playback_rate);

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<CdmContext> cdm_context_;
  std::unique_ptr<TestAudioConsumer> audio_consumer_;
  std::unique_ptr<TestStreamSink> stream_sink_;
  std::unique_ptr<TestDemuxerStream> demuxer_stream_;
  TestRendererClient client_;

  std::unique_ptr<AudioRenderer> audio_renderer_;
  TimeSource* time_source_;
  base::TimeDelta demuxer_stream_pos_;
};

void FuchsiaAudioRendererTest::CreateUninitializedRenderer() {
  fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle;
  audio_consumer_ =
      std::make_unique<TestAudioConsumer>(audio_consumer_handle.NewRequest());
  audio_renderer_ = std::make_unique<FuchsiaAudioRenderer>(
      /*media_log=*/nullptr, std::move(audio_consumer_handle));
  time_source_ = audio_renderer_->GetTimeSource();
}

void FuchsiaAudioRendererTest::CreateTestDemuxerStream() {
  AudioDecoderConfig config(AudioCodec::kPCM, kSampleFormatF32,
                            CHANNEL_LAYOUT_MONO, kDefaultSampleRate, {},
                            EncryptionScheme::kUnencrypted);

  if (GetParam().simulate_fuchsia_cdm) {
    config.SetIsEncrypted(true);
    cdm_context_ = std::make_unique<TestFuchsiaCdmContext>();
  }

  demuxer_stream_ = std::make_unique<TestDemuxerStream>(config);
}

void FuchsiaAudioRendererTest::InitializeRenderer() {
  if (!demuxer_stream_)
    CreateTestDemuxerStream();

  base::RunLoop run_loop;
  PipelineStatus pipeline_status;
  audio_renderer_->Initialize(
      demuxer_stream_.get(), cdm_context_.get(), &client_,
      base::BindLambdaForTesting(
          [&run_loop, &pipeline_status](PipelineStatus s) {
            pipeline_status = s;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_EQ(pipeline_status, PIPELINE_OK);

  audio_consumer_->UpdateStatus(absl::nullopt, absl::nullopt);

  task_environment_.RunUntilIdle();
}

void FuchsiaAudioRendererTest::CreateAndInitializeRenderer() {
  CreateUninitializedRenderer();
  InitializeRenderer();
}

void FuchsiaAudioRendererTest::ProduceDemuxerPacket(base::TimeDelta duration) {
  // Create a dummy packet that contains just 1 byte.
  const size_t kBufferSize = 1;
  scoped_refptr<DecoderBuffer> buffer = new DecoderBuffer(kBufferSize);
  buffer->set_timestamp(demuxer_stream_pos_);
  buffer->set_duration(duration);
  demuxer_stream_pos_ += duration;
  demuxer_stream_->QueueReadResult(TestDemuxerStream::ReadResult(buffer));
}

void FuchsiaAudioRendererTest::FillDemuxerStream(base::TimeDelta end_pos) {
  EXPECT_LT(demuxer_stream_pos_, end_pos);
  while (demuxer_stream_pos_ < end_pos) {
    ProduceDemuxerPacket(kPacketDuration);
  }
}

void FuchsiaAudioRendererTest::FillBuffer() {
  if (!stream_sink_) {
    stream_sink_ = audio_consumer_->WaitStreamSinkConnected();
  }

  // The renderer expects one extra packet after reaching kMinLeadTime to get
  // to the BUFFERING_HAVE_ENOUGH state.
  const size_t kNumPackets = kMinLeadTime / kPacketDuration + 1;
  for (size_t i = 0; i < kNumPackets; ++i) {
    ProduceDemuxerPacket(kPacketDuration);
  }
  task_environment_.RunUntilIdle();

  // Renderer should not start reading demuxer untile StartPlaying() is
  // called.
  EXPECT_EQ(client_.buffering_state(), BUFFERING_HAVE_NOTHING);
  EXPECT_EQ(stream_sink_->received_packets()->size(), 0U);

  // Start playback. The renderer should push queued packets to the
  // AudioConsumer and updated buffering state.
  audio_renderer_->StartPlaying();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(stream_sink_->received_packets()->size(), kNumPackets);
  EXPECT_EQ(client_.buffering_state(), BUFFERING_HAVE_ENOUGH);
}

void FuchsiaAudioRendererTest::StartPlayback(base::TimeDelta start_time) {
  EXPECT_FALSE(audio_consumer_->started());
  time_source_->SetMediaTime(start_time);

  ASSERT_NO_FATAL_FAILURE(FillBuffer());
  time_source_->StartTicking();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(audio_consumer_->started());
  EXPECT_EQ(audio_consumer_->start_media_time(), start_time);

  audio_consumer_->UpdateStatus(base::TimeTicks::Now(), start_time);
  task_environment_.RunUntilIdle();
}

void FuchsiaAudioRendererTest::CheckGetWallClockTimes(
    absl::optional<base::TimeDelta> media_timestamp,
    base::TimeTicks expected_wall_clock,
    bool is_time_moving) {
  std::vector<base::TimeDelta> media_timestamps;
  if (media_timestamp)
    media_timestamps.push_back(media_timestamp.value());
  std::vector<base::TimeTicks> wall_clock;
  bool result = time_source_->GetWallClockTimes(media_timestamps, &wall_clock);
  EXPECT_EQ(wall_clock[0], expected_wall_clock);
  EXPECT_EQ(result, is_time_moving);
}

void FuchsiaAudioRendererTest::StartPlaybackAndVerifyClock(
    base::TimeDelta start_time,
    float playback_rate) {
  time_source_->SetMediaTime(start_time);
  time_source_->SetPlaybackRate(playback_rate);

  demuxer_stream_pos_ = start_time;
  ASSERT_NO_FATAL_FAILURE(FillBuffer());

  EXPECT_FALSE(audio_consumer_->started());
  time_source_->StartTicking();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(audio_consumer_->started());

  // Start position should be reported before updated status is received.
  EXPECT_EQ(time_source_->CurrentMediaTime(), start_time);
  task_environment_.FastForwardBy(kTimeStep);
  EXPECT_EQ(time_source_->CurrentMediaTime(), start_time);

  CheckGetWallClockTimes(absl::nullopt, base::TimeTicks(), false);
  CheckGetWallClockTimes(start_time + kTimeStep,
                         base::TimeTicks::Now() + kTimeStep, false);

  // MediaTime will start moving once AudioConsumer updates timeline.
  const base::TimeDelta kStartDelay = base::Milliseconds(3);
  base::TimeTicks start_wall_clock = base::TimeTicks::Now() + kStartDelay;
  audio_consumer_->UpdateStatus(start_wall_clock, start_time);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(time_source_->CurrentMediaTime(),
            start_time - kStartDelay * playback_rate);
  task_environment_.FastForwardBy(kTimeStep);
  EXPECT_EQ(time_source_->CurrentMediaTime(),
            start_time + (-kStartDelay + kTimeStep) * playback_rate);

  CheckGetWallClockTimes(absl::nullopt, base::TimeTicks::Now(), true);
  CheckGetWallClockTimes(start_time + kTimeStep,
                         start_wall_clock + kTimeStep / playback_rate, true);
  CheckGetWallClockTimes(start_time + 2 * kTimeStep,
                         start_wall_clock + 2.0 * kTimeStep / playback_rate,
                         true);
}

// Run all FuchsiaAudioRendererTests with CDM enabled and disabled.
INSTANTIATE_TEST_SUITE_P(Unencrypted,
                         FuchsiaAudioRendererTest,
                         testing::Values(RendererTestConfig{false}));
INSTANTIATE_TEST_SUITE_P(Encrypted,
                         FuchsiaAudioRendererTest,
                         testing::Values(RendererTestConfig{true}));

TEST_P(FuchsiaAudioRendererTest, Initialize) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
}

TEST_P(FuchsiaAudioRendererTest, InitializeAndBuffer) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(FillBuffer());

  // Extra packets should be sent to AudioConsumer immediately.
  stream_sink_->received_packets()->clear();
  ProduceDemuxerPacket(base::Milliseconds(10));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(stream_sink_->received_packets()->size(), 1U);
}

TEST_P(FuchsiaAudioRendererTest, StartPlaybackBeforeStreamSinkConnected) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());

  // Start playing immediately after initialization. The renderer should wait
  // for buffers to be allocated before it starts reading from the demuxer.
  audio_renderer_->StartPlaying();
  ProduceDemuxerPacket(base::Milliseconds(10));
  task_environment_.RunUntilIdle();

  stream_sink_ = audio_consumer_->WaitStreamSinkConnected();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(stream_sink_->received_packets()->size(), 1U);
}

TEST_P(FuchsiaAudioRendererTest, StartTicking) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlaybackAndVerifyClock(
      /*start_pos=*/base::Milliseconds(123),
      /*playback_rate=*/1.0));
}

TEST_P(FuchsiaAudioRendererTest, StartTickingRate1_5) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlaybackAndVerifyClock(
      /*start_pos=*/base::Milliseconds(123),
      /*playback_rate=*/1.5));
}

TEST_P(FuchsiaAudioRendererTest, StartTickingRate0_5) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlaybackAndVerifyClock(
      /*start_pos=*/base::Milliseconds(123),
      /*playback_rate=*/0.5));
}

// Verify that the renderer doesn't send packets more than kMaxLeadTime ahead of
// time.
TEST_P(FuchsiaAudioRendererTest, MaxLeadTime) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(FillBuffer());

  // Queue packets up to kMaxLeadTime with 10 extra packets. The Renderer
  // shouldn't read these extra packets at the end.
  FillDemuxerStream(kMaxLeadTime + kPacketDuration * 10);

  task_environment_.RunUntilIdle();

  // Verify that the renderer has filled the buffer to kMaxLeadTime.
  size_t expected_packets = kMaxLeadTime / kPacketDuration;
  EXPECT_EQ(expected_packets, stream_sink_->received_packets()->size());
}

TEST_P(FuchsiaAudioRendererTest, Seek) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());

  constexpr base::TimeDelta kStartPos = base::TimeDelta();
  ASSERT_NO_FATAL_FAILURE(StartPlayback(kStartPos));
  task_environment_.FastForwardBy(kTimeStep);
  time_source_->StopTicking();
  demuxer_stream_->DiscardQueueAndAbortRead();

  // Media time should be stopped after StopTicking().
  EXPECT_EQ(time_source_->CurrentMediaTime(), kStartPos + kTimeStep);
  task_environment_.FastForwardBy(kTimeStep);
  EXPECT_EQ(time_source_->CurrentMediaTime(), kStartPos + kTimeStep);

  // Flush the renderer.
  base::RunLoop run_loop;
  audio_renderer_->Flush(run_loop.QuitClosure());
  run_loop.Run();

  // Restart playback from a new position.
  const base::TimeDelta kSeekPos = base::Milliseconds(123);
  ASSERT_NO_FATAL_FAILURE(StartPlaybackAndVerifyClock(kSeekPos,
                                                      /*playback_rate=*/1.0));

  ProduceDemuxerPacket(kPacketDuration);

  // Verify that old packets were discarded and StreamSink started received
  // packets at the correct position.
  EXPECT_GT(stream_sink_->discarded_packets()->size(), 0u);
  EXPECT_EQ(stream_sink_->received_packets()->at(0).pts,
            kSeekPos.ToZxDuration());
}

TEST_P(FuchsiaAudioRendererTest, ChangeConfig) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlayback());

  const auto kConfigChangePos = base::Seconds(1);

  // Queue packets up to kConfigChangePos.
  FillDemuxerStream(kConfigChangePos);

  const size_t kNewSampleRate = 44100;
  const std::vector<uint8_t> kArbitraryExtraData = {1, 2, 3};
  AudioDecoderConfig updated_config(
      AudioCodec::kOpus, kSampleFormatF32, CHANNEL_LAYOUT_STEREO,
      kNewSampleRate, kArbitraryExtraData, EncryptionScheme::kUnencrypted);
  demuxer_stream_->QueueReadResult(
      TestDemuxerStream::ReadResult(updated_config));

  // Queue one more packet with the new config.
  ProduceDemuxerPacket(kPacketDuration);

  task_environment_.FastForwardBy(kConfigChangePos);

  // The renderer should have created new StreamSink when config was changed.
  auto new_stream_sink = audio_consumer_->TakeStreamSink();
  ASSERT_TRUE(new_stream_sink);

  ASSERT_TRUE(client_.last_config_change().has_value());
  EXPECT_TRUE(client_.last_config_change()->Matches(updated_config));

  EXPECT_EQ(stream_sink_->stream_type().channels, 1U);
  EXPECT_EQ(stream_sink_->stream_type().frames_per_second,
            static_cast<uint32_t>(kDefaultSampleRate));
  EXPECT_EQ(stream_sink_->received_packets()->size(),
            kConfigChangePos / kPacketDuration);

  EXPECT_EQ(new_stream_sink->stream_type().channels,
            static_cast<uint32_t>(updated_config.channels()));
  EXPECT_EQ(new_stream_sink->stream_type().frames_per_second, kNewSampleRate);
  EXPECT_TRUE(new_stream_sink->compression());
  EXPECT_EQ(new_stream_sink->compression()->type,
            fuchsia::media::AUDIO_ENCODING_OPUS);
  EXPECT_EQ(new_stream_sink->compression()->parameters, kArbitraryExtraData);
  EXPECT_EQ(new_stream_sink->received_packets()->size(), 1U);
  EXPECT_EQ(new_stream_sink->received_packets()->at(0).pts,
            kConfigChangePos.ToZxDuration());
}

TEST_P(FuchsiaAudioRendererTest, UpdateTimeline) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlayback());

  FillDemuxerStream(base::Seconds(2));

  const auto kTimelineChangePos = base::Seconds(1);
  task_environment_.FastForwardBy(kTimelineChangePos);

  // Shift the timeline by 2ms.
  const auto kMediaDelta = base::Milliseconds(2);
  audio_consumer_->UpdateStatus(base::TimeTicks::Now(),
                                kTimelineChangePos + kMediaDelta);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(time_source_->CurrentMediaTime(), kTimelineChangePos + kMediaDelta);
  task_environment_.FastForwardBy(kTimeStep);
  EXPECT_EQ(time_source_->CurrentMediaTime(),
            kTimelineChangePos + kMediaDelta + kTimeStep);
}

TEST_P(FuchsiaAudioRendererTest, PauseAndResume) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlayback());

  const auto kPauseTimestamp = base::Seconds(1);
  const auto kStreamLength = base::Seconds(2);

  FillDemuxerStream(kStreamLength);

  task_environment_.FastForwardBy(kPauseTimestamp);

  // Pause playback by setting playback rate to 0.0.
  time_source_->SetPlaybackRate(0.0);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(audio_consumer_->playback_rate(), 0.0);

  task_environment_.FastForwardBy(kTimeStep);
  audio_consumer_->UpdateStatus(base::TimeTicks::Now(), kPauseTimestamp);
  task_environment_.RunUntilIdle();

  const size_t kExpectedQueuedPackets =
      (kPauseTimestamp + kMaxLeadTime) / kPacketDuration + 1;
  EXPECT_EQ(stream_sink_->received_packets()->size(), kExpectedQueuedPackets);
  EXPECT_EQ(time_source_->CurrentMediaTime(), kPauseTimestamp);

  // Keep the stream paused for 10 seconds. The Renderer should not be sending
  // new packets
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(stream_sink_->received_packets()->size(), kExpectedQueuedPackets);
  EXPECT_EQ(time_source_->CurrentMediaTime(), kPauseTimestamp);

  // Resume playback.
  time_source_->SetPlaybackRate(1.0);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(audio_consumer_->playback_rate(), 1.0);
  audio_consumer_->UpdateStatus(base::TimeTicks::Now(), kPauseTimestamp);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(time_source_->CurrentMediaTime(), kPauseTimestamp);

  // The renderer should start sending packets again.
  task_environment_.FastForwardBy(kPacketDuration);
  EXPECT_EQ(stream_sink_->received_packets()->size(),
            kExpectedQueuedPackets + 1);

  EXPECT_EQ(time_source_->CurrentMediaTime(),
            kPauseTimestamp + kPacketDuration);
}

// Verify that end-of-stream is handled correctly when the renderer is buffered.
TEST_P(FuchsiaAudioRendererTest, EndOfStreamBuffered) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  ASSERT_NO_FATAL_FAILURE(StartPlayback());

  const auto kStreamLength = base::Seconds(1);
  FillDemuxerStream(kStreamLength);
  demuxer_stream_->QueueReadResult(
      TestDemuxerStream::ReadResult(DecoderBuffer::CreateEOSBuffer()));

  // Queue second EOS buffer. The renderer should not read it.
  demuxer_stream_->QueueReadResult(
      TestDemuxerStream::ReadResult(DecoderBuffer::CreateEOSBuffer()));

  task_environment_.FastForwardBy(kStreamLength);

  EXPECT_EQ(stream_sink_->received_packets()->size(),
            kStreamLength / kPacketDuration);
  EXPECT_TRUE(stream_sink_->received_end_of_stream());

  client_.ExpectEos();
  audio_consumer_->SignalEndOfStream();
  task_environment_.RunUntilIdle();
}

// Verifies that buffering state is updated after reaching EOS. See
// https://crbug.com/1162503 .
TEST_P(FuchsiaAudioRendererTest, EndOfStreamWhenBuffering) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  stream_sink_ = audio_consumer_->WaitStreamSinkConnected();

  // Produce stream shorter than kMinLeadTime.
  const auto kStreamLength = kMinLeadTime / 2;
  FillDemuxerStream(kStreamLength);
  demuxer_stream_->QueueReadResult(
      TestDemuxerStream::ReadResult(DecoderBuffer::CreateEOSBuffer()));
  task_environment_.RunUntilIdle();

  // Start playback. The renderer should push queued packets to the
  // AudioConsumer and updated buffering state when it reaches EOS.
  audio_renderer_->StartPlaying();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(client_.buffering_state(), BUFFERING_HAVE_ENOUGH);
  EXPECT_TRUE(stream_sink_->received_end_of_stream());
}

TEST_P(FuchsiaAudioRendererTest, EndOfStreamStart) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  stream_sink_ = audio_consumer_->WaitStreamSinkConnected();

  // Queue EOS without any preceding packets.
  demuxer_stream_->QueueReadResult(
      TestDemuxerStream::ReadResult(DecoderBuffer::CreateEOSBuffer()));
  task_environment_.RunUntilIdle();

  // Start playback. The renderer should push queued packets to the
  // AudioConsumer and updated buffering state when it reaches EOS.
  audio_renderer_->StartPlaying();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(client_.buffering_state(), BUFFERING_HAVE_ENOUGH);
  EXPECT_TRUE(stream_sink_->received_end_of_stream());
}

TEST_P(FuchsiaAudioRendererTest, SetVolume) {
  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());

  audio_renderer_->SetVolume(0.5);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(audio_consumer_->volume(), 0.5);
}

TEST_P(FuchsiaAudioRendererTest, SetVolumeBeforeInitialize) {
  ASSERT_NO_FATAL_FAILURE(CreateUninitializedRenderer());

  // SetVolume() may be called before AudioRenderer is initialized. It should
  // still be handled.
  audio_renderer_->SetVolume(0.5);

  ASSERT_NO_FATAL_FAILURE(InitializeRenderer());
  EXPECT_EQ(audio_consumer_->volume(), 0.5);
}

// Verify that the case when StartTicking() is called shortly after
// StartPlaying() is handled correctly. AudioConsumer::Start() should be sent
// only after CreateStreamSink(). See crbug.com/1219147 .
TEST_P(FuchsiaAudioRendererTest, PlaybackBeforeSinkCreation) {
  CreateTestDemuxerStream();
  const auto kStreamLength = base::Milliseconds(100);
  FillDemuxerStream(kStreamLength);
  demuxer_stream_->QueueReadResult(
      TestDemuxerStream::ReadResult(DecoderBuffer::CreateEOSBuffer()));

  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());

  // Call StartTicking() shortly after StartPlayback(). At this point sysmem
  // buffer allocation hasn't been complete, so AudioConsumer::Start() should be
  // delayed until the buffer are allocated.
  audio_renderer_->StartPlaying();
  time_source_->StartTicking();

  // Wait until the stream is started. Start() should be called only after
  // StreamSink() is connected and the packets are buffered.
  audio_consumer_->WaitStarted();
  stream_sink_ = audio_consumer_->TakeStreamSink();
  EXPECT_GT(stream_sink_->received_packets()->size(), 0U);
}

void FuchsiaAudioRendererTest::TestPcmStream(
    SampleFormat sample_format,
    size_t bytes_per_sample_input,
    fuchsia::media::AudioSampleFormat fuchsia_sample_format,
    size_t bytes_per_sample_output) {
  AudioDecoderConfig config(AudioCodec::kPCM, sample_format,
                            CHANNEL_LAYOUT_STEREO, kDefaultSampleRate, {},
                            EncryptionScheme::kUnencrypted);

  demuxer_stream_ = std::make_unique<TestDemuxerStream>(config);

  ASSERT_NO_FATAL_FAILURE(CreateAndInitializeRenderer());
  stream_sink_ = audio_consumer_->WaitStreamSinkConnected();
  EXPECT_EQ(stream_sink_->stream_type().sample_format, fuchsia_sample_format);

  // Create a dummy packet that contains 1 sample.
  const size_t kNumSamples = 10;
  const size_t kChannels = 2;
  size_t input_buffer_size = kNumSamples * kChannels * bytes_per_sample_input;
  scoped_refptr<DecoderBuffer> buffer = new DecoderBuffer(input_buffer_size);
  buffer->set_timestamp(demuxer_stream_pos_);
  buffer->set_duration(kPacketDuration);
  for (size_t i = 0; i < input_buffer_size; ++i) {
    buffer->writable_data()[i] = i;
  }
  demuxer_stream_->QueueReadResult(TestDemuxerStream::ReadResult(buffer));

  // Start playback. The renderer will process the packet queued above.
  audio_renderer_->StartPlaying();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(stream_sink_->received_packets()->size(), 1U);
  auto packet = stream_sink_->received_packets()->at(0);

  // Read and verify packet content
  size_t output_size = kNumSamples * kChannels * bytes_per_sample_output;
  EXPECT_EQ(packet.payload_size, output_size);
  uint8_t data[output_size];
  zx_status_t result = stream_sink_->buffers()[packet.payload_buffer_id].read(
      data, 0, output_size);
  ZX_CHECK(result == ZX_OK, result);

  for (size_t i = 0; i < output_size; ++i) {
    size_t pos_within_sample = i % bytes_per_sample_output;
    uint8_t expected_value =
        (pos_within_sample < bytes_per_sample_input)
            ? (i / bytes_per_sample_output * bytes_per_sample_input +
               pos_within_sample)
            : 0;
    EXPECT_EQ(data[i], expected_value);
  }
}

TEST_F(FuchsiaAudioRendererTest, PcmU8Stream) {
  TestPcmStream(kSampleFormatU8, 1,
                fuchsia::media::AudioSampleFormat::UNSIGNED_8, 1);
}

TEST_F(FuchsiaAudioRendererTest, PcmS16Stream) {
  TestPcmStream(kSampleFormatS16, 2,
                fuchsia::media::AudioSampleFormat::SIGNED_16, 2);
}

TEST_F(FuchsiaAudioRendererTest, PcmS24Stream) {
  TestPcmStream(kSampleFormatS24, 3,
                fuchsia::media::AudioSampleFormat::SIGNED_24_IN_32, 4);
}

TEST_F(FuchsiaAudioRendererTest, PcmF32Stream) {
  TestPcmStream(kSampleFormatF32, 4, fuchsia::media::AudioSampleFormat::FLOAT,
                4);
}

}  // namespace media
