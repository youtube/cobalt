// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_H_

#include "build/build_config.h"
#include "third_party/blink/public/common/buildflags.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

#include <map>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "content/public/browser/devtools_agent_host.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class SharedStorageWorkletDevToolsAgentHost;
class SharedStorageWorkletHost;
class RenderFrameHostImpl;

// Manages `SharedStorageWorkletDevToolsAgentHost`s for Shared Storage Worklets.
class SharedStorageWorkletDevToolsManager {
 public:
  class Observer {
   public:
    virtual void SharedStorageWorkletCreated(
        SharedStorageWorkletDevToolsAgentHost* host,
        bool& should_pause_on_start) {}

    virtual void SharedStorageWorkletDestroyed(
        SharedStorageWorkletDevToolsAgentHost* host) {}

    virtual ~Observer() = default;
  };

  // Returns the SharedStorageWorkletDevToolsManager singleton.
  static SharedStorageWorkletDevToolsManager* GetInstance();

  SharedStorageWorkletDevToolsManager(
      const SharedStorageWorkletDevToolsManager&) = delete;
  SharedStorageWorkletDevToolsManager& operator=(
      const SharedStorageWorkletDevToolsManager&) = delete;

  void AddAllAgentHosts(std::vector<scoped_refptr<DevToolsAgentHost>>* result);

  void WorkletCreated(SharedStorageWorkletHost& worklet_host,
                      const base::UnguessableToken& devtools_worklet_token,
                      bool& wait_for_debugger);
  void WorkletReadyForInspection(
      SharedStorageWorkletHost& worklet_host,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver);
  void WorkletDestroyed(SharedStorageWorkletHost& worklet_host);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void GetAllForFrame(RenderFrameHostImpl* frame, DevToolsAgentHost::List* out);

 private:
  friend struct base::DefaultSingletonTraits<
      SharedStorageWorkletDevToolsManager>;

  SharedStorageWorkletDevToolsManager();
  ~SharedStorageWorkletDevToolsManager();

  base::ObserverList<Observer>::Unchecked observer_list_;

  // We retatin agent hosts as long as the shared storage worklet is alive.
  std::map<SharedStorageWorkletHost*,
           scoped_refptr<SharedStorageWorkletDevToolsAgentHost>>
      hosts_;
};

}  // namespace content

#else  // BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

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

#endif  // BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_STORAGE_WORKLET_DEVTOOLS_MANAGER_H_
