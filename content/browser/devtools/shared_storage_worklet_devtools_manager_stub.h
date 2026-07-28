// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_STUB_H_

#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"

namespace content {

class SharedStorageWorkletHost;

class SharedStorageWorkletDevToolsManager {
 public:
  static SharedStorageWorkletDevToolsManager* GetInstance() {
    return base::Singleton<SharedStorageWorkletDevToolsManager>::get();
  }

  void WorkletCreated(SharedStorageWorkletHost& worklet_host,
                      const base::UnguessableToken& devtools_worklet_token,
                      bool& wait_for_debugger) {}
  void WorkletReadyForInspection(
      SharedStorageWorkletHost& worklet_host,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver) {}
  void WorkletDestroyed(SharedStorageWorkletHost& worklet_host) {}

 private:
  friend struct base::DefaultSingletonTraits<
      SharedStorageWorkletDevToolsManager>;
  SharedStorageWorkletDevToolsManager() = default;
  ~SharedStorageWorkletDevToolsManager() = default;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_STUB_H_
