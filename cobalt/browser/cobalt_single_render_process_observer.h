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

#ifndef COBALT_BROWSER_COBALT_SINGLE_RENDER_PROCESS_OBSERVER_H_
#define COBALT_BROWSER_COBALT_SINGLE_RENDER_PROCESS_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/content_constants.h"

namespace content {
class RenderProcessHost;
class RenderProcessHostObserver;
}  // namespace content

namespace cobalt {

// This class keeps track of a Renderer ID during its lifetime. It must
// be used on the UI thread, and can observe one such Renderer.
class CobaltSingleRenderProcessObserver
    : public content::RenderProcessHostObserver {
 public:
  CobaltSingleRenderProcessObserver();

  CobaltSingleRenderProcessObserver(const CobaltSingleRenderProcessObserver&) =
      delete;
  CobaltSingleRenderProcessObserver& operator=(
      const CobaltSingleRenderProcessObserver&) = delete;

  ~CobaltSingleRenderProcessObserver() override;

  void UpdateRenderProcessHost(content::RenderProcessHost* host);
  int renderer_id() const { return renderer_id_; }

  // content::RenderProcessHostObserver implementation
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void InProcessRendererExiting(content::RenderProcessHost* host) override;

 private:
  int renderer_id_ = content::kInvalidChildProcessUniqueId;
  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      process_observation_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_SINGLE_RENDER_PROCESS_OBSERVER_H_
