// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "build/build_config.h"
#include "third_party/blink/public/common/buildflags.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

#include "content/browser/devtools/worker_or_worklet_devtools_agent_host.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace content {

namespace protocol {
class TargetAutoAttacher;
}  // namespace protocol

class DedicatedWorkerHost;

class DedicatedWorkerDevToolsAgentHost final
    : public WorkerOrWorkletDevToolsAgentHost {
 public:
  static DedicatedWorkerDevToolsAgentHost* GetFor(
      const DedicatedWorkerHost* host);

  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  DedicatedWorkerDevToolsAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback);

  void DisconnectIfNotCreated();

  // A DedicatedWorkerDevToolsAgentHost is created before it is known if a
  // worker thread will be created in the renderer. This method is used to
  // indicate the the worker thread is actually created and this agent host is
  // associated with a renderer agent.
  void ChildWorkerCreated(
      const GURL& url,
      const std::string& name,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> callback);

 private:
  ~DedicatedWorkerDevToolsAgentHost() override;

  // DevToolsAgentHost overrides
  std::string GetType() override;

  // DevToolsAgentHostImpl overrides
  bool AttachSession(DevToolsSession* session) override;
  protocol::TargetAutoAttacher* auto_attacher() override;
  std::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

  DedicatedWorkerHost* GetDedicatedWorkerHost();

  std::unique_ptr<protocol::TargetAutoAttacher> const auto_attacher_;
  bool child_worker_created_ = false;
};

}  // namespace content

#else  // BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class DedicatedWorkerHost;

class DedicatedWorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  static DedicatedWorkerDevToolsAgentHost* GetFor(
      const DedicatedWorkerHost* host);

  const base::UnguessableToken& devtools_worker_token() const {
    static const base::UnguessableToken token;
    return token;
  }
};

}  // namespace content

#endif  // BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)
#endif  // CONTENT_BROWSER_DEVTOOLS_DEDICATED_WORKER_DEVTOOLS_AGENT_HOST_H_
