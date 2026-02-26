// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_tvos.h"

#import <UIKit/UIKit.h>

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/tvos/simple_begin_frame_observer.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"

namespace content {

RenderWidgetHostViewTVOS::RenderWidgetHostViewTVOS(RenderWidgetHost* widget)
    : RenderWidgetHostViewIOS(widget) {
  display_tree_ =
      std::make_unique<ui::DisplayCALayerTree>([GetNativeView().Get() layer]);
}

RenderWidgetHostViewTVOS::~RenderWidgetHostViewTVOS() = default;

void RenderWidgetHostViewTVOS::UpdateCALayerTree(
    const gfx::CALayerParams& ca_layer_params) {
  DCHECK(display_tree_);
  display_tree_->UpdateCALayerTree(ca_layer_params);
}

void RenderWidgetHostViewTVOS::ShowWithVisibility(
    PageVisibilityState page_visibility) {
  RenderWidgetHostViewIOS::ShowWithVisibility(page_visibility);

  if (page_visibility != PageVisibilityState::kVisible) {
    return;
  }

  auto* webContents =
      WebContents::FromRenderViewHost(RenderViewHostImpl::From(host()));
  if (SimpleBeginFrameObserver::FromWebContents(webContents)) {
    return;
  }

  // Create SimpleBeginFrameObserver after ui::Compositor is created.
  SimpleBeginFrameObserver::CreateForWebContents(webContents, GetCompositor());
}

}  // namespace content
