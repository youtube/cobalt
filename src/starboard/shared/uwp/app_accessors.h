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

#ifndef STARBOARD_SHARED_UWP_APP_ACCESSORS_H_
#define STARBOARD_SHARED_UWP_APP_ACCESSORS_H_

// A set of application and main-thread accessors
// so as to avoid including application_uwp.h

#include <agile.h>
#include <ppltasks.h>

#include <string>

namespace starboard {
namespace shared {
namespace uwp {

// Returns the main window's CoreDispatcher.
Platform::Agile<Windows::UI::Core::CoreDispatcher> GetDispatcher();

// Returns the main window's SystemMediaTransportControls.
Platform::Agile<Windows::Media::SystemMediaTransportControls>
    GetTransportControls();

// Asks the screen to remain active via
// Windows::System::DisplayRequest->RequestActive()
void DisplayRequestActive();

// Releases previous screen active request via
// Windows::System::DisplayRequest->RequestRelease()
void DisplayRequestRelease();

// Schedules a lambda to run on the main thread and returns immediately.
template<typename T>
void RunInMainThreadAsync(const T& lambda) {
  GetDispatcher()->RunAsync(
    Windows::UI::Core::CoreDispatcherPriority::Normal,
    ref new Windows::UI::Core::DispatchedHandler(lambda));
}

// Tries to fetch an SSO token, returning nullptr or exception on failure
concurrency::task<
    Windows::Security::Authentication::Web::Core::WebTokenRequestResult^>
    TryToFetchSsoToken(const std::string& url);

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
#endif  // STARBOARD_SHARED_UWP_APP_ACCESSORS_H_
