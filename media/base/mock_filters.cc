// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

namespace media {

MockDataSource::MockDataSource() {}

MockDataSource::~MockDataSource() {}

MockDemuxer::MockDemuxer() {}

MockDemuxer::~MockDemuxer() {}

MockDemuxerStream::MockDemuxerStream() {}

MockDemuxerStream::~MockDemuxerStream() {}

MockVideoDecoder::MockVideoDecoder() {}

MockVideoDecoder::~MockVideoDecoder() {}

MockAudioDecoder::MockAudioDecoder() {}

MockAudioDecoder::~MockAudioDecoder() {}

MockVideoRenderer::MockVideoRenderer() {}

MockVideoRenderer::~MockVideoRenderer() {}

MockAudioRenderer::MockAudioRenderer() {}

MockAudioRenderer::~MockAudioRenderer() {}

MockFilterCollection::MockFilterCollection()
    : data_source_(new MockDataSource()),
      demuxer_(new MockDemuxer()),
      video_decoder_(new MockVideoDecoder()),
      audio_decoder_(new MockAudioDecoder()),
      video_renderer_(new MockVideoRenderer()),
      audio_renderer_(new MockAudioRenderer()) {
}

MockFilterCollection::~MockFilterCollection() {}

FilterCollection* MockFilterCollection::filter_collection(
    bool include_data_source) const {
  FilterCollection* collection = new FilterCollection();

  if (include_data_source) {
    collection->AddDataSource(data_source_);
  }
  collection->AddDemuxer(demuxer_);
  collection->AddVideoDecoder(video_decoder_);
  collection->AddAudioDecoder(audio_decoder_);
  collection->AddVideoRenderer(video_renderer_);
  collection->AddAudioRenderer(audio_renderer_);
  return collection;
}

void RunFilterCallback(::testing::Unused, FilterCallback* callback) {
  callback->Run();
  delete callback;
}

void RunFilterCallback3(::testing::Unused, FilterCallback* callback,
                        ::testing::Unused) {
  callback->Run();
  delete callback;
}

void DestroyFilterCallback(::testing::Unused, FilterCallback* callback) {
  delete callback;
}

void RunStopFilterCallback(FilterCallback* callback) {
  callback->Run();
  delete callback;
}

MockFilter::MockFilter() {
}

MockFilter::~MockFilter() {}

}  // namespace media
