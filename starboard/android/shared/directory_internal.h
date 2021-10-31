// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_DIRECTORY_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_DIRECTORY_INTERNAL_H_

#include <android/asset_manager.h>

#include <dirent.h>

#include "starboard/directory.h"
#include "starboard/shared/internal_only.h"

struct SbDirectoryPrivate {
  // Note: Only one of these two fields will be valid for any given file.

  // The ISO C directory stream handle, or NULL if it's an asset directory.
  DIR* directory;

  // If not NULL this is an Android asset directory.
  AAssetDir* asset_dir;

  SbDirectoryPrivate() : directory(NULL), asset_dir(NULL) {}
};

#endif  // STARBOARD_ANDROID_SHARED_DIRECTORY_INTERNAL_H_
