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
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace h5vcc_runtime {

class H5vccRuntimeImpl;

enum class PendingAck {
  kNone,
  kReveal,
  kConceal,
  kBlur,
};

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

  // Called when a WebContents has completed conceal (hidden).
  virtual void OnAllFramesConcealed(content::WebContents* web_contents) {}

  // Called when a WebContents has completed blur.
  virtual void OnAllFramesBlurred(content::WebContents* web_contents) {}

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
                     content::RenderFrameHost* frame);

  // Called when a document is destroyed or navigated away.
  // Removes the frame from tracking.
  void UnregisterFrame(content::WebContents* web_contents,
                       content::RenderFrameHost* frame);

  // Called when the renderer signals that a frame is visible (Reveal ACK).
  void OnPageVisibilityVisible(content::WebContents* web_contents,
                               content::RenderFrameHost* frame);

  // Called when the renderer signals that a frame is hidden (Conceal ACK).
  void OnPageVisibilityHidden(content::WebContents* web_contents,
                              content::RenderFrameHost* frame);

  // Called when the renderer signals that a frame has lost focus (Blur ACK).
  void OnPageBlurred(content::WebContents* web_contents,
                     content::RenderFrameHost* frame);

  // Observers are notified when all frames of a WebContents become visible.
  // Supported observers:
  // - ShellPlatformDelegate: to unblock native window focus.
  // - AppEventDelegate: to unblock lifecycle transitions.
  void AddObserver(H5vccRuntimeObserver* observer);
  void RemoveObserver(H5vccRuntimeObserver* observer);

  // Called to start waiting for a specific ACK type.
  void StartWaitingForAck(content::WebContents* web_contents,
                          PendingAck ack_type);

 private:
  friend class base::NoDestructor<H5vccRuntimeManager>;

  H5vccRuntimeManager();
  ~H5vccRuntimeManager();

  void CheckCompletion(content::WebContents* web_contents);
  void NotifyStartWaitingForReveal(
      base::WeakPtr<content::WebContents> web_contents);
  void OnAckTimeout(base::WeakPtr<content::WebContents> web_contents,
                    PendingAck ack_type);
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  class WebContentsTracker : public content::WebContentsObserver {
   public:
    WebContentsTracker(content::WebContents* web_contents,
                       H5vccRuntimeManager* manager);
    ~WebContentsTracker() override;

    void RenderFrameDeleted(
        content::RenderFrameHost* render_frame_host) override;
    void WebContentsDestroyed() override;

   private:
    H5vccRuntimeManager* manager_;
  };

  std::map<content::WebContents*, content::RenderFrameHost*> main_frames_;
  std::map<content::WebContents*, std::set<content::RenderFrameHost*>> frames_;

  // The single set of frames we are waiting for an ACK from.
  std::map<content::WebContents*, std::set<content::RenderFrameHost*>>
      pending_ack_frames_;

  // What we are waiting for per WebContents.
  std::map<content::WebContents*, PendingAck> pending_acks_;

  std::map<content::WebContents*, std::unique_ptr<WebContentsTracker>>
      trackers_;

  base::ObserverList<H5vccRuntimeObserver>::Unchecked observers_;

  base::WeakPtrFactory<H5vccRuntimeManager> weak_factory_{this};
};

}  // namespace h5vcc_runtime

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_MANAGER_H_
