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

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "starboard/configuration_constants.h"
#include "starboard/elf_loader/sabi_string.h"
#include "starboard/event.h"
#include "starboard/loader_app/app_key_files.h"
#include "starboard/loader_app/drain_file.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/loader_app/installation_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace loader_app {
namespace {

const char kTestAppKey[] = "1234";
const char kTestApp2Key[] = "ABCD";

void SbEventFake(const SbEvent*) {}

const char* GetEvergreenSabiStringFake() {
  return "bad";
}

const char* GetCobaltUserAgentStringFake() {
  return "";
}

class MockLibraryLoader : public LibraryLoader {
 public:
  MOCK_METHOD2(Load,
               bool(const std::string& library_path,
                    const std::string& content_path));
  MOCK_METHOD1(Resolve, void*(const std::string& symbol));
};

class SlotManagementTest : public testing::Test {
 protected:
  virtual void SetUp() {
    slot_0_libcobalt_path_ =
        CreatePath({"content", "app", "cobalt", "lib", "libcobalt.so.lz4"});
    slot_0_content_path_ = CreatePath({"content", "app", "cobalt", "content"});

    slot_1_libcobalt_path_ =
        CreatePath({"installation_1", "lib", "libcobalt.so.lz4"});
    slot_1_content_path_ = CreatePath({"installation_1", "content"});

    slot_2_libcobalt_path_ =
        CreatePath({"installation_2", "lib", "libcobalt.so.lz4"});
    slot_2_content_path_ = CreatePath({"installation_2", "content"});

    std::vector<char> buf(kSbFileMaxPath);
    storage_path_implemented_ = SbSystemGetPath(kSbSystemPathStorageDirectory,
                                                buf.data(), kSbFileMaxPath);
  }

  std::string CreatePath(std::initializer_list<std::string> path_elements) {
    std::string result;
    for (const std::string& path : path_elements) {
      result += kSbFileSepString;
      result += path;
    }
    return result;
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
    ASSERT_EQ(exists, SbFileExists(good_app_key_file_path.c_str()));
  }

  void VerifyBadFile(int index, const std::string& app_key, bool exists) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ImGetInstallationPath(index, installation_path.data(), kSbFileMaxPath);
    std::string bad_app_key_file_path =
        starboard::loader_app::GetBadAppKeyFilePath(installation_path.data(),
                                                    app_key);
    ASSERT_TRUE(!bad_app_key_file_path.empty());
    SB_LOG(INFO) << "bad_app_key_file_path=" << bad_app_key_file_path;
    ASSERT_EQ(exists, SbFileExists(bad_app_key_file_path.c_str()));
  }

  void CreateDrainFile(int index, const std::string& app_key) {
    std::vector<char> installation_path(kSbFileMaxPath);
    ASSERT_EQ(IM_SUCCESS, ImGetInstallationPath(index, installation_path.data(),
                                                kSbFileMaxPath));
    ASSERT_TRUE(DrainFileTryDrain(installation_path.data(), app_key.c_str()));
  }

  void VerifyLoad(const std::string& lib, const std::string& content) {
    MockLibraryLoader library_loader;

    EXPECT_CALL(library_loader,
                Load(testing::EndsWith(lib), testing::EndsWith(content)))
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
              LoadSlotManagedLibrary(kTestAppKey, "", &library_loader));
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

TEST_F(SlotManagementTest, SystemSlot) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ImUninitialize();
  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(0, kTestAppKey, false);
  VerifyBadFile(0, kTestAppKey, false);
}

TEST_F(SlotManagementTest, AdoptSlot) {
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
  VerifyLoad(slot_1_libcobalt_path_, slot_1_content_path_);
  VerifyGoodFile(1, kTestAppKey, true);
  VerifyBadFile(1, kTestAppKey, false);
}

TEST_F(SlotManagementTest, GoodSlot) {
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
  VerifyLoad(slot_2_libcobalt_path_, slot_2_content_path_);
  VerifyGoodFile(2, kTestAppKey, true);
  VerifyBadFile(2, kTestAppKey, false);
}

TEST_F(SlotManagementTest, NotAdoptSlot) {
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
  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(2, kTestAppKey, false);
  VerifyBadFile(2, kTestAppKey, true);
}

TEST_F(SlotManagementTest, BadSlot) {
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
  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(1, kTestAppKey, false);
}

TEST_F(SlotManagementTest, DrainingSlot) {
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
  VerifyLoad(slot_0_libcobalt_path_, slot_0_content_path_);
  VerifyGoodFile(1, kTestAppKey, false);
  VerifyBadFile(1, kTestAppKey, true);
}

TEST_F(SlotManagementTest, AlternativeContent) {
  if (!storage_path_implemented_) {
    return;
  }
  ImInitialize(3, kTestAppKey);
  ImReset();
  ASSERT_EQ(IM_SUCCESS, ImRollForward(1));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  ImUninitialize();

  MockLibraryLoader library_loader;

  EXPECT_CALL(library_loader, Load(testing::EndsWith(slot_0_libcobalt_path_),
                                   testing::EndsWith("/foo")))
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
            LoadSlotManagedLibrary(kTestAppKey, "/foo", &library_loader));
}

TEST_F(SlotManagementTest, BadSabi) {
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

  MockLibraryLoader library_loader;

  EXPECT_CALL(library_loader, Load(testing::EndsWith(slot_2_libcobalt_path_),
                                   testing::EndsWith(slot_2_content_path_)))
      .Times(1)
      .WillOnce(testing::Return(true));

  EXPECT_CALL(library_loader, Resolve("GetEvergreenSabiString"))
      .Times(2)
      .WillOnce(
          testing::Return(reinterpret_cast<void*>(&GetEvergreenSabiStringFake)))
      .WillOnce(
          testing::Return(reinterpret_cast<void*>(&GetEvergreenSabiString)));

  EXPECT_CALL(library_loader, Load(testing::EndsWith(slot_1_libcobalt_path_),
                                   testing::EndsWith(slot_1_content_path_)))
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
            LoadSlotManagedLibrary(kTestAppKey, "", &library_loader));
}

}  // namespace
}  // namespace loader_app
}  // namespace starboard
#endif  // #if SB_IS(EVERGREEN_COMPATIBLE)
