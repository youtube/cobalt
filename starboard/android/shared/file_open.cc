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

#include "starboard/file.h"

#include <android/asset_manager.h>

#include "starboard/memory.h"
#include "starboard/string.h"

#include "starboard/android/shared/file_internal.h"
#include "starboard/shared/posix/impl/file_open.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;

namespace {

// We don't package most font files in Cobalt content and fallback to the system
// font file of the same name.
const char kFontsXml[] = "fonts.xml";
const char kSystemFontsDir[] = "/system/fonts/";
const char kCobaltFontsDir[] = "/cobalt/assets/fonts/";
const size_t kSystemFontsDirLen = sizeof(kSystemFontsDir) - 1;
const size_t kCobaltFontsDirLen = sizeof(kCobaltFontsDir) - 1;

// Returns the fallback for the given asset path, or NULL if none. If not NULL
// the result is in a buffer allocated by this function and that can be freed
// with SbMemoryDeallocate.
// NOTE: While Cobalt now provides a mechanism for loading system fonts through
//       SbSystemGetPath(), using the fallback logic within SbFileOpen() is
//       still preferred for Android's fonts. The reason for this is that the
//       Android OS actually allows fonts to be loaded from two locations: one
//       that it provides; and one that the devices running its OS, which it
//       calls vendors, can provide. Rather than including the full Android font
//       package, vendors have the option of using a smaller Android font
//       package and supplementing it with their own fonts.
//
//       If Android were to use SbSystemGetPath() for its fonts, vendors would
//       have no way of providing those supplemental fonts to Cobalt, which
//       could result in a limited selection of fonts being available. By
//       treating Android's fonts as Cobalt's fonts, Cobalt can still offer a
//       straightforward mechanism for including vendor fonts via
//       SbSystemGetPath().
const char* FallbackPath(const char* path) {
  // Fonts fallback to the system fonts.
  if (path && strncmp(path, kCobaltFontsDir, kCobaltFontsDirLen) == 0) {
    const char* file_name = path + kCobaltFontsDirLen;
    // fonts.xml doesn't fallback.
    if (strcmp(file_name, kFontsXml) != 0) {
      size_t length = kSystemFontsDirLen + SbStringGetLength(file_name);
      char* fallback_path = static_cast<char*>(SbMemoryAllocate(length + 1));
      SbStringCopyUnsafe(fallback_path, kSystemFontsDir);
      SbStringCopyUnsafe(fallback_path + kSystemFontsDirLen, file_name);
      return fallback_path;
    }
  }
  return NULL;
}

}  // namespace

SbFile SbFileOpen(const char* path,
                  int flags,
                  bool* out_created,
                  SbFileError* out_error) {
  if (!IsAndroidAssetPath(path)) {
    return ::starboard::shared::posix::impl::FileOpen(
        path, flags, out_created, out_error);
  }

  // Assets are never created and are always read-only, whether it's actually an
  // asset or we end up opening a fallback path.
  if (out_created) {
    *out_created = false;
  }
  bool can_read = flags & kSbFileRead;
  bool can_write = flags & kSbFileWrite;
  if (!can_read || can_write) {
    if (out_error) {
      *out_error = kSbFileErrorAccessDenied;
    }
    return kSbFileInvalid;
  }

  AAsset* asset = OpenAndroidAsset(path);
  if (asset) {
    SbFile result = new SbFilePrivate();
    result->asset = asset;
    return result;
  }

  const char* fallback_path = FallbackPath(path);
  if (fallback_path) {
    SbFile result = ::starboard::shared::posix::impl::FileOpen(
        fallback_path, flags, out_created, out_error);
    SbMemoryDeallocate(const_cast<char*>(fallback_path));
    return result;
  }

  if (out_error) {
    *out_error = kSbFileErrorFailed;
  }
  return kSbFileInvalid;
}
