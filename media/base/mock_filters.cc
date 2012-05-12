// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/filter_host.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

MockDemuxer::MockDemuxer() {}

MockDemuxer::~MockDemuxer() {}

MockDemuxerStream::MockDemuxerStream() {}

MockDemuxerStream::~MockDemuxerStream() {}

MockVideoDecoder::MockVideoDecoder() {
  EXPECT_CALL(*this, HasAlpha()).WillRepeatedly(Return(false));
}

MockVideoDecoder::~MockVideoDecoder() {}

MockAudioDecoder::MockAudioDecoder() {}

MockAudioDecoder::~MockAudioDecoder() {}

MockVideoRenderer::MockVideoRenderer() {}

MockVideoRenderer::~MockVideoRenderer() {}

MockAudioRenderer::MockAudioRenderer() {}

MockAudioRenderer::~MockAudioRenderer() {}

MockFilterCollection::MockFilterCollection()
    : demuxer_(new MockDemuxer()),
      video_decoder_(new MockVideoDecoder()),
      audio_decoder_(new MockAudioDecoder()),
      video_renderer_(new MockVideoRenderer()),
      audio_renderer_(new MockAudioRenderer()) {
}

MockFilterCollection::~MockFilterCollection() {}

scoped_ptr<FilterCollection> MockFilterCollection::Create() {
  scoped_ptr<FilterCollection> collection(new FilterCollection());
  collection->SetDemuxer(demuxer_);
  collection->AddVideoDecoder(video_decoder_);
  collection->AddAudioDecoder(audio_decoder_);
  collection->AddVideoRenderer(video_renderer_);
  collection->AddAudioRenderer(audio_renderer_);
  return collection.Pass();
}

void RunPipelineStatusCB(const PipelineStatusCB& status_cb) {
  status_cb.Run(PIPELINE_OK);
}

void RunPipelineStatusCB2(::testing::Unused,
                          const PipelineStatusCB& status_cb) {
  status_cb.Run(PIPELINE_OK);
}

void RunPipelineStatusCB3(::testing::Unused, const PipelineStatusCB& status_cb,
                          ::testing::Unused) {
  status_cb.Run(PIPELINE_OK);
}

void RunPipelineStatusCB4(::testing::Unused, const PipelineStatusCB& status_cb,
                          ::testing::Unused, ::testing::Unused) {
  status_cb.Run(PIPELINE_OK);
}

void RunClosure(const base::Closure& closure) {
  closure.Run();
}

MockFilter::MockFilter() {}

MockFilter::~MockFilter() {}

MockStatisticsCB::MockStatisticsCB() {}

MockStatisticsCB::~MockStatisticsCB() {}

}  // namespace media
