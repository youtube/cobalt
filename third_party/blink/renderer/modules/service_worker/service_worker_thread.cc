/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

ServiceWorkerThread::ServiceWorkerThread(
    std::unique_ptr<ServiceWorkerGlobalScopeProxy> global_scope_proxy,
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager,
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote,
    scoped_refptr<base::SingleThreadTaskRunner>
        parent_thread_default_task_runner,
    const blink::ServiceWorkerToken& service_worker_token)
    : WorkerThread(*global_scope_proxy,
                   std::move(parent_thread_default_task_runner)),
      global_scope_proxy_(std::move(global_scope_proxy)),
      worker_backing_thread_(std::make_unique<WorkerBackingThread>(
          ThreadCreationParams(GetThreadType()))),
      installed_scripts_manager_(std::move(installed_scripts_manager)),
      cache_storage_remote_(std::move(cache_storage_remote)),
      service_worker_token_(service_worker_token) {}

ServiceWorkerThread::~ServiceWorkerThread() {
  global_scope_proxy_->Detach();
}

void ServiceWorkerThread::ClearWorkerBackingThread() {
  worker_backing_thread_ = nullptr;
}

void ServiceWorkerThread::TerminateForTesting() {
  global_scope_proxy_->TerminateWorkerContext();
  WorkerThread::TerminateForTesting();
}

WorkerOrWorkletGlobalScope* ServiceWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  return ServiceWorkerGlobalScope::Create(
      this, std::move(creation_params), std::move(installed_scripts_manager_),
      std::move(cache_storage_remote_), time_origin_, service_worker_token_);
}

}  // namespace blink
