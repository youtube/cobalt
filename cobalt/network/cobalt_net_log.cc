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
#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"

namespace cobalt {
namespace network {

CobaltNetLog::CobaltNetLog(const base::FilePath& log_path,
                           net::NetLogCaptureMode capture_mode)
    : file_net_log_observer_(net::FileNetLogObserver::CreateUnbounded(
          log_path, capture_mode, nullptr)) {
  DCHECK(file_net_log_observer_);
}

CobaltNetLog::~CobaltNetLog() {
  // Remove the observers we own before we're destroyed.
  StopObserving();
}

void CobaltNetLog::StartObserving() {
  if (!is_observing_) {
    is_observing_ = true;
    file_net_log_observer_->StartObserving(net::NetLog::Get());
  } else {
    DLOG(WARNING) << "Already observing NetLog.";
  }
}

void CobaltNetLog::StopObserving() {
  if (is_observing_) {
    is_observing_ = false;
    file_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
  }
}

}  // namespace network
}  // namespace cobalt
