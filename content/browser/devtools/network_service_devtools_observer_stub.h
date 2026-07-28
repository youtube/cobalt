// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_STUB_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"

namespace content {

class FrameTreeNode;

class NetworkServiceDevToolsObserver {
 public:
  static mojo::PendingRemote<::network::mojom::DevToolsObserver> MakeSelfOwned(
      const std::string& id) {
    return {};
  }
  static mojo::PendingRemote<::network::mojom::DevToolsObserver> MakeSelfOwned(
      FrameTreeNode* frame_tree_node) {
    return {};
  }
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_NETWORK_SERVICE_DEVTOOLS_OBSERVER_STUB_H_
