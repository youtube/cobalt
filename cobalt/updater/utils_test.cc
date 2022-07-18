// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/updater/utils.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "starboard/common/file.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace updater {
namespace {

const char kEvergreenManifestFilename[] = "manifest.json";
const char kEvergreenLibDirname[] = "lib";

class UtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    temp_dir_path_.resize(kSbFileMaxPath);
    ASSERT_TRUE(SbSystemGetPath(kSbSystemPathTempDirectory,
                                temp_dir_path_.data(), temp_dir_path_.size()));
  }

  void TearDown() override {
    ASSERT_TRUE(starboard::SbFileDeleteRecursive(temp_dir_path_.data(), true));
  }

  void CreateManifest(const char* content, const std::string& directory) {
    std::string manifest_path =
        base::StrCat({directory, kSbFileSepString, kEvergreenManifestFilename});
    SbFile sb_file =
        SbFileOpen(manifest_path.c_str(), kSbFileOpenAlways | kSbFileRead,
                   nullptr, nullptr);
    ASSERT_TRUE(SbFileIsValid(sb_file));
    ASSERT_TRUE(SbFileClose(sb_file));

    ASSERT_TRUE(
        SbFileAtomicReplace(manifest_path.c_str(), content, strlen(content)));
  }

  void DeleteManifest(const std::string& directory) {
    std::string manifest_path =
        base::StrCat({directory, kSbFileSepString, kEvergreenManifestFilename});
    ASSERT_TRUE(SbFileDelete(manifest_path.c_str()));
  }

  void CreateEmptyLibrary(const std::string& name,
                          const std::string& installation_path) {
    std::string lib_path = base::StrCat(
        {installation_path, kSbFileSepString, kEvergreenLibDirname});
    ASSERT_TRUE(SbDirectoryCreate(lib_path.c_str()));

    lib_path = base::StrCat({lib_path, kSbFileSepString, name});
    SbFile sb_file = SbFileOpen(
        lib_path.c_str(), kSbFileOpenAlways | kSbFileRead, nullptr, nullptr);
    ASSERT_TRUE(SbFileIsValid(sb_file));
    ASSERT_TRUE(SbFileClose(sb_file));
  }

  void DeleteLibraryDirRecursively(const std::string& installation_path) {
    std::string lib_path = base::StrCat(
        {installation_path, kSbFileSepString, kEvergreenLibDirname});
    ASSERT_TRUE(starboard::SbFileDeleteRecursive(lib_path.c_str(), false));
  }

  std::vector<char> temp_dir_path_;
};


TEST_F(UtilsTest, ReadEvergreenVersionReturnsVersionForValidManifest) {
  std::string installation_path = base::StrCat(
      {temp_dir_path_.data(), kSbFileSepString, "some_installation_path"});
  ASSERT_TRUE(SbDirectoryCreate(installation_path.c_str()));
  char manifest_content[] = R"json(
  {
    "manifest_version": 2,
    "name": "Cobalt",
    "description": "Cobalt",
    "version": "1.2.0"
  })json";
  CreateManifest(manifest_content, installation_path);

  base::Version version =
      ReadEvergreenVersion(base::FilePath(installation_path));

  ASSERT_EQ(version.GetString(), "1.2.0");

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest,
       ReadEvergreenVersionReturnsInvalidVersionForVersionlessManifest) {
  std::string installation_path = base::StrCat(
      {temp_dir_path_.data(), kSbFileSepString, "some_installation_path"});
  ASSERT_TRUE(SbDirectoryCreate(installation_path.c_str()));
  char versionless_manifest_content[] = R"json(
  {
    "manifest_version": 2,
    "name": "Cobalt",
    "description": "Cobalt",
  })json";
  CreateManifest(versionless_manifest_content, installation_path);

  base::Version version =
      ReadEvergreenVersion(base::FilePath(installation_path));

  ASSERT_FALSE(version.IsValid());

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest, ReadEvergreenVersionReturnsInvalidVersionForMissingManifest) {
  base::Version version =
      ReadEvergreenVersion(base::FilePath("nonexistent_manifest_path"));

  ASSERT_FALSE(version.IsValid());
}

TEST_F(UtilsTest,
       ReturnsValidCurrentEvergreenVersionForManifestInLoadedInstallation) {
  std::vector<char> system_path_content_dir(kSbFileMaxPath);
  SbSystemGetPath(kSbSystemPathContentDirectory, system_path_content_dir.data(),
                  kSbFileMaxPath);
  // Since the libupdater_test.so library has already been loaded,
  // kSbSystemPathContentDirectory points to the content dir of the running
  // library and the installation dir is therefore its parent.
  std::string installation_path =
      base::FilePath(std::string(system_path_content_dir.begin(),
                                 system_path_content_dir.end()))
          .DirName()
          .value();
  char manifest_content[] = R"json(
  {
    "manifest_version": 2,
    "name": "Cobalt",
    "description": "Cobalt",
    "version": "1.2.0"
  })json";
  CreateManifest(manifest_content, installation_path);

  std::string version = GetCurrentEvergreenVersion();

  ASSERT_EQ(version, "1.2.0");

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest,
       ReturnsDefaultEvergreenVersionForManifestMissingFromLoadedInstallation) {
  std::string version = GetCurrentEvergreenVersion();

  ASSERT_EQ(version, kDefaultManifestVersion);
}

TEST_F(UtilsTest,
       ReturnsValidCurrentMetadataForValidFilesInLoadedInstallation) {
  std::vector<char> system_path_content_dir(kSbFileMaxPath);
  SbSystemGetPath(kSbSystemPathContentDirectory, system_path_content_dir.data(),
                  kSbFileMaxPath);
  // Since the libupdater_test.so library has already been loaded,
  // kSbSystemPathContentDirectory points to the content dir of the running
  // library and the installation dir is therefore its parent.
  std::string installation_path =
      base::FilePath(std::string(system_path_content_dir.begin(),
                                 system_path_content_dir.end()))
          .DirName()
          .value();
  char manifest_content[] = R"json(
  {
    "manifest_version": 2,
    "name": "Cobalt",
    "description": "Cobalt",
    "version": "1.2.0"
  })json";
  CreateManifest(manifest_content, installation_path);
  CreateEmptyLibrary("libcobalt.so", installation_path);

  EvergreenLibraryMetadata metadata = GetCurrentEvergreenLibraryMetadata();

  ASSERT_EQ(metadata.version, "1.2.0");
  ASSERT_EQ(metadata.file_type, "Uncompressed");

  DeleteManifest(installation_path);
  DeleteLibraryDirRecursively(installation_path);
}

TEST_F(UtilsTest,
       ReturnsUnknownFileTypeInMetadataForUnexpectedLibInLoadedInstallation) {
  std::vector<char> system_path_content_dir(kSbFileMaxPath);
  SbSystemGetPath(kSbSystemPathContentDirectory, system_path_content_dir.data(),
                  kSbFileMaxPath);
  // Since the libupdater_test.so library has already been loaded,
  // kSbSystemPathContentDirectory points to the content dir of the running
  // library and the installation dir is therefore its parent.
  std::string installation_path =
      base::FilePath(std::string(system_path_content_dir.begin(),
                                 system_path_content_dir.end()))
          .DirName()
          .value();
  CreateEmptyLibrary("libcobalt.unexpected", installation_path);

  EvergreenLibraryMetadata metadata = GetCurrentEvergreenLibraryMetadata();

  ASSERT_EQ(metadata.file_type, "FileTypeUnknown");

  DeleteLibraryDirRecursively(installation_path);
}

}  // namespace
}  // namespace updater
}  // namespace cobalt
