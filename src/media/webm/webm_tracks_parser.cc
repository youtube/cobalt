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
      log_cb_(log_cb),
      audio_client_(log_cb),
      video_client_(log_cb) {
}

WebMTracksParser::~WebMTracksParser() {}

int WebMTracksParser::Parse(const uint8* buf, int size) {
  track_type_ =-1;
  track_num_ = -1;
  audio_track_num_ = -1;
  AudioDecoderConfig invalid_audio_decoder_config;
  audio_decoder_config_.CopyFrom(invalid_audio_decoder_config);
  video_track_num_ = -1;
  VideoDecoderConfig invalid_video_decoder_config;
  video_decoder_config_.CopyFrom(invalid_video_decoder_config);

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
    codec_id_ = "";
    codec_private_.clear();
    audio_client_.Reset();
    video_client_.Reset();
    return this;
  }

  if (id == kWebMIdAudio)
    return &audio_client_;

  if (id == kWebMIdVideo)
    return &video_client_;

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
      if (audio_track_num_ == -1) {
        audio_track_num_ = track_num_;
        audio_encryption_key_id_ = encryption_key_id;

        DCHECK(!audio_decoder_config_.IsValidConfig());
        if (!audio_client_.InitializeConfig(
                codec_id_, codec_private_, 0/*seek_preroll_*/, 0/*codec_delay_*/,
                !audio_encryption_key_id_.empty(), &audio_decoder_config_)) {
          return false;
        }
      } else {
        MEDIA_LOG(log_cb_) << "Ignoring audio track " << track_num_;
      }
    } else if (track_type_ == kWebMTrackTypeVideo) {
      if (video_track_num_ == -1) {
        video_track_num_ = track_num_;
        video_encryption_key_id_ = encryption_key_id;

        DCHECK(!video_decoder_config_.IsValidConfig());
        if (!video_client_.InitializeConfig(
                codec_id_, codec_private_, !video_encryption_key_id_.empty(),
                &video_decoder_config_)) {
          return false;
        }
      } else {
        MEDIA_LOG(log_cb_) << "Ignoring video track " << track_num_;
      }
    }

    track_type_ = -1;
    track_num_ = -1;
    track_content_encodings_client_.reset();
    codec_id_ = "";
    codec_private_.clear();
    audio_client_.Reset();
    video_client_.Reset();
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
  if (id == kWebMIdCodecPrivate) {
    if (!codec_private_.empty()) {
      MEDIA_LOG(log_cb_) << "Multiple CodecPrivate fields in a track.";
      return false;
    }

    codec_private_.assign(data, data + size);
    return true;
  }
  return true;
}

bool WebMTracksParser::OnString(int id, const std::string& str) {
  if (id == kWebMIdCodecID) {
    if (!codec_id_.empty()) {
      MEDIA_LOG(log_cb_) << "Multiple CodecID fields in a track";
      return false;
    }

    codec_id_ = str;
    return true;
  }

  return true;
}

}  // namespace media
