// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_collection.h"

#include "base/logging.h"
#include "media/base/audio_decoder.h"
#include "media/base/video_decoder.h"

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

void FilterCollection::AddAudioRenderer(AudioRenderer* filter) {
  AddFilter(AUDIO_RENDERER, filter);
}

void FilterCollection::AddVideoRenderer(VideoRenderer* filter) {
  AddFilter(VIDEO_RENDERER, filter);
}

bool FilterCollection::IsEmpty() const {
  return filters_.empty() && audio_decoders_.empty() && video_decoders_.empty();
}

void FilterCollection::Clear() {
  filters_.clear();
  audio_decoders_.clear();
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

void FilterCollection::SelectAudioRenderer(
    scoped_refptr<AudioRenderer>* filter_out) {
  SelectFilter<AUDIO_RENDERER>(filter_out);
}

void FilterCollection::SelectVideoRenderer(
    scoped_refptr<VideoRenderer>* filter_out) {
  SelectFilter<VIDEO_RENDERER>(filter_out);
}

void FilterCollection::AddFilter(FilterType filter_type,
                                 Filter* filter) {
  filters_.push_back(FilterListElement(filter_type, filter));
}

template<FilterCollection::FilterType filter_type, typename F>
void FilterCollection::SelectFilter(scoped_refptr<F>* filter_out) {
  scoped_refptr<Filter> filter;
  SelectFilter(filter_type, &filter);
  *filter_out = reinterpret_cast<F*>(filter.get());
}

void FilterCollection::SelectFilter(
    FilterType filter_type,
    scoped_refptr<Filter>* filter_out)  {

  FilterList::iterator it = filters_.begin();
  while (it != filters_.end()) {
    if (it->first == filter_type)
      break;
    ++it;
  }

  if (it != filters_.end()) {
    *filter_out = it->second.get();
    filters_.erase(it);
  }
}

}  // namespace media
