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

#ifndef STARBOARD_ANDROID_SHARED_INPUT_EVENTS_GENERATOR_H_
#define STARBOARD_ANDROID_SHARED_INPUT_EVENTS_GENERATOR_H_

#include <android/input.h>

#include "starboard/shared/starboard/application.h"
#include "starboard/window.h"

namespace starboard {
namespace android {
namespace shared {

/**
 * Translates an Android input event into a Starboard application event. The
 * caller owns the new event and must delete it when done with it.
 */
::starboard::shared::starboard::Application::Event*
CreateInputEvent(AInputEvent* androidEvent, SbWindow window);

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_INPUT_EVENTS_GENERATOR_H_
