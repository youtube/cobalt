// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"

DocumentPipNativeWidgetMac::DocumentPipNativeWidgetMac(
    views::internal::NativeWidgetDelegate* delegate)
    : NativeWidgetMac(delegate) {}

DocumentPipNativeWidgetMac::~DocumentPipNativeWidgetMac() = default;

void DocumentPipNativeWidgetMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  // Match the Picture-in-Picture branch of
  // BrowserNativeWidgetMac::PopulateCreateWindowParams so the standalone window
  // gets the same rounded corners, drop shadow, and resize affordance as the
  // Browser-backed Document PiP window, while still drawing its own title bar.
  params->window_class = remote_cocoa::mojom::WindowClass::kFrameless;
  params->style_mask = NSWindowStyleMaskFullSizeContentView |
                       NSWindowStyleMaskTitled | NSWindowStyleMaskResizable;
  params->window_title_hidden = true;
}
