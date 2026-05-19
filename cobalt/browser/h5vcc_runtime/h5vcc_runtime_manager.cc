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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace h5vcc_runtime {

// static
H5vccRuntimeManager* H5vccRuntimeManager::GetInstance() {
  static base::NoDestructor<H5vccRuntimeManager> instance;
  return instance.get();
}

H5vccRuntimeManager::H5vccRuntimeManager() = default;
H5vccRuntimeManager::~H5vccRuntimeManager() = default;

void H5vccRuntimeManager::RegisterFrame(content::WebContents* web_contents,
                                        H5vccRuntimeImpl* frame) {
  frames_[web_contents].insert(frame);
  if (!frame->GetRenderFrameHost().GetParent()) {
    main_frames_[web_contents] = frame;
    for (auto& observer : observers_) {
      observer.OnMainFrameRegistered(web_contents);
    }
  }
}

void H5vccRuntimeManager::UnregisterFrame(content::WebContents* web_contents,
                                          H5vccRuntimeImpl* frame) {
  frames_[web_contents].erase(frame);
  pending_reveal_frames_[web_contents].erase(frame);
  if (main_frames_[web_contents] == frame) {
    main_frames_.erase(web_contents);
  }
  if (frames_[web_contents].empty()) {
    frames_.erase(web_contents);
    pending_reveal_frames_.erase(web_contents);
  }
  CheckCompletion(web_contents);
}

void H5vccRuntimeManager::OnPageVisibilityVisible(
    content::WebContents* web_contents,
    H5vccRuntimeImpl* frame) {
  pending_reveal_frames_[web_contents].erase(frame);
  CheckCompletion(web_contents);
}

void H5vccRuntimeManager::AddObserver(H5vccRuntimeObserver* observer) {
  observers_.AddObserver(observer);
}

void H5vccRuntimeManager::RemoveObserver(H5vccRuntimeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void H5vccRuntimeManager::StartWaitingForReveal(
    content::WebContents* web_contents) {
  waiting_for_reveal_.insert(web_contents);
  auto* main_frame = main_frames_[web_contents];
  if (main_frame) {
    pending_reveal_frames_[web_contents].insert(main_frame);
  }

  // Post task to notify observers to avoid deadlock with AppEventDelegate lock.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&H5vccRuntimeManager::NotifyStartWaitingForReveal,
                     base::Unretained(this), web_contents->GetWeakPtr()));

  if (pending_reveal_frames_[web_contents].empty()) {
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
  if (waiting_for_reveal_.count(web_contents) &&
      pending_reveal_frames_[web_contents].empty()) {
    waiting_for_reveal_.erase(web_contents);

    // ONLY make this specific WebContents visible!
    web_contents->WasShown();

    for (auto& observer : observers_) {
      observer.OnAllFramesVisible(web_contents);
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

}  // namespace h5vcc_runtime
