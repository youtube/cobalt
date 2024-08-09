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

#ifndef COBALT_MEDIA_BASE_CHUNK_DEMUXER_HOLDER_H_
#define COBALT_MEDIA_BASE_CHUNK_DEMUXER_HOLDER_H_

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "cobalt/media/base/chrome_media.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/chromium/media/filters/chunk_demuxer.h"

namespace cobalt {
namespace media {

// Holds an ChunkDemuxer object from one of the supported Chrome Media versions.
// It include methods dispatching to the underlying ChunkDemuxer objects, and
// abstract out any differences to Cobalt Media.
class ChunkDemuxerHolder {
 public:
  ChunkDemuxerHolder() {
#define VERIFY_CHROME_MEDIA_ENUM_VALUES(x)                        \
  static_assert(static_cast<int>(::media_m96::ChunkDemuxer::x) == \
                static_cast<int>(::media_m114::ChunkDemuxer::x));

    VERIFY_CHROME_MEDIA_ENUM_VALUES(kOk);
    VERIFY_CHROME_MEDIA_ENUM_VALUES(kNotSupported);
    VERIFY_CHROME_MEDIA_ENUM_VALUES(kReachedIdLimit);

#undef VERIFY_CHROME_MEDIA_ENUM_VALUES
  }

  enum Status {
    kOk = ::media_m114::ChunkDemuxer::kOk,
    kNotSupported = ::media_m114::ChunkDemuxer::kNotSupported,
    kReachedIdLimit = ::media_m114::ChunkDemuxer::kReachedIdLimit,
  };

  bool valid() const { return chunk_demuxer_m96_ || chunk_demuxer_m114_; }

  bool is_m114() const {
    DCHECK(valid());
    return chunk_demuxer_m114_ != nullptr;
  }

  void set(::media_m96::ChunkDemuxer* chunk_demuxer) {
    DCHECK(chunk_demuxer);
    DCHECK(!chunk_demuxer_m96_);
    DCHECK(!chunk_demuxer_m114_);
    chunk_demuxer_m96_.reset(chunk_demuxer);
  }

  void set(::media::ChunkDemuxer* chunk_demuxer) {
    DCHECK(chunk_demuxer);
    DCHECK(!chunk_demuxer_m96_);
    DCHECK(!chunk_demuxer_m114_);
    chunk_demuxer_m114_.reset(chunk_demuxer);
  }

  void reset() {
    chunk_demuxer_m96_.reset();
    chunk_demuxer_m114_.reset();
  }

  template <typename ChunkDemuxerPtrType>
  ChunkDemuxerPtrType As();

  template <>
  ::media_m96::ChunkDemuxer* As<::media_m96::ChunkDemuxer*>() {
    DCHECK(chunk_demuxer_m96_);
    return chunk_demuxer_m96_.get();
  }

  template <>
  ::media::ChunkDemuxer* As<::media_m114::ChunkDemuxer*>() {
    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_.get();
  }

  Status AddId(const std::string& id, const std::string& mime_type) {
    if (chunk_demuxer_m96_) {
      return static_cast<Status>(chunk_demuxer_m96_->AddId(id, mime_type));
    }

    DCHECK(chunk_demuxer_m114_);
    return static_cast<Status>(chunk_demuxer_m114_->AddId(id, mime_type));
  }

  void RemoveId(const std::string& id) {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->RemoveId(id);
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->RemoveId(id);
  }

  bool AppendBuffer(const std::string& id, const uint8_t* data, size_t length,
                    base::TimeDelta append_window_start,
                    base::TimeDelta append_window_end,
                    base::TimeDelta* timestamp_offset) {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->AppendData(
          id, data, length, append_window_start, append_window_end,
          timestamp_offset);
    }

    DCHECK(chunk_demuxer_m114_);

    if (!chunk_demuxer_m114_->AppendToParseBuffer(id, data, length)) {
      return false;
    }

    using ::media_m114::StreamParser;
    StreamParser::ParseStatus result =
        StreamParser::ParseStatus::kSuccessHasMoreData;
    while (result == StreamParser::ParseStatus::kSuccessHasMoreData) {
      result = chunk_demuxer_m114_->RunSegmentParserLoop(
          id, append_window_start, append_window_end, timestamp_offset);
    }
    return result == StreamParser::ParseStatus::kSuccess;
  }

  void Remove(const std::string& id, base::TimeDelta start,
              base::TimeDelta end) {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->Remove(id, start, end);
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->Remove(id, start, end);
  }

  bool HasAnyStreams() const {
    if (chunk_demuxer_m96_) {
      return !chunk_demuxer_m96_->GetAllStreams().empty();
    }

    DCHECK(chunk_demuxer_m114_);
    return !chunk_demuxer_m114_->GetAllStreams().empty();
  }

  void SetDuration(double duration) {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->SetDuration(duration);
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->SetDuration(duration);
  }

  double GetDuration() const {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->GetDuration();
    }

    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_->GetDuration();
  }

  void StartWaitingForSeek(base::TimeDelta seek_time) {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->StartWaitingForSeek(seek_time);
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->StartWaitingForSeek(seek_time);
  }

  void CancelPendingSeek(base::TimeDelta seek_time) {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->CancelPendingSeek(seek_time);
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->CancelPendingSeek(seek_time);
  }

  base::TimeDelta GetHighestPresentationTimestamp(const std::string& id) const {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->GetHighestPresentationTimestamp(id);
    }

    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_->GetHighestPresentationTimestamp(id);
  }

  size_t GetSourceBufferStreamMemoryLimit(const std::string& id) const {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->GetSourceBufferStreamMemoryLimit(id);
    }

    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_->GetSourceBufferStreamMemoryLimit(id);
  }

  base::TimeDelta GetWriteHead(const std::string& id) const {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->GetWriteHead(id);
    }

    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_->GetWriteHead(id);
  }

  void SetSourceBufferStreamMemoryLimit(const std::string& id,
                                        size_t memory_limit) const {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->SetSourceBufferStreamMemoryLimit(id, memory_limit);
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->SetSourceBufferStreamMemoryLimit(id, memory_limit);
  }

  bool IsParsingMediaSegment(const std::string& id) const {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->IsParsingMediaSegment(id);
    }

    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_->IsParsingMediaSegment(id);
  }

  void SetParseWarningCallback(const std::string& id) {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->SetParseWarningCallback(
          id, base::BindRepeating(
                  [](::media_m96::SourceBufferParseWarning warning) {
                    LOG(WARNING) << "Encountered SourceBufferParseWarning "
                                 << static_cast<int>(warning);
                  }));
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->SetParseWarningCallback(
        id,
        base::BindRepeating([](::media_m114::SourceBufferParseWarning warning) {
          LOG(WARNING) << "Encountered SourceBufferParseWarning "
                       << static_cast<int>(warning);
        }));
  }

  void SetSequenceMode(const std::string& id, bool sequence_mode) const {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->SetSequenceMode(id, sequence_mode);
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->SetSequenceMode(id, sequence_mode);
  }

  void SetGroupStartTimestampIfInSequenceMode(
      const std::string& id, base::TimeDelta timestamp_offset) {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->SetGroupStartTimestampIfInSequenceMode(
          id, timestamp_offset);
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->SetGroupStartTimestampIfInSequenceMode(
        id, timestamp_offset);
  }

  void ResetParserState(const std::string& id,
                        base::TimeDelta append_window_start,
                        base::TimeDelta append_window_end,
                        base::TimeDelta* timestamp_offset) {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->ResetParserState(
          id, append_window_start, append_window_end, timestamp_offset);
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->ResetParserState(id, append_window_start,
                                          append_window_end, timestamp_offset);
  }

  void MarkEndOfStream(int status_code) {
    if (chunk_demuxer_m96_) {
      DCHECK(status_code == ::media_m96::PIPELINE_OK ||
             status_code ==
                 ::media_m96::CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR ||
             status_code ==
                 ::media_m96::CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR);
      chunk_demuxer_m96_->MarkEndOfStream(
          static_cast<::media_m96::PipelineStatus>(status_code));
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    DCHECK(status_code == ::media::PIPELINE_OK ||
           status_code ==
               ::media::CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR ||
           status_code == ::media::CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR);
    chunk_demuxer_m114_->MarkEndOfStream(
        static_cast<::media::PipelineStatusCodes>(status_code));
  }

  void UnmarkEndOfStream() {
    if (chunk_demuxer_m96_) {
      chunk_demuxer_m96_->UnmarkEndOfStream();
      return;
    }

    DCHECK(chunk_demuxer_m114_);
    chunk_demuxer_m114_->UnmarkEndOfStream();
  }

  bool EvictCodedFrames(const std::string& id, base::TimeDelta currentMediaTime,
                        size_t newDataSize) {
    if (chunk_demuxer_m96_) {
      return chunk_demuxer_m96_->EvictCodedFrames(id, currentMediaTime,
                                                  newDataSize);
    }

    DCHECK(chunk_demuxer_m114_);
    return chunk_demuxer_m114_->EvictCodedFrames(id, currentMediaTime,
                                                 newDataSize);
  }

  template <typename TimeRanges>
  scoped_refptr<TimeRanges> GetBufferedRanges(const std::string& id) {
    scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
    if (chunk_demuxer_m96_) {
      auto ranges = chunk_demuxer_m96_->GetBufferedRanges(id);
      for (size_t i = 0; i < ranges.size(); i++) {
        time_ranges->Add(ranges.start(i).InSecondsF(),
                         ranges.end(i).InSecondsF());
      }
    } else {
      DCHECK(chunk_demuxer_m114_);
      auto ranges = chunk_demuxer_m114_->GetBufferedRanges(id);
      for (size_t i = 0; i < ranges.size(); i++) {
        time_ranges->Add(ranges.start(i).InSecondsF(),
                         ranges.end(i).InSecondsF());
      }
    }

    return time_ranges;
  }

 private:
  std::unique_ptr<::media_m96::ChunkDemuxer> chunk_demuxer_m96_;
  std::unique_ptr<::media::ChunkDemuxer> chunk_demuxer_m114_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerHolder);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_CHUNK_DEMUXER_HOLDER_H_
