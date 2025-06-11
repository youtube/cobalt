// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_CREATOR_H_
#define COBALT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_CREATOR_H_

#include <memory>

namespace content {

class WebContents;
class WebContentsViewDelegate;

std::unique_ptr<WebContentsViewDelegate> CreateShellWebContentsViewDelegate(
    WebContents* web_contents);

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_CREATOR_H_
