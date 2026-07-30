// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture_pip_utils.h"

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/public/browser/browser_thread.h"

namespace content::desktop_capture {

std::optional<DesktopMediaID::Id> GetPipWindowToExcludeFromScreenCapture(
    DesktopMediaID::Id desktop_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    return coordinator->GetPipWindowToExcludeFromScreenCapture(desktop_id);
  }

  return std::nullopt;
}

void AddPipExclusionObserver(PipScreenCaptureExclusionObserver* observer) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    coordinator->AddExclusionObserver(observer);
  }
}

void RemovePipExclusionObserver(PipScreenCaptureExclusionObserver* observer) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    coordinator->RemoveExclusionObserver(observer);
  }
}

bool IsPipExcludedFromScreenCapture() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    return coordinator->IsExcludedFromScreenCapture();
  }
  return false;
}

base::UnguessableToken RegisterDesktopMediaPickerAsCapture(
    const GlobalRenderFrameHostId& render_frame_host_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    return coordinator->RegisterMediaPickerAsCapture(render_frame_host_id);
  }
  return base::UnguessableToken::Null();
}

void UnregisterDesktopMediaPickerAsCapture(
    const base::UnguessableToken& session_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    coordinator->UnregisterMediaPickerAsCapture(session_id);
  }
}

}  // namespace content::desktop_capture
