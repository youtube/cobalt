// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/installation_manager.h"

#include <string>
#include <vector>

#include "starboard/loader_app/installation_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= SB_STORAGE_PATH_VERSION && \
    SB_API_VERSION >= SB_FILE_ATOMIC_REPLACE_VERSION

#define NUMBER_INSTALLS_PARAMS ::testing::Values(2, 3, 4, 5, 6)

namespace starboard {
namespace loader_app {
namespace installation_manager {

namespace {

class InstallationManagerTest : public ::testing::TestWithParam<int> {
 protected:
  virtual void SetUp() {
    char buf[SB_FILE_MAX_PATH];
    storage_path_implemented_ =
        SbSystemGetPath(kSbSystemPathStorageDirectory, buf, SB_FILE_MAX_PATH);
    // Short-circuit the tests if the kSbSystemPathStorageDirectory is not
    // implemented.
    if (!storage_path_implemented_) {
      return;
    }
    storage_path_ = buf;
    ASSERT_TRUE(!storage_path_.empty());
    SbDirectoryCreate(storage_path_.c_str());

    installation_store_path_ = storage_path_;
    installation_store_path_ += SB_FILE_SEP_STRING;
    installation_store_path_ += IM_STORE_FILE_NAME;
  }

  void SaveStorageState(
      const cobalt::loader::InstallationStore& installation_store) {
    char buf[IM_MAX_INSTALLATION_STORE_SIZE];
    ASSERT_GT(IM_MAX_INSTALLATION_STORE_SIZE, installation_store.ByteSize());
    installation_store.SerializeToArray(buf, installation_store.ByteSize());

    ASSERT_TRUE(SbFileAtomicReplace(installation_store_path_.c_str(), buf,
                                    installation_store.ByteSize()));
  }

  void ReadStorageState(cobalt::loader::InstallationStore* installation_store) {
    SbFile file;

    file = SbFileOpen(installation_store_path_.c_str(),
                      kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    ASSERT_TRUE(file);

    char buf[IM_MAX_INSTALLATION_STORE_SIZE];
    int count = SbFileReadAll(file, buf, IM_MAX_INSTALLATION_STORE_SIZE);
    SB_DLOG(INFO) << "SbFileReadAll: count=" << count;
    ASSERT_NE(-1, count);
    ASSERT_TRUE(installation_store->ParseFromArray(buf, count));
    SbFileClose(file);
  }

  // Roll forward to |index| installation in a |max_num_installations|
  // configuration. The priorities at each corresponding index are provided in
  // |setup_priorities|. After the transition the new priorities at each
  // corresponding index should match |expected_priorities|.
  void RollForwardHelper(int index,
                         int max_num_installations,
                         int* setup_priorities,
                         int* expected_priorities) {
    cobalt::loader::InstallationStore installation_store;
    int priority = 0;
    for (int i = 0; i < max_num_installations; i++) {
      installation_store.add_installations();
      installation_store.mutable_installations(i)->set_is_successful(false);
      installation_store.mutable_installations(i)->set_num_tries_left(
          IM_MAX_NUM_TRIES);
      installation_store.mutable_installations(i)->set_priority(
          setup_priorities[i]);
    }
    installation_store.set_roll_forward_to_installation(-1);

    SaveStorageState(installation_store);

    ASSERT_EQ(IM_SUCCESS, ImInitialize(max_num_installations));
    ASSERT_EQ(IM_SUCCESS, ImRequestRollForwardToInstallation(index));
    ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
    ASSERT_EQ(index, ImGetCurrentInstallationIndex());
    ImUninitialize();

    ReadStorageState(&installation_store);
    for (int i = 0; i < max_num_installations; i++) {
      ASSERT_EQ(expected_priorities[i],
                installation_store.installations(i).priority());
    }
  }

  // Revert to a previous successful installation in a |max_num_installations|
  // configuration. After the installation is reverted the priorities at each
  // corresponding index should match |expected_priorities|.
  // The |expected_succeed| indicate whether the revert is expected to succeed.
  // For example if there is no successful installation it is impossible to
  // revert. The |priority_success_pairs| have initialization values for
  // priority and is_success states for each corresponding installation.
  template <size_t N>
  void RevertHelper(int max_num_installations,
                    int (*priority_success_pairs)[N],
                    int* expected_priorities,
                    bool expected_succeed) {
    cobalt::loader::InstallationStore installation_store;
    int priority = 0;
    for (int i = 0; i < max_num_installations; i++) {
      installation_store.add_installations();
      installation_store.mutable_installations(i)->set_priority(
          priority_success_pairs[i][0]);
      installation_store.mutable_installations(i)->set_is_successful(
          priority_success_pairs[i][1]);
      installation_store.mutable_installations(i)->set_num_tries_left(
          IM_MAX_NUM_TRIES);
    }
    installation_store.set_roll_forward_to_installation(-1);

    SaveStorageState(installation_store);

    ASSERT_EQ(IM_SUCCESS, ImInitialize(max_num_installations));
    int result = ImRevertToSuccessfulInstallation();
    if (!expected_succeed) {
      ASSERT_EQ(IM_ERROR, result);
    }

    if (expected_succeed) {
      int index = ImGetCurrentInstallationIndex();
      ASSERT_EQ(IM_INSTALLATION_STATUS_SUCCESS, ImGetInstallationStatus(index));
    }

    ImUninitialize();

    if (expected_succeed) {
      ReadStorageState(&installation_store);
      for (int i = 0; i < max_num_installations; i++) {
        ASSERT_EQ(expected_priorities[i],
                  installation_store.installations(i).priority());
      }
    }
  }

  virtual void TearDown() {
    if (!storage_path_implemented_) {
      return;
    }
    ImUninitialize();
    SbDirectory dir = SbDirectoryOpen(storage_path_.c_str(), NULL);
    SbDirectoryEntry dir_entry;
    std::vector<std::string> dir_;

    while (SbDirectoryGetNext(dir, &dir_entry)) {
      std::string full_path = storage_path_;
      full_path += SB_FILE_SEP_STRING;
      full_path += dir_entry.name;
      SbFileDelete(full_path.c_str());
    }
    SbDirectoryClose(dir);
    SbFileDelete(storage_path_.c_str());
  }

 protected:
  std::string storage_path_;
  std::string installation_store_path_;
  bool storage_path_implemented_;
};

}  // namespace

TEST_P(InstallationManagerTest, InitializeMultiple) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  ImUninitialize();
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
}

TEST_P(InstallationManagerTest, DefaultInstallationSuccessful) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  ASSERT_EQ(IM_INSTALLATION_STATUS_NOT_SUCCESS, ImGetInstallationStatus(0));
}

TEST_P(InstallationManagerTest, MarkInstallationSuccessful) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  ASSERT_EQ(IM_INSTALLATION_STATUS_NOT_SUCCESS, ImGetInstallationStatus(0));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(0));
  ASSERT_EQ(IM_INSTALLATION_STATUS_SUCCESS, ImGetInstallationStatus(0));
}

TEST_P(InstallationManagerTest, DecrementInstallationNumTries) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  int max_num_tries = ImGetInstallationNumTriesLeft(0);
  ASSERT_EQ(IM_SUCCESS, ImDecrementInstallationNumTries(0));
  ASSERT_EQ(max_num_tries - 1, ImGetInstallationNumTriesLeft(0));
  int num_tries = max_num_tries - 1;
  while (num_tries--) {
    ASSERT_EQ(IM_SUCCESS, ImDecrementInstallationNumTries(0));
    ASSERT_EQ(num_tries, ImGetInstallationNumTriesLeft(0));
  }
  ASSERT_EQ(IM_ERROR, ImDecrementInstallationNumTries(0));
}

TEST_P(InstallationManagerTest, GetCurrentInstallationIndex) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  ASSERT_EQ(0, ImGetCurrentInstallationIndex());
}

TEST_P(InstallationManagerTest, SelectNewInstallationIndex) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  int index = ImSelectNewInstallationIndex();
  if (GetParam() > 2) {
    for (int i = 0; i < 10; i++) {
      ASSERT_EQ(GetParam() > 2 ? GetParam() - 1 : 1, index);
    }
  }
}

TEST_P(InstallationManagerTest, GetInstallationPath) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  char buf0[SB_FILE_MAX_PATH];
  ASSERT_EQ(IM_SUCCESS, ImGetInstallationPath(0, buf0, SB_FILE_MAX_PATH));
  ASSERT_TRUE(SbFileExists(buf0));
  char buf1[SB_FILE_MAX_PATH];
  ASSERT_EQ(IM_SUCCESS, ImGetInstallationPath(1, buf1, SB_FILE_MAX_PATH));
  ASSERT_TRUE(SbFileExists(buf1));
}

TEST_P(InstallationManagerTest, RollForwardIfNeeded) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
  ASSERT_EQ(0, ImGetCurrentInstallationIndex());
  ASSERT_EQ(IM_SUCCESS, ImRequestRollForwardToInstallation(1));
  ASSERT_EQ(0, ImGetCurrentInstallationIndex());
  ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  if (GetParam() > 2) {
    for (int i = 2; i < GetParam() - 1; i++) {
      ASSERT_EQ(IM_SUCCESS, ImRequestRollForwardToInstallation(i));
      ASSERT_EQ(i - 1, ImGetCurrentInstallationIndex());
      ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
      ASSERT_EQ(i, ImGetCurrentInstallationIndex());
    }
  }
  for (int i = 0; i < 10; i++) {
    int new_index = i % GetParam();
    ASSERT_EQ(IM_SUCCESS, ImRequestRollForwardToInstallation(new_index));
    ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
    ASSERT_EQ(new_index, ImGetCurrentInstallationIndex());
  }
}

TEST_P(InstallationManagerTest, RevertToSuccessfulInstallation) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(GetParam()));
  ASSERT_EQ(0, ImGetCurrentInstallationIndex());
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(0));
  ASSERT_EQ(IM_SUCCESS, ImRequestRollForwardToInstallation(1));
  ASSERT_EQ(0, ImGetCurrentInstallationIndex());
  ASSERT_EQ(IM_SUCCESS, ImRollForwardIfNeeded());
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(1));
  ASSERT_EQ(1, ImGetCurrentInstallationIndex());
  ASSERT_EQ(0, ImRevertToSuccessfulInstallation());
  ASSERT_EQ(0, ImGetCurrentInstallationIndex());
}

TEST_F(InstallationManagerTest, InvalidInput) {
  if (!storage_path_implemented_) {
    return;
  }
  ASSERT_EQ(IM_SUCCESS, ImInitialize(3));
  ASSERT_EQ(IM_INSTALLATION_STATUS_ERROR, ImGetInstallationStatus(10));
  ASSERT_EQ(IM_SUCCESS, ImMarkInstallationSuccessful(0));
  ASSERT_EQ(IM_INSTALLATION_STATUS_ERROR, ImGetInstallationStatus(-2));
  ASSERT_EQ(IM_ERROR, ImRevertToSuccessfulInstallation());
  ASSERT_EQ(IM_ERROR, ImMarkInstallationSuccessful(10));
  ASSERT_EQ(IM_ERROR, ImMarkInstallationSuccessful(-2));
  ASSERT_EQ(IM_ERROR, ImDecrementInstallationNumTries(10));
  ASSERT_EQ(IM_ERROR, ImDecrementInstallationNumTries(-2));

  char buf[SB_FILE_MAX_PATH];
  ASSERT_EQ(IM_ERROR, ImGetInstallationPath(10, buf, SB_FILE_MAX_PATH));
  ASSERT_EQ(IM_ERROR, ImGetInstallationPath(-2, buf, SB_FILE_MAX_PATH));
  ASSERT_EQ(IM_ERROR, ImGetInstallationPath(-2, NULL, SB_FILE_MAX_PATH));
}

TEST_F(InstallationManagerTest, RollForwardMatrix20) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 2;
  int setup_priorities[2][2] = {
      {0, 1}, {1, 0},
  };

  int expected_priorities[2][2] = {
      {0, 1}, {0, 1},
  };

  for (int i = 0; i < 2; i++) {
    SB_LOG(INFO) << "Testing Maxtrix20 expected_priorities at index: " << i;
    RollForwardHelper(0, 2, setup_priorities[i], expected_priorities[i]);
  }
}

TEST_F(InstallationManagerTest, RollForwardMatrix21) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 2;
  int setup_priorities[2][2] = {
      {0, 1}, {1, 0},
  };

  int expected_priorities[2][2] = {
      {1, 0}, {1, 0},
  };

  for (int i = 0; i < 2; i++) {
    SB_LOG(INFO) << "Testing Maxtrix21 expected_priorities at index: " << i;
    RollForwardHelper(1, 2, setup_priorities[i], expected_priorities[i]);
  }
}

TEST_F(InstallationManagerTest, RollForwardMatrix30) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 3;
  int setup_priorities[6][3] = {
      {0, 1, 2}, {0, 2, 1}, {1, 2, 0}, {1, 0, 2}, {2, 0, 1}, {2, 1, 0},
  };

  int expected_priorities[6][3] = {
      {0, 1, 2}, {0, 2, 1}, {0, 2, 1}, {0, 1, 2}, {0, 1, 2}, {0, 2, 1},
  };

  for (int i = 0; i < 6; i++) {
    SB_LOG(INFO) << "Testing Maxtrix30 expected_priorities at index: " << i;
    RollForwardHelper(0, 3, setup_priorities[i], expected_priorities[i]);
  }
}

TEST_F(InstallationManagerTest, RollForwardMatrix31) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 3;
  int setup_priorities[6][3] = {
      {0, 1, 2}, {0, 2, 1}, {1, 2, 0}, {1, 0, 2}, {2, 0, 1}, {2, 1, 0},
  };

  int expected_priorities[6][3] = {
      {1, 0, 2}, {1, 0, 2}, {2, 0, 1}, {1, 0, 2}, {2, 0, 1}, {2, 0, 1},
  };

  for (int i = 0; i < 6; i++) {
    SB_LOG(INFO) << "Testing Maxtrix31 expected_priorities at index: " << i;
    RollForwardHelper(1, 3, setup_priorities[i], expected_priorities[i]);
  }
}

TEST_F(InstallationManagerTest, RollForwardMatrix32) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 3;
  int setup_priorities[6][3] = {
      {0, 1, 2}, {0, 2, 1}, {1, 2, 0}, {1, 0, 2}, {2, 0, 1}, {2, 1, 0},
  };

  int expected_priorities[6][3] = {
      {1, 2, 0}, {1, 2, 0}, {1, 2, 0}, {2, 1, 0}, {2, 1, 0}, {2, 1, 0},
  };
  for (int i = 0; i < 6; i++) {
    SB_LOG(INFO) << "Testing Maxtrix32 expected_priorities at index: " << i;
    RollForwardHelper(2, 3, setup_priorities[i], expected_priorities[i]);
  }
}

TEST_F(InstallationManagerTest, RevertMatrix2) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 2;
  // List of 2 index pairs of (priority, is_success) for which the revert would
  // fail.
  int priority_success_pairs1[4][2][2] = {
      {{0, 1}, {1, 0}}, {{0, 0}, {1, 0}}, {{1, 0}, {0, 1}}, {{1, 0}, {0, 0}},
  };

  // List of 2 index pairs of (priority, is_success) for which the revert would
  // succeed.
  int priority_success_pairs2[4][2][2] = {
      {{0, 0}, {1, 1}}, {{1, 1}, {0, 0}}, {{0, 1}, {1, 1}}, {{1, 1}, {0, 1}},
  };

  int expected_priorities[4][2] = {
      {1, 0}, {0, 1}, {1, 0}, {0, 1},
  };

  for (int i = 0; i < 4; i++) {
    SB_LOG(INFO) << "Testing RevertMaxtrix2 expected_priorities at index: "
                 << i;
    RevertHelper(2, priority_success_pairs1[i], NULL, false);
  }
  for (int i = 0; i < 4; i++) {
    SB_LOG(INFO) << "Testing RevertMaxtrix2 expected_priorities at index: "
                 << i;
    RevertHelper(2, priority_success_pairs2[i], expected_priorities[i], true);
  }
}

TEST_F(InstallationManagerTest, RevertMatrix3) {
  if (!storage_path_implemented_) {
    return;
  }
  int max_num_installations = 3;

  // List of 3 index pairs of (priority, is_success) for which the revert would
  // fail.
  int priority_success_pairs1[12][3][2] = {
      {{0, 0}, {1, 0}, {2, 0}}, {{0, 1}, {1, 0}, {2, 0}},

      {{0, 0}, {2, 0}, {1, 0}}, {{0, 1}, {2, 0}, {1, 0}},

      {{1, 0}, {2, 0}, {0, 0}}, {{1, 0}, {2, 0}, {0, 1}},

      {{1, 0}, {0, 0}, {2, 0}}, {{1, 0}, {0, 1}, {2, 0}},

      {{2, 0}, {0, 0}, {1, 0}}, {{2, 0}, {0, 1}, {1, 0}},

      {{2, 0}, {1, 0}, {0, 0}}, {{2, 0}, {1, 0}, {0, 1}},
  };

  // List of 3 index pairs of (priority, is_success) for which the revert would
  // succeed.
  int priority_success_pairs2[36][3][2] = {
      {{0, 0}, {1, 0}, {2, 1}}, {{0, 0}, {1, 1}, {2, 0}},
      {{0, 0}, {1, 1}, {2, 1}}, {{0, 1}, {1, 1}, {2, 0}},
      {{0, 1}, {1, 1}, {2, 1}}, {{0, 1}, {1, 0}, {2, 1}},

      {{0, 0}, {2, 0}, {1, 1}}, {{0, 0}, {2, 1}, {1, 0}},
      {{0, 0}, {2, 1}, {1, 1}}, {{0, 1}, {2, 1}, {1, 0}},
      {{0, 1}, {2, 0}, {1, 1}}, {{0, 1}, {2, 1}, {1, 1}},

      {{1, 0}, {2, 1}, {0, 0}}, {{1, 1}, {2, 0}, {0, 0}},
      {{1, 0}, {2, 1}, {0, 1}}, {{1, 1}, {2, 1}, {0, 0}},
      {{1, 1}, {2, 0}, {0, 1}}, {{1, 1}, {2, 1}, {0, 1}},

      {{1, 0}, {0, 0}, {2, 1}}, {{1, 1}, {0, 0}, {2, 0}},
      {{1, 0}, {0, 1}, {2, 1}}, {{1, 1}, {0, 1}, {2, 0}},
      {{1, 1}, {0, 0}, {2, 1}}, {{1, 1}, {0, 1}, {2, 1}},

      {{2, 0}, {0, 0}, {1, 1}}, {{2, 1}, {0, 0}, {1, 0}},
      {{2, 0}, {0, 1}, {1, 1}}, {{2, 1}, {0, 1}, {1, 0}},
      {{2, 1}, {0, 0}, {1, 1}}, {{2, 1}, {0, 1}, {1, 1}},

      {{2, 0}, {1, 1}, {0, 0}}, {{2, 1}, {1, 0}, {0, 0}},
      {{2, 0}, {1, 1}, {0, 1}}, {{2, 1}, {1, 1}, {0, 0}},
      {{2, 1}, {1, 0}, {0, 1}}, {{2, 1}, {1, 1}, {0, 1}},
  };

  int expected_priorities[36][3] = {
      {2, 1, 0}, {2, 0, 1}, {2, 0, 1}, {2, 0, 1}, {2, 0, 1}, {2, 1, 0},
      {2, 1, 0}, {2, 0, 1}, {2, 1, 0}, {2, 0, 1}, {2, 1, 0}, {2, 1, 0},
      {1, 0, 2}, {0, 1, 2}, {1, 0, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
      {1, 2, 0}, {0, 2, 1}, {1, 2, 0}, {0, 2, 1}, {0, 2, 1}, {0, 2, 1},
      {1, 2, 0}, {0, 2, 1}, {1, 2, 0}, {0, 2, 1}, {1, 2, 0}, {1, 2, 0},
      {1, 0, 2}, {0, 1, 2}, {1, 0, 2}, {1, 0, 2}, {0, 1, 2}, {1, 0, 2},
  };

  for (int i = 0; i < 12; i++) {
    SB_LOG(INFO) << "Testing RevertMaxtrix3 expected_priorities at index: "
                 << i;
    RevertHelper(3, priority_success_pairs1[i], NULL, false);
  }

  for (int i = 0; i < 36; i++) {
    SB_LOG(INFO) << "Testing RevertMaxtrix3 expected_priorities at index: "
                 << i;
    RevertHelper(3, priority_success_pairs2[i], expected_priorities[i], true);
  }
}
INSTANTIATE_TEST_CASE_P(NumberOfMaxInstallations,
                        InstallationManagerTest,
                        NUMBER_INSTALLS_PARAMS);

}  // namespace installation_manager
}  // namespace loader_app
}  // namespace starboard

#endif  // SB_API_VERSION >= SB_STORAGE_PATH_VERSION && SB_API_VERSION >=
        // SB_FILE_ATOMIC_REPLACE_VERSION
