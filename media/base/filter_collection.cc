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

void FilterCollection::AddAudioRenderer(AudioRenderer* audio_renderer) {
  audio_renderers_.push_back(audio_renderer);
}

void FilterCollection::AddVideoRenderer(VideoRenderer* video_renderer) {
  video_renderers_.push_back(video_renderer);
}

void FilterCollection::Clear() {
  audio_decoders_.clear();
  video_decoders_.clear();
  audio_renderers_.clear();
  video_renderers_.clear();
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

FilterCollection::AudioDecoderList* FilterCollection::GetAudioDecoders() {
  return &audio_decoders_;
}

FilterCollection::VideoDecoderList* FilterCollection::GetVideoDecoders() {
  return &video_decoders_;
}

}  // namespace media
