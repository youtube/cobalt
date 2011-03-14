// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_collection.h"

#include "base/logging.h"

namespace media {

FilterCollection::FilterCollection() {}

FilterCollection::~FilterCollection() {}

void FilterCollection::SetDataSourceFactory(DataSourceFactory* factory) {
  DCHECK(factory);
  data_source_factory_.reset(factory);
}

DataSourceFactory* FilterCollection::GetDataSourceFactory() {
  return data_source_factory_.get();
}

void FilterCollection::AddDemuxer(Demuxer* filter) {
  AddFilter(DEMUXER, filter);
}

void FilterCollection::AddVideoDecoder(VideoDecoder* filter) {
  AddFilter(VIDEO_DECODER, filter);
}

void FilterCollection::AddAudioDecoder(AudioDecoder* filter) {
  AddFilter(AUDIO_DECODER, filter);
}

void FilterCollection::AddVideoRenderer(VideoRenderer* filter) {
  AddFilter(VIDEO_RENDERER, filter);
}

void FilterCollection::AddAudioRenderer(AudioRenderer* filter) {
  AddFilter(AUDIO_RENDERER, filter);
}

bool FilterCollection::IsEmpty() const {
  return filters_.empty();
}

void FilterCollection::Clear() {
  filters_.clear();
}

void FilterCollection::SelectDemuxer(scoped_refptr<Demuxer>* filter_out) {
  SelectFilter<DEMUXER>(filter_out);
}

void FilterCollection::SelectVideoDecoder(
    scoped_refptr<VideoDecoder>* filter_out) {
  SelectFilter<VIDEO_DECODER>(filter_out);
}

void FilterCollection::SelectAudioDecoder(
    scoped_refptr<AudioDecoder>* filter_out) {
  SelectFilter<AUDIO_DECODER>(filter_out);
}

void FilterCollection::SelectVideoRenderer(
    scoped_refptr<VideoRenderer>* filter_out) {
  SelectFilter<VIDEO_RENDERER>(filter_out);
}

void FilterCollection::SelectAudioRenderer(
    scoped_refptr<AudioRenderer>* filter_out) {
  SelectFilter<AUDIO_RENDERER>(filter_out);
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
