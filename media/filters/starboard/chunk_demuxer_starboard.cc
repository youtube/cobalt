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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/media_types.h"
#include "media/base/mime_util.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/source_buffer_state.h"
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

// The media format named by a SourceBuffer MIME type, such as
// `video/webm; codecs="vp9"`. The codec name is the one GetStreamCodec()
// returns for the streams of a SourceBuffer of that type.
struct MimeTypeFormat {
  DemuxerStream::Type type;
  std::string codec;
  // For video, the profile, bit depth, color and so on, from the codec string.
  std::optional<VideoType> video;
  // For audio, whether the codec is xHE-AAC, which shares the codec name with
  // AAC but needs another decoder.
  bool is_xhe_aac = false;
  // The parameters other than `codecs`, lowercased and sorted, for example
  // `eotf=smpte2084` or `channels=6`, which Starboard reads too.
  std::vector<std::string> parameters;
};

// Returns std::nullopt unless |mime_type| names exactly one known codec.
std::optional<MimeTypeFormat> GetMimeTypeFormat(const std::string& mime_type) {
  const std::vector<std::string> tokens = base::SplitString(
      mime_type, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty()) {
    return std::nullopt;
  }
  const std::string container = base::ToLowerASCII(tokens[0]);
  static constexpr char kCodecs[] = "codecs=";
  std::string codecs_parameter;
  std::vector<std::string> parameters;
  for (size_t i = 1; i < tokens.size(); ++i) {
    if (codecs_parameter.empty() &&
        base::StartsWith(tokens[i], kCodecs,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      codecs_parameter = tokens[i].substr(sizeof(kCodecs) - 1);
      continue;
    }
    std::string parameter;
    base::RemoveChars(base::ToLowerASCII(tokens[i]), " \t", &parameter);
    parameters.push_back(std::move(parameter));
  }
  std::sort(parameters.begin(), parameters.end());
  std::vector<std::string> codecs;
  SplitCodecs(codecs_parameter, &codecs);
  if (codecs.size() != 1) {
    return std::nullopt;
  }
  if (const std::optional<VideoType> video = ParseVideoCodecString(
          container, codecs[0], /*allow_ambiguous_matches=*/true)) {
    return MimeTypeFormat{DemuxerStream::VIDEO, GetCodecName(video->codec),
                          video, /*is_xhe_aac=*/false, std::move(parameters)};
  }
  bool is_ambiguous = false;
  AudioCodec audio_codec = AudioCodec::kUnknown;
  if (ParseAudioCodecString(container, codecs[0], &is_ambiguous,
                            &audio_codec)) {
    return MimeTypeFormat{
        DemuxerStream::AUDIO, GetCodecName(audio_codec), std::nullopt,
        base::EqualsCaseInsensitiveASCII(codecs[0], "mp4a.40.42"),
        std::move(parameters)};
  }
  return std::nullopt;
}

// Returns true if a switch from |current| to |next| keeps the format, so that
// the SbPlayer handles it like a config change within one SourceBuffer, which
// doesn't ask SbMediaCanChangeType() either. That is the same codec and, for
// video, the same profile, bit depth, chroma subsampling, color and HDR
// metadata: for example, a switch from VP9 profile 0 (SDR) to VP9 profile 2
// (10-bit, maybe HDR) doesn't keep the format, and needs the platform's
// support. Only the level may differ, as it does between the resolutions of
// one video. The other parameters of the MIME types, such as `eotf` or
// `channels`, must match too, and for audio, AAC and xHE-AAC differ.
bool IsSameFormat(const MimeTypeFormat& current, const MimeTypeFormat& next) {
  if (current.type != next.type || current.codec != next.codec ||
      current.is_xhe_aac != next.is_xhe_aac ||
      current.parameters != next.parameters) {
    return false;
  }
  if (!current.video || !next.video) {
    return !current.video && !next.video;
  }
  const VideoType& a = *current.video;
  const VideoType& b = *next.video;
  return a.profile == b.profile && a.bit_depth == b.bit_depth &&
         a.subsampling == b.subsampling && a.color_space == b.color_space &&
         a.hdr_metadata_type == b.hdr_metadata_type;
}

// As above, for the MIME types of two SourceBuffers. Identical MIME types
// (including two empty ones, for SourceBuffers added without a MIME type)
// always keep the format; ones that can't be parsed never do.
bool IsSameFormat(const std::string& current_type,
                  const std::string& next_type) {
  if (current_type == next_type) {
    return true;
  }
  const std::optional<MimeTypeFormat> current = GetMimeTypeFormat(current_type);
  const std::optional<MimeTypeFormat> next = GetMimeTypeFormat(next_type);
  return current && next && IsSameFormat(*current, *next);
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

// static
bool ChunkDemuxer::CanChangeTo(const std::string& current_type,
                               const std::string& next_type) {
  if (!IsMultiSourceBufferSwitchingEnabled()) {
    return false;
  }
  const std::optional<MimeTypeFormat> current = GetMimeTypeFormat(current_type);
  const std::optional<MimeTypeFormat> next = GetMimeTypeFormat(next_type);
  if (!current || !next || current->type != next->type) {
    return false;
  }
  // The same rule as CanSwitchStreams_Locked(), which compares the same MIME
  // types, so that a page can check a switch before it buffers the media to
  // switch to.
  if (IsSameFormat(*current, *next)) {
    return true;
  }
  return GetSbMediaInterface()->CanChangeType(current_type.c_str(),
                                              next_type.c_str());
}

bool ChunkDemuxer::CanSwitchStreams_Locked(ChunkDemuxerStream* from,
                                           ChunkDemuxerStream* to) const {
  lock_.AssertAcquired();
  if (from == to) {
    return true;
  }
  const StreamCodec current = GetStreamCodec(from);
  const StreamCodec next = GetStreamCodec(to);
  // A switch that keeps the format is like a config change within a
  // SourceBuffer, which the SbPlayer handles without SbMediaCanChangeType().
  // Not asking keeps switching available on platforms where
  // SbMediaCanChangeType() returns false.
  if (current.codec == next.codec &&
      IsSameFormat(current.mime_type, next.mime_type)) {
    return true;
  }
  // Within a SourceBuffer, changeType() checks SbMediaCanChangeType() before
  // changing codecs. Check here too, also for a change of profile, bit depth
  // or color within a codec (for example SDR to HDR), so that the SbPlayer
  // never receives a transition across SourceBuffers it can't make.
  if (GetSbMediaInterface()->CanChangeType(current.mime_type.c_str(),
                                           next.mime_type.c_str())) {
    return true;
  }
  const std::string message = base::StrCat(
      {"SourceBuffer switch rejected: ", current.codec, " (", current.mime_type,
       ") -> ", next.codec, " (", next.mime_type,
       ") is not supported by SbMediaCanChangeType()"});
  LOG(ERROR) << message;
  MEDIA_LOG(ERROR, media_log_) << message;
  return false;
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
  } else {
    // Only the stream that the SelectedTrackStream reads is enabled. Another
    // stream of the type stays disabled, whatever CreateDemuxerStream() did.
    stream->SetEnabledAtCreation(false);
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
    if (selected_stream && !std::any_of(switches.begin(), switches.end(),
                                        [selected_stream](const auto& entry) {
                                          return entry.first == selected_stream;
                                        })) {
      return false;
    }
  }
  // Refuse a switch the SbPlayer can't make here, where the page can fall
  // back, rather than failing playback at the seek.
  for (const auto& [selected_stream, stream] : switches) {
    if (!CanSwitchStreams_Locked(selected_stream->served_stream(), stream)) {
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

std::vector<DemuxerStream*> ChunkDemuxer::OnTracksChangedForSwitching_Locked(
    DemuxerStream::Type track_type,
    const std::vector<MediaTrack::Id>& track_ids) {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  // A page that enables an audio track or selects a video track (for example
  // a track of a standby SourceBuffer) doesn't switch playback: only
  // ChangeTo() does, with the checks and the seek that a switch needs. The
  // streams stay as they are, and the pipeline keeps reading the
  // SelectedTrackStream, which is the only stream of |track_type| it knows.
  SelectedTrackStream* selected_stream = nullptr;
  if (track_type == DemuxerStream::AUDIO) {
    selected_stream = selected_audio_stream_.get();
  } else if (track_type == DemuxerStream::VIDEO) {
    selected_stream = selected_video_stream_.get();
  }
  if (!selected_stream || track_ids.empty()) {
    return {};
  }
  MEDIA_LOG(INFO, media_log_)
      << "Track change ignored: with multi-SourceBuffer switching, "
         "MediaSource.changeTo() selects the media that plays.";
  return {selected_stream};
}

void ChunkDemuxer::OnRemoveIdForSwitching_Locked(const std::string& source_id) {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  standby_ids_.erase(source_id);
  // RemoveId() keeps the streams, in case the renderer still holds pointers to
  // them, but nothing reads their media after this.
  auto itr = source_state_map_.find(source_id);
  if (itr != source_state_map_.end()) {
    itr->second->Remove(base::TimeDelta(), kInfiniteDuration,
                        kInfiniteDuration);
  }
}

base::TimeDelta ChunkDemuxer::GetEvictionMediaTime_Locked(
    const std::string& source_id,
    base::TimeDelta media_time) const {
  lock_.AssertAcquired();
  DCHECK(multi_source_buffer_switching_);
  if (IsSourceUsedForPlayback_Locked(source_id)) {
    return media_time;
  }
  auto itr = source_state_map_.find(source_id);
  if (itr == source_state_map_.end()) {
    return media_time;
  }
  // The current time is on the timeline of the media that plays now. Using it
  // here would evict the media of this SourceBuffer that is earlier than it
  // as if it had played. Eviction keeps the media from where this
  // SourceBuffer's media starts (0 if it has none), and frees the latest
  // media first instead.
  return itr->second->GetLowestPresentationTimestamp();
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
  if (state_ == ENDED) {
    // MarkEndOfStream() marked the other SourceBuffers, and
    // UnmarkEndOfStream() expects every one to be marked.
    source_state->MarkEndOfStream();
  }
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
