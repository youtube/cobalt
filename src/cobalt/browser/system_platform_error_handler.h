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

#ifndef COBALT_BROWSER_SYSTEM_PLATFORM_ERROR_HANDLER_H_
#define COBALT_BROWSER_SYSTEM_PLATFORM_ERROR_HANDLER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "starboard/mutex.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {

// This class handles raising system errors to the platform and returning the
// response to the caller via the provided callback.
class SystemPlatformErrorHandler {
 public:
  // Type of callback to run when the platform responds to a system error.
  typedef base::Callback<void(SbSystemPlatformErrorResponse response)>
      SystemPlatformErrorCallback;

  // Options structure for raised system platform errors, including both the
  // system error being raised and the callback to run with the response.
  struct SystemPlatformErrorOptions {
    SbSystemPlatformErrorType error_type;
    SystemPlatformErrorCallback callback;
  };

  // Raises a system error with the specified error type and callback.
  void RaiseSystemPlatformError(const SystemPlatformErrorOptions& options);

 private:
  // This specifies the user data passed to the error response callback.
  struct CallbackData {
    starboard::Mutex* mutex;
    SystemPlatformErrorCallback callback;
  };

  // This is called when the platform responds to the system error.
  static void HandleSystemPlatformErrorResponse(
      SbSystemPlatformErrorResponse response, void* user_data);

  starboard::Mutex mutex_;
  std::vector<std::unique_ptr<CallbackData>> callback_data_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SYSTEM_PLATFORM_ERROR_HANDLER_H_
