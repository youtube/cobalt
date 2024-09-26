// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Size;
struct PresentationFeedback;
}  // namespace gfx

namespace ui {

class Compositor;

// A compositor observer is notified when compositing completes.
class COMPOSITOR_EXPORT CompositorObserver {
 public:
  virtual ~CompositorObserver() = default;

  // A commit proxies information from the main thread to the compositor
  // thread. It typically happens when some state changes that will require a
  // composite. In the multi-threaded case, many commits may happen between
  // two successive composites. In the single-threaded, a single commit
  // between two composites (just before the composite as part of the
  // composite cycle). If the compositor is locked, it will not send this
  // this signal.
  virtual void OnCompositingDidCommit(Compositor* compositor) {}

  // Called when compositing started: it has taken all the layer changes into
  // account and has issued the graphics commands.
  virtual void OnCompositingStarted(Compositor* compositor,
                                    base::TimeTicks start_time) {}

  // Called when compositing completes: the present to screen has completed.
  virtual void OnCompositingEnded(Compositor* compositor) {}

  // Called when a child of the compositor is resizing.
  virtual void OnCompositingChildResizing(Compositor* compositor) {}

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when a swap with new size is completed.
  virtual void OnCompositingCompleteSwapWithNewSize(ui::Compositor* compositor,
                                                    const gfx::Size& size) {}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // Called at the top of the compositor's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnCompositingShuttingDown(Compositor* compositor) {}

  // Called when the presentation feedback was received from the viz.
  virtual void OnDidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) {}

  // Called when first AnimationObserver was added to the compositor.
  virtual void OnFirstAnimationStarted(Compositor* compositor) {}

  // Called on first BeginMainFrame after the last animation has finished.
  // This presents "animations finished" event from user point of view.
  // When animations are temporary stopped and restarted in between painting
  // two frames technically animations have stopped, but users will never
  // notice it because animations are immediately restarted. This way we delay
  // "Last Animation Ended" notification to the BeginMainFrame stage so that it
  // only fires if there was a frame painted without animations.
  // See go/report-ux-metrics-at-painting for details.
  virtual void OnFirstNonAnimatedFrameStarted(Compositor* compositor) {}

  virtual void OnFrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) {}

  // Called at the end of the BeginMainFrame.
  virtual void OnDidBeginMainFrame(Compositor* compositor) {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
