// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/window.h"

#include "starboard/android/shared/application_android.h"

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
void SbWindowUpdateOnScreenKeyboardSuggestions(SbWindow window,
                                               const char* suggestions[],
                                               int num_suggestions,
                                               int ticket) {
  std::vector<std::string> suggestions_data;
  for (int i = 0; i < num_suggestions; ++i) {
    suggestions_data.push_back(suggestions[i]);
  }
  starboard::android::shared::ApplicationAndroid::Get()
      ->SbWindowUpdateOnScreenKeyboardSuggestions(window, suggestions_data,
                                                  ticket);
  return;
}
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
