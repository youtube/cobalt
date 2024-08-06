// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MOCK_DEMUXER_HOST_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MOCK_DEMUXER_HOST_H_

#include "base/macros.h"
#include "third_party/chromium/media/base/demuxer.h"
#include "third_party/chromium/media/base/text_track_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {

class MockDemuxerHost : public DemuxerHost {
 public:
  MockDemuxerHost();

  MockDemuxerHost(const MockDemuxerHost&) = delete;
  MockDemuxerHost& operator=(const MockDemuxerHost&) = delete;

  ~MockDemuxerHost() override;

  MOCK_METHOD1(OnBufferedTimeRangesChanged,
               void(const Ranges<base::TimeDelta>&));
  MOCK_METHOD1(SetDuration, void(base::TimeDelta duration));
  MOCK_METHOD1(OnDemuxerError, void(PipelineStatus error));
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MOCK_DEMUXER_HOST_H_
