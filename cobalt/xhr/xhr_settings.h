// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_XHR_XHR_SETTINGS_H_
#define COBALT_XHR_XHR_SETTINGS_H_

#include <string>

#include "base/logging.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace xhr {

// Holds browser wide settings for XMLHttpRequest.  Their default values are set
// in XMLHttpRequest related classes, and the default values will be overridden
// if the return values of the member functions are non-empty.
// Please refer to where these functions are called for the particular
// XMLHttpRequest behaviors being controlled by them.
class XhrSettings {
 public:
  virtual base::Optional<bool> IsFetchBufferPoolEnabled() const = 0;
  virtual base::Optional<int> GetDefaultFetchBufferSize() const = 0;
  virtual base::Optional<bool> IsTryLockForProgressCheckEnabled() const = 0;

 protected:
  XhrSettings() = default;
  ~XhrSettings() = default;

  XhrSettings(const XhrSettings&) = delete;
  XhrSettings& operator=(const XhrSettings&) = delete;
};

// Allows setting the values of XMLHttpRequest settings via a name and an int
// value.
// This class is thread safe.
class XhrSettingsImpl : public XhrSettings {
 public:
  base::Optional<bool> IsFetchBufferPoolEnabled() const override {
    base::AutoLock auto_lock(lock_);
    return is_fetch_buffer_pool_enabled_;
  }
  base::Optional<int> GetDefaultFetchBufferSize() const override {
    base::AutoLock auto_lock(lock_);
    return default_fetch_buffer_size_;
  }
  base::Optional<bool> IsTryLockForProgressCheckEnabled() const override {
    base::AutoLock auto_lock(lock_);
    return is_try_lock_for_progress_check_enabled_;
  }

  bool Set(const std::string& name, int32_t value) {
    if (name == "XHR.EnableFetchBufferPool") {
      if (value == 0 || value == 1) {
        LOG(INFO) << name << ": set to " << value;

        base::AutoLock auto_lock(lock_);
        is_fetch_buffer_pool_enabled_ = value != 0;
        return true;
      }
    } else if (name == "XHR.DefaultFetchBufferSize") {
      if (value > 0) {
        LOG(INFO) << name << ": set to " << value;

        base::AutoLock auto_lock(lock_);
        default_fetch_buffer_size_ = value;
        return true;
      }
    } else if (name == "XHR.EnableTryLockForProgressCheck") {
      if (value == 0 || value == 1) {
        LOG(INFO) << name << ": set to " << value;

        base::AutoLock auto_lock(lock_);
        is_try_lock_for_progress_check_enabled_ = value != 0;
        return true;
      }
    }

    return false;
  }

 private:
  mutable base::Lock lock_;
  base::Optional<bool> is_fetch_buffer_pool_enabled_;
  base::Optional<int> default_fetch_buffer_size_;
  base::Optional<bool> is_try_lock_for_progress_check_enabled_;
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_XHR_SETTINGS_H_
