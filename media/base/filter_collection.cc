// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_collection.h"

#include "base/logging.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/demuxer.h"
#include "media/base/video_decoder.h"
#include "media/base/video_renderer.h"

namespace media {

FilterCollection::FilterCollection() {}

FilterCollection::~FilterCollection() {}

void FilterCollection::SetDemuxer(const scoped_refptr<Demuxer>& demuxer) {
  demuxer_ = demuxer;
}

const scoped_refptr<Demuxer>& FilterCollection::GetDemuxer() {
  return demuxer_;
}

void FilterCollection::AddAudioDecoder(AudioDecoder* audio_decoder) {
  audio_decoders_.push_back(audio_decoder);
}

void FilterCollection::AddVideoDecoder(VideoDecoder* video_decoder) {
  video_decoders_.push_back(video_decoder);
}

void FilterCollection::AddAudioRenderer(AudioRenderer* audio_renderer) {
  audio_renderers_.push_back(audio_renderer);
}

void FilterCollection::AddVideoRenderer(VideoRenderer* video_renderer) {
  video_renderers_.push_back(video_renderer);
}

bool FilterCollection::IsEmpty() const {
  return audio_decoders_.empty() &&
      video_decoders_.empty() &&
      audio_renderers_.empty() &&
      video_renderers_.empty();
}

void FilterCollection::Clear() {
  audio_decoders_.clear();
  video_decoders_.clear();
  audio_renderers_.clear();
  video_renderers_.clear();
}

void FilterCollection::SelectAudioDecoder(scoped_refptr<AudioDecoder>* out) {
  if (audio_decoders_.empty()) {
    *out = NULL;
    return;
  }
  *out = audio_decoders_.front();
  audio_decoders_.pop_front();
}

void FilterCollection::SelectVideoDecoder(scoped_refptr<VideoDecoder>* out) {
  if (video_decoders_.empty()) {
    *out = NULL;
    return;
  }
  *out = video_decoders_.front();
  video_decoders_.pop_front();
}

void FilterCollection::SelectAudioRenderer(scoped_refptr<AudioRenderer>* out) {
  if (audio_renderers_.empty()) {
    *out = NULL;
    return;
  }
  *out = audio_renderers_.front();
  audio_renderers_.pop_front();
}

void FilterCollection::SelectVideoRenderer(scoped_refptr<VideoRenderer>* out) {
  if (video_renderers_.empty()) {
    *out = NULL;
    return;
  }
  *out = video_renderers_.front();
  video_renderers_.pop_front();
}

}  // namespace media
