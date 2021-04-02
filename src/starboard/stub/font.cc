// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/stub/font.h"

#include "cobalt/extension/font.h"

namespace starboard {
namespace stub {

namespace {

bool GetPathFallbackFontDirectory(char* path, int path_size) {
  return false;
}

const CobaltExtensionFontApi kFontApi = {
    kCobaltExtensionFontName,
    1,  // API version that's implemented.
    &GetPathFallbackFontDirectory,
};

}  // namespace

const void* GetFontApi() {
  return &kFontApi;
}

}  // namespace stub
}  // namespace starboard
