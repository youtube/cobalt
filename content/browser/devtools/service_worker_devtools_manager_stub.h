// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_STUB_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "services/network/public/mojom/document_isolation_policy.mojom-forward.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
namespace mojom {
class URLResponseHead;
}
}  // namespace network

#include "content/browser/devtools/devtools_throttle_handle.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;
class ServiceWorkerDevToolsAgentHost;

class CONTENT_EXPORT ServiceWorkerDevToolsManager {
 public:
  class Observer {
   public:
    virtual void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                               bool* should_pause_on_start) {}
    virtual void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {}

   protected:
    virtual ~Observer() {}
  };

  static ServiceWorkerDevToolsManager* GetInstance() {
    static ServiceWorkerDevToolsManager instance;
    return &instance;
  }

  ServiceWorkerDevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id) {
    return nullptr;
  }
  ServiceWorkerDevToolsAgentHost* GetDevToolsAgentHostForNewInstallingWorker(
      const ServiceWorkerContextWrapper* context_wrapper,
      int64_t version_id) {
    return nullptr;
  }

  void AddAllAgentHosts(
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result) {}
  void AddAllAgentHostsForBrowserContext(
      BrowserContext* browser_context,
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result) {}

  void WorkerMainScriptFetchingStarting(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id,
      const GURL& url,
      const GURL& scope,
      const GlobalRenderFrameHostId& requesting_frame_id,
      scoped_refptr<DevToolsThrottleHandle> throttle_handle) {}

  void WorkerMainScriptFetchingFailed(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id) {}

  void WorkerStarting(
      int worker_process_id,
      int worker_route_id,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id,
      const GURL& url,
      const GURL& scope,
      bool is_installed_version,
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter>
          dip_reporter,
      base::UnguessableToken* devtools_worker_token,
      bool* pause_on_start) {}
  void WorkerReadyForInspection(
      int worker_process_id,
      int worker_route_id,
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {}

  void WorkerVersionInstalled(int worker_process_id, int worker_route_id) {}
  void WorkerVersionDoomed(
      int worker_process_id,
      int worker_route_id,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id) {}
  void WorkerStopped(int worker_process_id, int worker_route_id) {}
  void WorkerDestroyed(int worker_process_id,
                       int worker_route_id,
                       const blink::DedicatedWorkerToken& worker_token) {}

  void NavigationPreloadRequestSent(int worker_process_id,
                                    int worker_route_id,
                                    const std::string& request_id,
                                    const network::ResourceRequest& request) {}
  void NavigationPreloadResponseReceived(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const GURL& url,
      const network::mojom::URLResponseHead& head) {}
  void NavigationPreloadCompleted(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status) {}

  void AddObserver(Observer* observer) {}
  void RemoveObserver(Observer* observer) {}

  void set_debug_service_worker_on_start(bool debug_on_start) {}
  bool debug_service_worker_on_start() const { return false; }
  void AgentHostDestroyed(ServiceWorkerDevToolsAgentHost* agent_host) {}

 private:
  ServiceWorkerDevToolsManager() = default;
  ~ServiceWorkerDevToolsManager() = default;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_STUB_H_
