// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_NATIVE_WIDGET_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_NATIVE_WIDGET_MAC_H_

#include "ui/views/widget/native_widget_mac.h"

namespace views::internal {
class NativeWidgetDelegate;
}  // namespace views::internal

// This NativeWidget gives the standalone Document Picture-in-Picture widget the
// default macOS window styling (rounded corners + drop shadow) even though it
// removes the standard frame and draws its own title bar. Without it, the
// `remove_standard_frame` widget is a borderless NSWindow with square corners
// and no shadow, which does not match the Browser-backed Document PiP window.
// Mirrors the Picture-in-Picture branch of
// BrowserNativeWidgetMac::PopulateCreateWindowParams.
class DocumentPipNativeWidgetMac : public views::NativeWidgetMac {
 public:
  explicit DocumentPipNativeWidgetMac(
      views::internal::NativeWidgetDelegate* delegate);
  DocumentPipNativeWidgetMac(const DocumentPipNativeWidgetMac&) = delete;
  DocumentPipNativeWidgetMac& operator=(const DocumentPipNativeWidgetMac&) =
      delete;

  ~DocumentPipNativeWidgetMac() override;

 protected:
  // views::NativeWidgetMac:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_NATIVE_WIDGET_MAC_H_
