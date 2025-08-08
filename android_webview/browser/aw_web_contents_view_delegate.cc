// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_contents_view_delegate.h"

#include <memory>

#include "content/public/browser/web_contents.h"
#include "ui/gfx/color_space.h"

namespace android_webview {

AwWebContentsViewDelegate::AwWebContentsViewDelegate(
    content::WebContents* web_contents) {
  // Cannot instantiate web_contents_view_delegate_ here because
  // AwContents::SetWebDelegate is not called yet.
}

AwWebContentsViewDelegate::~AwWebContentsViewDelegate() {}

content::WebDragDestDelegate* AwWebContentsViewDelegate::GetDragDestDelegate() {
  // GetDragDestDelegate is a pure virtual method from WebContentsViewDelegate
  // and must have an implementation although android doesn't use it.
  NOTREACHED();
  return NULL;
}

}  // namespace android_webview
