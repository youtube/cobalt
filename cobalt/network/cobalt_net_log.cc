// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/cobalt_net_log.h"

#include <algorithm>

#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"

namespace cobalt {
namespace network {

CobaltNetLog::CobaltNetLog(const base::FilePath& log_path,
                           net::NetLogCaptureMode capture_mode)
    : capture_mode_(capture_mode)
#if !defined(COBALT_BUILD_TYPE_GOLD)
      ,
      net_log_logger_(
          net::FileNetLogObserver::CreateUnbounded(log_path, nullptr)) {
  LOG(INFO) << "YO THOR COBALT NETLOG OBSERVEING:" << log_path.value();
#else   // !defined(COBALT_BUILD_TYPE_GOLD)
{
  LOG(INFO) << "YO THOR COBALT NETLOG ***NOT***    OBSERVEING";
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}

CobaltNetLog::~CobaltNetLog() {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  // Remove the observers we own before we're destroyed.
  StopObserving();
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}

void CobaltNetLog::StartObserving() {
  if (!is_observing_) {
    is_observing_ = true;
    net_log_logger_->StartObserving(this, capture_mode_);
  } else {
    DLOG(WARNING) << "Already observing NetLog.";
  }
}

void CobaltNetLog::StopObserving() {
  if (is_observing_) {
    is_observing_ = false;
    net_log_logger_->StopObserving(nullptr, base::OnceClosure());
  }
}

}  // namespace network
}  // namespace cobalt
