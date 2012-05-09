// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef MEDIA_BASE_MOCK_DEMUXER_HOST_H_
#define MEDIA_BASE_MOCK_DEMUXER_HOST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDemuxerHost : public DemuxerHost {
 public:
  MockDemuxerHost();
  virtual ~MockDemuxerHost();

  // DataSourceHost implementation.
  MOCK_METHOD1(SetTotalBytes, void(int64 total_bytes));
  MOCK_METHOD1(SetBufferedBytes, void(int64 buffered_bytes));
  MOCK_METHOD1(SetNetworkActivity, void(bool network_activity));

  // DemuxerHost implementation.
  MOCK_METHOD1(OnDemuxerError, void(PipelineStatus error));
  MOCK_METHOD1(SetDuration, void(base::TimeDelta duration));
  MOCK_METHOD1(SetCurrentReadPosition, void(int64 offset));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxerHost);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_DEMUXER_HOST_H_
