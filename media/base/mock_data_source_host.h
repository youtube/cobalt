// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef MEDIA_BASE_MOCK_DATA_SOURCE_HOST_H_
#define MEDIA_BASE_MOCK_DATA_SOURCE_HOST_H_

#include <string>

#include "media/base/data_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDataSourceHost : public DataSourceHost {
 public:
  MockDataSourceHost();
  virtual ~MockDataSourceHost();

  // DataSourceHost implementation.
  MOCK_METHOD1(SetTotalBytes, void(int64 total_bytes));
  MOCK_METHOD1(SetBufferedBytes, void(int64 buffered_bytes));
  MOCK_METHOD1(SetNetworkActivity, void(bool network_activity));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDataSourceHost);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_DATA_SOURCE_HOST_H_
