// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy.h"

namespace base {

MessageLoopProxy::MessageLoopProxy() {
}

MessageLoopProxy::~MessageLoopProxy() {
}

void MessageLoopProxy::OnDestruct() {
  delete this;
}

}  // namespace base
