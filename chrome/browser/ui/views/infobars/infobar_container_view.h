// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_targeter_delegate.h"

// The views-specific implementation of InfoBarContainer.
class InfoBarContainerView : public views::AccessiblePaneView,
                             public infobars::InfoBarContainer {
 public:
  METADATA_HEADER(InfoBarContainerView);
  explicit InfoBarContainerView(Delegate* delegate);
  InfoBarContainerView(const InfoBarContainerView&) = delete;
  InfoBarContainerView& operator=(const InfoBarContainerView&) = delete;
  ~InfoBarContainerView() override;

  // views::AccessiblePaneView:
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;

  // InfobarContainer:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificInfoBarStateChanged(bool is_animating) override;

 private:
  // This view draws the shadow over the web contents below the
  // lowest infobar. A separate view with a layer is used so it can
  // draw outside the bounds of |this|.
  raw_ptr<views::View> content_shadow_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
