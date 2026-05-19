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

#ifndef COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_MANAGER_H_
#define COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace content {
class WebContents;
}

namespace h5vcc_runtime {

class H5vccRuntimeImpl;

class H5vccRuntimeObserver {
 public:
  // Called when all frames of a specific WebContents have completed reveal.
  virtual void OnAllFramesVisible(content::WebContents* web_contents) = 0;

  // Called when the main frame of a WebContents is registered.
  virtual void OnMainFrameRegistered(content::WebContents* web_contents) {}

  // Called when a WebContents starts waiting for reveal. This is used by
  // observers (like AppEventDelegate) to know that they should expect
  // a corresponding OnAllFramesVisible call later, and thus should defer
  // focus if it arrives too early.
  virtual void OnStartWaitingForReveal(content::WebContents* web_contents) {}

 protected:
  virtual ~H5vccRuntimeObserver() = default;
};

// H5vccRuntimeManager coordinates the Reveal ACK process across multiple
// frames. It is used to ensure that focus is not granted to the application
// until all registered frames have completed layout and are visible after a
// reveal transition. This prevents "early focus" bugs where focus events are
// processed before the page is ready.
//
// It takes WebContents* to scope the tracking per window/tab. This ensures that
// a background tab failing to become visible does not block the reveal and
// focus of the active foreground tab.
//
// A `WebContents` typically represents a "tab" or window. Each tab can have
// multiple `H5vccRuntimeImpl` instances (e.g., one for the main frame and one
// for each iframe). Since `H5vccRuntimeImpl` is a `DocumentService` bound to a
// frame's lifetime, the manager is needed to coordinate across all these
// isolated contexts within the same tab.
//
// It is a global singleton to avoid layering violations between cobalt/app
// and cobalt/shell.
class H5vccRuntimeManager {
 public:
  // Returns the singleton instance.
  // This is a global singleton rather than being owned by content::Shell
  // to avoid a layering violation: AppEventDelegate (in cobalt/app) needs
  // to observe reveal ACKs, but it cannot depend on content::Shell
  // (in cobalt/shell).
  static H5vccRuntimeManager* GetInstance();

  H5vccRuntimeManager(const H5vccRuntimeManager&) = delete;
  H5vccRuntimeManager& operator=(const H5vccRuntimeManager&) = delete;

  // Called when a new document is created and H5vccRuntimeImpl is bound to it.
  // Registers the frame with the manager to track its visibility.
  void RegisterFrame(content::WebContents* web_contents,
                     H5vccRuntimeImpl* frame);

  // Called when a document is destroyed or navigated away.
  // Removes the frame from tracking.
  void UnregisterFrame(content::WebContents* web_contents,
                       H5vccRuntimeImpl* frame);

  // Called when the renderer signals that a frame is visible (Reveal ACK).
  // This can come from JS calling H5vccRuntime.onAllFramesVisible() or
  // automatically from Blink on resume.
  void OnPageVisibilityVisible(content::WebContents* web_contents,
                               H5vccRuntimeImpl* frame);

  // Observers are notified when all frames of a WebContents become visible.
  // Supported observers:
  // - ShellPlatformDelegate: to unblock native window focus.
  // - AppEventDelegate: to unblock lifecycle transitions.
  void AddObserver(H5vccRuntimeObserver* observer);
  void RemoveObserver(H5vccRuntimeObserver* observer);

  // Called when the application is revealed (resumed or started).
  // Puts the manager in a waiting state for the specified WebContents,
  // expecting Reveal ACKs from all registered frames.
  void StartWaitingForReveal(content::WebContents* web_contents);

 private:
  friend class base::NoDestructor<H5vccRuntimeManager>;

  H5vccRuntimeManager();
  ~H5vccRuntimeManager();

  void CheckCompletion(content::WebContents* web_contents);
  void NotifyStartWaitingForReveal(
      base::WeakPtr<content::WebContents> web_contents);

  std::map<content::WebContents*, H5vccRuntimeImpl*> main_frames_;
  std::map<content::WebContents*, std::set<H5vccRuntimeImpl*>> frames_;
  std::map<content::WebContents*, std::set<H5vccRuntimeImpl*>>
      pending_reveal_frames_;
  base::ObserverList<H5vccRuntimeObserver>::Unchecked observers_;

  // Track waiting state per WebContents.
  std::set<content::WebContents*> waiting_for_reveal_;

  base::WeakPtrFactory<H5vccRuntimeManager> weak_factory_{this};
};

}  // namespace h5vcc_runtime

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_MANAGER_H_
