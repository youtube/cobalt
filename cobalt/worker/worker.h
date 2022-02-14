// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_WORKER_H_
#define COBALT_WORKER_WORKER_H_

#include <memory>
#include <string>

#include "cobalt/script/environment_settings.h"
#include "cobalt/worker/message_port.h"
#include "cobalt/worker/worker_options.h"

namespace cobalt {
namespace worker {

class Worker {
 public:
  Worker();

  void RunDedicated(const std::string& url,
                    script::EnvironmentSettings* settings,
                    MessagePort* outside_port, const WorkerOptions options) {
    Run(true, url, settings, outside_port, options);
  }

  void Run(bool is_shared, const std::string& url,
           script::EnvironmentSettings* settings, MessagePort* outside_port,
           const WorkerOptions& options);
  void Terminate();

  MessagePort* inside_port() const { return inside_port_.get(); }

 private:
  void Obtain();
  void RunLoop();
  void Finalize();

  bool is_shared_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Worker);

  std::unique_ptr<MessagePort> inside_port_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_H_
