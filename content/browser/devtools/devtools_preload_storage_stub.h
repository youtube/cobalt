// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PRELOAD_STORAGE_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PRELOAD_STORAGE_STUB_H_

namespace content {

class RenderFrameHost;

class DevToolsPreloadStorage {
 public:
  static DevToolsPreloadStorage* GetOrCreateForCurrentDocument(
      RenderFrameHost* rfh) {
    return nullptr;
  }
  static DevToolsPreloadStorage* GetForCurrentDocument(RenderFrameHost* rfh) {
    return nullptr;
  }
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PRELOAD_STORAGE_STUB_H_
