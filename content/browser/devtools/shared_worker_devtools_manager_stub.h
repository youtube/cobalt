// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_STUB_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"

namespace content {

class SharedWorkerDevToolsAgentHost;
class SharedWorkerHost;

class SharedWorkerDevToolsManager {
 public:
  static SharedWorkerDevToolsManager* GetInstance() {
    return base::Singleton<SharedWorkerDevToolsManager>::get();
  }

  void AddAllAgentHosts(
      std::vector<scoped_refptr<SharedWorkerDevToolsAgentHost>>* result) {}
  void AgentHostDestroyed(SharedWorkerDevToolsAgentHost* agent_host) {}

  void WorkerCreated(SharedWorkerHost* worker_host,
                     bool* pause_on_start,
                     base::UnguessableToken* devtools_worker_token) {}
  void WorkerReadyForInspection(
      SharedWorkerHost* worker_host,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver) {}
  void WorkerDestroyed(SharedWorkerHost* worker_host) {}
  SharedWorkerDevToolsAgentHost* GetDevToolsHost(SharedWorkerHost* host) {
    return nullptr;
  }

 private:
  friend struct base::DefaultSingletonTraits<SharedWorkerDevToolsManager>;
  SharedWorkerDevToolsManager() = default;
  ~SharedWorkerDevToolsManager() = default;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_STUB_H_
