// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

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

MockDecryptor::MockDecryptor() {}

MockDecryptor::~MockDecryptor() {}

void MockDecryptor::InitializeAudioDecoder(
    scoped_ptr<AudioDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  InitializeAudioDecoderMock(*config, init_cb);
}

void MockDecryptor::InitializeVideoDecoder(
    scoped_ptr<VideoDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  InitializeVideoDecoderMock(*config, init_cb);
}

MockDecryptorClient::MockDecryptorClient() {}

MockDecryptorClient::~MockDecryptorClient() {}

void MockDecryptorClient::NeedKey(const std::string& key_system,
                                  const std::string& session_id,
                                  const std::string& type,
                                  scoped_array<uint8> init_data,
                                  int init_data_length) {
  NeedKeyMock(key_system, session_id, type, init_data.get(), init_data_length);
}

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
  collection->GetVideoDecoders()->push_back(video_decoder_);
  collection->GetAudioDecoders()->push_back(audio_decoder_);
  collection->AddVideoRenderer(video_renderer_);
  collection->AddAudioRenderer(audio_renderer_);
  return collection.Pass();
}

MockStatisticsCB::MockStatisticsCB() {}

MockStatisticsCB::~MockStatisticsCB() {}

}  // namespace media
