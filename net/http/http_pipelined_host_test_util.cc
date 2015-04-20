// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_test_util.h"

#include "net/base/ssl_config_service.h"
#include "net/proxy/proxy_info.h"

namespace net {

MockHostDelegate::MockHostDelegate() {
}

MockHostDelegate::~MockHostDelegate() {
}

MockPipelineFactory::MockPipelineFactory() {
}

MockPipelineFactory::~MockPipelineFactory() {
}

MockPipeline::MockPipeline(int depth, bool usable, bool active)
    : depth_(depth),
      usable_(usable),
      active_(active) {
}

MockPipeline::~MockPipeline() {
}

}  // namespace net
