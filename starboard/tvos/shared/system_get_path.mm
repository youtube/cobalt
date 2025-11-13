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

#import <Foundation/Foundation.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || path_size <= 0) {
    return false;
  }

  @autoreleasepool {
    NSString* path;

    switch (path_id) {
      case kSbSystemPathContentDirectory: {
        path = [[NSBundle mainBundle].bundlePath
            stringByAppendingPathComponent:@"content"];
        break;
      }
      case kSbSystemPathStorageDirectory: {
        path = [NSSearchPathForDirectoriesInDomains(
            NSCachesDirectory, NSUserDomainMask, YES) lastObject];
        break;
      }
      case kSbSystemPathCacheDirectory: {
        path = [NSSearchPathForDirectoriesInDomains(
            NSCachesDirectory, NSUserDomainMask, YES) lastObject];
        break;
      }
      case kSbSystemPathDebugOutputDirectory: {
        path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"debug"];
        break;
      }
      case kSbSystemPathTempDirectory: {
        path = NSTemporaryDirectory();
        break;
      }
      case kSbSystemPathExecutableFile:
        path = [NSBundle mainBundle].executablePath;
        break;
      case kSbSystemPathFontDirectory: {
        // These are for system fonts in the cases where we don't want to
        // package fonts (or as many fonts) in the content directory.
        path = [[NSBundle mainBundle].bundlePath
            stringByAppendingPathComponent:@"data/fonts"];
        break;
      }
      case kSbSystemPathFontConfigurationDirectory:
      default:
        SB_NOTIMPLEMENTED();
        return false;
    }

    const char* path_cstring = [path cStringUsingEncoding:NSUTF8StringEncoding];
    if (!path_cstring || path_size < strlen(path_cstring) + 1) {
      return false;
    }
    starboard::strlcpy(out_path, path_cstring, path_size);
    return true;
  }
}
