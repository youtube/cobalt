// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/progressive/demuxer_extension_wrapper.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/encryption_scheme.h"
#include "media/base/sample_format.h"
#include "media/base/starboard_utils.h"
#include "media/base/video_types.h"
#include "media/filters/h264_to_annex_b_bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#include "starboard/extension/demuxer.h"
#include "starboard/system.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cobalt {
namespace media {

using ::media::AudioCodec;
using ::media::AudioDecoderConfig;
using ::media::ChannelLayout;
using ::media::DecoderBuffer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::EncryptionScheme;
using ::media::H264ToAnnexBBitstreamConverter;
using ::media::MediaTrack;
using ::media::PipelineStatus;
using ::media::PipelineStatusCallback;
using ::media::PipelineStatusCB;
using ::media::Ranges;
using ::media::SampleFormat;
using ::media::VideoCodec;
using ::media::VideoCodecProfile;
using ::media::VideoColorSpace;
using ::media::VideoDecoderConfig;
using ::media::VideoPixelFormat;
using ::media::VideoTransformation;
using ::media::mp4::AVCDecoderConfigurationRecord;

// Used to convert a lambda to a pure C function.
// |user_data| is a callback of type T, which takes a U*.
template <typename T, typename U>
static void CallCB(U* u, void* user_data) {
  (*static_cast<T*>(user_data))(u);
}

// Converts AVCC h.264 frames to Annex B. This is necessary because the decoder
// expects packets in Annex B format.
class DemuxerExtensionWrapper::H264AnnexBConverter {
 public:
  // Creates an H264AnnexBConverter from the MP4 file's header data.
  static std::unique_ptr<H264AnnexBConverter> Create(const uint8_t* extra_data,
                                                     size_t extra_data_size) {
    if (!extra_data || extra_data_size == 0) {
      LOG(ERROR) << "Invalid inputs to H264AnnexBConverter::Create.";
      return nullptr;
    }
    AVCDecoderConfigurationRecord config;
    std::unique_ptr<H264ToAnnexBBitstreamConverter> converter(
        new H264ToAnnexBBitstreamConverter);
    if (!converter->ParseConfiguration(
            extra_data, static_cast<int>(extra_data_size), &config)) {
      LOG(ERROR) << "Could not parse AVCC config.";
      return nullptr;
    }
    return std::unique_ptr<H264AnnexBConverter>(
        new H264AnnexBConverter(std::move(config), std::move(converter)));
  }

  // Disallow copy and assign.
  H264AnnexBConverter(const H264AnnexBConverter&) = delete;
  H264AnnexBConverter& operator=(const H264AnnexBConverter&) = delete;

  ~H264AnnexBConverter() = default;

  // Attempts to convert the data in |data| from AVCC to AnnexB format,
  // returning the data as a DecoderBuffer. Upon failure, the data will be
  // returned unmodified in the DecoderBuffer.
  scoped_refptr<DecoderBuffer> Convert(const uint8_t* data, size_t data_size) {
    const auto* const config = config_.has_value() ? &*config_ : nullptr;

    std::vector<uint8_t> rewritten(
        converter_->CalculateNeededOutputBufferSize(data, data_size, config));

    uint32_t rewritten_size = rewritten.size();
    if (rewritten.empty() ||
        !converter_->ConvertNalUnitStreamToByteStream(
            data, data_size, config, rewritten.data(), &rewritten_size)) {
      // TODO(b/231994311): Add the buffer's side_data here, for HDR10+ support.
      return DecoderBuffer::CopyFrom(data, data_size);
    } else {
      // The data was successfully rewritten.

      // The SPS and PPS NALUs -- generated from the config -- should only be
      // sent with the first real NALU.
      config_ = base::nullopt;

      // TODO(b/231994311): Add the buffer's side_data here, for HDR10+ support.
      return DecoderBuffer::CopyFrom(rewritten.data(), rewritten.size());
    }
  }

 private:
  explicit H264AnnexBConverter(
      AVCDecoderConfigurationRecord config,
      std::unique_ptr<H264ToAnnexBBitstreamConverter> converter)
      : config_(std::move(config)), converter_(std::move(converter)) {}

  // This config data is only sent with the first NALU (as SPS and PPS NALUs).
  base::Optional<AVCDecoderConfigurationRecord> config_;
  std::unique_ptr<H264ToAnnexBBitstreamConverter> converter_;
};

DemuxerExtensionStream::DemuxerExtensionStream(
    CobaltExtensionDemuxer* demuxer,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CobaltExtensionDemuxerVideoDecoderConfig config)
    : demuxer_(demuxer), task_runner_(std::move(task_runner)) {
  CHECK(demuxer_);
  CHECK(task_runner_);
  std::vector<uint8_t> extra_data;
  if (config.extra_data_size > 0 && config.extra_data != nullptr) {
    extra_data.assign(config.extra_data,
                      config.extra_data + config.extra_data_size);
  }

  video_config_.emplace(
      static_cast<VideoCodec>(config.codec),
      static_cast<VideoCodecProfile>(config.profile),
      static_cast<VideoDecoderConfig::AlphaMode>(config.alpha_mode),
      VideoColorSpace(
          config.color_space_primaries, config.color_space_transfer,
          config.color_space_matrix,
          static_cast<gfx::ColorSpace::RangeID>(config.color_space_range_id)),
      VideoTransformation(), gfx::Size(config.coded_width, config.coded_height),
      gfx::Rect(config.visible_rect_x, config.visible_rect_y,
                config.visible_rect_width, config.visible_rect_height),
      gfx::Size(config.natural_width, config.natural_height), extra_data,
      static_cast<EncryptionScheme>(config.encryption_scheme));

  LOG_IF(ERROR, !video_config_->IsValidConfig())
      << "Video config is not valid!";
}

DemuxerExtensionStream::DemuxerExtensionStream(
    CobaltExtensionDemuxer* demuxer,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CobaltExtensionDemuxerAudioDecoderConfig config)
    : demuxer_(demuxer), task_runner_(std::move(task_runner)) {
  CHECK(demuxer_);
  CHECK(task_runner_);
  std::vector<uint8_t> extra_data;
  if (config.extra_data_size > 0 && config.extra_data != nullptr) {
    extra_data.assign(config.extra_data,
                      config.extra_data + config.extra_data_size);
  }

  audio_config_.emplace(
      static_cast<AudioCodec>(config.codec),
      static_cast<SampleFormat>(config.sample_format),
      static_cast<ChannelLayout>(config.channel_layout),
      config.samples_per_second, extra_data,
      static_cast<EncryptionScheme>(config.encryption_scheme));

  LOG_IF(ERROR, !audio_config_->IsValidConfig())
      << "Audio config is not valid!";
}

void DemuxerExtensionStream::Read(uint32_t count, ReadCB read_cb) {
  DCHECK(!read_cb.is_null());
  base::AutoLock auto_lock(lock_);
  if (stopped_) {
    LOG(INFO) << "Already stopped.";
    std::vector<scoped_refptr<DecoderBuffer>> buffers;
    buffers.push_back(std::move(DecoderBuffer::CreateEOSBuffer()));
    std::move(read_cb).Run(DemuxerStream::kOk, buffers);
    return;
  }

  // Buffers are only queued when there are no pending reads.
  CHECK(buffer_queue_.empty() || read_queue_.empty());

  if (buffer_queue_.empty()) {
    read_queue_.push_back(std::move(read_cb));
    return;
  }

  // We already have a buffer queued. Send the oldest buffer back.
  scoped_refptr<DecoderBuffer> buffer = buffer_queue_.front();
  if (!buffer->end_of_stream()) {
    // Do not pop EOS buffers, so that subsequent read requests also get EOS.
    total_buffer_size_ -= buffer->data_size();
    buffer_queue_.pop_front();
  }

  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  buffers.push_back(std::move(buffer));
  std::move(read_cb).Run(DemuxerStream::kOk, buffers);
}

AudioDecoderConfig DemuxerExtensionStream::audio_decoder_config() {
  DCHECK(audio_config_.has_value());
  return *audio_config_;
}

VideoDecoderConfig DemuxerExtensionStream::video_decoder_config() {
  DCHECK(video_config_.has_value());
  return *video_config_;
}

DemuxerStream::Type DemuxerExtensionStream::type() const {
  const uint8_t is_audio = static_cast<int>(audio_config_.has_value());
  const uint8_t is_video = static_cast<int>(video_config_.has_value());
  DCHECK((is_audio ^ is_video) == 1);
  return is_audio ? Type::AUDIO : Type::VIDEO;
}

Ranges<base::TimeDelta> DemuxerExtensionStream::GetBufferedRanges() {
  return buffered_ranges_;
}

void DemuxerExtensionStream::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  base::AutoLock auto_lock(lock_);

  if (stopped_) {
    // It is possible due to pipelining -- both downstream and within the
    // demuxer -- that several pipelined reads will be enqueuing packets on a
    // stopped stream. These will be dropped.
    LOG(WARNING) << "attempted to enqueue packet on stopped stream";
    return;
  }

  if (buffer->end_of_stream()) {
    LOG(INFO) << "Received EOS";
  } else if (buffer->timestamp() != ::media::kNoTimestamp) {
    if (last_buffer_timestamp_ != ::media::kNoTimestamp &&
        last_buffer_timestamp_ < buffer->timestamp()) {
      buffered_ranges_.Add(last_buffer_timestamp_, buffer->timestamp());
    }
    last_buffer_timestamp_ = buffer->timestamp();
  } else {
    LOG(WARNING) << "Bad timestamp info on enqueued buffer.";
  }

  if (read_queue_.empty()) {
    buffer_queue_.push_back(buffer);
    if (!buffer->end_of_stream()) {
      total_buffer_size_ += buffer->data_size();
    }
    return;
  }

  // A pending read implies that the buffer queue was empty; otherwise it should
  // never have been added to the read queue in the first place.
  CHECK_EQ(buffer_queue_.size(), 0);
  ReadCB read_cb(std::move(read_queue_.front()));
  read_queue_.pop_front();
  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  buffers.push_back(std::move(buffer));
  std::move(read_cb).Run(DemuxerStream::kOk, buffers);
}

void DemuxerExtensionStream::FlushBuffers() {
  base::AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  total_buffer_size_ = 0;
  last_buffer_timestamp_ = ::media::kNoTimestamp;
}

void DemuxerExtensionStream::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  total_buffer_size_ = 0;
  last_buffer_timestamp_ = ::media::kNoTimestamp;
  // Fulfill any pending callbacks with EOS buffers set to end timestamp.
  for (auto& read_cb : read_queue_) {
    std::vector<scoped_refptr<DecoderBuffer>> buffers;
    buffers.push_back(std::move(DecoderBuffer::CreateEOSBuffer()));
    std::move(read_cb).Run(DemuxerStream::kOk, buffers);
  }
  read_queue_.clear();
  stopped_ = true;
}

base::TimeDelta DemuxerExtensionStream::GetLastBufferTimestamp() const {
  base::AutoLock auto_lock(lock_);
  return last_buffer_timestamp_;
}

size_t DemuxerExtensionStream::GetTotalBufferSize() const {
  base::AutoLock auto_lock(lock_);
  return total_buffer_size_;
}

PositionalDataSource::PositionalDataSource(
    scoped_refptr<DataSourceReader> reader)
    : reader_(std::move(reader)), position_(0) {
  CHECK(reader_);
}

PositionalDataSource::~PositionalDataSource() = default;

void PositionalDataSource::Stop() { reader_->Stop(); }

int PositionalDataSource::BlockingRead(uint8_t* data, int bytes_requested) {
  const int bytes_read =
      reader_->BlockingRead(position_, bytes_requested, data);
  if (bytes_read != DataSourceReader::kReadError) {
    position_ += bytes_read;
  }
  return bytes_read;
}

void PositionalDataSource::SeekTo(int position) { position_ = position; }

int64_t PositionalDataSource::GetPosition() const { return position_; }

int64_t PositionalDataSource::GetSize() { return reader_->FileSize(); }

// Functions for converting a PositionalDataSource to
// CobaltExtensionDemuxerDataSource.
static int CobaltExtensionDemuxerDataSource_BlockingReadRead(
    uint8_t* data, int bytes_requested, void* user_data) {
  return static_cast<PositionalDataSource*>(user_data)->BlockingRead(
      data, bytes_requested);
}

static void CobaltExtensionDemuxerDataSource_SeekTo(int position,
                                                    void* user_data) {
  static_cast<PositionalDataSource*>(user_data)->SeekTo(position);
}

static int64_t CobaltExtensionDemuxerDataSource_GetPosition(void* user_data) {
  return static_cast<PositionalDataSource*>(user_data)->GetPosition();
}

static int64_t CobaltExtensionDemuxerDataSource_GetSize(void* user_data) {
  return static_cast<PositionalDataSource*>(user_data)->GetSize();
}

std::unique_ptr<DemuxerExtensionWrapper> DemuxerExtensionWrapper::Create(
    DataSource* data_source,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const CobaltExtensionDemuxerApi* demuxer_api) {
  if (demuxer_api == nullptr) {
    // Attempt to use the Cobalt extension.
    demuxer_api = static_cast<const CobaltExtensionDemuxerApi*>(
        SbSystemGetExtension(kCobaltExtensionDemuxerApi));
    if (!demuxer_api ||
        strcmp(demuxer_api->name, kCobaltExtensionDemuxerApi) != 0) {
      return nullptr;
    }
  }

  DCHECK(demuxer_api);
  if (demuxer_api->version < 1) {
    LOG(ERROR) << "Demuxer API version is too low: " << demuxer_api->version;
    return nullptr;
  }

  if (!data_source || !task_runner) {
    LOG(ERROR) << "data_source and task_runner cannot be null.";
    return nullptr;
  }

  scoped_refptr<DataSourceReader> reader = new DataSourceReader;
  reader->SetDataSource(data_source);

  std::unique_ptr<PositionalDataSource> positional_data_source(
      new PositionalDataSource(std::move(reader)));

  std::unique_ptr<CobaltExtensionDemuxerDataSource> c_data_source(
      new CobaltExtensionDemuxerDataSource{
          /*BlockingRead=*/&CobaltExtensionDemuxerDataSource_BlockingReadRead,
          /*SeekTo=*/&CobaltExtensionDemuxerDataSource_SeekTo,
          /*GetPosition=*/&CobaltExtensionDemuxerDataSource_GetPosition,
          /*GetSize=*/&CobaltExtensionDemuxerDataSource_GetSize,
          /*is_streaming=*/false,
          /*user_data=*/positional_data_source.get()});

  // TODO(b/231632632): Populate these vectors.
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = demuxer_api->CreateDemuxer(
      c_data_source.get(), supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  if (!demuxer) {
    LOG(ERROR) << "Failed to create a CobaltExtensionDemuxer.";
    return nullptr;
  }

  return std::unique_ptr<DemuxerExtensionWrapper>(new DemuxerExtensionWrapper(
      demuxer_api, demuxer, std::move(positional_data_source),
      std::move(c_data_source), std::move(task_runner)));
}

DemuxerExtensionWrapper::DemuxerExtensionWrapper(
    const CobaltExtensionDemuxerApi* demuxer_api,
    CobaltExtensionDemuxer* demuxer,
    std::unique_ptr<PositionalDataSource> data_source,
    std::unique_ptr<CobaltExtensionDemuxerDataSource> c_data_source,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : demuxer_api_(demuxer_api),
      impl_(demuxer),
      data_source_(std::move(data_source)),
      c_data_source_(std::move(c_data_source)),
      blocking_thread_("DemuxerExtensionWrapperBlockingThread"),
      task_runner_(std::move(task_runner)) {
  CHECK(demuxer_api_);
  CHECK(impl_);
  CHECK(data_source_);
  CHECK(c_data_source_);
  CHECK(task_runner_);
}

DemuxerExtensionWrapper::~DemuxerExtensionWrapper() {
  if (impl_) {
    demuxer_api_->DestroyDemuxer(impl_);
  }
  // Explicitly stop |blocking_thread_| to ensure that it stops before the
  // destruction of any other members.
  blocking_thread_.Stop();
}

std::vector<DemuxerStream*> DemuxerExtensionWrapper::GetAllStreams() {
  std::vector<DemuxerStream*> streams;
  if (audio_stream_.has_value()) {
    streams.push_back(&*audio_stream_);
  }
  if (video_stream_.has_value()) {
    streams.push_back(&*video_stream_);
  }
  return streams;
}

std::string DemuxerExtensionWrapper::GetDisplayName() const {
  return "DemuxerExtensionWrapper";
}
void DemuxerExtensionWrapper::Initialize(DemuxerHost* host,
                                         PipelineStatusCallback status_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  host_ = host;

  // Start the blocking thread and have it download and parse the media config.
  if (!blocking_thread_.Start()) {
    LOG(ERROR) << "Unable to start blocking thread";
    std::move(status_cb).Run(::media::DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // |status_cb| cannot be called until this function returns, so we post a task
  // here.
  blocking_thread_.task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(impl_->Initialize, impl_->user_data),
      base::BindOnce(&DemuxerExtensionWrapper::OnInitializeDone,
                     base::Unretained(this), std::move(status_cb)));
}

void DemuxerExtensionWrapper::OnInitializeDone(
    PipelineStatusCallback status_cb, CobaltExtensionDemuxerStatus status) {
  if (status == kCobaltExtensionDemuxerOk) {
    // Set up the stream(s) on this end.
    CobaltExtensionDemuxerAudioDecoderConfig audio_config = {};
    if (impl_->GetAudioConfig(&audio_config, impl_->user_data)) {
      if (audio_config.encryption_scheme !=
          kCobaltExtensionDemuxerEncryptionSchemeUnencrypted) {
        // TODO(b/232957482): Determine whether we need to handle this case.
        LOG(ERROR)
            << "Encrypted audio is not supported for progressive playback.";
        std::move(status_cb).Run(::media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
        return;
      }
      audio_stream_.emplace(impl_, task_runner_, std::move(audio_config));
    }
    CobaltExtensionDemuxerVideoDecoderConfig video_config = {};
    if (impl_->GetVideoConfig(&video_config, impl_->user_data)) {
      if (video_config.encryption_scheme !=
          kCobaltExtensionDemuxerEncryptionSchemeUnencrypted) {
        // TODO(b/232957482): Determine whether we need to handle this case.
        LOG(ERROR)
            << "Encrypted video is not supported for progressive playback.";
        std::move(status_cb).Run(::media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
        return;
      }
      if (video_config.extra_data && video_config.extra_data_size > 0 &&
          video_config.codec == kCobaltExtensionDemuxerCodecH264) {
        // This is probably an AVCC stream. We'll need to convert each packet
        // from AVCC to AnnexB, so we create the converter based on the "extra
        // data". This extra data will be passed in the form of SPS and PPS NALU
        // packets in the AnnexB stream.
        h264_converter_ = H264AnnexBConverter::Create(
            video_config.extra_data, video_config.extra_data_size);
        video_config.extra_data = nullptr;
        video_config.extra_data_size = 0;
      }
      video_stream_.emplace(impl_, task_runner_, std::move(video_config));
    }

    if (!audio_stream_.has_value() && !video_stream_.has_value()) {
      // Even though initialization seems to have succeeded, something is wrong
      // if there are no streams.
      LOG(ERROR) << "No streams are present";
      std::move(status_cb).Run(::media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
      return;
    }

    host_->SetDuration(base::TimeDelta::FromMicroseconds(
        impl_->GetDuration(impl_->user_data)));

    // Begin downloading data.
    Request(audio_stream_.has_value() ? DemuxerStream::AUDIO
                                      : DemuxerStream::VIDEO);
  } else {
    LOG(ERROR) << "Initialization failed with status " << status;
  }
  std::move(status_cb).Run(static_cast<PipelineStatus::Codes>(status));
}

void DemuxerExtensionWrapper::AbortPendingReads() {}

void DemuxerExtensionWrapper::StartWaitingForSeek(base::TimeDelta seek_time) {}

void DemuxerExtensionWrapper::CancelPendingSeek(base::TimeDelta seek_time) {}

void DemuxerExtensionWrapper::Seek(base::TimeDelta time,
                                   PipelineStatusCallback status_cb) {
  // It's safe to use base::Unretained here because blocking_thread_ will be
  // stopped in this class's destructor.
  blocking_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DemuxerExtensionWrapper::SeekTask, base::Unretained(this),
                     time, BindPostTaskToCurrentDefault(std::move(status_cb))));
}

// TODO(b/232984963): Determine whether it's OK to have reads and seeks on the
// same thread.
void DemuxerExtensionWrapper::SeekTask(base::TimeDelta time,
                                       PipelineStatusCallback status_cb) {
  CHECK(blocking_thread_.task_runner()->RunsTasksInCurrentSequence());

  // clear any enqueued buffers on demuxer streams
  if (video_stream_.has_value()) video_stream_->FlushBuffers();
  if (audio_stream_.has_value()) audio_stream_->FlushBuffers();

  const CobaltExtensionDemuxerStatus status =
      impl_->Seek(time.InMicroseconds(), impl_->user_data);

  if (status != kCobaltExtensionDemuxerOk) {
    LOG(ERROR) << "Seek failed with status " << status;
    std::move(status_cb).Run(::media::PIPELINE_ERROR_READ);
    return;
  }

  // If all streams had finished downloading, we need to restart the request.
  const bool issue_new_request =
      (!video_stream_.has_value() || video_reached_eos_) &&
      (!audio_stream_.has_value() || audio_reached_eos_);
  audio_reached_eos_ = false;
  video_reached_eos_ = false;
  flushing_ = true;
  std::move(status_cb).Run(::media::PIPELINE_OK);

  if (issue_new_request) {
    IssueNextRequest();
  }
}

Ranges<base::TimeDelta> DemuxerExtensionWrapper::GetBufferedRanges() {
  DCHECK(audio_stream_.has_value() || video_stream_.has_value());

  if (!audio_stream_.has_value()) {
    return video_stream_->GetBufferedRanges();
  }
  if (!video_stream_.has_value()) {
    return audio_stream_->GetBufferedRanges();
  }
  return video_stream_->GetBufferedRanges().IntersectionWith(
      audio_stream_->GetBufferedRanges());
}

void DemuxerExtensionWrapper::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_for_stopped_);
    stopped_ = true;
  }
  data_source_->Stop();
}

base::TimeDelta DemuxerExtensionWrapper::GetStartTime() const {
  return base::TimeDelta::FromMicroseconds(
      impl_->GetStartTime(impl_->user_data));
}

base::Time DemuxerExtensionWrapper::GetTimelineOffset() const {
  const base::TimeDelta reported_time = base::TimeDelta::FromMicroseconds(
      impl_->GetTimelineOffset(impl_->user_data));
  return reported_time.is_zero()
             ? base::Time()
             : base::Time::FromDeltaSinceWindowsEpoch(reported_time);
}

int64_t DemuxerExtensionWrapper::GetMemoryUsage() const {
  NOTREACHED();
  return 0;
}

void DemuxerExtensionWrapper::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids, base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  NOTREACHED();
}

void DemuxerExtensionWrapper::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids, base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  NOTREACHED();
}

void DemuxerExtensionWrapper::Request(DemuxerStream::Type type) {
  static const auto kRequestDelay = base::TimeDelta::FromMilliseconds(100);

  if (type == DemuxerStream::AUDIO) {
    DCHECK(audio_stream_.has_value());
  } else {
    DCHECK(video_stream_.has_value());
  }

  if (!blocking_thread_.task_runner()->RunsTasksInCurrentSequence()) {
    blocking_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&DemuxerExtensionWrapper::Request,
                              base::Unretained(this), type));
    return;
  }

  if (HasStopped()) {
    return;
  }

  const size_t total_buffer_size =
      (audio_stream_.has_value() ? audio_stream_->GetTotalBufferSize() : 0) +
      (video_stream_.has_value() ? video_stream_->GetTotalBufferSize() : 0);

  int progressive_budget = 0;
  if (video_stream_.has_value()) {
    const VideoDecoderConfig video_config =
        video_stream_->video_decoder_config();
    // Only sdr video is supported in progressive mode.
    // TODO(b/231994311): Figure out how to set this value properly.
    constexpr int kBitDepth = 8;
    progressive_budget = SbMediaGetProgressiveBufferBudget(
        MediaVideoCodecToSbMediaVideoCodec(video_config.codec()),
        video_config.visible_rect().size().width(),
        video_config.visible_rect().size().height(), kBitDepth);
  } else {
    progressive_budget = SbMediaGetAudioBufferBudget();
  }

  if (total_buffer_size >= progressive_budget) {
    // Retry after a delay.
    blocking_thread_.task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DemuxerExtensionWrapper::Request, base::Unretained(this),
                   type),
        kRequestDelay);
    return;
  }

  scoped_refptr<DecoderBuffer> decoder_buffer;
  bool called_cb = false;
  auto read_cb = [this, type, &decoder_buffer,
                  &called_cb](CobaltExtensionDemuxerBuffer* buffer) {
    called_cb = true;
    if (!buffer) {
      return;
    }

    if (buffer->end_of_stream) {
      decoder_buffer = DecoderBuffer::CreateEOSBuffer();
      return;
    }

    if (h264_converter_ && type == DemuxerExtensionStream::VIDEO) {
      // This converts from AVCC to AnnexB format for h.264 video.
      decoder_buffer =
          h264_converter_->Convert(buffer->data, buffer->data_size);
    } else {
      // TODO(b/231994311): Add the buffer's side_data here, for HDR10+ support.
      decoder_buffer = DecoderBuffer::CopyFrom(buffer->data, buffer->data_size);
    }

    decoder_buffer->set_timestamp(
        base::TimeDelta::FromMicroseconds(buffer->pts));
    decoder_buffer->set_duration(
        base::TimeDelta::FromMicroseconds(buffer->duration));
    decoder_buffer->set_is_key_frame(buffer->is_keyframe);
  };
  impl_->Read(static_cast<CobaltExtensionDemuxerStreamType>(type),
              &CallCB<decltype(read_cb), CobaltExtensionDemuxerBuffer>,
              &read_cb, impl_->user_data);

  if (!called_cb) {
    LOG(ERROR)
        << "Demuxer extension implementation did not call the read callback.";
    host_->OnDemuxerError(::media::PIPELINE_ERROR_READ);
    return;
  }
  if (!decoder_buffer) {
    LOG(ERROR) << "Received a null buffer from the demuxer.";
    host_->OnDemuxerError(::media::PIPELINE_ERROR_READ);
    return;
  }

  auto& stream =
      (type == DemuxerStream::AUDIO) ? *audio_stream_ : *video_stream_;
  bool& eos_status =
      (type == DemuxerStream::AUDIO) ? audio_reached_eos_ : video_reached_eos_;

  eos_status = decoder_buffer->end_of_stream();
  stream.EnqueueBuffer(std::move(decoder_buffer));
  if (!eos_status) {
    host_->OnBufferedTimeRangesChanged(GetBufferedRanges());
  }

  // If we reach this point, enqueueing the buffer was successful.
  IssueNextRequest();
  return;
}

void DemuxerExtensionWrapper::IssueNextRequest() {
  {
    base::AutoLock lock(lock_for_stopped_);
    if (stopped_) {
      LOG(INFO) << "Already stopped; request loop is stopping.";
      return;
    }
  }

  DemuxerStream::Type type = DemuxerStream::UNKNOWN;
  if (audio_reached_eos_ || video_reached_eos_) {
    // If we have eos in one or both buffers, the decision is easy.
    if ((audio_reached_eos_ && video_reached_eos_) ||
        (audio_reached_eos_ && !video_stream_.has_value()) ||
        (video_reached_eos_ && !audio_stream_.has_value())) {
      LOG(INFO) << "All streams at EOS, request loop is stopping.";
      return;
    }
    // Only one of two streams is at eos; download data for the stream NOT at
    // eos.
    type = audio_reached_eos_ ? DemuxerStream::VIDEO : DemuxerStream::AUDIO;
  } else if (!audio_stream_.has_value() || !video_stream_.has_value()) {
    // If only one stream is present and not at eos, just download that data.
    type =
        audio_stream_.has_value() ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
  } else {
    // Both streams are present, and neither is at eos. Priority order for
    // figuring out what to download next.
    const base::TimeDelta audio_stamp = audio_stream_->GetLastBufferTimestamp();
    const base::TimeDelta video_stamp = video_stream_->GetLastBufferTimestamp();
    // If the audio demuxer stream is empty, always fill it first.
    if (audio_stamp == ::media::kNoTimestamp) {
      type = DemuxerStream::AUDIO;
    } else if (video_stamp == ::media::kNoTimestamp) {
      // The video demuxer stream is empty; we need data for it.
      type = DemuxerStream::VIDEO;
    } else if (video_stamp < audio_stamp) {
      // Video is earlier; fill it first.
      type = DemuxerStream::VIDEO;
    } else {
      type = DemuxerStream::AUDIO;
    }
  }

  DCHECK_NE(type, DemuxerStream::UNKNOWN);
  // We cannot call Request() directly even if this function is also run on
  // |blocking_thread_| as otherwise it is possible that this function is
  // running in a tight loop and seek/stop requests would have no chance to kick
  // in.
  blocking_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&DemuxerExtensionWrapper::Request,
                            base::Unretained(this), type));
}

bool DemuxerExtensionWrapper::HasStopped() {
  base::AutoLock lock(lock_for_stopped_);
  return stopped_;
}

namespace {

// Ensure that the demuxer extension's enums match up with the internal enums.
// This doesn't affect any code, but prevents compilation if there's a mismatch
// somewhere.
#define DEMUXER_EXTENSION_ENUM_EQ(a, b) \
  COMPILE_ASSERT(static_cast<int>(a) == static_cast<int>(b), mismatching_enums)

// Pipeline status.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerOk, ::media::PIPELINE_OK);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorNetwork,
                          ::media::PIPELINE_ERROR_NETWORK);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorAbort,
                          ::media::PIPELINE_ERROR_ABORT);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorInitializationFailed,
                          ::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorRead,
                          ::media::PIPELINE_ERROR_READ);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorInvalidState,
                          ::media::PIPELINE_ERROR_INVALID_STATE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorCouldNotOpen,
                          ::media::DEMUXER_ERROR_COULD_NOT_OPEN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorCouldNotParse,
                          ::media::DEMUXER_ERROR_COULD_NOT_PARSE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerErrorNoSupportedStreams,
                          ::media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS);

// Audio codecs.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecUnknownAudio,
                          ::media::AudioCodec::kUnknown);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecAAC,
                          ::media::AudioCodec::kAAC);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecMP3,
                          ::media::AudioCodec::kMP3);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecPCM,
                          ::media::AudioCodec::kPCM);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecVorbis,
                          ::media::AudioCodec::kVorbis);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecFLAC,
                          ::media::AudioCodec::kFLAC);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecAMR_NB,
                          ::media::AudioCodec::kAMR_NB);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecAMR_WB,
                          ::media::AudioCodec::kAMR_WB);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecPCM_MULAW,
                          ::media::AudioCodec::kPCM_MULAW);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecGSM_MS,
                          ::media::AudioCodec::kGSM_MS);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecPCM_S16BE,
                          ::media::AudioCodec::kPCM_S16BE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecPCM_S24BE,
                          ::media::AudioCodec::kPCM_S24BE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecOpus,
                          ::media::AudioCodec::kOpus);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecEAC3,
                          ::media::AudioCodec::kEAC3);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecPCM_ALAW,
                          ::media::AudioCodec::kPCM_ALAW);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecALAC,
                          ::media::AudioCodec::kALAC);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecAC3,
                          ::media::AudioCodec::kAC3);


// Video codecs.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecUnknownVideo,
                          ::media::VideoCodec::kUnknown);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecH264,
                          ::media::VideoCodec::kH264);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecVC1,
                          ::media::VideoCodec::kVC1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecMPEG2,
                          ::media::VideoCodec::kMPEG2);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecMPEG4,
                          ::media::VideoCodec::kMPEG4);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecTheora,
                          ::media::VideoCodec::kTheora);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecVP8,
                          ::media::VideoCodec::kVP8);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecVP9,
                          ::media::VideoCodec::kVP9);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecHEVC,
                          ::media::VideoCodec::kHEVC);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecDolbyVision,
                          ::media::VideoCodec::kDolbyVision);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerCodecAV1,
                          ::media::VideoCodec::kAV1);

// Sample formats.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatUnknown,
                          ::media::kUnknownSampleFormat);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatU8,
                          ::media::kSampleFormatU8);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatS16,
                          ::media::kSampleFormatS16);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatS32,
                          ::media::kSampleFormatS32);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatF32,
                          ::media::kSampleFormatF32);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatPlanarS16,
                          ::media::kSampleFormatPlanarS16);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatPlanarF32,
                          ::media::kSampleFormatPlanarF32);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatPlanarS32,
                          ::media::kSampleFormatPlanarS32);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerSampleFormatS24,
                          ::media::kSampleFormatS24);


// Channel layouts.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutNone,
                          ::media::CHANNEL_LAYOUT_NONE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutUnsupported,
                          ::media::CHANNEL_LAYOUT_UNSUPPORTED);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutMono,
                          ::media::CHANNEL_LAYOUT_MONO);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutStereo,
                          ::media::CHANNEL_LAYOUT_STEREO);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout2_1,
                          ::media::CHANNEL_LAYOUT_2_1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutSurround,
                          ::media::CHANNEL_LAYOUT_SURROUND);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout4_0,
                          ::media::CHANNEL_LAYOUT_4_0);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout2_2,
                          ::media::CHANNEL_LAYOUT_2_2);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutQuad,
                          ::media::CHANNEL_LAYOUT_QUAD);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout5_0,
                          ::media::CHANNEL_LAYOUT_5_0);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout5_1,
                          ::media::CHANNEL_LAYOUT_5_1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout5_0Back,
                          ::media::CHANNEL_LAYOUT_5_0_BACK);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout5_1Back,
                          ::media::CHANNEL_LAYOUT_5_1_BACK);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout7_0,
                          ::media::CHANNEL_LAYOUT_7_0);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout7_1,
                          ::media::CHANNEL_LAYOUT_7_1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout7_1Wide,
                          ::media::CHANNEL_LAYOUT_7_1_WIDE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutStereoDownmix,
                          ::media::CHANNEL_LAYOUT_STEREO_DOWNMIX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout2point1,
                          ::media::CHANNEL_LAYOUT_2POINT1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout3_1,
                          ::media::CHANNEL_LAYOUT_3_1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout4_1,
                          ::media::CHANNEL_LAYOUT_4_1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout6_0,
                          ::media::CHANNEL_LAYOUT_6_0);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout6_0Front,
                          ::media::CHANNEL_LAYOUT_6_0_FRONT);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutHexagonal,
                          ::media::CHANNEL_LAYOUT_HEXAGONAL);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout6_1,
                          ::media::CHANNEL_LAYOUT_6_1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout6_1Back,
                          ::media::CHANNEL_LAYOUT_6_1_BACK);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout6_1Front,
                          ::media::CHANNEL_LAYOUT_6_1_FRONT);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout7_0Front,
                          ::media::CHANNEL_LAYOUT_7_0_FRONT);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout7_1WideBack,
                          ::media::CHANNEL_LAYOUT_7_1_WIDE_BACK);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutOctagonal,
                          ::media::CHANNEL_LAYOUT_OCTAGONAL);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutDiscrete,
                          ::media::CHANNEL_LAYOUT_DISCRETE);
DEMUXER_EXTENSION_ENUM_EQ(
    kCobaltExtensionDemuxerChannelLayoutStereoAndKeyboardMic,
    ::media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayout4_1QuadSide,
                          ::media::CHANNEL_LAYOUT_4_1_QUAD_SIDE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerChannelLayoutBitstream,
                          ::media::CHANNEL_LAYOUT_BITSTREAM);

// Video codec profiles.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVideoCodecProfileUnknown,
                          ::media::VIDEO_CODEC_PROFILE_UNKNOWN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileMin,
                          ::media::H264PROFILE_MIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileBaseline,
                          ::media::H264PROFILE_BASELINE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileMain,
                          ::media::H264PROFILE_MAIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileExtended,
                          ::media::H264PROFILE_EXTENDED);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileHigh,
                          ::media::H264PROFILE_HIGH);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileHigh10Profile,
                          ::media::H264PROFILE_HIGH10PROFILE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileHigh422Profile,
                          ::media::H264PROFILE_HIGH422PROFILE);
DEMUXER_EXTENSION_ENUM_EQ(
    kCobaltExtensionDemuxerH264ProfileHigh444PredictiveProfile,
    ::media::H264PROFILE_HIGH444PREDICTIVEPROFILE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileScalableBaseline,
                          ::media::H264PROFILE_SCALABLEBASELINE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileScalableHigh,
                          ::media::H264PROFILE_SCALABLEHIGH);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileStereoHigh,
                          ::media::H264PROFILE_STEREOHIGH);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileMultiviewHigh,
                          ::media::H264PROFILE_MULTIVIEWHIGH);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerH264ProfileMax,
                          ::media::H264PROFILE_MAX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp8ProfileMin,
                          ::media::VP8PROFILE_MIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp8ProfileAny,
                          ::media::VP8PROFILE_ANY);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp8ProfileMax,
                          ::media::VP8PROFILE_MAX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp9ProfileMin,
                          ::media::VP9PROFILE_MIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp9ProfileProfile0,
                          ::media::VP9PROFILE_PROFILE0);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp9ProfileProfile1,
                          ::media::VP9PROFILE_PROFILE1);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp9ProfileProfile2,
                          ::media::VP9PROFILE_PROFILE2);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp9ProfileProfile3,
                          ::media::VP9PROFILE_PROFILE3);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerVp9ProfileMax,
                          ::media::VP9PROFILE_MAX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerHevcProfileMin,
                          ::media::HEVCPROFILE_MIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerHevcProfileMain,
                          ::media::HEVCPROFILE_MAIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerHevcProfileMain10,
                          ::media::HEVCPROFILE_MAIN10);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerHevcProfileMainStillPicture,
                          ::media::HEVCPROFILE_MAIN_STILL_PICTURE);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerHevcProfileMax,
                          ::media::HEVCPROFILE_MAX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerDolbyVisionProfile0,
                          ::media::DOLBYVISION_PROFILE0);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerDolbyVisionProfile4,
                          ::media::DOLBYVISION_PROFILE4);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerDolbyVisionProfile5,
                          ::media::DOLBYVISION_PROFILE5);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerDolbyVisionProfile7,
                          ::media::DOLBYVISION_PROFILE7);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerTheoraProfileMin,
                          ::media::THEORAPROFILE_MIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerTheoraProfileAny,
                          ::media::THEORAPROFILE_ANY);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerTheoraProfileMax,
                          ::media::THEORAPROFILE_MAX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerAv1ProfileMin,
                          ::media::AV1PROFILE_MIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerAv1ProfileProfileMain,
                          ::media::AV1PROFILE_PROFILE_MAIN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerAv1ProfileProfileHigh,
                          ::media::AV1PROFILE_PROFILE_HIGH);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerAv1ProfileProfilePro,
                          ::media::AV1PROFILE_PROFILE_PRO);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerAv1ProfileMax,
                          ::media::AV1PROFILE_MAX);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerDolbyVisionProfile8,
                          ::media::DOLBYVISION_PROFILE8);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerDolbyVisionProfile9,
                          ::media::DOLBYVISION_PROFILE9);

// Color range IDs.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerColorSpaceRangeIdInvalid,
                          gfx::ColorSpace::RangeID::INVALID);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerColorSpaceRangeIdLimited,
                          gfx::ColorSpace::RangeID::LIMITED);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerColorSpaceRangeIdFull,
                          gfx::ColorSpace::RangeID::FULL);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerColorSpaceRangeIdDerived,
                          gfx::ColorSpace::RangeID::DERIVED);

// Alpha modes.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerHasAlpha,
                          ::media::VideoDecoderConfig::AlphaMode::kHasAlpha);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerIsOpaque,
                          ::media::VideoDecoderConfig::AlphaMode::kIsOpaque);

// Demuxer stream types.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerStreamTypeUnknown,
                          ::media::DemuxerStream::Type::UNKNOWN);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerStreamTypeAudio,
                          ::media::DemuxerStream::Type::AUDIO);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerStreamTypeVideo,
                          ::media::DemuxerStream::Type::VIDEO);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerStreamTypeText,
                          ::media::DemuxerStream::Type::TEXT);

// Encryption schemes.
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerEncryptionSchemeUnencrypted,
                          ::media::EncryptionScheme::kUnencrypted);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerEncryptionSchemeCenc,
                          ::media::EncryptionScheme::kCenc);
DEMUXER_EXTENSION_ENUM_EQ(kCobaltExtensionDemuxerEncryptionSchemeCbcs,
                          ::media::EncryptionScheme::kCbcs);

#undef DEMUXER_EXTENSION_ENUM_EQ

}  // namespace

}  // namespace media
}  // namespace cobalt
