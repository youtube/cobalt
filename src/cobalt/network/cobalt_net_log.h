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

#ifndef COBALT_NETWORK_COBALT_NET_LOG_H_
#define COBALT_NETWORK_COBALT_NET_LOG_H_

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"

namespace cobalt {
namespace network {

class CobaltNetLog : public ::net::NetLog {
 public:
  CobaltNetLog(const base::FilePath& log_path,
               ::net::NetLogCaptureMode capture_mode);
  ~CobaltNetLog() override;

 private:
  std::unique_ptr<net::FileNetLogObserver> net_log_logger_;

  DISALLOW_COPY_AND_ASSIGN(CobaltNetLog);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_COBALT_NET_LOG_H_
