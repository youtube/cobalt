// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_SCHEDULER_DELEGATE_WIN_H_
#define CHROME_BROWSER_VR_WIN_SCHEDULER_DELEGATE_WIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/scheduler_delegate.h"

namespace vr {

class SchedulerDelegateWin : public SchedulerDelegate {
 public:
  SchedulerDelegateWin();
  ~SchedulerDelegateWin() override;

  // Tell browser when poses available, when we rendered, etc.
  void OnPose(base::OnceCallback<void()> on_frame_ended,
              gfx::Transform head_pose,
              bool draw_overlay,
              bool draw_ui);

 private:
  void OnPause() override;
  void OnResume() override;

  void OnExitPresent() override;
  void SetBrowserRenderer(
      SchedulerBrowserRendererInterface* browser_renderer) override;
  void SubmitDrawnFrame(FrameType frame_type,
                        const gfx::Transform& head_pose) override;
  void AddInputSourceState(device::mojom::XRInputSourceStatePtr state) override;
  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options) override;

 private:
  raw_ptr<SchedulerBrowserRendererInterface> browser_renderer_ = nullptr;
  base::OnceCallback<void()> on_frame_ended_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_SCHEDULER_DELEGATE_WIN_H_
