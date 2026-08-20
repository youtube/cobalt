// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_THROTTLE_HANDLE_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_THROTTLE_HANDLE_STUB_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace content {

class DevToolsThrottleHandle : public base::RefCounted<DevToolsThrottleHandle> {
 public:
  explicit DevToolsThrottleHandle(
      base::OnceCallback<void()> throttle_callback) {
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

#endif  // CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_THROTTLE_HANDLE_STUB_H_
