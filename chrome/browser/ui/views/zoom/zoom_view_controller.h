// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ZOOM_ZOOM_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_ZOOM_ZOOM_VIEW_CONTROLLER_H_

#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}

namespace zoom {

// This class updates the corresponding page action to reflect the current zoom
// level in a given tab. In addition, it helps to show the ZoomBubbleView base
// on the current state.
class ZoomViewController {
 public:
  explicit ZoomViewController(tabs::TabInterface& tab_interface);

  ZoomViewController(const ZoomViewController&) = delete;
  ZoomViewController& operator=(const ZoomViewController&) = delete;

  ~ZoomViewController();

  // This is a wrapper that invokes `UpdatePageActionIcon` and
  // `UpdateBubbleVisibility` in the correct order.
  void UpdatePageActionIconAndBubbleVisibility(bool prefer_to_show_bubble,
                                               bool from_user_gesture);

  // Updates the page action icon, tooltip, and visibility based on
  // the current zoom state (below/above default). Does NOT show/hide the
  // bubble.
  void UpdatePageActionIcon();

  // Shows or hides the bubble depending on whether `prefer_to_show_bubble` is
  // true and whether we are at default zoom, etc. When showing the bubble,
  // `from_user_gesture` indicates if a direct user action triggered this.
  void UpdateBubbleVisibility(bool prefer_to_show_bubble,
                              bool from_user_gesture);

 private:
  bool IsBubbleVisible() const;

  // Determines if the page action should be visible under the current zoom
  // conditions, factoring in whether the bubble is already visible or allowed.
  bool CanBubbleBeVisible(bool prefer_to_show_bubble,
                          bool is_zoom_at_default) const;

  // Helper to retrieve the active WebContents from the tab.
  content::WebContents* GetWebContents() const;

  // Because zoom settings are per-tab, we store the tab interface by reference.
  // The TabInterface is guaranteed valid for this object’s lifetime.
  const raw_ref<tabs::TabInterface> tab_interface_;
};

}  // namespace zoom

#endif  // CHROME_BROWSER_UI_VIEWS_ZOOM_ZOOM_VIEW_CONTROLLER_H_
