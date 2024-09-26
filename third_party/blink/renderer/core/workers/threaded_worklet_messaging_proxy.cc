// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context,
    scoped_refptr<base::SingleThreadTaskRunner> parent_agent_group_task_runner)
    : ThreadedMessagingProxyBase(execution_context,
                                 parent_agent_group_task_runner) {}

void ThreadedWorkletMessagingProxy::Initialize(
    WorkerClients* worker_clients,
    WorkletModuleResponsesMap* module_responses_map,
    const absl::optional<WorkerBackingThreadStartupData>& thread_startup_data) {
  DCHECK(IsMainThread());
  if (AskedToTerminate())
    return;

  worklet_object_proxy_ =
      CreateObjectProxy(this, GetParentExecutionContextTaskRunners(),
                        GetParentAgentGroupTaskRunner());

  // For now we don't use global scope name for threaded worklets.
  // TODO(nhiroki): Threaded worklets may want to have the global scope name to
  // distinguish multiple worklets created from the same script URL like
  // LayoutWorklet and PaintWorklet.
  const String global_scope_name = g_empty_string;

  // TODO(crbug.com/1419253): ExecutionContext can be null for a worklet that is
  // not spawned from the original renderer (e.g. shared storage worklet). This
  // is acceptable from the scope of shared storage. Longer term, it'd be good
  // to support an out-of-process worklet architecture where the
  // GlobalScopeCreationParams is reasonably filled in.
  if (!GetExecutionContext()) {
    auto creation_params = std::make_unique<GlobalScopeCreationParams>(
        /*script_url=*/KURL(),
        /*script_type=*/mojom::blink::ScriptType::kModule, global_scope_name,
        /*user_agent=*/String(),
        /*ua_metadata=*/absl::optional<UserAgentMetadata>(),
        /*web_worker_fetch_context=*/nullptr,
        /*outside_content_security_policies=*/
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        /*response_content_security_policies=*/
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        /*referrer_policy=*/network::mojom::ReferrerPolicy::kDefault,
        /*starter_origin=*/nullptr,
        /*starter_secure_context=*/false,
        /*starter_https_state=*/HttpsState::kNone,
        /*worker_clients=*/nullptr,
        /*content_settings_client=*/nullptr,
        /*inherited_trial_features=*/nullptr,
        /*parent_devtools_token=*/base::UnguessableToken::Create(),
        /*worker_settings=*/nullptr,
        /*v8_cache_options=*/mojom::blink::V8CacheOptions::kDefault,
        /*module_responses_map=*/nullptr);

    InitializeWorkerThread(std::move(creation_params), thread_startup_data,
                           absl::nullopt);
    return;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  ContentSecurityPolicy* csp = window->GetContentSecurityPolicy();
  DCHECK(csp);

  LocalFrameClient* frame_client = window->GetFrame()->Client();
  // For now we should prioritize to send full UA string if opted into both
  // Reduction and SendFullUserAgentAfterReduction Origin Trial
  const String user_agent =
      RuntimeEnabledFeatures::SendFullUserAgentAfterReductionEnabled(window)
          ? frame_client->FullUserAgent()
          : (RuntimeEnabledFeatures::UserAgentReductionEnabled(window)
                 ? frame_client->ReducedUserAgent()
                 : frame_client->UserAgent());

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          window->Url(), mojom::blink::ScriptType::kModule, global_scope_name,
          user_agent, frame_client->UserAgentMetadata(),
          frame_client->CreateWorkerFetchContext(),
          mojo::Clone(csp->GetParsedPolicies()),
          Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
          window->GetReferrerPolicy(), window->GetSecurityOrigin(),
          window->IsSecureContext(), window->GetHttpsState(), worker_clients,
          frame_client->CreateWorkerContentSettingsClient(),
          OriginTrialContext::GetInheritedTrialFeatures(window).get(),
          base::UnguessableToken::Create(),
          std::make_unique<WorkerSettings>(window->GetFrame()->GetSettings()),
          mojom::blink::V8CacheOptions::kDefault, module_responses_map,
          mojo::NullRemote() /* browser_interface_broker */,
          window->GetFrame()->Loader().CreateWorkerCodeCacheHost(),
          window->GetFrame()->GetBlobUrlStorePendingRemote(),
          BeginFrameProviderParams(), nullptr /* parent_permissions_policy */,
          window->GetAgentClusterID(), ukm::kInvalidSourceId,
          window->GetExecutionContextToken(),
          window->CrossOriginIsolatedCapability(), window->IsIsolatedContext());

  // Worklets share the pre-initialized backing thread so that we don't have to
  // specify the backing thread startup data.
  InitializeWorkerThread(std::move(global_scope_creation_params),
                         thread_startup_data, absl::nullopt);
}

void ThreadedWorkletMessagingProxy::Trace(Visitor* visitor) const {
  ThreadedMessagingProxyBase::Trace(visitor);
}

void ThreadedWorkletMessagingProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::CredentialsMode credentials_mode,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletObjectProxy::FetchAndInvokeScript,
          CrossThreadUnretained(worklet_object_proxy_.get()), module_url_record,
          credentials_mode, outside_settings_object.CopyData(),
          WrapCrossThreadPersistent(&outside_resource_timing_notifier),
          std::move(outside_settings_task_runner),
          WrapCrossThreadPersistent(pending_tasks),
          CrossThreadUnretained(GetWorkerThread())));
}

void ThreadedWorkletMessagingProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  ParentObjectDestroyed();
}

void ThreadedWorkletMessagingProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  TerminateGlobalScope();
}

std::unique_ptr<ThreadedWorkletObjectProxy>
ThreadedWorkletMessagingProxy::CreateObjectProxy(
    ThreadedWorkletMessagingProxy* messaging_proxy,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    scoped_refptr<base::SingleThreadTaskRunner>
        parent_agent_group_task_runner) {
  return ThreadedWorkletObjectProxy::Create(
      messaging_proxy, parent_execution_context_task_runners,
      std::move(parent_agent_group_task_runner));
}

ThreadedWorkletObjectProxy&
ThreadedWorkletMessagingProxy::WorkletObjectProxy() {
  DCHECK(worklet_object_proxy_);
  return *worklet_object_proxy_;
}

}  // namespace blink
