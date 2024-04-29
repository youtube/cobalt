// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_NETWORK_CUSTOM_DATA_PROTOCOL_HANDLER_H_
#define COBALT_NETWORK_CUSTOM_DATA_PROTOCOL_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {

class URLRequestJob;

// Implements a ProtocolHandler for Data jobs.
class NET_EXPORT DataProtocolHandler
    : public URLRequestJobFactory::ProtocolHandler {
 public:
  DataProtocolHandler();
  DataProtocolHandler(const DataProtocolHandler&) = delete;
  DataProtocolHandler& operator=(const DataProtocolHandler&) = delete;

  std::unique_ptr<URLRequestJob> CreateJob(URLRequest* request) const override;

  bool IsSafeRedirectTarget(const GURL& location) const override;
};

}  // namespace net

#endif  // COBALT_NETWORK_CUSTOM_DATA_PROTOCOL_HANDLER_H_
