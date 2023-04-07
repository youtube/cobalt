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

#include <windows.graphics.display.h>
#include "starboard/window.h"

using Windows::Graphics::Display::DisplayInformation;

float SbWindowGetDiagonalSizeInInches(SbWindow window) {
  DisplayInformation ^ displayInfo = DisplayInformation::GetForCurrentView();
  Platform::IBox<double> ^ diagonalSize = displayInfo->DiagonalSizeInInches;
  if (!SbWindowIsValid(window) || diagonalSize == nullptr) {
    // 0 is a signal meaning that the platform doesn't know the diagonal screen
    // length.
    return 0.f;
  } else {
    return diagonalSize->Value;
  }
}
