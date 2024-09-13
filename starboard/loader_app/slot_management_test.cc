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

#include "starboard/loader_app/slot_management.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/elf_loader/sabi_string.h"
#include "starboard/event.h"
#include "starboard/loader_app/app_key_files.h"
#include "starboard/loader_app/drain_file.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/loader_app/installation_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"
#include "third_party/jsoncpp/source/include/json/writer.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace loader_app {
namespace {

const char kTestAppKey[] = "1234";
const char kTestApp2Key[] = "ABCD";
const char kTestEvergreenVersion1[] = "1.2";
const char kTestEvergreenVersion2[] = "1.2.1";
const char kTestEvergreenVersion3[] = "1.2.3";
const char kTestEvergreenVersion4[] = "2.2.3";
const kTestSlotIndex = 0;
// The max length of Evergreen version string.
const int kMaxEgVersionLength = 20;

// Filename for the manifest file which contains the Evergreen version.
const char kManifestFileName[] = "manifest.json";

// Deliminator of the Evergreen version string segments.
const char kEgVersionDeliminator = '.';

// Evergreen version key in the manifest file.
const char kVersionKey[] = "version";

void SbEventFake(const SbEvent*) {}

const char* GetEvergreenSabiStringFake() {
  return "bad";
}

const char* GetCobaltUserAgentStringFake() {
  return "";
}

class MockLibraryLoader : public LibraryLoader {
 public:
  MOCK_METHOD4(Load,
               bool(const std::string& library_path,
                    const std::string& content_path,
                    bool use_compression,
                    bool use_memory_mapped_file));
  MOCK_METHOD1(Resolve, void*(const std::string& symbol));
};

class SlotManagementTest : public testing::TestWithParam<bool> {
 protected:
  virtual void SetUp() {
    slot_0_libcobalt_path_ =
        CreatePath({"content", "app", "cobalt", "lib", "libcobalt"});
    slot_0_content_path_ = CreatePath({"content", "app", "cobalt", "content"});
    slot_1_libcobalt_path_ = CreatePath({"installation_1", "lib", "libcobalt"});
    slot_1_content_path_ = CreatePath({"installation_1", "content"});
    slot_2_libcobalt_path_ = CreatePath({"installation_2", "lib", "libcobalt"});
    slot_2_content_path_ = CreatePath({"installation_2", "content"});

    std::vector<char> buf(kSbFileMaxPath);
    storage_path_implemented_ = SbSystemGetPath(kSbSystemPathStorageDirectory,
                                                buf.data(), kSbFileMaxPath);
  }

  void AddFileExtension(std::string& path) {
    if (GetParam()) {
      path += ".lz4";
    } else {
      path += ".so";
    }
  }

  std::string CreatePath(std::initializer_list<std::string> path_elements) {
    std::string result;
    for (const std::string& path : path_elements) {
      result += kSbFileSepString;
      result += path;
    }
    return result;
  }

  bool FileExists(const char* path) {
    struct stat info;
    return stat(path, &info) == 0;
  }

  void CreateBadFile(int index, const std::string& app_key) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ASSERT_EQ(IM_SUCCESS, ImGetInstallationPath(index, installation_path.data(),
                                                kSbFileMaxPath));
    std::string bad_app_key_file_path =
        starboard::loader_app::GetBadAppKeyFilePath(installation_path.data(),
                                                    app_key);
    ASSERT_TRUE(!bad_app_key_file_path.empty());
    ASSERT_TRUE(starboard::loader_app::CreateAppKeyFile(bad_app_key_file_path));
  }

  void CreateGoodFile(int index, const std::string& app_key) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ASSERT_EQ(IM_SUCCESS, ImGetInstallationPath(index, installation_path.data(),
                                                kSbFileMaxPath));
    std::string good_app_key_file_path =
        starboard::loader_app::GetGoodAppKeyFilePath(installation_path.data(),
                                                     app_key);
    ASSERT_TRUE(!good_app_key_file_path.empty());
    ASSERT_TRUE(
        starboard::loader_app::CreateAppKeyFile(good_app_key_file_path));
  }

  void VerifyGoodFile(int index, const std::string& app_key, bool exists) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ImGetInstallationPath(index, installation_path.data(), kSbFileMaxPath);
    std::string good_app_key_file_path =
        starboard::loader_app::GetGoodAppKeyFilePath(installation_path.data(),
                                                     app_key);
    ASSERT_TRUE(!good_app_key_file_path.empty());
    ASSERT_EQ(exists, FileExists(good_app_key_file_path.c_str()));
  }

  void VerifyBadFile(int index, const std::string& app_key, bool exists) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ImGetInstallationPath(index, installation_path.data(), kSbFileMaxPath);
    std::string bad_app_key_file_path =
        starboard::loader_app::GetBadAppKeyFilePath(installation_path.data(),
                                                    app_key);
    ASSERT_TRUE(!bad_app_key_file_path.empty());
    SB_LOG(INFO) << "bad_app_key_file_path=" << bad_app_key_file_path;
    ASSERT_EQ(exists, FileExists(bad_app_key_file_path.c_str()));
  }

  void CreateDrainFile(int index, const std::string& app_key) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ASSERT_EQ(IM_SUCCESS, ImGetInstallationPath(index, installation_path.data(),
                                                kSbFileMaxPath));
    ASSERT_TRUE(DrainFileTryDrain(installation_path.data(), app_key.c_str()));
  }

  void VerifyLoad(const std::string& lib, const std::string& content) {
    bool use_compression = GetParam();
    MockLibraryLoader library_loader;

    std::string full_lib_path = lib;
    AddFileExtension(full_lib_path);
    EXPECT_CALL(library_loader,
                Load(testing::EndsWith(full_lib_path),
                     testing::EndsWith(content), use_compression, false))
        .Times(1)
        .WillOnce(testing::Return(true));
    EXPECT_CALL(library_loader, Resolve("GetEvergreenSabiString"))
        .Times(1)
        .WillOnce(
            testing::Return(reinterpret_cast<void*>(&GetEvergreenSabiString)));
    EXPECT_CALL(library_loader, Resolve("GetCobaltUserAgentString"))
        .Times(1)
        .WillOnce(testing::Return(
            reinterpret_cast<void*>(&GetCobaltUserAgentStringFake)));
    EXPECT_CALL(library_loader, Resolve("SbEventHandle"))
        .Times(1)
        .WillOnce(testing::Return(reinterpret_cast<void*>(&SbEventFake)));
    ASSERT_EQ(&SbEventFake,
              LoadSlotManagedLibrary(kTestAppKey, "", &library_loader, false));
  }

  std::string CreateDirs(const std::string& base,
                         std::initializer_list<std::string> dirs,
                         std::string& out_top_created_dir) {
    std::string path = base;
    for (const std::string& dir : dirs) {
      path += kSbFileSepString;
      path += dir;
      if (!FileExists(path.c_str())) {
        struct stat info;
        EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
                    (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode)));

        if (out_top_created_dir.empty()) {
          // This new dir should be recursively deleted during cleanup.
          out_top_created_dir = path;
        }
      }
    }
    return path;
  }

  std::string CreateEmptyLibraryFile(const std::string& library_path) {
    std::string path;
    std::string top_created_dir;
    if (library_path == slot_0_libcobalt_path_) {
      // It's the system slot.
      std::vector<char> buf(kSbFileMaxPath);
      SbSystemGetPath(kSbSystemPathContentDirectory, buf.data(),
                      kSbFileMaxPath);
      path = CreateDirs(buf.data(), {"app", "cobalt", "lib"}, top_created_dir);

    } else {
      // It's an installation slot.
      std::vector<char> buf(kSbFileMaxPath);
      SbSystemGetPath(kSbSystemPathStorageDirectory, buf.data(),
                      kSbFileMaxPath);
      path =
          CreateDirs(buf.data(),
                     {library_path == slot_1_libcobalt_path_ ? "installation_1"
                                                             : "installation_2",
                      "lib"},
                     top_created_dir);
    }

    path += kSbFileSepString;
    path += "libcobalt";
    AddFileExtension(path);
    int sb_file = open(path.c_str(), O_CREAT | O_RDONLY);
    EXPECT_TRUE(starboard::IsValid(sb_file));
    close(sb_file);

    return !top_created_dir.empty() ? top_created_dir : path;
  }

 protected:
  std::string slot_0_libcobalt_path_;
  std::string slot_0_content_path_;
  std::string slot_1_libcobalt_path_;
  std::string slot_1_content_path_;
  std::string slot_2_libcobalt_path_;
  std::string slot_2_content_path_;
  bool storage_path_implemented_;
};

TEST_P(SlotManagementTest, SystemSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_0_libcobalt_path_);

  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(0, kTestAppKey, false);
  VerifyBadFile(0, kTestAppKey, false);

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, AdoptSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(1));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());

  VerifyGoodFile(1, kTestAppKey, false);
  CreateGoodFile(1, kTestApp2Key);
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_1_libcobalt_path_);

  VerifyLoad(slot_1_libcobalt_path_, slot_1_content_path_);
  VerifyGoodFile(1, kTestAppKey, true);
  VerifyBadFile(1, kTestAppKey, false);

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, GoodSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(2));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(2));
  ASSERT_EQ(2, ImGetCurrentInstallationIndex());

  CreateGoodFile(2, kTestAppKey);
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_2_libcobalt_path_);

  VerifyLoad(slot_2_libcobalt_path_, slot_2_content_path_);
  VerifyGoodFile(2, kTestAppKey, true);
  VerifyBadFile(2, kTestAppKey, false);

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, NotAdoptSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(2));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(2));
  ASSERT_EQ(2, ImGetCurrentInstallationIndex());

  VerifyGoodFile(2, kTestAppKey, false);
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_0_libcobalt_path_);

  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(2, kTestAppKey, false);
  VerifyBadFile(2, kTestAppKey, true);

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, BadSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(1));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  CreateBadFile(1, kTestAppKey);
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_0_libcobalt_path_);

  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(1, kTestAppKey, false);

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, DrainingSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(1));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  CreateDrainFile(1, kTestApp2Key);
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_0_libcobalt_path_);

  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(1, kTestAppKey, false);
  VerifyBadFile(1, kTestAppKey, false);

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, AlternativeContent) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(1));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  ImUninitialize();

  std::string path = CreateEmptyLibraryFile(slot_0_libcobalt_path_);

  MockLibraryLoader library_loader;

  std::string full_lib_path = slot_0_libcobalt_path_;
  AddFileExtension(full_lib_path);
  EXPECT_CALL(library_loader,
              Load(testing::EndsWith(full_lib_path), testing::EndsWith("/foo"),
                   GetParam(), false))
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(library_loader, Resolve("GetEvergreenSabiString"))
      .Times(1)
      .WillOnce(
          testing::Return(reinterpret_cast<void*>(&GetEvergreenSabiString)));

  EXPECT_CALL(library_loader, Resolve("GetCobaltUserAgentString"))
      .Times(1)
      .WillOnce(testing::Return(
          reinterpret_cast<void*>(&GetCobaltUserAgentStringFake)));
  EXPECT_CALL(library_loader, Resolve("SbEventHandle"))
      .Times(1)
      .WillOnce(testing::Return(reinterpret_cast<void*>(&SbEventFake)));
  ASSERT_EQ(&SbEventFake, LoadSlotManagedLibrary(kTestAppKey, "/foo",
                                                 &library_loader, false));

  SbFileDeleteRecursive(path.c_str(), false);
}

TEST_P(SlotManagementTest, BadSabi) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(1));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  CreateGoodFile(1, kTestAppKey);

  ASSERT_EQ(IM_SUCCESS, ImRollForward(2));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(2));
  ASSERT_EQ(2, ImGetCurrentInstallationIndex());
  CreateGoodFile(2, kTestAppKey);

  ImUninitialize();

  std::string bad_path = CreateEmptyLibraryFile(slot_2_libcobalt_path_);
  std::string good_path = CreateEmptyLibraryFile(slot_1_libcobalt_path_);

  MockLibraryLoader library_loader;

  std::string slot2_libcobalt_full = slot_2_libcobalt_path_;
  AddFileExtension(slot2_libcobalt_full);
  EXPECT_CALL(library_loader,
              Load(testing::EndsWith(slot2_libcobalt_full),
                   testing::EndsWith(slot_2_content_path_), GetParam(), false))
      .Times(1)
      .WillOnce(testing::Return(true));

  EXPECT_CALL(library_loader, Resolve("GetEvergreenSabiString"))
      .Times(2)
      .WillOnce(
          testing::Return(reinterpret_cast<void*>(&GetEvergreenSabiStringFake)))
      .WillOnce(
          testing::Return(reinterpret_cast<void*>(&GetEvergreenSabiString)));

  std::string slot1_libcobalt_full = slot_1_libcobalt_path_;
  AddFileExtension(slot1_libcobalt_full);
  EXPECT_CALL(library_loader,
              Load(testing::EndsWith(slot1_libcobalt_full),
                   testing::EndsWith(slot_1_content_path_), GetParam(), false))
      .Times(1)
      .WillOnce(testing::Return(true));

  EXPECT_CALL(library_loader, Resolve("GetCobaltUserAgentString"))
      .Times(1)
      .WillOnce(testing::Return(
          reinterpret_cast<void*>(&GetCobaltUserAgentStringFake)));

  EXPECT_CALL(library_loader, Resolve("SbEventHandle"))
      .Times(1)
      .WillOnce(testing::Return(reinterpret_cast<void*>(&SbEventFake)));

  ASSERT_EQ(&SbEventFake,
            LoadSlotManagedLibrary(kTestAppKey, "", &library_loader, false));

  SbFileDeleteRecursive(bad_path.c_str(), false);
  SbFileDeleteRecursive(good_path.c_str(), false);
}

TEST_P(SlotManagementTest, CompareEvergreenVersion) {
  if (!storage_path_implemented_) {
    return;
  }
  std::vector<char> v1(kTestEvergreenVersion1,
                       kTestEvergreenVersion1 + strlen(kTestEvergreenVersion1));
  std::vector<char> v2(kTestEvergreenVersion2,
                       kTestEvergreenVersion2 + strlen(kTestEvergreenVersion2));
  std::vector<char> v3(kMaxEgVersionLength);
  ASSERT_EQ(0, CompareEvergreenVersion(&v1, &v3));
  ASSERT_EQ(0, CompareEvergreenVersion(&v1, &v1));
  ASSERT_EQ(-1, CompareEvergreenVersion(&v1, &v2));
  v3.assign(kTestEvergreenVersion3,
            kTestEvergreenVersion3 + strlen(kTestEvergreenVersion3));
  ASSERT_EQ(1, CompareEvergreenVersion(&v3, &v2))
  std::vector<char> v4(kTestEvergreenVersion4,
                       kTestEvergreenVersion4 + strlen(kTestEvergreenVersion4));
  ASSERT_EQ(1, CompareEvergreenVersion(&v4, &v3));
}

TEST_P(SlotManagementTest, ReadEvergreenVersion) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();

  std::vector<char> current_version(kMaxEgVersionLength);
  Json::Value root;
  Json::Value manifest_version;
  manifest_version["manifest_version"] = 2;
  root.append(manifest_version);
  Json::StyledStreamWriter writer;

  std::vector<char> installation_path(kSbFileMaxPath);
  if (ImGetInstallationPath(kTestSlotIndex, installation_path.data(),
                            kSbFileMaxPath) == IM_ERROR) {
    SB_LOG(WARNING) << "Failed to get installation path.";
    return false;
  }
  std::vector<char> test_dir_path(kSbFileMaxPath);
  snprintf(test_dir_path.data(), kSbFileMaxPath, "%s%s%s",
           installation_path.data(), kSbFileSepString, "test_dir", );
  std::vector<char> manifest_file_path(kSbFileMaxPath);
  snprintf(manifest_file_path.data(), kSbFileMaxPath, "%s%s%s",
           test_dir_path.data(), kSbFileSepString, kManifestFileName);

  ScopedFile manifest_file(manifest_file_path.data(), O_RDWR | O_CREAT,
                           S_IRWXU | S_IRWXG);
  std::stringstream manifest_file_s1();
  writer.write(manifest_file_s1, root);
  std::string manifest_file_str1 = manifest_file_s1.str();
  manifest_file.WriteAll(manifest_file_str1.c_str(),
                         manifest_file_str1.length());

  ASSERT_FALSE(ReadEvergreenVersion(&manifest_file_path, current_version.data(),
                                    kMaxEgVersionLength));

  Json::Value evergreen_version;
  evergreen_version[kVersionKey] = kTestEvergreenVersion2;
  root.append(evergreen_version);
  std::stringstream manifest_file_s2();
  writer.write(manifest_file_s2, root);
  std::string manifest_file_str2 = manifest_file_s2.str();
  manifest_file.WriteAll(manifest_file_str2.c_str(),
                         manifest_file_str2.length());

  ASSERT_TRUE(ReadEvergreenVersion(&manifest_file_path, current_version.data(),
                                   kMaxEgVersionLength));
  ASSERT_EQ(kTestEvergreenVersion2, current_version.data());

  ImUninitialize();
  SbFileDeleteRecursive(test_dir_path.data(), false);
}

INSTANTIATE_TEST_CASE_P(SlotManagementTests,
                        SlotManagementTest,
                        ::testing::Bool());

}  // namespace
}  // namespace loader_app
}  // namespace starboard
#endif  // #if SB_IS(EVERGREEN_COMPATIBLE)
