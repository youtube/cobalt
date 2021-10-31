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

#ifndef STARBOARD_LOADER_APP_APP_KEY_FILES_H_
#define STARBOARD_LOADER_APP_APP_KEY_FILES_H_

#include <string>

namespace starboard {
namespace loader_app {

// Gets the file path for a good app key file in the |dir| directory.
// The file is used to mark an installation slot as good for the app.
// Returns empty string on failure.
std::string GetGoodAppKeyFilePath(const std::string& dir,
                                  const std::string& app_key);

// Gets the file path for a bad app key file in the |dir| directory.
// The file is used to mark an installation slot as bad for the app.
// Returns empty string on failure.
std::string GetBadAppKeyFilePath(const std::string& dir,
                                 const std::string& app_key);

// Helper function to create a file with the specified full path name.
bool CreateAppKeyFile(const std::string& file_name_path);

// Returns true if there is any good app key file.
// Used for adopting an installation slot updated by different app.
bool AnyGoodAppKeyFile(const std::string& dir);

}  // namespace loader_app
}  // namespace starboard

#endif  // STARBOARD_LOADER_APP_APP_KEY_FILES_H_
