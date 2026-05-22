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

#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_manager.h"

#include "base/no_destructor.h"
#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace h5vcc_runtime {

// static
H5vccRuntimeManager* H5vccRuntimeManager::GetInstance() {
  static base::NoDestructor<H5vccRuntimeManager> instance;
  return instance.get();
}

H5vccRuntimeManager::H5vccRuntimeManager() = default;
H5vccRuntimeManager::~H5vccRuntimeManager() = default;

H5vccRuntimeManager::WebContentsTracker::WebContentsTracker(
    content::WebContents* web_contents,
    H5vccRuntimeManager* manager)
    : content::WebContentsObserver(web_contents), manager_(manager) {}

H5vccRuntimeManager::WebContentsTracker::~WebContentsTracker() = default;

void H5vccRuntimeManager::WebContentsTracker::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  manager_->UnregisterFrame(web_contents(), render_frame_host);
}

void H5vccRuntimeManager::WebContentsTracker::WebContentsDestroyed() {
  manager_->OnWebContentsDestroyed(web_contents());
}

void H5vccRuntimeManager::RegisterFrame(content::WebContents* web_contents,
                                        content::RenderFrameHost* frame) {
  frames_[web_contents].insert(frame);

  // Create tracker if it doesn't exist.
  if (trackers_.find(web_contents) == trackers_.end()) {
    trackers_[web_contents] =
        std::make_unique<WebContentsTracker>(web_contents, this);
  }

  // Add to pending set if we are waiting for an ACK.
  PendingAck pending_ack = pending_acks_[web_contents];
  if (pending_ack != PendingAck::kNone) {
    pending_ack_frames_[web_contents].insert(frame);
  }

  if (!frame->GetParent()) {
    main_frames_[web_contents] = frame;
    for (auto& observer : observers_) {
      observer.OnMainFrameRegistered(web_contents);
    }
  }
}

void H5vccRuntimeManager::UnregisterFrame(content::WebContents* web_contents,
                                          content::RenderFrameHost* frame) {
  frames_[web_contents].erase(frame);
  pending_ack_frames_[web_contents].erase(frame);

  if (main_frames_[web_contents] == frame) {
    main_frames_.erase(web_contents);
  }

  if (frames_[web_contents].empty()) {
    frames_.erase(web_contents);
    pending_ack_frames_.erase(web_contents);
    pending_acks_.erase(web_contents);
    // Do NOT erase tracker here, as this might be called from the tracker
    // itself!
  }
  CheckCompletion(web_contents);
}

void H5vccRuntimeManager::OnPageVisibilityVisible(
    content::WebContents* web_contents,
    content::RenderFrameHost* frame) {
  pending_ack_frames_[web_contents].erase(frame);
  CheckCompletion(web_contents);
}

void H5vccRuntimeManager::OnPageVisibilityHidden(
    content::WebContents* web_contents,
    content::RenderFrameHost* frame) {
  pending_ack_frames_[web_contents].erase(frame);
  CheckCompletion(web_contents);
}

void H5vccRuntimeManager::OnPageBlurred(content::WebContents* web_contents,
                                        content::RenderFrameHost* frame) {
  pending_ack_frames_[web_contents].erase(frame);
  CheckCompletion(web_contents);
}

void H5vccRuntimeManager::AddObserver(H5vccRuntimeObserver* observer) {
  observers_.AddObserver(observer);
}

void H5vccRuntimeManager::RemoveObserver(H5vccRuntimeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void H5vccRuntimeManager::StartWaitingForAck(content::WebContents* web_contents,
                                             PendingAck ack_type) {
  pending_acks_[web_contents] = ack_type;

  if (ack_type == PendingAck::kReveal) {
    auto* main_frame = main_frames_[web_contents];
    if (main_frame) {
      pending_ack_frames_[web_contents].insert(main_frame);
    }
    // Post task to notify observers to avoid deadlock with AppEventDelegate
    // lock.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&H5vccRuntimeManager::NotifyStartWaitingForReveal,
                       base::Unretained(this), web_contents->GetWeakPtr()));
  } else {
    pending_ack_frames_[web_contents] = frames_[web_contents];
  }

  // Post a 2-second timer as a fallback for all ACKs.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&H5vccRuntimeManager::OnAckTimeout, base::Unretained(this),
                     web_contents->GetWeakPtr(), ack_type),
      base::Seconds(2));

  if (pending_ack_frames_[web_contents].empty()) {
    CheckCompletion(web_contents);
  }
}

// CheckCompletion is called whenever the set of pending frames for a specific
// WebContents might have become empty.
// If we are waiting for reveal on this WebContents and all its registered
// frames have reported visible, it completes the reveal sequence for THIS
// WebContents:
// 1. Clearing the waiting flag in the platform delegate.
// 2. Making the specific WebContents visible to trigger visibility events in
// JS.
// 3. Notifying observers to unblock focus for this WebContents.
// This enforces a strict ordering: Visibility -> Focus per WebContents.
void H5vccRuntimeManager::CheckCompletion(content::WebContents* web_contents) {
  PendingAck pending_ack = pending_acks_[web_contents];
  if (pending_ack == PendingAck::kNone) {
    return;
  }

  if (pending_ack_frames_[web_contents].empty()) {
    pending_acks_[web_contents] = PendingAck::kNone;
    pending_ack_frames_.erase(web_contents);

    switch (pending_ack) {
      case PendingAck::kReveal:
        web_contents->WasShown();
        for (auto& observer : observers_) {
          observer.OnAllFramesVisible(web_contents);
        }
        break;
      case PendingAck::kConceal:
        for (auto& observer : observers_) {
          observer.OnAllFramesConcealed(web_contents);
        }
        break;
      case PendingAck::kBlur:
        for (auto& observer : observers_) {
          observer.OnAllFramesBlurred(web_contents);
        }
        break;
      default:
        break;
    }
  }
}

void H5vccRuntimeManager::NotifyStartWaitingForReveal(
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnStartWaitingForReveal(web_contents.get());
  }
}

void H5vccRuntimeManager::OnAckTimeout(
    base::WeakPtr<content::WebContents> web_contents,
    PendingAck ack_type) {
  if (!web_contents) {
    return;
  }
  content::WebContents* wc = web_contents.get();
  if (pending_acks_[wc] == ack_type) {
    LOG(WARNING) << "Timeout fired for ack = " << static_cast<int>(ack_type)
                 << " for wc = " << wc
                 << ". This should NEVER happen! Forcing completion.";
    pending_ack_frames_[wc].clear();
    CheckCompletion(wc);
  }
}

void H5vccRuntimeManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  trackers_.erase(web_contents);
}

}  // namespace h5vcc_runtime
