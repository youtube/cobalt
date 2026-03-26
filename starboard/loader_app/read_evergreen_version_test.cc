// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/read_evergreen_version.h"

#include <unistd.h>

#include <vector>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"
#include "third_party/jsoncpp/source/include/json/writer.h"

namespace loader_app {
namespace {

// Filename for the manifest file which contains the Evergreen version.
const char kManifestFileName[] = "manifest.json";

const char kTestEvergreenVersion[] = "1.2.1";

// Evergreen version key in the manifest file.
const char kVersionKey[] = "version";

class ReadEvergreenVersionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    dir_.resize(kSbFileMaxPath);
    ASSERT_TRUE(
        SbSystemGetPath(kSbSystemPathTempDirectory, dir_.data(), dir_.size()));
    manifest_path_.resize(kSbFileMaxPath);
    snprintf(manifest_path_.data(), manifest_path_.size(), "%s%s%s",
             dir_.data(), kSbFileSepString, kManifestFileName);
  }

  virtual void TearDown() { ASSERT_EQ(unlink(manifest_path_.data()), 0); }

  std::vector<char> dir_;
  std::vector<char> manifest_path_;
};

TEST_F(ReadEvergreenVersionTest, ReadEvergreenVersionReturnsFalseIfAbsent) {
  std::vector<char> current_version(kMaxEgVersionLength);
  Json::Value root;
  root["manifest_version"] = 2;

  starboard::ScopedFile manifest_file(manifest_path_.data(), O_RDWR | O_CREAT,
                                      S_IRWXU | S_IRWXG);
  ASSERT_TRUE(manifest_file.IsValid());
  Json::StreamWriterBuilder builder;
  std::string manifest_file_str = Json::writeString(builder, root);
  ASSERT_EQ(manifest_file.WriteAll(manifest_file_str.c_str(),
                                   manifest_file_str.length()),
            manifest_file_str.length());

  ASSERT_FALSE(ReadEvergreenVersion(manifest_path_, current_version.data(),
                                    kMaxEgVersionLength));
}

TEST_F(ReadEvergreenVersionTest,
       ReadEvergreenVersionReadsInVersionFromManifest) {
  std::vector<char> current_version(kMaxEgVersionLength);
  Json::Value root;
  root["manifest_version"] = 2;
  root[kVersionKey] = kTestEvergreenVersion;

  starboard::ScopedFile manifest_file(manifest_path_.data(), O_RDWR | O_CREAT,
                                      S_IRWXU | S_IRWXG);
  ASSERT_TRUE(manifest_file.IsValid());
  Json::StreamWriterBuilder builder;
  std::string manifest_file_str = Json::writeString(builder, root);
  ASSERT_EQ(manifest_file.WriteAll(manifest_file_str.c_str(),
                                   manifest_file_str.length()),
            manifest_file_str.length());

  ASSERT_TRUE(ReadEvergreenVersion(manifest_path_, current_version.data(),
                                   kMaxEgVersionLength));
  ASSERT_STREQ(kTestEvergreenVersion, current_version.data());
}

TEST_F(ReadEvergreenVersionTest, ReadEvergreenVersionReturnsFalseIfTruncated) {
  const char kLongEvergreenVersion[] = "1.2.3.4.5.6.7.8.9.10.11.12";
  ASSERT_GE(strlen(kLongEvergreenVersion), kMaxEgVersionLength);

  std::vector<char> current_version(kMaxEgVersionLength);
  Json::Value root;
  root["manifest_version"] = 2;
  root[kVersionKey] = kLongEvergreenVersion;

  starboard::ScopedFile manifest_file(manifest_path_.data(), O_RDWR | O_CREAT,
                                      S_IRWXU | S_IRWXG);
  ASSERT_TRUE(manifest_file.IsValid());
  Json::StreamWriterBuilder builder;
  std::string manifest_file_str = Json::writeString(builder, root);
  ASSERT_EQ(manifest_file.WriteAll(manifest_file_str.c_str(),
                                   manifest_file_str.length()),
            manifest_file_str.length());

  ASSERT_FALSE(ReadEvergreenVersion(manifest_path_, current_version.data(),
                                    kMaxEgVersionLength));
}

}  // namespace
}  // namespace loader_app
