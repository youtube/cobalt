// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/blink/websourcebuffer_impl.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/string_number_conversions.h"
#include "cobalt/media/base/media_tracks.h"
#include "cobalt/media/base/timestamp_constants.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebSourceBufferClient.h"

namespace media {

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!std::isnan(time));
  DCHECK_NE(time, -std::numeric_limits<double>::infinity());

  if (time == std::numeric_limits<double>::infinity()) return kInfiniteDuration;

  // Don't use base::TimeDelta::Max() here, as we want the largest finite time
  // delta.
  base::TimeDelta max_time = base::TimeDelta::FromInternalValue(
      std::numeric_limits<int64_t>::max() - 1);
  double max_time_in_seconds = max_time.InSecondsF();

  if (time >= max_time_in_seconds) return max_time;

  return base::TimeDelta::FromMicroseconds(time *
                                           base::Time::kMicrosecondsPerSecond);
}

WebSourceBufferImpl::WebSourceBufferImpl(const std::string& id,
                                         ChunkDemuxer* demuxer)
    : id_(id),
      demuxer_(demuxer),
      client_(NULL),
      append_window_end_(kInfiniteDuration) {
  DCHECK(demuxer_);
  demuxer_->SetTracksWatcher(
      id, base::Bind(&WebSourceBufferImpl::InitSegmentReceived,
                     base::Unretained(this)));
}

WebSourceBufferImpl::~WebSourceBufferImpl() {
  DCHECK(!demuxer_) << "Object destroyed w/o removedFromMediaSource() call";
  DCHECK(!client_);
}

void WebSourceBufferImpl::setClient(blink::WebSourceBufferClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

bool WebSourceBufferImpl::setMode(WebSourceBuffer::AppendMode mode) {
  if (demuxer_->IsParsingMediaSegment(id_)) return false;

  switch (mode) {
    case WebSourceBuffer::AppendModeSegments:
      demuxer_->SetSequenceMode(id_, false);
      return true;
    case WebSourceBuffer::AppendModeSequence:
      demuxer_->SetSequenceMode(id_, true);
      return true;
  }

  NOTREACHED();
  return false;
}

blink::WebTimeRanges WebSourceBufferImpl::buffered() {
  Ranges<base::TimeDelta> ranges = demuxer_->GetBufferedRanges(id_);
  blink::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

double WebSourceBufferImpl::highestPresentationTimestamp() {
  return demuxer_->GetHighestPresentationTimestamp(id_).InSecondsF();
}

bool WebSourceBufferImpl::evictCodedFrames(double currentPlaybackTime,
                                           size_t newDataSize) {
  return demuxer_->EvictCodedFrames(
      id_, base::TimeDelta::FromSecondsD(currentPlaybackTime), newDataSize);
}

bool WebSourceBufferImpl::append(const unsigned char* data, unsigned length,
                                 double* timestamp_offset) {
  base::TimeDelta old_offset = timestamp_offset_;
  bool success = demuxer_->AppendData(id_, data, length, append_window_start_,
                                      append_window_end_, &timestamp_offset_);

  // Coded frame processing may update the timestamp offset. If the caller
  // provides a non-NULL |timestamp_offset| and frame processing changes the
  // timestamp offset, report the new offset to the caller. Do not update the
  // caller's offset otherwise, to preserve any pre-existing value that may have
  // more than microsecond precision.
  if (timestamp_offset && old_offset != timestamp_offset_)
    *timestamp_offset = timestamp_offset_.InSecondsF();

  return success;
}

void WebSourceBufferImpl::resetParserState() {
  demuxer_->ResetParserState(id_, append_window_start_, append_window_end_,
                             &timestamp_offset_);

  // TODO(wolenetz): resetParserState should be able to modify the caller
  // timestamp offset (just like WebSourceBufferImpl::append).
  // See http://crbug.com/370229 for further details.
}

void WebSourceBufferImpl::remove(double start, double end) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  demuxer_->Remove(id_, DoubleToTimeDelta(start), DoubleToTimeDelta(end));
}

bool WebSourceBufferImpl::setTimestampOffset(double offset) {
  if (demuxer_->IsParsingMediaSegment(id_)) return false;

  timestamp_offset_ = DoubleToTimeDelta(offset);

  // http://www.w3.org/TR/media-source/#widl-SourceBuffer-timestampOffset
  // Step 6: If the mode attribute equals "sequence", then set the group start
  // timestamp to new timestamp offset.
  demuxer_->SetGroupStartTimestampIfInSequenceMode(id_, timestamp_offset_);
  return true;
}

void WebSourceBufferImpl::setAppendWindowStart(double start) {
  DCHECK_GE(start, 0);
  append_window_start_ = DoubleToTimeDelta(start);
}

void WebSourceBufferImpl::setAppendWindowEnd(double end) {
  DCHECK_GE(end, 0);
  append_window_end_ = DoubleToTimeDelta(end);
}

void WebSourceBufferImpl::removedFromMediaSource() {
  demuxer_->RemoveId(id_);
  demuxer_ = NULL;
  client_ = NULL;
}

blink::WebMediaPlayer::TrackType mediaTrackTypeToBlink(MediaTrack::Type type) {
  switch (type) {
    case MediaTrack::Audio:
      return blink::WebMediaPlayer::AudioTrack;
    case MediaTrack::Text:
      return blink::WebMediaPlayer::TextTrack;
    case MediaTrack::Video:
      return blink::WebMediaPlayer::VideoTrack;
  }
  NOTREACHED();
  return blink::WebMediaPlayer::AudioTrack;
}

void WebSourceBufferImpl::InitSegmentReceived(
    std::unique_ptr<MediaTracks> tracks) {
  DCHECK(tracks.get());
  DVLOG(1) << __func__ << " tracks=" << tracks->tracks().size();

  std::vector<blink::WebSourceBufferClient::MediaTrackInfo> trackInfoVector;
  for (const auto& track : tracks->tracks()) {
    blink::WebSourceBufferClient::MediaTrackInfo trackInfo;
    trackInfo.trackType = mediaTrackTypeToBlink(track->type());
    trackInfo.id = blink::WebString::fromUTF8(track->id());
    trackInfo.byteStreamTrackID = blink::WebString::fromUTF8(
        base::UintToString(track->bytestream_track_id()));
    trackInfo.kind = blink::WebString::fromUTF8(track->kind());
    trackInfo.label = blink::WebString::fromUTF8(track->label());
    trackInfo.language = blink::WebString::fromUTF8(track->language());
    trackInfoVector.push_back(trackInfo);
  }

  client_->initializationSegmentReceived(trackInfoVector);
}

}  // namespace media
