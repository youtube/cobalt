// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/media/sandbox/media_source_demuxer.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/sys_byteorder.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/bind_to_current_loop.h"
#include "cobalt/media/base/demuxer.h"
#include "cobalt/media/base/pipeline_status.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "starboard/memory.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {

using base::Bind;
using ::media::BindToCurrentLoop;
using ::media::DecoderBuffer;
using ::media::DemuxerStream;
using ::media::ChunkDemuxer;

typedef base::Callback<void(::media::DecoderBuffer*)> AppendBufferCB;

const char kSourceId[] = "id";

// Stub log function.
void Log(const std::string& message) { UNREFERENCED_PARAMETER(message); }

// Stub need key callback.
void NeedKeyCB(const std::string& type, scoped_array<uint8> init_data,
               int init_data_size) {
  NOTREACHED();
}

bool IsMP4(const std::vector<uint8>& content) {
  return content.size() >= 8 && SbMemoryCompare(&content[4], "ftyp", 4) == 0;
}

std::vector<std::string> MakeStringVector(const char* string) {
  std::vector<std::string> result;
  result.push_back(string);
  return result;
}

bool AddSourceBuffer(bool is_mp4, ChunkDemuxer* demuxer) {
  const char kMP4Mime[] = "video/mp4";
  const char kAVCCodecs[] = "avc1.640028";
  const char kWebMMime[] = "video/webm";
  const char kVp9Codecs[] = "vp9";

  std::vector<std::string> codecs =
      MakeStringVector(is_mp4 ? kAVCCodecs : kVp9Codecs);
  ChunkDemuxer::Status status =
      demuxer->AddId(kSourceId, is_mp4 ? kMP4Mime : kWebMMime, codecs);
  CHECK_EQ(status, ChunkDemuxer::kOk);
  return status == ChunkDemuxer::kOk;
}

// Helper class to load an adaptive video file and call |append_buffer_cb| for
// every frame.
class Loader : public ::media::DemuxerHost {
 public:
  Loader(const std::vector<uint8>& content,
         const AppendBufferCB& append_buffer_cb)
      : valid_(true),
        ended_(false),
        append_buffer_cb_(append_buffer_cb),
        content_(content),
        offset_(0),
        read_in_progress_(false) {
    DCHECK(!append_buffer_cb.is_null());

    demuxer_ = new ChunkDemuxer(
        BindToCurrentLoop(Bind(&Loader::OnDemuxerOpen, base::Unretained(this),
                               IsMP4(content))),
        Bind(NeedKeyCB), Bind(Log));
    demuxer_->Initialize(
        this, ::media::BindToCurrentLoop(
                  Bind(&Loader::OnDemuxerStatus, base::Unretained(this))));
    while (valid_ && !ended_) {
      MessageLoop::current()->RunUntilIdle();
    }
  }

  bool valid() const { return valid_; }
  const ::media::VideoDecoderConfig& config() const { return config_; }

 private:
  void SetTotalBytes(int64 total_bytes) override {
    UNREFERENCED_PARAMETER(total_bytes);
  }
  void AddBufferedByteRange(int64 start, int64 end) override {
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
  }
  void AddBufferedTimeRange(base::TimeDelta start,
                            base::TimeDelta end) override {
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
  }

  void SetDuration(base::TimeDelta duration) override {
    UNREFERENCED_PARAMETER(duration);
  }
  void OnDemuxerError(::media::PipelineStatus error) override {
    valid_ = false;
  }
  void OnDemuxerOpen(bool is_mp4) {
    valid_ &= AddSourceBuffer(is_mp4, demuxer_);
    if (!valid_) {
      return;
    }
    ConsumeContent();
    if (!demuxer_->GetStream(DemuxerStream::VIDEO)) {
      valid_ = false;
      return;
    }
    while (valid_ && !ended_) {
      ConsumeContent();
      ProduceBuffer();
      MessageLoop::current()->RunUntilIdle();
    }
  }
  void OnDemuxerStatus(::media::PipelineStatus status) {
    if (status == ::media::PIPELINE_OK) {
      config_.CopyFrom(
          demuxer_->GetStream(DemuxerStream::VIDEO)->video_decoder_config());
    } else {
      valid_ = false;
    }
  }
  void ConsumeContent() {
    const float kLowWaterMarkInSeconds = 1.f;
    const size_t kMaxBytesToAppend = 5 * 1024 * 1024;

    ::media::Ranges<base::TimeDelta> ranges =
        demuxer_->GetBufferedRanges(kSourceId);
    if (offset_ < content_.size()) {
      base::TimeDelta range_end;
      if (ranges.size() != 0) {
        range_end = ranges.end(ranges.size() - 1);
      }
      if ((range_end - timestamp_of_last_buffer_).InSecondsF() >
          kLowWaterMarkInSeconds) {
        return;
      }
      size_t bytes_to_append =
          std::min(kMaxBytesToAppend, content_.size() - offset_);
      demuxer_->AppendData(kSourceId, &content_[0] + offset_, bytes_to_append);
      offset_ += bytes_to_append;
    }
    if (offset_ == content_.size()) {
      demuxer_->EndOfStream(::media::PIPELINE_OK);
      ++offset_;
    }
  }
  void ProduceBuffer() {
    if (!read_in_progress_) {
      read_in_progress_ = true;
      demuxer_->GetStream(DemuxerStream::VIDEO)
          ->Read(Bind(&Loader::OnDemuxerRead, base::Unretained(this)));
    }
  }
  void OnDemuxerRead(DemuxerStream::Status status,
                     const scoped_refptr<DecoderBuffer>& buffer) {
    // This should only happen during seeking which won't happen to this class.
    DCHECK_NE(status, DemuxerStream::kAborted);
    if (status == DemuxerStream::kConfigChanged) {
      config_.CopyFrom(
          demuxer_->GetStream(DemuxerStream::VIDEO)->video_decoder_config());
      ProduceBuffer();
      return;
    }
    DCHECK_EQ(status, DemuxerStream::kOk);
    if (buffer->IsEndOfStream()) {
      ended_ = true;
    } else {
      timestamp_of_last_buffer_ = buffer->GetTimestamp();
      append_buffer_cb_.Run(buffer);
    }
    read_in_progress_ = false;
  }

  bool valid_;
  bool ended_;
  AppendBufferCB append_buffer_cb_;
  const std::vector<uint8>& content_;
  size_t offset_;
  bool read_in_progress_;
  base::TimeDelta timestamp_of_last_buffer_;
  scoped_refptr<ChunkDemuxer> demuxer_;
  ::media::VideoDecoderConfig config_;
};

// This is a very loose IVF parser for fuzzing purpose and it ignores any
// invalid structure and just retrieves the frame data.
// IVF format (all data is little endian):
//   32 bytes file header starts with DKIF
//     (4 bytes frame size + 8 bytes timestamp + <size> bytes frame data)*
bool LoadIVF(const std::vector<uint8>& content,
             const AppendBufferCB& append_buffer_cb) {
  const size_t kIVFFileHeaderSize = 32;
  const size_t kIVFFrameHeaderSize = 12;
  if (content.size() < kIVFFileHeaderSize ||
      SbMemoryCompare(&content[0], "DKIF", 4) != 0) {
    return false;
  }
  size_t offset = kIVFFileHeaderSize;
  while (offset + kIVFFrameHeaderSize < content.size()) {
    uint32 frame_size = base::ByteSwapToLE32(
        *reinterpret_cast<const uint32*>(&content[offset]));
    offset += kIVFFrameHeaderSize;
    if (offset + frame_size > content.size()) {
      break;
    }
    append_buffer_cb.Run(::media::StreamParserBuffer::CopyFrom(
        &content[offset], frame_size, false));
    offset += frame_size;
  }
  return true;
}

}  // namespace

MediaSourceDemuxer::MediaSourceDemuxer(const std::vector<uint8>& content)
    : valid_(true) {
  // Try to load it as an ivf first.
  if (LoadIVF(content, Bind(&MediaSourceDemuxer::AppendBuffer,
                            base::Unretained(this)))) {
    config_.Initialize(::media::kCodecVP9, ::media::VP9PROFILE_MAIN,
                       ::media::VideoFrame::YV12,
                       ::media::COLOR_SPACE_HD_REC709, gfx::Size(1, 1),
                       gfx::Rect(1, 1), gfx::Size(1, 1), NULL, 0, false, false);
    valid_ = descs_.size() > 0;
    return;
  }

  Loader loader(
      content, Bind(&MediaSourceDemuxer::AppendBuffer, base::Unretained(this)));
  valid_ = loader.valid();
  if (valid_) {
    config_.CopyFrom(loader.config());
  } else {
    au_data_.clear();
    descs_.clear();
  }
}

void MediaSourceDemuxer::AppendBuffer(::media::DecoderBuffer* buffer) {
  AUDescriptor desc = {0};
  desc.is_keyframe = buffer->IsKeyframe();
  desc.offset = au_data_.size();
  desc.size = buffer->GetDataSize();
  desc.timestamp = buffer->GetTimestamp();

  au_data_.insert(au_data_.end(), buffer->GetData(),
                  buffer->GetData() + buffer->GetDataSize());
  descs_.push_back(desc);
}

const MediaSourceDemuxer::AUDescriptor& MediaSourceDemuxer::GetFrame(
    size_t index) const {
  DCHECK_LT(index, descs_.size());

  return descs_[index];
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
