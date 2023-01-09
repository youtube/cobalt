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

#include "components/update_client/cobalt_slot_management.h"

#include <algorithm>
#include <vector>

#include "base/strings/string_util.h"
#include "starboard/common/file.h"
#include "starboard/extension/free_space.h"
#include "starboard/loader_app/app_key_files.h"
#include "starboard/loader_app/drain_file.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/loader_app/system_get_extension_shim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {
namespace {

constexpr char kTestAppKey1[] = "test_key1";
constexpr char kTestAppKey2[] = "test_key2";
constexpr char kManifestV1[] = R"json({
  "manifest_version": 2,
  "name": "Cobalt",
  "description": "Cobalt",
  "version": "1.1.0"
})json";

constexpr char kManifestV2[] = R"json(
{
  "manifest_version": 2,
  "name": "Cobalt",
  "description": "Cobalt",
  "version": "1.2.0"
})json";

class CobaltSlotManagementTest : public testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> buf(kSbFileMaxPath);
    storage_path_implemented_ = SbSystemGetPath(kSbSystemPathStorageDirectory,
                                                buf.data(), kSbFileMaxPath);
    if (!storage_path_implemented_) {
      return;
    }

    storage_path_ = buf.data();
    ASSERT_TRUE(!storage_path_.empty());

    starboard::SbFileDeleteRecursive(storage_path_.c_str(), true);
    ImInitialize(3, kTestAppKey1);
    api_ = static_cast<const CobaltExtensionInstallationManagerApi*>(
        starboard::loader_app::SbSystemGetExtensionShim(
            kCobaltExtensionInstallationManagerName));
  }

  void TearDown() override {
    starboard::SbFileDeleteRecursive(storage_path_.c_str(), true);
    ImUninitialize();
  }

  void CreateManifest(const char* slot_path,
                      const char* data,
                      size_t data_length) {
    std::string manifest1_path = storage_path_;
    manifest1_path += kSbFileSepString;
    manifest1_path += slot_path;
    manifest1_path += kSbFileSepString;
    manifest1_path += "manifest.json";

    ASSERT_TRUE(SbFileAtomicReplace(manifest1_path.c_str(), data, data_length));
  }

  const CobaltExtensionInstallationManagerApi* api_;
  bool storage_path_implemented_;
  std::string storage_path_;
};

TEST_F(CobaltSlotManagementTest, Init) {
  if (!storage_path_implemented_) {
    return;
  }
  CobaltSlotManagement cobalt_slot_management;
  ASSERT_TRUE(cobalt_slot_management.Init(api_));
}

TEST_F(CobaltSlotManagementTest, NegativeInit) {
  if (!storage_path_implemented_) {
    return;
  }
  CobaltSlotManagement cobalt_slot_management;
  ASSERT_FALSE(cobalt_slot_management.Init(nullptr));
}

TEST_F(CobaltSlotManagementTest, SelectEmptySlot) {
  if (!storage_path_implemented_) {
    return;
  }
  CobaltSlotManagement cobalt_slot_management;
  ASSERT_TRUE(cobalt_slot_management.Init(api_));
  base::FilePath dir;
  ASSERT_TRUE(cobalt_slot_management.SelectSlot(&dir));
  ASSERT_TRUE(DrainFileIsAppDraining(dir.value().c_str(), kTestAppKey1));
  ASSERT_TRUE(base::EndsWith(dir.value(), "installation_1",
                             base::CompareCase::SENSITIVE));
}

TEST_F(CobaltSlotManagementTest, SelectSlotBailOnDraining) {
  if (!storage_path_implemented_) {
    return;
  }

  CobaltSlotManagement cobalt_slot_management;
  std::string slot_path = storage_path_;
  slot_path += kSbFileSepString;
  slot_path += "installation_1";

  // If there is is non-expired drain file from
  // different app the current app should bail out.
  ASSERT_TRUE(DrainFileTryDrain(slot_path.c_str(), kTestAppKey2));
  ASSERT_TRUE(cobalt_slot_management.Init(api_));

  base::FilePath dir;
  ASSERT_FALSE(cobalt_slot_management.SelectSlot(&dir));
}

TEST_F(CobaltSlotManagementTest, SelectMinVersionSlot) {
  if (!storage_path_implemented_) {
    return;
  }

  // In slot 2 create manifest v1.
  CreateManifest("installation_2", kManifestV1, strlen(kManifestV1));

  // In slot 1 create manifest v2.
  CreateManifest("installation_1", kManifestV2, strlen(kManifestV2));

  CobaltSlotManagement cobalt_slot_management;
  ASSERT_TRUE(cobalt_slot_management.Init(api_));
  base::FilePath dir;
  cobalt_slot_management.SelectSlot(&dir);
  ASSERT_TRUE(DrainFileIsAppDraining(dir.value().c_str(), kTestAppKey1));
  LOG(INFO) << "dir=" << dir;

  ASSERT_TRUE(base::EndsWith(dir.value(), "installation_2",
                             base::CompareCase::SENSITIVE));
}

TEST_F(CobaltSlotManagementTest, ConfirmSlot) {
  if (!storage_path_implemented_) {
    return;
  }

  ImRollForward(1);
  ImDecrementInstallationNumTries(1);
  ASSERT_LE(ImGetInstallationNumTriesLeft(1), IM_MAX_NUM_TRIES);
  ImMarkInstallationSuccessful(1);
  ASSERT_EQ(IM_INSTALLATION_STATUS_SUCCESS, ImGetInstallationStatus(1));
  CobaltSlotManagement cobalt_slot_management;
  ASSERT_TRUE(cobalt_slot_management.Init(api_));
  base::FilePath dir;
  ASSERT_TRUE(cobalt_slot_management.SelectSlot(&dir));
  LOG(INFO) << "dir=" << dir;

  ASSERT_TRUE(base::EndsWith(dir.value(), "installation_1",
                             base::CompareCase::SENSITIVE));

  ASSERT_TRUE(DrainFileIsAppDraining(dir.value().c_str(), kTestAppKey1));

  ASSERT_TRUE(cobalt_slot_management.ConfirmSlot(dir));

  ASSERT_EQ(IM_INSTALLATION_STATUS_NOT_SUCCESS, ImGetInstallationStatus(1));
  ASSERT_EQ(IM_MAX_NUM_TRIES, ImGetInstallationNumTriesLeft(1));
}

TEST_F(CobaltSlotManagementTest, CleanupAllDrainFiles) {
  if (!storage_path_implemented_) {
    return;
  }
  CobaltSlotManagement cobalt_slot_management;
  ASSERT_TRUE(cobalt_slot_management.Init(api_));
  base::FilePath dir;
  ASSERT_TRUE(cobalt_slot_management.SelectSlot(&dir));
  ASSERT_TRUE(DrainFileIsAppDraining(dir.value().c_str(), kTestAppKey1));
  cobalt_slot_management.CleanupAllDrainFiles();
  ASSERT_FALSE(DrainFileIsAppDraining(dir.value().c_str(), kTestAppKey1));
}

TEST_F(CobaltSlotManagementTest, CobaltFinishInstallation) {
  std::string slot_path = storage_path_;
  slot_path += kSbFileSepString;
  slot_path += "installation_1";
  std::string good_file_path =
      starboard::loader_app::GetGoodAppKeyFilePath(slot_path, kTestAppKey1);

  ImRollForward(2);

  // Cleanup pending requests for roll forward.
  ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());

  ASSERT_EQ(2, ImGetCurrentInstallationIndex());
  ASSERT_FALSE(SbFileExists(good_file_path.c_str()));
  ASSERT_TRUE(CobaltFinishInstallation(api_, 1, slot_path, kTestAppKey1));
  ASSERT_TRUE(SbFileExists(good_file_path.c_str()));
  ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
}

TEST_F(CobaltSlotManagementTest, GoodCobaltQuickUpdate) {
  // In slot 1 create manifest v1.
  CreateManifest("installation_1", kManifestV1, strlen(kManifestV1));

  // In slot 2 create manifest v2.
  CreateManifest("installation_2", kManifestV2, strlen(kManifestV2));

  // Mark slot 2 good for app 2.
  std::string slot_path = storage_path_;
  slot_path += kSbFileSepString;
  slot_path += "installation_2";
  std::string good_file_path =
      starboard::loader_app::GetGoodAppKeyFilePath(slot_path, kTestAppKey2);
  starboard::loader_app::CreateAppKeyFile(good_file_path);
  base::Version version("1.1.0");
  ASSERT_TRUE(CobaltQuickUpdate(api_, version));
}

TEST_F(CobaltSlotManagementTest, NegativeCobaltQuickUpdateBadVersion) {
  base::Version version;
  ASSERT_FALSE(CobaltQuickUpdate(api_, version));
}

TEST_F(CobaltSlotManagementTest, NegativeCobaltQuickUpdate) {
  base::Version version("1.0.0");
  ASSERT_FALSE(CobaltQuickUpdate(api_, version));
}

TEST_F(CobaltSlotManagementTest, CobaltSkipUpdateNoInstallationMoreSpace) {
  ASSERT_FALSE(
      CobaltSkipUpdate(api_, 1024 /* min */, 1025 /* free */, 0 /* cleanup */));
}

TEST_F(CobaltSlotManagementTest, CobaltSkipUpdateNoInstallationExactSpace) {
  ASSERT_FALSE(
      CobaltSkipUpdate(api_, 1024 /* min */, 1024 /* free */, 0 /* cleanup */));
}

TEST_F(CobaltSlotManagementTest, CobaltSkipUpdateNoInstallationNotEnoughSpace) {
  ASSERT_TRUE(
      CobaltSkipUpdate(api_, 1024 /* min */, 1023 /* free */, 0 /* cleanup */));
}

TEST_F(CobaltSlotManagementTest, CobaltSkipUpdateWithInstallationMoreSpace) {
  ASSERT_FALSE(
      CobaltSkipUpdate(api_, 1024 /* min */, 1024 /* free */, 1 /* cleanup */));
}

TEST_F(CobaltSlotManagementTest, CobaltSkipUpdateWithInstallationExactSpace) {
  ASSERT_FALSE(
      CobaltSkipUpdate(api_, 1024 /* min */, 1023 /* free */, 1 /* cleanup */));
}

TEST_F(CobaltSlotManagementTest,
       CobaltSkipUpdateWithInstallationNotEnoughSpace) {
  ASSERT_TRUE(
      CobaltSkipUpdate(api_, 1024 /* min */, 1022 /* free */, 1 /* cleanup */));
}

TEST_F(CobaltSlotManagementTest, CobaltInstallationCleanupSizeNoInstallation) {
  uint64_t size = CobaltInstallationCleanupSize(api_);
  ASSERT_EQ(size, 0);
}

TEST_F(CobaltSlotManagementTest,
       CobaltInstallationCleanupSizeTwoInstallations) {
  int len1 = strlen(kManifestV1);
  int len2 = strlen(kManifestV2);
  ASSERT_NE(len1, len2);

  CreateManifest("installation_1", kManifestV2, len1);
  CreateManifest("installation_2", kManifestV1, len2);

  uint64_t size = CobaltInstallationCleanupSize(api_);
  ASSERT_EQ(size, std::min(len1, len2));
}
}  // namespace

}  // namespace update_client
