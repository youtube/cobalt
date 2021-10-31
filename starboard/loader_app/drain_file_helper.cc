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

#include "starboard/common/file.h"
#include "starboard/loader_app/drain_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace loader_app {

ScopedDrainFile::ScopedDrainFile(const std::string& dir,
                                 const std::string& app_key,
                                 SbTime timestamp) {
  app_key_.assign(app_key);

  path_.assign(dir);
  path_.append(kSbFileSepString);
  path_.append(kDrainFilePrefix);
  path_.append(app_key);
  path_.append("_");
  path_.append(std::to_string(timestamp / kDrainFileAgeUnit));

  CreateFile();
}

ScopedDrainFile::~ScopedDrainFile() {
  if (!Exists())
    return;
  EXPECT_TRUE(SbFileDelete(path_.c_str()));
}

bool ScopedDrainFile::Exists() const {
  return SbFileExists(path_.c_str());
}

const std::string& ScopedDrainFile::app_key() const {
  return app_key_;
}

const std::string& ScopedDrainFile::path() const {
  return path_;
}

void ScopedDrainFile::CreateFile() {
  SbFileError error = kSbFileOk;
  starboard::ScopedFile file(path_.c_str(), kSbFileCreateOnly | kSbFileWrite,
                             NULL, &error);

  EXPECT_TRUE(file.IsValid());
}

}  // namespace loader_app
}  // namespace starboard
