// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_THROTTLE_HANDLE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_THROTTLE_HANDLE_H_

#include "build/build_config.h"
#include "third_party/blink/public/common/buildflags.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace content {

// A simple class that holds the callback that unthrottles either a SW or a
// navigation. It is refcounted and runs `throttle_callback` when destroyed.
class DevToolsThrottleHandle : public base::RefCounted<DevToolsThrottleHandle> {
 public:
  explicit DevToolsThrottleHandle(base::OnceCallback<void()> throttle_callback);

  DevToolsThrottleHandle(const DevToolsThrottleHandle&) = delete;
  DevToolsThrottleHandle& operator=(const DevToolsThrottleHandle&) = delete;

 private:
  friend class base::RefCounted<DevToolsThrottleHandle>;

  ~DevToolsThrottleHandle();

  base::OnceCallback<void()> throttle_callback_;
};

}  // namespace content

#else  // BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

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

#endif  // BUILDFLAG(ENABLE_DEVTOOLS_BACKEND)

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_THROTTLE_HANDLE_H_
