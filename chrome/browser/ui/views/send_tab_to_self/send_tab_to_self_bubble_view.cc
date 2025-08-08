// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"

namespace send_tab_to_self {

SendTabToSelfBubbleView::SendTabToSelfBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents) {}

}  // namespace send_tab_to_self
