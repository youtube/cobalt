// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_renderer.h"

#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer_client.h"
#include "media/fuchsia/cdm/fuchsia_cdm_context.h"
#include "media/fuchsia/common/decrypting_sysmem_buffer_stream.h"
#include "media/fuchsia/common/passthrough_sysmem_buffer_stream.h"

namespace media {

namespace {

// nullopt is returned in case the codec is not supported. nullptr is returned
// for uncompressed PCM streams.
absl::optional<std::unique_ptr<fuchsia::media::Compression>>
GetFuchsiaCompressionFromDecoderConfig(AudioDecoderConfig config) {
  auto compression = std::make_unique<fuchsia::media::Compression>();
  switch (config.codec()) {
    case AudioCodec::kAAC:
      compression->type = fuchsia::media::AUDIO_ENCODING_AAC;
      break;
    case AudioCodec::kMP3:
      compression->type = fuchsia::media::AUDIO_ENCODING_MP3;
      break;
    case AudioCodec::kVorbis:
      compression->type = fuchsia::media::AUDIO_ENCODING_VORBIS;
      break;
    case AudioCodec::kOpus:
      compression->type = fuchsia::media::AUDIO_ENCODING_OPUS;
      break;
    case AudioCodec::kFLAC:
      compression->type = fuchsia::media::AUDIO_ENCODING_FLAC;
      break;
    case AudioCodec::kPCM:
      compression.reset();
      break;

    default:
      return absl::nullopt;
  }

  if (!config.extra_data().empty()) {
    compression->parameters = config.extra_data();
  }

  return std::move(compression);
}

absl::optional<fuchsia::media::AudioSampleFormat>
GetFuchsiaSampleFormatFromSampleFormat(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatU8:
      return fuchsia::media::AudioSampleFormat::UNSIGNED_8;
    case kSampleFormatS16:
      return fuchsia::media::AudioSampleFormat::SIGNED_16;
    case kSampleFormatS24:
      return fuchsia::media::AudioSampleFormat::SIGNED_24_IN_32;
    case kSampleFormatF32:
      return fuchsia::media::AudioSampleFormat::FLOAT;

    default:
      return absl::nullopt;
  }
}

// Helper that converts a PCM stream in kStreamFormatS24 to the layout
// expected by AudioConsumer (i.e. SIGNED_24_IN_32).
scoped_refptr<media::DecoderBuffer> PreparePcm24Buffer(
    scoped_refptr<media::DecoderBuffer> buffer) {
  static_assert(ARCH_CPU_LITTLE_ENDIAN,
                "Only little-endian CPUs are supported.");

  size_t samples = buffer->data_size() / 3;
  scoped_refptr<media::DecoderBuffer> result =
      base::MakeRefCounted<media::DecoderBuffer>(samples * 4);
  for (size_t i = 0; i < samples - 1; ++i) {
    reinterpret_cast<uint32_t*>(result->writable_data())[i] =
        *reinterpret_cast<const uint32_t*>(buffer->data() + i * 3) & 0x00ffffff;
  }
  size_t last_sample = samples - 1;
  reinterpret_cast<uint32_t*>(result->writable_data())[last_sample] =
      buffer->data()[last_sample * 3] |
      (buffer->data()[last_sample * 3 + 1] << 8) |
      (buffer->data()[last_sample * 3 + 2] << 16);

  result->set_timestamp(buffer->timestamp());
  result->set_duration(buffer->duration());

  if (buffer->decrypt_config())
    result->set_decrypt_config(buffer->decrypt_config()->Clone());

  return result;
}

}  // namespace

// Size of a single audio buffer: 100kB. It's enough to cover 100ms of PCM at
// 48kHz, 2 channels, 16 bps.
constexpr size_t kBufferSize = 100 * 1024;

// Total number of buffers. 16 is the maximum allowed by AudioConsumer.
constexpr size_t kNumBuffers = 16;

FuchsiaAudioRenderer::FuchsiaAudioRenderer(
    MediaLog* media_log,
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle)
    : audio_consumer_handle_(std::move(audio_consumer_handle)) {
  DETACH_FROM_THREAD(thread_checker_);
}

FuchsiaAudioRenderer::~FuchsiaAudioRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FuchsiaAudioRenderer::Initialize(DemuxerStream* stream,
                                      CdmContext* cdm_context,
                                      RendererClient* client,
                                      PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!demuxer_stream_);

  DCHECK(!init_cb_);
  init_cb_ = std::move(init_cb);

  demuxer_stream_ = stream;
  client_ = client;

  audio_consumer_.Bind(std::move(audio_consumer_handle_));
  audio_consumer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioConsumer disconnected.";
    OnError(AUDIO_RENDERER_ERROR);
  });

  UpdateVolume();

  audio_consumer_.events().OnEndOfStream = [this]() { OnEndOfStream(); };
  RequestAudioConsumerStatus();

  // AAC streams require bitstream conversion. Without it the demuxer may
  // produce decoded stream without ADTS headers which are required for AAC
  // streams in AudioConsumer.
  // TODO(crbug.com/1120095): Reconsider this logic.
  if (stream->audio_decoder_config().codec() == AudioCodec::kAAC) {
    stream->EnableBitstreamConverter();
  }

  if (stream->audio_decoder_config().is_encrypted()) {
    if (!cdm_context) {
      DLOG(ERROR) << "No cdm context for encrypted stream.";
      OnError(AUDIO_RENDERER_ERROR);
      return;
    }

    FuchsiaCdmContext* fuchsia_cdm = cdm_context->GetFuchsiaCdmContext();
    if (fuchsia_cdm) {
      sysmem_buffer_stream_ = fuchsia_cdm->CreateStreamDecryptor(false);
    } else {
      sysmem_buffer_stream_ = std::make_unique<DecryptingSysmemBufferStream>(
          &sysmem_allocator_, cdm_context, Decryptor::kAudio);
    }

  } else {
    sysmem_buffer_stream_ =
        std::make_unique<PassthroughSysmemBufferStream>(&sysmem_allocator_);
  }

  sysmem_buffer_stream_->Initialize(this, kBufferSize, kNumBuffers);

  std::move(init_cb_).Run(PIPELINE_OK);
}

void FuchsiaAudioRenderer::UpdateVolume() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(audio_consumer_);
  if (!volume_control_) {
    audio_consumer_->BindVolumeControl(volume_control_.NewRequest());
    volume_control_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "VolumeControl disconnected.";
    });
  }
  volume_control_->SetVolume(volume_);
}

void FuchsiaAudioRenderer::OnBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem::SingleBufferSettings&) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  input_buffers_ = std::move(buffers);
  InitializeStreamSink();

  while (!delayed_packets_.empty()) {
    auto packet = std::move(delayed_packets_.front());
    delayed_packets_.pop_front();
    SendInputPacket(std::move(packet));
  }

  if (is_at_end_of_stream_) {
    OnSysmemBufferStreamEndOfStream();
  }
}

void FuchsiaAudioRenderer::InitializeStreamSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!stream_sink_);

  // Clone |buffers| to pass to StreamSink.
  std::vector<zx::vmo> vmos_for_stream_sink;
  vmos_for_stream_sink.reserve(input_buffers_.size());
  for (VmoBuffer& buffer : input_buffers_) {
    vmos_for_stream_sink.push_back(buffer.Duplicate(/*writable=*/false));
  }

  auto config = demuxer_stream_->audio_decoder_config();
  auto compression = GetFuchsiaCompressionFromDecoderConfig(config);
  if (!compression) {
    LOG(ERROR) << "Unsupported audio codec: " << GetCodecName(config.codec());
    OnError(AUDIO_RENDERER_ERROR);
    return;
  }

  fuchsia::media::AudioStreamType stream_type;
  stream_type.channels = config.channels();
  stream_type.frames_per_second = config.samples_per_second();

  // Set sample_format for uncompressed streams.
  if (!compression.value()) {
    absl::optional<fuchsia::media::AudioSampleFormat> sample_format =
        GetFuchsiaSampleFormatFromSampleFormat(config.sample_format());
    if (!sample_format) {
      LOG(ERROR) << "Unsupported sample format: "
                 << SampleFormatToString(config.sample_format());
      OnError(AUDIO_RENDERER_ERROR);
      return;
    }
    stream_type.sample_format = sample_format.value();
  } else {
    // For compressed formats sample format is determined by the decoder, but
    // this field is still required in AudioStreamType.
    stream_type.sample_format = fuchsia::media::AudioSampleFormat::SIGNED_16;
  }

  audio_consumer_->CreateStreamSink(
      std::move(vmos_for_stream_sink), std::move(stream_type),
      std::move(compression).value(), stream_sink_.NewRequest());

  if (GetPlaybackState() == PlaybackState::kStartPending)
    StartAudioConsumer();

  ScheduleReadDemuxerStream();
}

TimeSource* FuchsiaAudioRenderer::GetTimeSource() {
  return this;
}

void FuchsiaAudioRenderer::Flush(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  FlushInternal();
  renderer_started_ = false;

  std::move(callback).Run();
}

void FuchsiaAudioRenderer::StartPlaying() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  renderer_started_ = true;
  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  volume_ = volume;
  if (audio_consumer_)
    UpdateVolume();
}

void FuchsiaAudioRenderer::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  // TODO(crbug.com/1131116): Implement at some later date after we've vetted
  // the API shape and usefulness outside of fuchsia.
}

void FuchsiaAudioRenderer::SetPreservesPitch(bool preserves_pitch) {}

void FuchsiaAudioRenderer::SetAutoplayInitiated(bool autoplay_initiated) {}

void FuchsiaAudioRenderer::StartTicking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If StreamSink hasn't been created yet, then delay starting AudioConsumer
  // until StreamSink is created.
  if (!stream_sink_) {
    base::AutoLock lock(timeline_lock_);
    SetPlaybackState(PlaybackState::kStartPending);
    return;
  }

  StartAudioConsumer();
}

void FuchsiaAudioRenderer::StartAudioConsumer() {
  DCHECK(stream_sink_);

  fuchsia::media::AudioConsumerStartFlags flags{};
  if (demuxer_stream_->liveness() == DemuxerStream::LIVENESS_LIVE) {
    flags = fuchsia::media::AudioConsumerStartFlags::LOW_LATENCY;
  }

  // Stop the AudioConsumer if it's been started.
  switch (GetPlaybackState()) {
    case PlaybackState::kStopped:
    case PlaybackState::kStartPending:
      break;

    case PlaybackState::kStarting:
    case PlaybackState::kPlaying:
      audio_consumer_->Stop();
      break;
  }

  base::TimeDelta media_pos;
  {
    base::AutoLock lock(timeline_lock_);
    media_pos = media_pos_;
    SetPlaybackState(PlaybackState::kStarting);
  }

  audio_consumer_->Start(flags, fuchsia::media::NO_TIMESTAMP,
                         media_pos.ToZxDuration());
}

void FuchsiaAudioRenderer::StopTicking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(GetPlaybackState() != PlaybackState::kStopped);

  audio_consumer_->Stop();

  base::AutoLock lock(timeline_lock_);
  UpdateTimelineOnStop();
  SetPlaybackState(PlaybackState::kStopped);
}

void FuchsiaAudioRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_consumer_->SetRate(playback_rate);

  // AudioConsumer will update media timeline asynchronously. That update is
  // processed in OnAudioConsumerStatusChanged(). This might cause the clock to
  // go back. It's not desirable, e.g. because VideoRenderer could drop some
  // video frames that should be shown when the stream is resumed. To avoid this
  // issue update the timeline synchronously. OnAudioConsumerStatusChanged()
  // will still process the update from AudioConsumer to save the position when
  // the stream was actually paused, but that update would not move the clock
  // backward.
  if (playback_rate == 0.0) {
    base::AutoLock lock(timeline_lock_);
    UpdateTimelineOnStop();
  }
}

void FuchsiaAudioRenderer::SetMediaTime(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(GetPlaybackState() == PlaybackState::kStopped);

  {
    base::AutoLock lock(timeline_lock_);
    media_pos_ = time;

    // Reset reference timestamp. This is necessary to ensure that the correct
    // value is returned from GetWallClockTimes() until playback is resumed:
    // GetWallClockTimes() is required to return 0 wall clock between
    // SetMediaTime() and StartTicking().
    reference_time_ = base::TimeTicks();
  }

  FlushInternal();
  ScheduleReadDemuxerStream();
}

base::TimeDelta FuchsiaAudioRenderer::CurrentMediaTime() {
  base::AutoLock lock(timeline_lock_);
  if (!IsTimeMoving())
    return media_pos_;

  return CurrentMediaTimeLocked();
}

bool FuchsiaAudioRenderer::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  wall_clock_times->reserve(media_timestamps.size());
  auto now = base::TimeTicks::Now();

  base::AutoLock lock(timeline_lock_);

  const bool is_time_moving = IsTimeMoving();

  if (media_timestamps.empty()) {
    wall_clock_times->push_back(is_time_moving ? now : reference_time_);
    return is_time_moving;
  }

  base::TimeTicks wall_clock_base = is_time_moving ? reference_time_ : now;

  for (base::TimeDelta timestamp : media_timestamps) {
    base::TimeTicks wall_clock_time;

    auto relative_pos = timestamp - media_pos_;
    if (is_time_moving) {
      // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
      relative_pos = relative_pos * reference_delta_ / media_delta_;
    }
    wall_clock_time = wall_clock_base + relative_pos;
    wall_clock_times->push_back(wall_clock_time);
  }

  return is_time_moving;
}

FuchsiaAudioRenderer::PlaybackState FuchsiaAudioRenderer::GetPlaybackState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return state_;
}

void FuchsiaAudioRenderer::SetPlaybackState(PlaybackState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = state;
}

void FuchsiaAudioRenderer::OnError(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_consumer_.Unbind();
  stream_sink_.Unbind();
  sysmem_buffer_stream_.reset();

  if (is_demuxer_read_pending_) {
    drop_next_demuxer_read_result_ = true;
  }

  if (init_cb_) {
    std::move(init_cb_).Run(status);
  } else if (client_) {
    client_->OnError(status);
  }
}

void FuchsiaAudioRenderer::RequestAudioConsumerStatus() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_consumer_->WatchStatus(fit::bind_member(
      this, &FuchsiaAudioRenderer::OnAudioConsumerStatusChanged));
}

void FuchsiaAudioRenderer::OnAudioConsumerStatusChanged(
    fuchsia::media::AudioConsumerStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (status.has_error()) {
    LOG(ERROR) << "fuchsia::media::AudioConsumer reported an error";
    OnError(AUDIO_RENDERER_ERROR);
    return;
  }

  bool reschedule_read_timer = false;

  if (status.has_presentation_timeline()) {
    if (GetPlaybackState() != PlaybackState::kStopped) {
      base::AutoLock lock(timeline_lock_);
      if (GetPlaybackState() == PlaybackState::kStarting) {
        SetPlaybackState(PlaybackState::kPlaying);
      }
      reference_time_ = base::TimeTicks::FromZxTime(
          status.presentation_timeline().reference_time);
      media_pos_ = base::TimeDelta::FromZxDuration(
          status.presentation_timeline().subject_time);
      reference_delta_ = status.presentation_timeline().reference_delta;
      media_delta_ = status.presentation_timeline().subject_delta;

      reschedule_read_timer = true;
    }
  }

  if (status.has_min_lead_time()) {
    auto new_min_lead_time =
        base::TimeDelta::FromZxDuration(status.min_lead_time());
    DCHECK(!new_min_lead_time.is_zero());
    if (new_min_lead_time != min_lead_time_) {
      min_lead_time_ = new_min_lead_time;
      reschedule_read_timer = true;
    }
  }
  if (status.has_max_lead_time()) {
    auto new_max_lead_time =
        base::TimeDelta::FromZxDuration(status.max_lead_time());
    DCHECK(!new_max_lead_time.is_zero());
    if (new_max_lead_time != max_lead_time_) {
      max_lead_time_ = new_max_lead_time;
      reschedule_read_timer = true;
    }
  }

  if (reschedule_read_timer) {
    read_timer_.Stop();
    ScheduleReadDemuxerStream();
  }

  RequestAudioConsumerStatus();
}

void FuchsiaAudioRenderer::ScheduleReadDemuxerStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!renderer_started_ || !demuxer_stream_ || read_timer_.IsRunning() ||
      is_demuxer_read_pending_ || is_at_end_of_stream_) {
    return;
  }

  base::TimeDelta next_read_delay;
  if (!last_packet_timestamp_.is_min()) {
    std::vector<base::TimeTicks> wall_clock_times;
    bool is_time_moving =
        GetWallClockTimes({last_packet_timestamp_}, &wall_clock_times);
    base::TimeDelta relative_buffer_pos =
        wall_clock_times[0] - base::TimeTicks::Now();

    // Check if we have buffered more than |max_lead_time_|.
    if (relative_buffer_pos >= max_lead_time_) {
      // If playback is not active then there is no need to buffer more.
      if (!is_time_moving)
        return;

      // If the buffer is larger than |max_lead_time_|, then the next read
      // should be delayed.
      next_read_delay = relative_buffer_pos - max_lead_time_;
    }
  }

  read_timer_.Start(FROM_HERE, next_read_delay,
                    base::BindOnce(&FuchsiaAudioRenderer::ReadDemuxerStream,
                                   base::Unretained(this)));
}

void FuchsiaAudioRenderer::ReadDemuxerStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(demuxer_stream_);
  DCHECK(!is_demuxer_read_pending_);

  is_demuxer_read_pending_ = true;
  demuxer_stream_->Read(
      base::BindOnce(&FuchsiaAudioRenderer::OnDemuxerStreamReadDone,
                     weak_factory_.GetWeakPtr()));
}

void FuchsiaAudioRenderer::OnDemuxerStreamReadDone(
    DemuxerStream::Status read_status,
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_demuxer_read_pending_);

  is_demuxer_read_pending_ = false;

  if (drop_next_demuxer_read_result_) {
    drop_next_demuxer_read_result_ = false;
    ScheduleReadDemuxerStream();
    return;
  }

  if (read_status != DemuxerStream::kOk) {
    if (read_status == DemuxerStream::kError) {
      OnError(PIPELINE_ERROR_READ);
    } else if (read_status == DemuxerStream::kConfigChanged) {
      stream_sink_.Unbind();
      sysmem_buffer_stream_->Reset();

      InitializeStreamSink();
      client_->OnAudioConfigChange(demuxer_stream_->audio_decoder_config());
    } else {
      DCHECK_EQ(read_status, DemuxerStream::kAborted);
    }
    return;
  }

  if (buffer->end_of_stream()) {
    is_at_end_of_stream_ = true;
  } else {
    if (buffer->data_size() > kBufferSize) {
      DLOG(ERROR) << "Demuxer returned buffer that is too big: "
                  << buffer->data_size();
      OnError(AUDIO_RENDERER_ERROR);
      return;
    }

    last_packet_timestamp_ = buffer->timestamp();
    if (buffer->duration() != kNoTimestamp)
      last_packet_timestamp_ += buffer->duration();
  }

  // Update layout for 24-bit PCM streams.
  if (!buffer->end_of_stream() &&
      demuxer_stream_->audio_decoder_config().codec() == AudioCodec::kPCM &&
      demuxer_stream_->audio_decoder_config().sample_format() ==
          kSampleFormatS24) {
    buffer = PreparePcm24Buffer(std::move(buffer));
  }

  sysmem_buffer_stream_->EnqueueBuffer(std::move(buffer));

  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::SendInputPacket(
    StreamProcessorHelper::IoPacket packet) {
  fuchsia::media::StreamPacket stream_packet;
  stream_packet.payload_buffer_id = packet.buffer_index();
  stream_packet.pts = packet.timestamp().ToZxDuration();
  stream_packet.payload_offset = packet.offset();
  stream_packet.payload_size = packet.size();

  stream_sink_->SendPacket(
      std::move(stream_packet),
      [this, packet = std::make_unique<StreamProcessorHelper::IoPacket>(
                 std::move(packet))]() mutable {
        OnStreamSendDone(std::move(packet));
      });

  // AudioConsumer doesn't report exact time when the data is decoded, but it's
  // safe to report it as decoded right away since the packet is expected to be
  // decoded soon after AudioConsumer receives it.
  PipelineStatistics stats;
  stats.audio_bytes_decoded = packet.size();
  client_->OnStatisticsUpdate(stats);
}

void FuchsiaAudioRenderer::OnStreamSendDone(
    std::unique_ptr<StreamProcessorHelper::IoPacket> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Check if we need to update buffering state after sending more than
  // |min_lead_time_| to the AudioConsumer.
  if (buffer_state_ == BUFFERING_HAVE_NOTHING) {
    std::vector<base::TimeTicks> wall_clock_times;
    GetWallClockTimes({packet->timestamp()}, &wall_clock_times);
    base::TimeDelta relative_buffer_pos =
        wall_clock_times[0] - base::TimeTicks::Now();
    if (relative_buffer_pos >= min_lead_time_)
      SetBufferState(BUFFERING_HAVE_ENOUGH);
  }

  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::SetBufferState(BufferingState buffer_state) {
  if (buffer_state != buffer_state_) {
    buffer_state_ = buffer_state;
    client_->OnBufferingStateChange(buffer_state_,
                                    BUFFERING_CHANGE_REASON_UNKNOWN);
  }
}

void FuchsiaAudioRenderer::FlushInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(GetPlaybackState() == PlaybackState::kStopped || is_at_end_of_stream_);

  if (stream_sink_)
    stream_sink_->DiscardAllPacketsNoReply();

  SetBufferState(BUFFERING_HAVE_NOTHING);
  last_packet_timestamp_ = base::TimeDelta::Min();
  read_timer_.Stop();
  is_at_end_of_stream_ = false;

  if (is_demuxer_read_pending_) {
    drop_next_demuxer_read_result_ = true;
  }
}

void FuchsiaAudioRenderer::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->OnEnded();
}

bool FuchsiaAudioRenderer::IsTimeMoving() {
  return state_ == PlaybackState::kPlaying && media_delta_ > 0;
}

void FuchsiaAudioRenderer::UpdateTimelineOnStop() {
  if (!IsTimeMoving())
    return;

  media_pos_ = CurrentMediaTimeLocked();
  reference_time_ = base::TimeTicks::Now();
  media_delta_ = 0;
}

base::TimeDelta FuchsiaAudioRenderer::CurrentMediaTimeLocked() {
  DCHECK(IsTimeMoving());

  // Calculate media position using formula specified by the TimelineFunction.
  // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
  return media_pos_ + (base::TimeTicks::Now() - reference_time_) *
                          media_delta_ / reference_delta_;
}

void FuchsiaAudioRenderer::OnSysmemBufferStreamBufferCollectionToken(
    fuchsia::sysmem::BufferCollectionTokenPtr token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Drop old buffers.
  input_buffers_.clear();
  stream_sink_.Unbind();

  // Acquire buffers for the new buffer collection.
  input_buffer_collection_ =
      sysmem_allocator_.BindSharedCollection(std::move(token));
  fuchsia::sysmem::BufferCollectionConstraints buffer_constraints =
      VmoBuffer::GetRecommendedConstraints(kNumBuffers, kBufferSize,
                                           /*writable=*/false);
  input_buffer_collection_->Initialize(std::move(buffer_constraints),
                                       "CrAudioRenderer");
  input_buffer_collection_->AcquireBuffers(base::BindOnce(
      &FuchsiaAudioRenderer::OnBuffersAcquired, base::Unretained(this)));
}

void FuchsiaAudioRenderer::OnSysmemBufferStreamOutputPacket(
    StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stream_sink_) {
    SendInputPacket(std::move(packet));
  } else {
    // The packet will be sent after StreamSink is connected.
    delayed_packets_.push_back(std::move(packet));
  }

  ScheduleReadDemuxerStream();
}

void FuchsiaAudioRenderer::OnSysmemBufferStreamEndOfStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_at_end_of_stream_);

  // Stream sink is not bound yet, don't send EOS.
  if (!stream_sink_)
    return;

  stream_sink_->EndOfStream();

  // No more data is going to be buffered. Update buffering state to ensure
  // RendererImpl starts playback in case it was waiting for buffering to
  // finish.
  SetBufferState(BUFFERING_HAVE_ENOUGH);
}

void FuchsiaAudioRenderer::OnSysmemBufferStreamError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnError(AUDIO_RENDERER_ERROR);
}

void FuchsiaAudioRenderer::OnSysmemBufferStreamNoKey() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->OnWaiting(WaitingReason::kNoDecryptionKey);
}

}  // namespace media
