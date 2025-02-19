// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/cobalt_single_render_process_observer.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

namespace cobalt {

CobaltSingleRenderProcessObserver::CobaltSingleRenderProcessObserver() =
    default;
CobaltSingleRenderProcessObserver::~CobaltSingleRenderProcessObserver() =
    default;

void CobaltSingleRenderProcessObserver::UpdateRenderProcessHost(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(renderer_id_, content::kInvalidChildProcessUniqueId)
      << "Cobalt should only have one renderer.";
  renderer_id_ = host->GetID();
  process_observation_.Reset();
  if (auto* rph = content::RenderProcessHost::FromID(renderer_id_)) {
    process_observation_.Observe(rph);
  }
}

void CobaltSingleRenderProcessObserver::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTIMPLEMENTED()
      << "CobaltSingleRenderProcessObserver only supports single process.";
}

void CobaltSingleRenderProcessObserver::InProcessRendererExiting(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(host->GetID(), renderer_id_)
      << "Cobalt should only have one renderer.";
  process_observation_.Reset();
}

}  // namespace cobalt
