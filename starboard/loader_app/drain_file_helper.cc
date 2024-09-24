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

#include "starboard/loader_app/drain_file_helper.h"

#include <sys/stat.h>
#include <unistd.h>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/loader_app/drain_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace loader_app {

ScopedDrainFile::ScopedDrainFile(const std::string& dir,
                                 const std::string& app_key,
                                 int64_t timestamp) {
  app_key_.assign(app_key);

  path_.assign(dir);
  path_.append(kSbFileSepString);
  path_.append(kDrainFilePrefix);
  path_.append(app_key);
  path_.append("_");
  path_.append(std::to_string(timestamp / kDrainFileAgeUnitUsec));

  CreateFile();
}

ScopedDrainFile::~ScopedDrainFile() {
  if (!Exists())
    return;
  EXPECT_TRUE(!unlink(path_.c_str()));
}

bool ScopedDrainFile::Exists() const {
  struct stat info;
  return stat(path_.c_str(), &info) == 0;
}

const std::string& ScopedDrainFile::app_key() const {
  return app_key_;
}

const std::string& ScopedDrainFile::path() const {
  return path_;
}

void ScopedDrainFile::CreateFile() {
  starboard::ScopedFile file(path_.c_str(), O_CREAT | O_EXCL | O_WRONLY);

  EXPECT_TRUE(file.IsValid());
}

}  // namespace loader_app
}  // namespace starboard
