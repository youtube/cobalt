// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/filters/source_buffer_state.h"

#include <algorithm>
#include <set>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "cobalt/media/base/media_track.h"
#include "cobalt/media/base/media_tracks.h"
#include "cobalt/media/base/mime_util.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "cobalt/media/filters/frame_processor.h"
#include "cobalt/media/filters/source_buffer_stream.h"

namespace media {

enum {
  // Limits the number of MEDIA_LOG() calls warning the user that a muxed stream
  // media segment is missing a block from at least one of the audio or video
  // tracks.
  kMaxMissingTrackInSegmentLogs = 10,
};

namespace {

TimeDelta EndTimestamp(const StreamParser::BufferQueue& queue) {
  return queue.back()->timestamp() + queue.back()->duration();
}

// Check the input |text_configs| and |bytestream_ids| and return false if
// duplicate track ids are detected.
bool CheckBytestreamTrackIds(
    const MediaTracks& tracks,
    const StreamParser::TextTrackConfigMap& text_configs) {
  std::set<StreamParser::TrackId> bytestream_ids;
  for (size_t i = 0; i < tracks.tracks().size(); ++i) {
    const StreamParser::TrackId& track_id =
        tracks.tracks()[i]->bytestream_track_id();
    if (bytestream_ids.find(track_id) != bytestream_ids.end()) {
      return false;
    }
    bytestream_ids.insert(track_id);
  }
  for (StreamParser::TextTrackConfigMap::const_iterator iter =
           text_configs.begin();
       iter != text_configs.end(); ++iter) {
    const StreamParser::TrackId& track_id = iter->first;
    if (bytestream_ids.find(track_id) != bytestream_ids.end()) {
      return false;
    }
    bytestream_ids.insert(track_id);
  }
  return true;
}

}  // namespace

// List of time ranges for each SourceBuffer.
// static
Ranges<TimeDelta> SourceBufferState::ComputeRangesIntersection(
    const RangesList& active_ranges, bool ended) {
  // TODO(servolk): Perhaps this can be removed in favor of blink implementation
  // (MediaSource::buffered)? Currently this is only used on Android and for
  // updating DemuxerHost's buffered ranges during AppendData() as well as
  // SourceBuffer.buffered property implementation.
  // Implementation of HTMLMediaElement.buffered algorithm in MSE spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#dom-htmlmediaelement.buffered

  // Step 1: If activeSourceBuffers.length equals 0 then return an empty
  //  TimeRanges object and abort these steps.
  if (active_ranges.empty()) return Ranges<TimeDelta>();

  // Step 2: Let active ranges be the ranges returned by buffered for each
  //  SourceBuffer object in activeSourceBuffers.
  // Step 3: Let highest end time be the largest range end time in the active
  //  ranges.
  TimeDelta highest_end_time;
  for (size_t i = 0; i < active_ranges.size(); ++i) {
    if (!active_ranges[i].size()) continue;

    highest_end_time = std::max(
        highest_end_time, active_ranges[i].end(active_ranges[i].size() - 1));
  }

  // Step 4: Let intersection ranges equal a TimeRange object containing a
  //  single range from 0 to highest end time.
  Ranges<TimeDelta> intersection_ranges;
  intersection_ranges.Add(TimeDelta(), highest_end_time);

  // Step 5: For each SourceBuffer object in activeSourceBuffers run the
  //  following steps:
  for (size_t i = 0; i < active_ranges.size(); ++i) {
    // Step 5.1: Let source ranges equal the ranges returned by the buffered
    //  attribute on the current SourceBuffer.
    Ranges<TimeDelta> source_ranges = active_ranges[i];

    // Step 5.2: If readyState is "ended", then set the end time on the last
    //  range in source ranges to highest end time.
    if (ended && source_ranges.size()) {
      source_ranges.Add(source_ranges.start(source_ranges.size() - 1),
                        highest_end_time);
    }

    // Step 5.3: Let new intersection ranges equal the intersection between
    // the intersection ranges and the source ranges.
    // Step 5.4: Replace the ranges in intersection ranges with the new
    // intersection ranges.
    intersection_ranges = intersection_ranges.IntersectionWith(source_ranges);
  }

  return intersection_ranges;
}

SourceBufferState::SourceBufferState(
    scoped_ptr<StreamParser> stream_parser,
    scoped_ptr<FrameProcessor> frame_processor,
    const CreateDemuxerStreamCB& create_demuxer_stream_cb,
    const scoped_refptr<MediaLog>& media_log)
    : num_missing_track_logs_(0),
      create_demuxer_stream_cb_(create_demuxer_stream_cb),
      timestamp_offset_during_append_(NULL),
      parsing_media_segment_(false),
      stream_parser_(stream_parser.release()),
      frame_processor_(frame_processor.release()),
      media_log_(media_log),
      state_(UNINITIALIZED),
      append_in_progress_(false),
      first_init_segment_received_(false),
      auto_update_timestamp_offset_(false) {
  DCHECK(!create_demuxer_stream_cb_.is_null());
  DCHECK(frame_processor_);
}

SourceBufferState::~SourceBufferState() { Shutdown(); }

void SourceBufferState::Init(
    const StreamParser::InitCB& init_cb, const std::string& expected_codecs,
    const StreamParser::EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    const NewTextTrackCB& new_text_track_cb) {
  DCHECK_EQ(state_, UNINITIALIZED);
  new_text_track_cb_ = new_text_track_cb;
  init_cb_ = init_cb;

  std::vector<std::string> expected_codecs_parsed;
  ParseCodecString(expected_codecs, &expected_codecs_parsed, false);

  std::vector<AudioCodec> expected_acodecs;
  std::vector<VideoCodec> expected_vcodecs;
  for (size_t i = 0; i < expected_codecs_parsed.size(); ++i) {
    AudioCodec acodec = StringToAudioCodec(expected_codecs_parsed[i]);
    if (acodec != kUnknownAudioCodec) {
      expected_audio_codecs_.push_back(acodec);
      continue;
    }
    VideoCodec vcodec = StringToVideoCodec(expected_codecs_parsed[i]);
    if (vcodec != kUnknownVideoCodec) {
      expected_video_codecs_.push_back(vcodec);
      continue;
    }
    MEDIA_LOG(INFO, media_log_) << "Unrecognized media codec: "
                                << expected_codecs_parsed[i];
  }

  state_ = PENDING_PARSER_CONFIG;
  stream_parser_->Init(
      base::Bind(&SourceBufferState::OnSourceInitDone, base::Unretained(this)),
      base::Bind(&SourceBufferState::OnNewConfigs, base::Unretained(this),
                 expected_codecs),
      base::Bind(&SourceBufferState::OnNewBuffers, base::Unretained(this)),
      new_text_track_cb_.is_null(), encrypted_media_init_data_cb,
      base::Bind(&SourceBufferState::OnNewMediaSegment, base::Unretained(this)),
      base::Bind(&SourceBufferState::OnEndOfMediaSegment,
                 base::Unretained(this)),
      media_log_);
}

void SourceBufferState::SetSequenceMode(bool sequence_mode) {
  DCHECK(!parsing_media_segment_);

  frame_processor_->SetSequenceMode(sequence_mode);
}

void SourceBufferState::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DCHECK(!parsing_media_segment_);

  frame_processor_->SetGroupStartTimestampIfInSequenceMode(timestamp_offset);
}

void SourceBufferState::SetTracksWatcher(
    const Demuxer::MediaTracksUpdatedCB& tracks_updated_cb) {
  DCHECK(init_segment_received_cb_.is_null());
  DCHECK(!tracks_updated_cb.is_null());
  init_segment_received_cb_ = tracks_updated_cb;
}

bool SourceBufferState::Append(const uint8_t* data, size_t length,
                               TimeDelta append_window_start,
                               TimeDelta append_window_end,
                               TimeDelta* timestamp_offset) {
  append_in_progress_ = true;
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;
  timestamp_offset_during_append_ = timestamp_offset;

  // TODO(wolenetz/acolwell): Curry and pass a NewBuffersCB here bound with
  // append window and timestamp offset pointer. See http://crbug.com/351454.
  bool result = stream_parser_->Parse(data, length);
  if (!result) {
    MEDIA_LOG(ERROR, media_log_)
        << __func__ << ": stream parsing failed. Data size=" << length
        << " append_window_start=" << append_window_start.InSecondsF()
        << " append_window_end=" << append_window_end.InSecondsF();
  }
  timestamp_offset_during_append_ = NULL;
  append_in_progress_ = false;
  return result;
}

void SourceBufferState::ResetParserState(TimeDelta append_window_start,
                                         TimeDelta append_window_end,
                                         base::TimeDelta* timestamp_offset) {
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  timestamp_offset_during_append_ = timestamp_offset;
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;

  stream_parser_->Flush();
  timestamp_offset_during_append_ = NULL;

  frame_processor_->Reset();
  parsing_media_segment_ = false;
  media_segment_has_data_for_track_.clear();
}

void SourceBufferState::Remove(TimeDelta start, TimeDelta end,
                               TimeDelta duration) {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->Remove(start, end, duration);
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->Remove(start, end, duration);
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->Remove(start, end, duration);
  }
}

bool SourceBufferState::EvictCodedFrames(DecodeTimestamp media_time,
                                         size_t newDataSize) {
  size_t total_buffered_size = 0;
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    total_buffered_size += it->second->GetBufferedSize();
  }
  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    total_buffered_size += it->second->GetBufferedSize();
  }
  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    total_buffered_size += it->second->GetBufferedSize();
  }

  DVLOG(3) << __func__ << " media_time=" << media_time.InSecondsF()
           << " newDataSize=" << newDataSize
           << " total_buffered_size=" << total_buffered_size;

  if (total_buffered_size == 0) return true;

  bool success = true;
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    uint64_t curr_size = it->second->GetBufferedSize();
    if (curr_size == 0) continue;
    uint64_t estimated_new_size = newDataSize * curr_size / total_buffered_size;
    DCHECK_LE(estimated_new_size, SIZE_MAX);
    success &= it->second->EvictCodedFrames(
        media_time, static_cast<size_t>(estimated_new_size));
  }
  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    uint64_t curr_size = it->second->GetBufferedSize();
    if (curr_size == 0) continue;
    uint64_t estimated_new_size = newDataSize * curr_size / total_buffered_size;
    DCHECK_LE(estimated_new_size, SIZE_MAX);
    success &= it->second->EvictCodedFrames(
        media_time, static_cast<size_t>(estimated_new_size));
  }
  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    uint64_t curr_size = it->second->GetBufferedSize();
    if (curr_size == 0) continue;
    uint64_t estimated_new_size = newDataSize * curr_size / total_buffered_size;
    DCHECK_LE(estimated_new_size, SIZE_MAX);
    success &= it->second->EvictCodedFrames(
        media_time, static_cast<size_t>(estimated_new_size));
  }

  DVLOG(3) << __func__ << " success=" << success;
  return success;
}

Ranges<TimeDelta> SourceBufferState::GetBufferedRanges(TimeDelta duration,
                                                       bool ended) const {
  RangesList ranges_list;
  for (DemuxerStreamMap::const_iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    ranges_list.push_back(it->second->GetBufferedRanges(duration));
  }

  for (DemuxerStreamMap::const_iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    ranges_list.push_back(it->second->GetBufferedRanges(duration));
  }

  for (DemuxerStreamMap::const_iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    ranges_list.push_back(it->second->GetBufferedRanges(duration));
  }

  return ComputeRangesIntersection(ranges_list, ended);
}

TimeDelta SourceBufferState::GetHighestPresentationTimestamp() const {
  TimeDelta max_pts;

  for (DemuxerStreamMap::const_iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    max_pts = std::max(max_pts, it->second->GetHighestPresentationTimestamp());
  }

  for (DemuxerStreamMap::const_iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    max_pts = std::max(max_pts, it->second->GetHighestPresentationTimestamp());
  }

  for (DemuxerStreamMap::const_iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    max_pts = std::max(max_pts, it->second->GetHighestPresentationTimestamp());
  }

  return max_pts;
}

TimeDelta SourceBufferState::GetMaxBufferedDuration() const {
  TimeDelta max_duration;

  for (DemuxerStreamMap::const_iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    max_duration = std::max(max_duration, it->second->GetBufferedDuration());
  }

  for (DemuxerStreamMap::const_iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    max_duration = std::max(max_duration, it->second->GetBufferedDuration());
  }

  for (DemuxerStreamMap::const_iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    max_duration = std::max(max_duration, it->second->GetBufferedDuration());
  }

  return max_duration;
}

void SourceBufferState::StartReturningData() {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->StartReturningData();
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->StartReturningData();
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->StartReturningData();
  }
}

void SourceBufferState::AbortReads() {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->AbortReads();
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->AbortReads();
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->AbortReads();
  }
}

void SourceBufferState::Seek(TimeDelta seek_time) {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->Seek(seek_time);
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->Seek(seek_time);
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->Seek(seek_time);
  }
}

void SourceBufferState::CompletePendingReadIfPossible() {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->CompletePendingReadIfPossible();
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->CompletePendingReadIfPossible();
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->CompletePendingReadIfPossible();
  }
}

void SourceBufferState::OnSetDuration(TimeDelta duration) {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->OnSetDuration(duration);
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->OnSetDuration(duration);
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->OnSetDuration(duration);
  }
}

void SourceBufferState::MarkEndOfStream() {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->MarkEndOfStream();
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->MarkEndOfStream();
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->MarkEndOfStream();
  }
}

void SourceBufferState::UnmarkEndOfStream() {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->UnmarkEndOfStream();
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->UnmarkEndOfStream();
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->UnmarkEndOfStream();
  }
}

void SourceBufferState::Shutdown() {
  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    it->second->Shutdown();
  }

  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    it->second->Shutdown();
  }

  for (DemuxerStreamMap::iterator it = text_streams_.begin();
       it != text_streams_.end(); ++it) {
    it->second->Shutdown();
  }
}

void SourceBufferState::SetMemoryLimits(DemuxerStream::Type type,
                                        size_t memory_limit) {
  switch (type) {
    case DemuxerStream::AUDIO:
      for (DemuxerStreamMap::iterator it = audio_streams_.begin();
           it != audio_streams_.end(); ++it) {
        it->second->SetStreamMemoryLimit(memory_limit);
      }
      break;
    case DemuxerStream::VIDEO:
      for (DemuxerStreamMap::iterator it = video_streams_.begin();
           it != video_streams_.end(); ++it) {
        it->second->SetStreamMemoryLimit(memory_limit);
      }
      break;
    case DemuxerStream::TEXT:
      for (DemuxerStreamMap::iterator it = text_streams_.begin();
           it != text_streams_.end(); ++it) {
        it->second->SetStreamMemoryLimit(memory_limit);
      }
      break;
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::NUM_TYPES:
      NOTREACHED();
      break;
  }
}

bool SourceBufferState::IsSeekWaitingForData() const {
  for (DemuxerStreamMap::const_iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    if (it->second->IsSeekWaitingForData()) return true;
  }

  for (DemuxerStreamMap::const_iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    if (it->second->IsSeekWaitingForData()) return true;
  }

  // NOTE: We are intentionally not checking the text tracks
  // because text tracks are discontinuous and may not have data
  // for the seek position. This is ok and playback should not be
  // stalled because we don't have cues. If cues, with timestamps after
  // the seek time, eventually arrive they will be delivered properly
  // in response to ChunkDemuxerStream::Read() calls.

  return false;
}

bool SourceBufferState::OnNewConfigs(
    std::string expected_codecs, scoped_ptr<MediaTracks> tracks,
    const StreamParser::TextTrackConfigMap& text_configs) {
  DCHECK(tracks.get());
  DVLOG(1) << __func__ << " expected_codecs=" << expected_codecs
           << " tracks=" << tracks->tracks().size();
  DCHECK_GE(state_, PENDING_PARSER_CONFIG);

  // Check that there is no clashing bytestream track ids.
  if (!CheckBytestreamTrackIds(*tracks, text_configs)) {
    MEDIA_LOG(ERROR, media_log_) << "Duplicate bytestream track ids detected";
    for (size_t i = 0; i < tracks->tracks().size(); ++i) {
      const StreamParser::TrackId& track_id =
          tracks->tracks()[i]->bytestream_track_id();
      MEDIA_LOG(DEBUG, media_log_)
          << TrackTypeToStr(tracks->tracks()[i]->type()) << " track "
          << " bytestream track id=" << track_id;
    }
    return false;
  }

  // MSE spec allows new configs to be emitted only during Append, but not
  // during Flush or parser reset operations.
  CHECK(append_in_progress_);

  bool success = true;

  // TODO(wolenetz): Update codec string strictness, if necessary, once spec
  // issue https://github.com/w3c/media-source/issues/161 is resolved.
  std::vector<AudioCodec> expected_acodecs = expected_audio_codecs_;
  std::vector<VideoCodec> expected_vcodecs = expected_video_codecs_;

  FrameProcessor::TrackIdChanges track_id_changes;
  for (size_t i = 0; i < tracks->tracks().size(); ++i) {
    StreamParser::TrackId track_id = tracks->tracks()[i]->bytestream_track_id();

    if (tracks->tracks()[i]->type() == MediaTrack::Audio) {
      AudioDecoderConfig audio_config = tracks->getAudioConfig(track_id);
      DVLOG(1) << "Audio track_id=" << track_id
               << " config: " << audio_config.AsHumanReadableString();
      DCHECK(audio_config.IsValidConfig());

      std::vector<AudioCodec>::iterator it =
          std::find(expected_acodecs.begin(), expected_acodecs.end(),
                    audio_config.codec());
      if (it == expected_acodecs.end()) {
        MEDIA_LOG(ERROR, media_log_) << "Audio stream codec "
                                     << GetCodecName(audio_config.codec())
                                     << " doesn't match SourceBuffer codecs.";
        return false;
      }
      expected_acodecs.erase(it);

      ChunkDemuxerStream* stream = NULL;
      if (!first_init_segment_received_) {
        DCHECK(audio_streams_.find(track_id) == audio_streams_.end());
        stream = create_demuxer_stream_cb_.Run(DemuxerStream::AUDIO);
        if (!stream || !frame_processor_->AddTrack(track_id, stream)) {
          MEDIA_LOG(ERROR, media_log_) << "Failed to create audio stream.";
          return false;
        }
        audio_streams_[track_id] = stream;
        media_log_->SetBooleanProperty("found_audio_stream", true);
        media_log_->SetStringProperty("audio_codec_name",
                                      GetCodecName(audio_config.codec()));
      } else {
        if (audio_streams_.size() > 1) {
          DemuxerStreamMap::iterator it = audio_streams_.find(track_id);
          if (it != audio_streams_.end()) stream = it->second;
        } else {
          // If there is only one audio track then bytestream id might change in
          // a new init segment. So update our state and notify frame processor.
          DemuxerStreamMap::iterator it = audio_streams_.begin();
          if (it != audio_streams_.end()) {
            stream = it->second;
            if (it->first != track_id) {
              track_id_changes[it->first] = track_id;
              audio_streams_[track_id] = stream;
              audio_streams_.erase(it->first);
            }
          }
        }
        if (!stream) {
          MEDIA_LOG(ERROR, media_log_) << "Got unexpected audio track"
                                       << " track_id=" << track_id;
          return false;
        }
      }

      tracks->tracks()[i]->set_id(stream->media_track_id());
      frame_processor_->OnPossibleAudioConfigUpdate(audio_config);
      success &= stream->UpdateAudioConfig(audio_config, media_log_);
    } else if (tracks->tracks()[i]->type() == MediaTrack::Video) {
      VideoDecoderConfig video_config = tracks->getVideoConfig(track_id);
      DVLOG(1) << "Video track_id=" << track_id
               << " config: " << video_config.AsHumanReadableString();
      DCHECK(video_config.IsValidConfig());

      std::vector<VideoCodec>::iterator it =
          std::find(expected_vcodecs.begin(), expected_vcodecs.end(),
                    video_config.codec());
      if (it == expected_vcodecs.end()) {
        MEDIA_LOG(ERROR, media_log_) << "Video stream codec "
                                     << GetCodecName(video_config.codec())
                                     << " doesn't match SourceBuffer codecs.";
        return false;
      }
      expected_vcodecs.erase(it);

      ChunkDemuxerStream* stream = NULL;
      if (!first_init_segment_received_) {
        DCHECK(video_streams_.find(track_id) == video_streams_.end());
        stream = create_demuxer_stream_cb_.Run(DemuxerStream::VIDEO);
        if (!stream || !frame_processor_->AddTrack(track_id, stream)) {
          MEDIA_LOG(ERROR, media_log_) << "Failed to create video stream.";
          return false;
        }
        video_streams_[track_id] = stream;
        media_log_->SetBooleanProperty("found_video_stream", true);
        media_log_->SetStringProperty("video_codec_name",
                                      GetCodecName(video_config.codec()));
      } else {
        if (video_streams_.size() > 1) {
          DemuxerStreamMap::iterator it = video_streams_.find(track_id);
          if (it != video_streams_.end()) stream = it->second;
        } else {
          // If there is only one video track then bytestream id might change in
          // a new init segment. So update our state and notify frame processor.
          DemuxerStreamMap::iterator it = video_streams_.begin();
          if (it != video_streams_.end()) {
            stream = it->second;
            if (it->first != track_id) {
              track_id_changes[it->first] = track_id;
              video_streams_[track_id] = stream;
              video_streams_.erase(it->first);
            }
          }
        }
        if (!stream) {
          MEDIA_LOG(ERROR, media_log_) << "Got unexpected video track"
                                       << " track_id=" << track_id;
          return false;
        }
      }

      tracks->tracks()[i]->set_id(stream->media_track_id());
      success &= stream->UpdateVideoConfig(video_config, media_log_);
    } else {
      MEDIA_LOG(ERROR, media_log_) << "Error: unsupported media track type "
                                   << tracks->tracks()[i]->type();
      return false;
    }
  }

  if (!expected_acodecs.empty() || !expected_vcodecs.empty()) {
    for (size_t i = 0; i < expected_acodecs.size(); ++i) {
      MEDIA_LOG(ERROR, media_log_) << "Initialization segment misses expected "
                                   << GetCodecName(expected_acodecs[i])
                                   << " track.";
    }
    for (size_t i = 0; i < expected_vcodecs.size(); ++i) {
      MEDIA_LOG(ERROR, media_log_) << "Initialization segment misses expected "
                                   << GetCodecName(expected_vcodecs[i])
                                   << " track.";
    }
    return false;
  }

  if (text_streams_.empty()) {
    for (StreamParser::TextTrackConfigMap::const_iterator itr =
             text_configs.begin();
         itr != text_configs.end(); ++itr) {
      ChunkDemuxerStream* const text_stream =
          create_demuxer_stream_cb_.Run(DemuxerStream::TEXT);
      if (!frame_processor_->AddTrack(itr->first, text_stream)) {
        success &= false;
        MEDIA_LOG(ERROR, media_log_) << "Failed to add text track ID "
                                     << itr->first << " to frame processor.";
        break;
      }
      text_stream->UpdateTextConfig(itr->second, media_log_);
      text_streams_[itr->first] = text_stream;
      new_text_track_cb_.Run(text_stream, itr->second);
    }
  } else {
    const size_t text_count = text_streams_.size();
    if (text_configs.size() != text_count) {
      success &= false;
      MEDIA_LOG(ERROR, media_log_)
          << "The number of text track configs changed.";
    } else if (text_count == 1) {
      StreamParser::TextTrackConfigMap::const_iterator config_itr =
          text_configs.begin();
      DemuxerStreamMap::iterator stream_itr = text_streams_.begin();
      ChunkDemuxerStream* text_stream = stream_itr->second;
      TextTrackConfig old_config = text_stream->text_track_config();
      TextTrackConfig new_config(
          config_itr->second.kind(), config_itr->second.label(),
          config_itr->second.language(), old_config.id());
      if (!new_config.Matches(old_config)) {
        success &= false;
        MEDIA_LOG(ERROR, media_log_)
            << "New text track config does not match old one.";
      } else {
        StreamParser::TrackId old_id = stream_itr->first;
        StreamParser::TrackId new_id = config_itr->first;
        if (new_id != old_id) {
          track_id_changes[old_id] = new_id;
          text_streams_.erase(old_id);
          text_streams_[new_id] = text_stream;
        }
      }
    } else {
      for (StreamParser::TextTrackConfigMap::const_iterator config_itr =
               text_configs.begin();
           config_itr != text_configs.end(); ++config_itr) {
        DemuxerStreamMap::iterator stream_itr =
            text_streams_.find(config_itr->first);
        if (stream_itr == text_streams_.end()) {
          success &= false;
          MEDIA_LOG(ERROR, media_log_)
              << "Unexpected text track configuration for track ID "
              << config_itr->first;
          break;
        }

        const TextTrackConfig& new_config = config_itr->second;
        ChunkDemuxerStream* stream = stream_itr->second;
        TextTrackConfig old_config = stream->text_track_config();
        if (!new_config.Matches(old_config)) {
          success &= false;
          MEDIA_LOG(ERROR, media_log_) << "New text track config for track ID "
                                       << config_itr->first
                                       << " does not match old one.";
          break;
        }
      }
    }
  }

  if (audio_streams_.empty() && video_streams_.empty()) {
    DVLOG(1) << __func__ << ": couldn't find a valid audio or video stream";
    return false;
  }

  if (!frame_processor_->UpdateTrackIds(track_id_changes)) {
    DVLOG(1) << __func__ << ": failed to remap track ids in frame processor";
    return false;
  }

  frame_processor_->SetAllTrackBuffersNeedRandomAccessPoint();

  if (!first_init_segment_received_) {
    first_init_segment_received_ = true;
    SetStreamMemoryLimits();
  }

  DVLOG(1) << "OnNewConfigs() : " << (success ? "success" : "failed");
  if (success) {
    if (state_ == PENDING_PARSER_CONFIG) state_ = PENDING_PARSER_INIT;
    DCHECK(!init_segment_received_cb_.is_null());
    init_segment_received_cb_.Run(tracks.Pass());
  }

  return success;
}

void SourceBufferState::SetStreamMemoryLimits() {}

void SourceBufferState::OnNewMediaSegment() {
  DVLOG(2) << "OnNewMediaSegment()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  parsing_media_segment_ = true;
  media_segment_has_data_for_track_.clear();
}

void SourceBufferState::OnEndOfMediaSegment() {
  DVLOG(2) << "OnEndOfMediaSegment()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  parsing_media_segment_ = false;

  for (DemuxerStreamMap::iterator it = audio_streams_.begin();
       it != audio_streams_.end(); ++it) {
    if (!media_segment_has_data_for_track_[it->first]) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_missing_track_logs_,
                        kMaxMissingTrackInSegmentLogs)
          << "Media segment did not contain any coded frames for track "
          << it->first << ", mismatching initialization segment. Therefore, MSE"
                          " coded frame processing may not interoperably detect"
                          " discontinuities in appended media.";
    }
  }
  for (DemuxerStreamMap::iterator it = video_streams_.begin();
       it != video_streams_.end(); ++it) {
    if (!media_segment_has_data_for_track_[it->first]) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_missing_track_logs_,
                        kMaxMissingTrackInSegmentLogs)
          << "Media segment did not contain any coded frames for track "
          << it->first << ", mismatching initialization segment. Therefore, MSE"
                          " coded frame processing may not interoperably detect"
                          " discontinuities in appended media.";
    }
  }
}

bool SourceBufferState::OnNewBuffers(
    const StreamParser::BufferQueueMap& buffer_queue_map) {
  DVLOG(2) << __func__ << " buffer_queues=" << buffer_queue_map.size();
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  DCHECK(timestamp_offset_during_append_);
  DCHECK(parsing_media_segment_);

  for (StreamParser::BufferQueueMap::const_iterator it =
           buffer_queue_map.begin();
       it != buffer_queue_map.end(); ++it) {
    const StreamParser::BufferQueue& bufq = it->second;
    DCHECK(!bufq.empty());
    media_segment_has_data_for_track_[it->first] = true;
  }

  const TimeDelta timestamp_offset_before_processing =
      *timestamp_offset_during_append_;

  // Calculate the new timestamp offset for audio/video tracks if the stream
  // parser has requested automatic updates.
  TimeDelta new_timestamp_offset = timestamp_offset_before_processing;
  if (auto_update_timestamp_offset_) {
    TimeDelta min_end_timestamp = kNoTimestamp;
    for (StreamParser::BufferQueueMap::const_iterator it =
             buffer_queue_map.begin();
         it != buffer_queue_map.end(); ++it) {
      const StreamParser::BufferQueue& bufq = it->second;
      DCHECK(!bufq.empty());
      if (min_end_timestamp == kNoTimestamp ||
          EndTimestamp(bufq) < min_end_timestamp) {
        min_end_timestamp = EndTimestamp(bufq);
        DCHECK_NE(kNoTimestamp, min_end_timestamp);
      }
    }
    if (min_end_timestamp != kNoTimestamp)
      new_timestamp_offset += min_end_timestamp;
  }

  if (!frame_processor_->ProcessFrames(
          buffer_queue_map, append_window_start_during_append_,
          append_window_end_during_append_, timestamp_offset_during_append_)) {
    return false;
  }

  // Only update the timestamp offset if the frame processor hasn't already.
  if (auto_update_timestamp_offset_ &&
      timestamp_offset_before_processing == *timestamp_offset_during_append_) {
    *timestamp_offset_during_append_ = new_timestamp_offset;
  }

  return true;
}
void SourceBufferState::OnSourceInitDone(
    const StreamParser::InitParameters& params) {
  DCHECK_EQ(state_, PENDING_PARSER_INIT);
  state_ = PARSER_INITIALIZED;
  auto_update_timestamp_offset_ = params.auto_update_timestamp_offset;
  base::ResetAndReturn(&init_cb_).Run(params);
}

}  // namespace media
