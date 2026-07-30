// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drag_state.h"

namespace webui_toolbar {

// static
WebUIToolbarDragState* WebUIToolbarDragState::GetOrCreateForWebContents(
    content::WebContents* contents) {
  CreateForWebContents(contents);
  return FromWebContents(contents);
}

WebUIToolbarDragState::WebUIToolbarDragState(content::WebContents* contents)
    : content::WebContentsUserData<WebUIToolbarDragState>(*contents) {}

WebUIToolbarDragState::~WebUIToolbarDragState() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebUIToolbarDragState);

}  // namespace webui_toolbar
