// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

class StatusBubbleViews;

namespace ui {
class LayerTreeOwner;
}

// ContentsWebView is used to present the WebContents of the active tab.
class ContentsWebView
    : public views::WebView,
      public WebContentsCloseHandlerDelegate {
 public:
  METADATA_HEADER(ContentsWebView);
  explicit ContentsWebView(content::BrowserContext* browser_context);
  ContentsWebView(const ContentsWebView&) = delete;
  ContentsWebView& operator=(const ContentsWebView&) = delete;
  ~ContentsWebView() override;

  // Sets the status bubble, which should be repositioned every time
  // this view changes visible bounds.
  void SetStatusBubble(StatusBubbleViews* status_bubble);
  StatusBubbleViews* GetStatusBubble() const;

  // Toggles whether the background is visible.
  void SetBackgroundVisible(bool background_visible);

  // WebView overrides:
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnThemeChanged() override;
  void RenderViewReady() override;
  void OnLetterboxingChanged() override;

  // ui::View overrides:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  // WebContentsCloseHandlerDelegate overrides:
  void CloneWebContentsLayer() override;
  void DestroyClonedLayer() override;

 private:
  void UpdateBackgroundColor();
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION StatusBubbleViews* status_bubble_;

  bool background_visible_ = true;

  std::unique_ptr<ui::LayerTreeOwner> cloned_layer_tree_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
