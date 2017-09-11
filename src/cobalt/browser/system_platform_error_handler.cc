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
namespace {

void OnSystemPlatformErrorResponse(SbSystemPlatformErrorResponse response,
                                   void* user_data) {
  DCHECK(user_data);
  SystemPlatformErrorHandler* error_handler =
      static_cast<SystemPlatformErrorHandler*>(user_data);
  error_handler->HandleSystemPlatformErrorResponse(response);
}

}  // namespace

void SystemPlatformErrorHandler::RaiseSystemPlatformError(
    const SystemPlatformErrorOptions& options) {
  current_system_platform_error_callback_ = options.callback;

  SbSystemPlatformError handle = SbSystemRaisePlatformError(
      options.error_type, OnSystemPlatformErrorResponse, this);
  if (!SbSystemPlatformErrorIsValid(handle) &&
      !current_system_platform_error_callback_.is_null()) {
    DLOG(WARNING) << "Did not handle error: " << options.error_type;
    current_system_platform_error_callback_.Reset();
  }
}

void SystemPlatformErrorHandler::HandleSystemPlatformErrorResponse(
    SbSystemPlatformErrorResponse response) {
  DCHECK(!current_system_platform_error_callback_.is_null());
  current_system_platform_error_callback_.Run(response);
  current_system_platform_error_callback_.Reset();
}

}  // namespace browser
}  // namespace cobalt
