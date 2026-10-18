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

// ChunkDemuxer and ChunkDemuxerStream methods for multi-SourceBuffer switching
// (see SetEnableMultiSourceBufferSwitching).

// clang-format off
#include "media/filters/chunk_demuxer.h"
// clang-format on

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/starboard/selected_track_stream.h"

namespace media {

namespace {

// The codec of the current config of a stream, and the MIME type of the
// SourceBuffer that the config comes from. The SbPlayer gets both with the
// config.
struct StreamCodec {
  std::string codec;
  std::string mime_type;
};

StreamCodec GetStreamCodec(ChunkDemuxerStream* stream) {
  if (stream->type() == DemuxerStream::AUDIO) {
    const AudioDecoderConfig config = stream->audio_decoder_config();
    return {GetCodecName(config.codec()), config.mime_type()};
  }
  const VideoDecoderConfig config = stream->video_decoder_config();
  return {GetCodecName(config.codec()), config.mime_type()};
}

}  // namespace

std::pair<DemuxerStream::ReadCB, uint32_t>
ChunkDemuxerStream::TakePendingRead() {
  base::AutoLock auto_lock(lock_);
  const uint32_t count = read_cb_ ? requested_buffer_count_ : 0;
  requested_buffer_count_ = 0;
  return {std::move(read_cb_), count};
}

bool ChunkDemuxer::CanAddId_Locked() const {
  lock_.AssertAcquired();
  if (state_ == WAITING_FOR_INIT || state_ == INITIALIZING) {
    return true;
  }
  // With multi-SourceBuffer switching, SourceBuffers of media that plays later
  // can be added during playback.
  return multi_source_buffer_switching_ && state_ == INITIALIZED;
}

bool ChunkDemuxer::CanCompleteTrackSwitches_Locked() const {
  lock_.AssertAcquired();
  for (SelectedTrackStream* selected_stream :
       {selected_audio_stream_.get(), selected_video_stream_.get()}) {
    if (!selected_stream) {
      continue;
    }
    ChunkDemuxerStream* const served_stream = selected_stream->served_stream();
    ChunkDemuxerStream* const stream = selected_stream->stream();
    if (served_stream == stream) {
      continue;
    }
    const StreamCodec current = GetStreamCodec(served_stream);
    const StreamCodec next = GetStreamCodec(stream);
    if (current.codec == next.codec) {
      continue;
    }
    // Within a SourceBuffer, changeType() checks SbMediaCanChangeType() before
    // changing codecs. Check here too so SbPlayer never receives an unsupported
    // codec transition across SourceBuffers.
    if (!GetSbMediaInterface()->CanChangeType(current.mime_type.c_str(),
                                              next.mime_type.c_str())) {
      const std::string message = base::StrCat(
          {"SourceBuffer switch rejected: ", current.codec, " (",
           current.mime_type, ") -> ", next.codec, " (", next.mime_type,
           ") is not supported by SbMediaCanChangeType()"});
      LOG(ERROR) << message;
      MEDIA_LOG(ERROR, media_log_) << message;
      return false;
    }
  }
  return true;
}

SelectedTrackStream* ChunkDemuxer::GetSelectedTrackStream_Locked(
    DemuxerStream::Type type) const {
  lock_.AssertAcquired();
  if (type == DemuxerStream::AUDIO) {
    return selected_audio_stream_.get();
  }
  if (type == DemuxerStream::VIDEO) {
    return selected_video_stream_.get();
  }
  return nullptr;
}

void ChunkDemuxer::InitializeLateSource_Locked(
    const std::string& source_id,
    const StreamParser::InitParameters& params) {
  lock_.AssertAcquired();
  CHECK(multi_source_buffer_switching_);
  DVLOG(1) << __func__ << " source_id=" << source_id;
  pending_source_init_ids_.erase(source_id);

  // The duration and the timeline offset were set when the demuxer
  // initialized.
  if (params.liveness != StreamLiveness::kUnknown) {
    for (ChunkDemuxerStream* stream : id_to_streams_map_[source_id]) {
      stream->SetLiveness(params.liveness);
    }
  }

  // Bring the new streams to the state of the others, as the initialization
  // of the demuxer does for all streams.
  SourceBufferState* source_state = source_state_map_[source_id].get();
  source_state->Seek(GetStartTime());
  if (returning_data_) {
    source_state->StartReturningData();
  }
}

bool ChunkDemuxer::IsSourceActive_Locked(const std::string& source_id) const {
  lock_.AssertAcquired();
  auto itr = id_to_streams_map_.find(source_id);
  if (itr == id_to_streams_map_.end()) {
    return false;
  }
  for (const ChunkDemuxerStream* stream : itr->second) {
    if (stream->IsEnabled()) {
      return true;
    }
  }
  return false;
}

bool ChunkDemuxer::IsSourceUsedForPlayback_Locked(
    const std::string& source_id) const {
  lock_.AssertAcquired();
  if (!multi_source_buffer_switching_ || IsSourceActive_Locked(source_id)) {
    return true;
  }
  // If no SourceBuffer is active, all are used, as without switching.
  for (const auto& [id, source_state] : source_state_map_) {
    if (IsSourceActive_Locked(id)) {
      return false;
    }
  }
  return true;
}

}  // namespace media
