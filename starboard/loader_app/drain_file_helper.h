// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LOADER_APP_DRAIN_FILE_HELPER_H_
#define STARBOARD_LOADER_APP_DRAIN_FILE_HELPER_H_

#include <string>

#include "starboard/time.h"

namespace starboard {
namespace loader_app {

// Creates and removes a file within its own lifetime. This class maintains the
// path to the file, and the app key that it was created with, to provide a
// convenient way of bundling the information and state of the file. This class
// is very similar in concept to the starboard::nplb::ScopedRandomFile, except
// that it allows you to choose where to create the file.
class ScopedDrainFile {
 public:
  ScopedDrainFile(const std::string& dir, const std::string& app_key,
                  SbTime timestamp);
  ~ScopedDrainFile();

  // Whether or not the created file still exists.
  bool Exists() const;

  const std::string& app_key() const;
  const std::string& path() const;

 private:
  void CreateFile();

  std::string app_key_;
  std::string path_;
};

}  // namespace loader_app
}  // namespace starboard

#endif  // STARBOARD_LOADER_APP_DRAIN_FILE_HELPER_H_
