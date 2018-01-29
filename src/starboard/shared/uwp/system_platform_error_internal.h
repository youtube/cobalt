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

#ifndef STARBOARD_SHARED_UWP_SYSTEM_PLATFORM_ERROR_INTERNAL_H_
#define STARBOARD_SHARED_UWP_SYSTEM_PLATFORM_ERROR_INTERNAL_H_

#include "starboard/atomic.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/system.h"

// Note that this is a "struct" and not a "class" because
// that's how it's defined in starboard/system.h
struct SbSystemPlatformErrorPrivate {
  typedef starboard::shared::uwp::ApplicationUwp ApplicationUwp;

  SbSystemPlatformErrorPrivate(const SbSystemPlatformErrorPrivate&) = delete;
  SbSystemPlatformErrorPrivate& operator=(const SbSystemPlatformErrorPrivate&)
      = delete;

  SbSystemPlatformErrorPrivate(ApplicationUwp* app,
      SbSystemPlatformErrorType type, SbSystemPlatformErrorCallback callback,
      void* user_data);
  bool IsValid() const;
  void ClearAndDelete();

 private:
  typedef Windows::UI::Popups::IUICommand IUICommand;
  typedef Windows::Foundation::IAsyncOperation<IUICommand^> DialogOperation;

  IUICommand^ MakeUICommand(ApplicationUwp* app, const char* id,
      const char* fallback, SbSystemPlatformErrorResponse response);

  SbSystemPlatformErrorCallback callback_;
  void* user_data_;
  Platform::Agile<DialogOperation> dialog_operation_;

  static SbAtomic32 s_error_count;
};

#endif  // STARBOARD_SHARED_UWP_SYSTEM_PLATFORM_ERROR_INTERNAL_H_
