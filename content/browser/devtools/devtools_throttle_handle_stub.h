// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_THROTTLE_HANDLE_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_THROTTLE_HANDLE_STUB_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace content {

class DevToolsThrottleHandle : public base::RefCounted<DevToolsThrottleHandle> {
 public:
  explicit DevToolsThrottleHandle(base::OnceCallback<void()> throttle_callback) {
    if (throttle_callback) {
      std::move(throttle_callback).Run();
    }
  }

  DevToolsThrottleHandle(const DevToolsThrottleHandle&) = delete;
  DevToolsThrottleHandle& operator=(const DevToolsThrottleHandle&) = delete;

 private:
  friend class base::RefCounted<DevToolsThrottleHandle>;
  ~DevToolsThrottleHandle() = default;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_THROTTLE_HANDLE_STUB_H_
