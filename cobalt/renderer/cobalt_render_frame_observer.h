// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_RENDERER_COBALT_RENDERER_FRAME_OBSERVER_H_
#define COBALT_RENDERER_COBALT_RENDERER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "v8/include/v8.h"

namespace cobalt {

// This class allows Cobalt to install custom Javascript implementations.
class CobaltRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit CobaltRenderFrameObserver(content::RenderFrame* render_frame);

  CobaltRenderFrameObserver(const CobaltRenderFrameObserver&) = delete;
  CobaltRenderFrameObserver& operator=(const CobaltRenderFrameObserver&) =
      delete;

 private:
  friend class CobaltRenderFrameObserverTest;

  // content::RenderFrameObserver implementation.
  void DidCreateScriptContext(v8::Local<v8::Context>, int32_t) override;
  void OnDestruct() override;

  static void DoWindowCloseOrMinimize(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  void InstallCloseAndMinimize();
  void set_user_on_exit_strategy_callback_for_testing(
      base::RepeatingClosure closure) {
    user_on_exit_strategy_callback_ = closure;
  }

  base::RepeatingClosure user_on_exit_strategy_callback_;
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_COBALT_RENDERER_FRAME_OBSERVER_H_
