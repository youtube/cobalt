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
// (see ChunkDemuxer::SetEnableMultiSourceBufferSwitching()). ChunkDemuxer calls
// them only when |multi_source_buffer_switching_| is true.

// clang-format off
#include "media/filters/chunk_demuxer.h"
// clang-format on

#include <algorithm>
#include <atomic>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
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

// Set by ChunkDemuxer::SetEnableMultiSourceBufferSwitching().
std::atomic<bool> g_enable_multi_source_buffer_switching{false};

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

void ChunkDemuxerStream::SetEnabledAtCreation(bool enabled) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!stream_);
  DCHECK(!read_cb_);
  is_enabled_ = enabled;
}

void ChunkDemuxerStream::SetWriteHead(base::TimeDelta time) {
  base::AutoLock auto_lock(lock_);
  write_head_ = time;
}

// static
void ChunkDemuxer::SetEnableMultiSourceBufferSwitching(bool enable) {
  LOG(INFO) << "Set multi-SourceBuffer switching: "
            << (enable ? "enabled" : "disabled");
  g_enable_multi_source_buffer_switching.store(enable,
                                               std::memory_order_relaxed);
}

// static
bool ChunkDemuxer::IsMultiSourceBufferSwitchingEnabled() {
  return g_enable_multi_source_buffer_switching.load(std::memory_order_relaxed);
}

bool ChunkDemuxer::CanCompleteTrackSwitches_Locked() const {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
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

std::vector<DemuxerStream*> ChunkDemuxer::GetSelectedTrackStreams_Locked()
    const {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  std::vector<DemuxerStream*> result;
  if (selected_audio_stream_) {
    result.push_back(selected_audio_stream_.get());
  }
  if (selected_video_stream_) {
    result.push_back(selected_video_stream_.get());
  }
  return result;
}

void ChunkDemuxer::OnDemuxerStreamCreatedForSwitching_Locked(
    const std::string& source_id,
    ChunkDemuxerStream* stream) {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  if (standby_ids_.contains(source_id)) {
    // CreateDemuxerStream() enables the first stream of each type, which can
    // be a standby SourceBuffer's.
    stream->SetEnabledAtCreation(false);
    return;
  }
  // The pipeline reads the first stream of each type that plays now, and the
  // streams selected later by ChangeTo(), through a SelectedTrackStream.
  std::unique_ptr<SelectedTrackStream>& selected_stream =
      stream->type() == DemuxerStream::AUDIO ? selected_audio_stream_
                                             : selected_video_stream_;
  if (!selected_stream) {
    stream->SetEnabledAtCreation(true);
    selected_stream =
        std::make_unique<SelectedTrackStream>(stream->type(), stream);
    if (!returning_data_) {
      // Like the other streams, it aborts reads until the next seek.
      selected_stream->AbortReads();
    }
  }
}

bool ChunkDemuxer::CanInitializeBeforeStandbySources_Locked() const {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  if (!selected_audio_stream_ && !selected_video_stream_) {
    return false;
  }
  for (const std::string& id : pending_source_init_ids_) {
    if (!standby_ids_.contains(id)) {
      return false;
    }
  }
  return true;
}

bool ChunkDemuxer::SetStandby(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  if (!multi_source_buffer_switching_ || !IsValidId_Locked(id)) {
    return false;
  }
  // The streams don't exist until the initialization segment arrives.
  DCHECK(!id_to_streams_map_.contains(id));
  standby_ids_.insert(id);
  return true;
}

bool ChunkDemuxer::ChangeTo(const std::vector<std::string>& ids) {
  base::AutoLock auto_lock(lock_);
  if (!multi_source_buffer_switching_ ||
      (state_ != INITIALIZED && state_ != ENDED)) {
    return false;
  }

  // The stream of each type to switch to, and the SelectedTrackStream that
  // the pipeline reads it through.
  std::vector<std::pair<SelectedTrackStream*, ChunkDemuxerStream*>> switches;
  for (const std::string& id : ids) {
    if (!IsValidId_Locked(id) || pending_source_init_ids_.contains(id)) {
      return false;
    }
    for (ChunkDemuxerStream* stream : id_to_streams_map_[id]) {
      SelectedTrackStream* selected_stream =
          stream->type() == DemuxerStream::AUDIO ? selected_audio_stream_.get()
                                                 : selected_video_stream_.get();
      if (!selected_stream) {
        return false;
      }
      for (const auto& [other_selected_stream, other_stream] : switches) {
        if (other_selected_stream == selected_stream) {
          return false;
        }
      }
      switches.emplace_back(selected_stream, stream);
    }
  }
  if (switches.empty()) {
    return false;
  }
  // Every media type switches at once, because the timeline starts over where
  // the media of |ids| starts.
  for (SelectedTrackStream* selected_stream :
       {selected_audio_stream_.get(), selected_video_stream_.get()}) {
    if (selected_stream &&
        !std::any_of(switches.begin(), switches.end(),
                     [selected_stream](const auto& entry) {
                       return entry.first == selected_stream;
                     })) {
      return false;
    }
  }

  for (const auto& [selected_stream, stream] : switches) {
    ChunkDemuxerStream* const previous_stream = selected_stream->stream();
    if (previous_stream == stream) {
      continue;
    }
    LOG(INFO) << "Switching to " << DemuxerStream::GetTypeName(stream->type())
              << " stream " << stream << "; it plays from the next seek.";
    // Switch before the previous stream is disabled, so that a read pending
    // on it is held for |stream| instead of returning end of stream. The seek
    // that follows positions |stream| and its write head.
    selected_stream->SwitchTo(stream);
    stream->SetEnabled(true, base::TimeDelta());
    previous_stream->SetEnabled(false, base::TimeDelta());
  }
  for (const std::string& id : ids) {
    standby_ids_.erase(id);
  }
  return true;
}

Demuxer::TrackChangeCB ChunkDemuxer::OnTracksChangedForSwitching_Locked(
    DemuxerStream::Type track_type,
    const std::set<ChunkDemuxerStream*>& enabled_streams,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  SelectedTrackStream* selected_stream = nullptr;
  if (track_type == DemuxerStream::AUDIO) {
    selected_stream = selected_audio_stream_.get();
  } else if (track_type == DemuxerStream::VIDEO) {
    selected_stream = selected_video_stream_.get();
  }
  if (!selected_stream) {
    return change_completed_cb;
  }

  // OnTracksChanged() enables at most one stream of each type.
  if (!enabled_streams.empty()) {
    ChunkDemuxerStream* stream = *enabled_streams.begin();
    if (selected_stream->stream() != stream) {
      LOG(INFO) << "Switching to " << DemuxerStream::GetTypeName(track_type)
                << " stream " << stream << "; it plays from the next seek.";
      selected_stream->SwitchTo(stream);
      // As in Seek(), the stream is read again from |curr_time|.
      stream->SetWriteHead(curr_time);
    }
  }

  // The pipeline only knows the SelectedTrackStream of each type.
  return base::BindOnce(
      [](DemuxerStream* selected_stream, TrackChangeCB change_completed_cb,
         const std::vector<DemuxerStream*>& streams) {
        if (streams.empty()) {
          std::move(change_completed_cb).Run(streams);
          return;
        }
        std::move(change_completed_cb).Run({selected_stream});
      },
      // OnTracksChanged() runs the callback before it returns.
      base::Unretained(selected_stream), std::move(change_completed_cb));
}

void ChunkDemuxer::StartSelectedTrackStreamsReturningData_Locked() {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  returning_data_ = true;
  for (SelectedTrackStream* selected_stream :
       {selected_audio_stream_.get(), selected_video_stream_.get()}) {
    if (selected_stream) {
      selected_stream->StartReturningData();
    }
  }
}

void ChunkDemuxer::AbortSelectedTrackStreamReads_Locked() {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  returning_data_ = false;
  for (SelectedTrackStream* selected_stream :
       {selected_audio_stream_.get(), selected_video_stream_.get()}) {
    if (selected_stream) {
      selected_stream->AbortReads();
    }
  }
}

void ChunkDemuxer::ShutdownSelectedTrackStreams_Locked() {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  for (SelectedTrackStream* selected_stream :
       {selected_audio_stream_.get(), selected_video_stream_.get()}) {
    if (selected_stream) {
      selected_stream->Shutdown();
    }
  }
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
  } else {
    source_state->AbortReads();
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
  DCHECK(multi_source_buffer_switching_);
  // Standby SourceBuffers play only after ChangeTo() selects them.
  if (standby_ids_.contains(source_id)) {
    return false;
  }
  if (IsSourceActive_Locked(source_id)) {
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
