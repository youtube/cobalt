// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/splash_screen_demuxer.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace media {

namespace {

std::string GetSplashScreenVideoPath() {
  std::vector<char> content_path(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                       content_path.size())) {
    return "";
  }
  base::FilePath path(content_path.data());
  return path.Append("splash.webm").value();
}

}  // namespace

class SplashScreenDemuxerStream : public DemuxerStream {
 public:
  explicit SplashScreenDemuxerStream(SplashScreenDemuxer* demuxer)
      : demuxer_(demuxer) {
    LOG(INFO) << "YO THOR SPLASH SCREEN DEMIXER STREAM";
  }

  // DemuxerStream implementation.
  void Read(uint32_t count, ReadCB read_cb) override {
    demuxer_->Read(VIDEO, std::move(read_cb));
  }
  AudioDecoderConfig audio_decoder_config() override {
    return AudioDecoderConfig();
  }
  VideoDecoderConfig video_decoder_config() override {
    return demuxer_->video_decoder_config();
  }
  Type type() const override { return VIDEO; }
  void EnableBitstreamConverter() override {}
  bool SupportsConfigChanges() override { return false; }

 private:
  SplashScreenDemuxer* demuxer_;
};

SplashScreenDemuxer::SplashScreenDemuxer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  LOG(INFO) << "YO THOR - SPLASH SCREEN DEMXUER";
}

SplashScreenDemuxer::~SplashScreenDemuxer() {
  if (reader_) {
    reader_->Close();
  }
}

std::vector<DemuxerStream*> SplashScreenDemuxer::GetAllStreams() {
  if (!video_stream_) {
    return {};
  }
  return {video_stream_.get()};
}

std::string SplashScreenDemuxer::GetDisplayName() const {
  return "SplashScreenDemuxer";
}

void SplashScreenDemuxer::Initialize(DemuxerHost* host,
                                     PipelineStatusCallback status_cb) {
  host_ = host;

  std::string video_path = GetSplashScreenVideoPath();
  if (video_path.empty()) {
    std::move(status_cb).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  reader_ = std::make_unique<mkvparser::MkvReader>();
  if (reader_->Open(video_path.c_str()) != 0) {
    std::move(status_cb).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  if (ebml_header.Parse(reader_.get(), pos) < 0) {
    std::move(status_cb).Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  mkvparser::Segment* segment_unowned;
  if (mkvparser::Segment::CreateInstance(reader_.get(), pos, segment_unowned) !=
      0) {
    std::move(status_cb).Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }
  segment_.reset(segment_unowned);

  if (segment_->Load() < 0) {
    std::move(status_cb).Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  FindVideoTrack();

  if (!video_track_) {
    std::move(status_cb).Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  video_stream_ = std::make_unique<SplashScreenDemuxerStream>(this);
  current_cluster_ = segment_->GetFirst();

  host_->SetDuration(
      base::Microseconds(segment_->GetInfo()->GetDuration() / 1000));

  std::move(status_cb).Run(PIPELINE_OK);
}

void SplashScreenDemuxer::AbortPendingReads() {}
void SplashScreenDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}
void SplashScreenDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {}
void SplashScreenDemuxer::Seek(base::TimeDelta time,
                               PipelineStatusCallback status_cb) {
  // Seeking is not supported.
  std::move(status_cb).Run(PIPELINE_OK);
}

void SplashScreenDemuxer::Stop() {
  if (reader_) {
    reader_->Close();
  }
  reader_.reset();
  segment_.reset();
}

base::TimeDelta SplashScreenDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

base::Time SplashScreenDemuxer::GetTimelineOffset() const {
  return base::Time();
}

int64_t SplashScreenDemuxer::GetMemoryUsage() const {
  return 0;
}

void SplashScreenDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  std::move(change_completed_cb).Run(DemuxerStream::VIDEO, {});
}

void SplashScreenDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  std::move(change_completed_cb).Run(DemuxerStream::VIDEO, {});
}

DemuxerType SplashScreenDemuxer::GetDemuxerType() const {
  return DemuxerType::kMockDemuxer;
}

bool SplashScreenDemuxer::IsSeekable() const {
  return false;
}

absl::optional<container_names::MediaContainerName>
SplashScreenDemuxer::GetContainerForMetrics() const {
  return absl::optional<container_names::MediaContainerName>();
}

void SplashScreenDemuxer::SetPlaybackRate(double rate) {}

VideoDecoderConfig SplashScreenDemuxer::video_decoder_config() const {
  if (!video_track_) {
    return VideoDecoderConfig();
  }
  gfx::Size natural_size(video_track_->GetWidth(), video_track_->GetHeight());
  return VideoDecoderConfig(VideoCodec::kVP9, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace::REC709(), kNoTransformation,
                            natural_size, gfx::Rect(natural_size), natural_size,
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);
}

void SplashScreenDemuxer::Read(DemuxerStream::Type type,
                               DemuxerStream::ReadCB read_cb) {
  LOG(INFO) << "YO THOR - SPLASH SCREEN DEMXUER REAADDDD";
  if (!current_cluster_ || current_cluster_->EOS()) {
    std::move(read_cb).Run(DemuxerStream::kAborted,
                           {DecoderBuffer::CreateEOSBuffer()});
    return;
  }

  if (!current_block_entry_) {
    long status = current_cluster_->GetFirst(current_block_entry_);
    if (status < 0) {
      std::move(read_cb).Run(DemuxerStream::kAborted, {});
      return;
    }
  }

  while (current_block_entry_ && !current_block_entry_->EOS()) {
    const mkvparser::Block* block = current_block_entry_->GetBlock();
    if (block->GetTrackNumber() == video_track_->GetNumber()) {
      for (int i = 0; i < block->GetFrameCount(); ++i) {
        const mkvparser::Block::Frame& frame = block->GetFrame(i);
        auto buffer = base::MakeRefCounted<DecoderBuffer>(frame.len);
        if (frame.Read(reader_.get(), buffer->writable_data()) != 0) {
          continue;
        }
        buffer->set_timestamp(
            base::Microseconds(block->GetTime(current_cluster_) / 1000));
        buffer->set_is_key_frame(block->IsKey());

        std::vector<scoped_refptr<DecoderBuffer>> buffers;
        buffers.push_back(buffer);
        std::move(read_cb).Run(DemuxerStream::kOk, buffers);
        return;
      }
    }

    long status =
        current_cluster_->GetNext(current_block_entry_, current_block_entry_);
    if (status < 0) {
      std::move(read_cb).Run(DemuxerStream::kAborted, {});
      return;
    }
  }

  current_cluster_ = segment_->GetNext(current_cluster_);
  current_block_entry_ = nullptr;
  Read(type, std::move(read_cb));
}

void SplashScreenDemuxer::FindVideoTrack() {
  const mkvparser::Tracks* tracks = segment_->GetTracks();
  for (unsigned long i = 0; i < tracks->GetTracksCount(); ++i) {
    const mkvparser::Track* track = tracks->GetTrackByIndex(i);
    if (track && track->GetType() == mkvparser::Track::kVideo) {
      video_track_ = static_cast<const mkvparser::VideoTrack*>(track);
      break;
    }
  }
}

}  // namespace media
