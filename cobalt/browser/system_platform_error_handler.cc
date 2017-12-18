// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/system_platform_error_handler.h"

#include "base/logging.h"

namespace cobalt {
namespace browser {

void SystemPlatformErrorHandler::RaiseSystemPlatformError(
    const SystemPlatformErrorOptions& options) {
  DCHECK(!options.callback.is_null());

  CallbackData* callback_data = new CallbackData{ &mutex_, options.callback };

  SbSystemPlatformError handle = SbSystemRaisePlatformError(options.error_type,
      &SystemPlatformErrorHandler::HandleSystemPlatformErrorResponse,
      callback_data);
  if (!SbSystemPlatformErrorIsValid(handle)) {
    DLOG(WARNING) << "Did not handle error: " << options.error_type;
    delete callback_data;
    callback_data = nullptr;
  }

  // In case the response callback is never called, track the callback data
  // for all active errors. When this object is destroyed, all dangling data
  // will be released.
  {
    starboard::ScopedLock lock(mutex_);

    // Delete any consumed callback data.
    for (size_t i = 0; i < callback_data_.size();) {
      if (callback_data_[i]->callback.is_null()) {
        callback_data_.erase(callback_data_.begin() + i);
      } else {
        ++i;
      }
    }

    if (callback_data) {
      callback_data_.emplace_back(callback_data);
    }
  }
}

// static
void SystemPlatformErrorHandler::HandleSystemPlatformErrorResponse(
    SbSystemPlatformErrorResponse response, void* user_data) {
  CallbackData* callback_data = static_cast<CallbackData*>(user_data);
  callback_data->callback.Run(response);

  {
    starboard::ScopedLock lock(*(callback_data->mutex));
    callback_data->callback.Reset();
  }
}

}  // namespace browser
}  // namespace cobalt
