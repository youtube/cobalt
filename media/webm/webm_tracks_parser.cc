// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_tracks_parser.h"

#include "base/logging.h"
#include "media/webm/webm_constants.h"

namespace media {

static const base::TimeDelta kNoDuration =
    base::TimeDelta::FromMicroseconds(-1);

WebMTracksParser::WebMTracksParser(int64 timecode_scale)
    : timecode_scale_(timecode_scale),
      track_type_(-1),
      track_num_(-1),
      track_default_duration_(-1),
      audio_track_num_(-1),
      audio_default_duration_(kNoDuration),
      video_track_num_(-1),
      video_default_duration_(kNoDuration) {
}

WebMTracksParser::~WebMTracksParser() {}

int WebMTracksParser::Parse(const uint8* buf, int size) {
  track_type_ =-1;
  track_num_ = -1;
  track_default_duration_ = -1;
  audio_track_num_ = -1;
  audio_default_duration_ = kNoDuration;
  video_track_num_ = -1;
  video_default_duration_ = kNoDuration;

  WebMListParser parser(kWebMIdTracks);
  int result = parser.Parse(buf, size, this);

  if (result <= 0)
    return result;

  // For now we do all or nothing parsing.
  return parser.IsParsingComplete() ? result : 0;
}


bool WebMTracksParser::OnListStart(int id) {
  if (id == kWebMIdTrackEntry) {
    track_type_ = -1;
    track_num_ = -1;
    track_default_duration_ = -1;
  }

  return true;
}

bool WebMTracksParser::OnListEnd(int id) {
  if (id == kWebMIdTrackEntry) {
    if (track_type_ == -1 || track_num_ == -1) {
      DVLOG(1) << "Missing TrackEntry data"
               << " TrackType " << track_type_
               << " TrackNum " << track_num_;
      return false;
    }

    // Convert nanoseconds to base::TimeDelta.
    base::TimeDelta default_duration = base::TimeDelta::FromMicroseconds(
        track_default_duration_ / 1000.0);

    if (track_type_ == kWebMTrackTypeVideo) {
      video_track_num_ = track_num_;
      video_default_duration_ = default_duration;
    } else if (track_type_ == kWebMTrackTypeAudio) {
      audio_track_num_ = track_num_;
      audio_default_duration_ = default_duration;
    } else {
      DVLOG(1) << "Unexpected TrackType " << track_type_;
      return false;
    }

    track_type_ = -1;
    track_num_ = -1;
  }

  return true;
}

bool WebMTracksParser::OnUInt(int id, int64 val) {
  int64* dst = NULL;

  switch (id) {
    case kWebMIdTrackNumber:
      dst = &track_num_;
      break;
    case kWebMIdTrackType:
      dst = &track_type_;
      break;
    case  kWebMIdDefaultDuration:
      dst = &track_default_duration_;
      break;
    default:
      return true;
  }

  if (*dst != -1) {
    DVLOG(1) << "Multiple values for id " << std::hex << id << " specified";
    return false;
  }

  *dst = val;
  return true;
}

bool WebMTracksParser::OnFloat(int id, double val) {
  DVLOG(1) << "Unexpected float for id" << std::hex << id;
  return false;
}

bool WebMTracksParser::OnBinary(int id, const uint8* data, int size) {
  return true;
}

bool WebMTracksParser::OnString(int id, const std::string& str) {
  if (id != kWebMIdCodecID)
    return false;

  if (str != "A_VORBIS" && str != "V_VP8") {
    DVLOG(1) << "Unexpected CodecID " << str;
    return false;
  }

  return true;
}

bool WebMTracksParser::OnSimpleBlock(int track_num, int timecode, int flags,
                                     const uint8* data, int size) {
  return false;
}

}  // namespace media
