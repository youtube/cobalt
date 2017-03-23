// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_MOCK_WEBURLLOADER_H_
#define COBALT_MEDIA_BLINK_MOCK_WEBURLLOADER_H_

#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"

namespace cobalt {
namespace media {

class MockWebURLLoader : public blink::WebURLLoader {
 public:
  MockWebURLLoader();
  virtual ~MockWebURLLoader();

  MOCK_METHOD5(loadSynchronously,
               void(const blink::WebURLRequest& request,
                    blink::WebURLResponse& response, blink::WebURLError& error,
                    blink::WebData& data, int64_t& encoded_data_length));
  MOCK_METHOD2(loadAsynchronously, void(const blink::WebURLRequest& request,
                                        blink::WebURLLoaderClient* client));
  MOCK_METHOD0(cancel, void());
  MOCK_METHOD1(setDefersLoading, void(bool value));
  MOCK_METHOD1(setLoadingTaskRunner, void(blink::WebTaskRunner* task_runner));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebURLLoader);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BLINK_MOCK_WEBURLLOADER_H_
