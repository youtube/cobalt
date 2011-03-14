// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "media/base/filter_host.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NotNull;

namespace media {

MockDataSource::MockDataSource()
    : total_bytes_(-1),
      buffered_bytes_(-1) {
}

MockDataSource::~MockDataSource() {}

void MockDataSource::set_host(FilterHost* filter_host) {
  Filter::set_host(filter_host);

  if (total_bytes_ > 0)
    host()->SetTotalBytes(total_bytes_);

  if (buffered_bytes_ > 0)
    host()->SetBufferedBytes(buffered_bytes_);
}

void MockDataSource::SetTotalAndBufferedBytes(int64 total_bytes,
                                              int64 buffered_bytes) {
  total_bytes_ = total_bytes;
  buffered_bytes_ = buffered_bytes;
}

MockDataSourceFactory::MockDataSourceFactory(MockDataSource* data_source)
    : data_source_(data_source),
      error_(PIPELINE_OK) {
}

MockDataSourceFactory::~MockDataSourceFactory() {}

void MockDataSourceFactory::SetError(PipelineError error) {
  error_ = error;
}

void MockDataSourceFactory::RunBuildCallback(const std::string& url,
                                             BuildCallback* callback) {
  scoped_ptr<BuildCallback> cb(callback);

  if (!data_source_.get()) {
    cb->Run(PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
            static_cast<DataSource*>(NULL));
    return;
  }

  scoped_refptr<MockDataSource> data_source = data_source_;
  data_source_ = NULL;

  if (error_ == PIPELINE_OK) {
    cb->Run(PIPELINE_OK, data_source.get());
    return;
  }

  cb->Run(error_, static_cast<DataSource*>(NULL));
}

void MockDataSourceFactory::DestroyBuildCallback(const std::string& url,
                                                 BuildCallback* callback) {
  delete callback;
}

DataSourceFactory* MockDataSourceFactory::Clone() const {
  return new MockDataSourceFactory(data_source_.get());
}

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
    bool include_data_source,
    bool run_build_callback,
    PipelineError build_error) const {
  FilterCollection* collection = new FilterCollection();

  MockDataSourceFactory* data_source_factory =
      new MockDataSourceFactory(include_data_source ? data_source_ : NULL);

  if (build_error != PIPELINE_OK)
    data_source_factory->SetError(build_error);

  if (run_build_callback) {
    ON_CALL(*data_source_factory, Build(_, NotNull()))
        .WillByDefault(Invoke(data_source_factory,
                              &MockDataSourceFactory::RunBuildCallback));
  } else {
    ON_CALL(*data_source_factory, Build(_, NotNull()))
        .WillByDefault(Invoke(data_source_factory,
                              &MockDataSourceFactory::DestroyBuildCallback));
  }
  EXPECT_CALL(*data_source_factory, Build(_, NotNull()));

  collection->SetDataSourceFactory(data_source_factory);
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

MockStatisticsCallback::MockStatisticsCallback() {}

MockStatisticsCallback::~MockStatisticsCallback() {}

}  // namespace media
