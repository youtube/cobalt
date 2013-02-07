// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_tracks_parser.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "media/base/buffers.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_content_encodings.h"

namespace media {

// Values for TrackType element.
static const int kWebMTrackTypeVideo = 1;
static const int kWebMTrackTypeAudio = 2;

WebMTracksParser::WebMTracksParser(const LogCB& log_cb)
    : track_type_(-1),
      track_num_(-1),
      audio_track_num_(-1),
      video_track_num_(-1),
      log_cb_(log_cb) {
}

WebMTracksParser::~WebMTracksParser() {}

int WebMTracksParser::Parse(const uint8* buf, int size) {
  track_type_ =-1;
  track_num_ = -1;
  audio_track_num_ = -1;
  video_track_num_ = -1;

  WebMListParser parser(kWebMIdTracks, this);
  int result = parser.Parse(buf, size);

  if (result <= 0)
    return result;

  // For now we do all or nothing parsing.
  return parser.IsParsingComplete() ? result : 0;
}

WebMParserClient* WebMTracksParser::OnListStart(int id) {
  if (id == kWebMIdContentEncodings) {
    DCHECK(!track_content_encodings_client_.get());
    track_content_encodings_client_.reset(
        new WebMContentEncodingsClient(log_cb_));
    return track_content_encodings_client_->OnListStart(id);
  }

  if (id == kWebMIdTrackEntry) {
    track_type_ = -1;
    track_num_ = -1;
    return this;
  }

  return this;
}

bool WebMTracksParser::OnListEnd(int id) {
  if (id == kWebMIdContentEncodings) {
    DCHECK(track_content_encodings_client_.get());
    return track_content_encodings_client_->OnListEnd(id);
  }

  if (id == kWebMIdTrackEntry) {
    if (track_type_ == -1 || track_num_ == -1) {
      MEDIA_LOG(log_cb_) << "Missing TrackEntry data for "
                         << " TrackType " << track_type_
                         << " TrackNum " << track_num_;
      return false;
    }

    if (track_type_ != kWebMTrackTypeAudio &&
        track_type_ != kWebMTrackTypeVideo) {
      MEDIA_LOG(log_cb_) << "Unexpected TrackType " << track_type_;
      return false;
    }

    std::string encryption_key_id;
    if (track_content_encodings_client_.get()) {
      DCHECK(!track_content_encodings_client_->content_encodings().empty());
      // If we have multiple ContentEncoding in one track. Always choose the
      // key id in the first ContentEncoding as the key id of the track.
      encryption_key_id = track_content_encodings_client_->
          content_encodings()[0]->encryption_key_id();
    }

    if (track_type_ == kWebMTrackTypeAudio) {
      audio_track_num_ = track_num_;
      audio_encryption_key_id_ = encryption_key_id;
    } else if (track_type_ == kWebMTrackTypeVideo) {
      video_track_num_ = track_num_;
      video_encryption_key_id_ = encryption_key_id;
    }

    track_type_ = -1;
    track_num_ = -1;
    track_content_encodings_client_.reset();
    return true;
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
    default:
      return true;
  }

  if (*dst != -1) {
    MEDIA_LOG(log_cb_) << "Multiple values for id " << std::hex << id
                       << " specified";
    return false;
  }

  *dst = val;
  return true;
}

bool WebMTracksParser::OnFloat(int id, double val) {
  return true;
}

bool WebMTracksParser::OnBinary(int id, const uint8* data, int size) {
  return true;
}

bool WebMTracksParser::OnString(int id, const std::string& str) {
  if (id == kWebMIdCodecID && str != "A_VORBIS" && str != "V_VP8") {
    MEDIA_LOG(log_cb_) << "Unexpected CodecID " << str;
    return false;
  }

  return true;
}

}  // namespace media
