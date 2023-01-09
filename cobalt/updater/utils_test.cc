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
#include "gmock/gmock.h"
#include "starboard/common/file.h"
#include "starboard/directory.h"
#include "starboard/extension/installation_manager.h"
#include "starboard/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace updater {
namespace {

const char kEvergreenManifestFilename[] = "manifest.json";
const char kEvergreenLibDirname[] = "lib";

// Based on the CobaltExtensionInstallationManagerApi struct typedef.
class MockInstallationManagerApi {
 public:
  MOCK_METHOD0(GetCurrentInstallationIndex, int());
  MOCK_METHOD3(GetInstallationPath,
               int(int installation_index, char* path, int path_length));
};

MockInstallationManagerApi* GetMockInstallationManagerApi() {
  static auto* const installation_manager_api = []() {
    auto* inner_installation_manager_api = new MockInstallationManagerApi;
    // Because the mocked methods are non-state-changing, this mock is really
    // just used as a stub. It's therefore ok for this mock object to be leaked
    // and not verified.
    testing::Mock::AllowLeak(inner_installation_manager_api);
    return inner_installation_manager_api;
  }();

  return installation_manager_api;
}

// Stub definitions that delegate to the mock installation manager API.
int StubGetCurrentInstallationIndex() {
  return GetMockInstallationManagerApi()->GetCurrentInstallationIndex();
}

int StubGetInstallationPath(int installation_index, char* path,
                            int path_length) {
  return GetMockInstallationManagerApi()->GetInstallationPath(
      installation_index, path, path_length);
}

// No-op stub definitions for functions that aren't exercised.
int StubMarkInstallationSuccessful(int installation_index) {
  return IM_EXT_SUCCESS;
}

int StubRequestRollForwardToInstallation(int installation_index) {
  return IM_EXT_SUCCESS;
}

int StubSelectNewInstallationIndex() { return IM_EXT_SUCCESS; }

int StubGetAppKey(char* app_key, int app_key_length) { return IM_EXT_SUCCESS; }

int StubGetMaxNumberInstallations() { return IM_EXT_SUCCESS; }

int StubResetInstallation(int installation_index) { return IM_EXT_SUCCESS; }

int StubReset() { return IM_EXT_SUCCESS; }

const CobaltExtensionInstallationManagerApi kStubInstallationManagerApi = {
    kCobaltExtensionInstallationManagerName,
    1,
    &StubGetCurrentInstallationIndex,
    &StubMarkInstallationSuccessful,
    &StubRequestRollForwardToInstallation,
    &StubGetInstallationPath,
    &StubSelectNewInstallationIndex,
    &StubGetAppKey,
    &StubGetMaxNumberInstallations,
    &StubResetInstallation,
    &StubReset,
};

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

TEST_F(UtilsTest, ReturnsEvergreenVersionFromCurrentManagedInstallation) {
  std::string installation_path = base::StrCat(
      {temp_dir_path_.data(), kSbFileSepString, "some_installation_path"});
  int installation_index = 1;

  MockInstallationManagerApi* mock_installation_manager_api =
      GetMockInstallationManagerApi();
  EXPECT_CALL(*mock_installation_manager_api, GetCurrentInstallationIndex())
      .Times(1)
      .WillOnce(testing::Return(installation_index));
  EXPECT_CALL(
      *mock_installation_manager_api,
      GetInstallationPath(installation_index, testing::_, kSbFileMaxPath))
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::SetArrayArgument<1>(installation_path.begin(),
                                                      installation_path.end()),
                         testing::Return(IM_EXT_SUCCESS)));
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return &kStubInstallationManagerApi; };

  ASSERT_TRUE(SbDirectoryCreate(installation_path.c_str()));
  char manifest_content[] = R"json(
  {
    "manifest_version": 2,
    "name": "Cobalt",
    "description": "Cobalt",
    "version": "1.2.0"
  })json";
  CreateManifest(manifest_content, installation_path);

  std::string version = GetCurrentEvergreenVersion(stub_get_extension_fn);

  ASSERT_EQ(version, "1.2.0");

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest,
       ReturnsDefaultVersionWhenManifestMissingFromCurrentManagedInstallation) {
  std::string installation_path = base::StrCat(
      {temp_dir_path_.data(), kSbFileSepString, "some_installation_path"});
  int installation_index = 1;

  MockInstallationManagerApi* mock_installation_manager_api =
      GetMockInstallationManagerApi();
  EXPECT_CALL(*mock_installation_manager_api, GetCurrentInstallationIndex())
      .Times(1)
      .WillOnce(testing::Return(installation_index));
  EXPECT_CALL(
      *mock_installation_manager_api,
      GetInstallationPath(installation_index, testing::_, kSbFileMaxPath))
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::SetArrayArgument<1>(installation_path.begin(),
                                                      installation_path.end()),
                         testing::Return(IM_EXT_SUCCESS)));
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return &kStubInstallationManagerApi; };

  ASSERT_TRUE(SbDirectoryCreate(installation_path.c_str()));
  // No manifest is created in the installation directory.

  std::string version = GetCurrentEvergreenVersion(stub_get_extension_fn);

  ASSERT_EQ(version, kDefaultManifestVersion);
}

TEST_F(UtilsTest,
       ReturnsVersionFromLoadedInstallationWhenErrorGettingInstallationPath) {
  int installation_index = 1;
  MockInstallationManagerApi* mock_installation_manager_api =
      GetMockInstallationManagerApi();
  EXPECT_CALL(*mock_installation_manager_api, GetCurrentInstallationIndex())
      .Times(1)
      .WillOnce(testing::Return(installation_index));
  EXPECT_CALL(
      *mock_installation_manager_api,
      GetInstallationPath(installation_index, testing::_, kSbFileMaxPath))
      .Times(1)
      .WillOnce(testing::Return(IM_EXT_ERROR));
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return &kStubInstallationManagerApi; };

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

  std::string version = GetCurrentEvergreenVersion(stub_get_extension_fn);

  ASSERT_EQ(version, "1.2.0");

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest,
       ReturnsVersionFromLoadedInstallationWhenErrorGettingInstallationIndex) {
  MockInstallationManagerApi* mock_installation_manager_api =
      GetMockInstallationManagerApi();
  EXPECT_CALL(*mock_installation_manager_api, GetCurrentInstallationIndex())
      .Times(1)
      .WillOnce(testing::Return(IM_EXT_ERROR));
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return &kStubInstallationManagerApi; };

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

  std::string version = GetCurrentEvergreenVersion(stub_get_extension_fn);

  ASSERT_EQ(version, "1.2.0");

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest,
       ReturnsVersionFromLoadedInstallationWhenErrorGettingIMExtension) {
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return nullptr; };

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

  std::string version = GetCurrentEvergreenVersion(stub_get_extension_fn);

  ASSERT_EQ(version, "1.2.0");

  DeleteManifest(installation_path);
}

TEST_F(UtilsTest,
       ReturnsDefaultVersionWhenManifestMissingFromLoadedInstallation) {
  // Two levels of resilience are actually tested...

  // First, the loaded installation is used because an error is encountered
  // while getting the Installation Manager extension. This is similar to
  // previous test cases.
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return nullptr; };

  // And second, kDefaultManifestVersion should be used because no manifest is
  // found in the loaded installation.

  std::string version = GetCurrentEvergreenVersion(stub_get_extension_fn);

  ASSERT_EQ(version, kDefaultManifestVersion);
}

TEST_F(UtilsTest,
       ReturnsEvergreenLibraryMetadataFromCurrentManagedInstallation) {
  std::string installation_path = base::StrCat(
      {temp_dir_path_.data(), kSbFileSepString, "some_installation_path"});
  int installation_index = 1;

  MockInstallationManagerApi* mock_installation_manager_api =
      GetMockInstallationManagerApi();
  EXPECT_CALL(*mock_installation_manager_api, GetCurrentInstallationIndex())
      .Times(1)
      .WillOnce(testing::Return(installation_index));
  EXPECT_CALL(
      *mock_installation_manager_api,
      GetInstallationPath(installation_index, testing::_, kSbFileMaxPath))
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::SetArrayArgument<1>(installation_path.begin(),
                                                      installation_path.end()),
                         testing::Return(IM_EXT_SUCCESS)));
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return &kStubInstallationManagerApi; };

  ASSERT_TRUE(SbDirectoryCreate(installation_path.c_str()));
  char manifest_content[] = R"json(
  {
    "manifest_version": 2,
    "name": "Cobalt",
    "description": "Cobalt",
    "version": "1.2.0"
  })json";
  CreateManifest(manifest_content, installation_path);
  CreateEmptyLibrary("libcobalt.so", installation_path);

  EvergreenLibraryMetadata metadata =
      GetCurrentEvergreenLibraryMetadata(stub_get_extension_fn);

  ASSERT_EQ(metadata.version, "1.2.0");
  ASSERT_EQ(metadata.file_type, "Uncompressed");

  DeleteManifest(installation_path);
  DeleteLibraryDirRecursively(installation_path);
}

TEST_F(UtilsTest,
       ReturnsFileTypeUnknownInMetadataForUnexpectedLibInManagedInstallation) {
  std::string installation_path = base::StrCat(
      {temp_dir_path_.data(), kSbFileSepString, "some_installation_path"});
  int installation_index = 1;

  MockInstallationManagerApi* mock_installation_manager_api =
      GetMockInstallationManagerApi();
  EXPECT_CALL(*mock_installation_manager_api, GetCurrentInstallationIndex())
      .Times(1)
      .WillOnce(testing::Return(installation_index));
  EXPECT_CALL(
      *mock_installation_manager_api,
      GetInstallationPath(installation_index, testing::_, kSbFileMaxPath))
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::SetArrayArgument<1>(installation_path.begin(),
                                                      installation_path.end()),
                         testing::Return(IM_EXT_SUCCESS)));
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return &kStubInstallationManagerApi; };

  ASSERT_TRUE(SbDirectoryCreate(installation_path.c_str()));
  CreateEmptyLibrary("libcobalt.unexpected", installation_path);

  EvergreenLibraryMetadata metadata =
      GetCurrentEvergreenLibraryMetadata(stub_get_extension_fn);

  ASSERT_EQ(metadata.file_type, "FileTypeUnknown");

  DeleteLibraryDirRecursively(installation_path);
}

TEST_F(UtilsTest,
       ReturnsEvergreenLibMetadataFromLoadedInstallationWhenErrorGettingIM) {
  std::function<const void*(const char*)> stub_get_extension_fn =
      [](const char* name) { return nullptr; };

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
  CreateEmptyLibrary("libcobalt.lz4", installation_path);

  EvergreenLibraryMetadata metadata =
      GetCurrentEvergreenLibraryMetadata(stub_get_extension_fn);

  ASSERT_EQ(metadata.version, "1.2.0");
  ASSERT_EQ(metadata.file_type, "Compressed");

  DeleteManifest(installation_path);
  DeleteLibraryDirRecursively(installation_path);
}

}  // namespace
}  // namespace updater
}  // namespace cobalt
