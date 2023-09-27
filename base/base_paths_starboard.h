// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef BASE_BASE_PATHS_STARBOARD_H_
#define BASE_BASE_PATHS_STARBOARD_H_

// This file declares Starboard-specific path keys for the base module.  These
// can be used with the PathService to access various special directories and
// files.

namespace base {

enum {
  PATH_STARBOARD_START = 500,

  DIR_CACHE,    // Directory where to put cache data.  Note this is
                // *not* where the browser cache lives, but the
                // browser cache can be a subdirectory.

  DIR_SYSTEM_FONTS,                // Directory where system font files can be
                                   // be found. This is only specified on
                                   // platforms that provide fonts usable by
                                   // Starboard applications.

  DIR_SYSTEM_FONTS_CONFIGURATION,  // Directory where system font configuration
                                   // metadata can be found. May be the same
                                   // directory as DIR_SYSTEM_FONTS, but not
                                   // necessarily. This is only specified on
                                   // platforms that provide fonts usable by
                                   // Starboard applications.

  PATH_STARBOARD_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_STARBOARD_H_
