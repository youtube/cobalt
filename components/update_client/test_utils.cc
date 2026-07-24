// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_utils.h"

#include "build/build_config.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

base::FilePath GetTestFilePath(const char* file_name) {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.Append(FILE_PATH_LITERAL("components"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("update_client"))
      .AppendUTF8(file_name);
}

base::FilePath DuplicateTestFile(const base::FilePath& temp_path,
                                 const char* file) {
  base::FilePath dest_path = temp_path.AppendUTF8(file);
  EXPECT_TRUE(base::CreateDirectory(dest_path.DirName()));
  EXPECT_TRUE(base::PathExists(GetTestFilePath(file)));
  EXPECT_TRUE(base::CopyFile(GetTestFilePath(file), dest_path));
  return dest_path;
}

}  // namespace update_client

#if BUILDFLAG(IS_STARBOARD)
#include "starboard/extension/installation_manager.h"
#include "starboard/system.h"

namespace {

char g_mock_installation_path[512] = "";
bool g_mock_request_roll_forward_success = true;

int MockGetCurrentInstallationIndex() { return 0; }
int MockMarkInstallationSuccessful(int index) { return IM_EXT_SUCCESS; }
int MockRequestRollForwardToInstallation(int index) {
  return g_mock_request_roll_forward_success ? IM_EXT_SUCCESS : IM_EXT_ERROR;
}
int MockGetInstallationPath(int index, char* path, int path_length) {
  if (path_length <= 0) {
    return IM_EXT_ERROR;
  }
  if (strlen(g_mock_installation_path) > 0) {
    strncpy(path, g_mock_installation_path, path_length - 1);
    path[path_length - 1] = '\0';
    return IM_EXT_SUCCESS;
  }
  return IM_EXT_ERROR;
}
int MockSelectNewInstallationIndex() { return 1; }
int MockGetAppKey(char* app_key, int app_key_length) {
  if (app_key_length > 1) {
    app_key[0] = 'A';
    app_key[1] = '\0';
    return IM_EXT_SUCCESS;
  }
  return IM_EXT_ERROR;
}
int MockGetMaxNumberInstallations() { return 2; }
int MockResetInstallation(int index) { return IM_EXT_SUCCESS; }
int MockReset() { return IM_EXT_SUCCESS; }

const CobaltExtensionInstallationManagerApi kMockInstallationManagerApi = {
    kCobaltExtensionInstallationManagerName,
    1,  // API version
    &MockGetCurrentInstallationIndex,
    &MockMarkInstallationSuccessful,
    &MockRequestRollForwardToInstallation,
    &MockGetInstallationPath,
    &MockSelectNewInstallationIndex,
    &MockGetAppKey,
    &MockGetMaxNumberInstallations,
    &MockResetInstallation,
    &MockReset,
};

}  // namespace

extern "C" SB_EXPORT const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionInstallationManagerName) == 0) {
    return &kMockInstallationManagerApi;
  }
  return nullptr;
}

namespace update_client {
void SetMockInstallationPath(const char* path) {
  strncpy(g_mock_installation_path, path, sizeof(g_mock_installation_path) - 1);
  g_mock_installation_path[sizeof(g_mock_installation_path) - 1] = '\0';
}

void SetMockRequestRollForwardSuccess(bool success) {
  g_mock_request_roll_forward_success = success;
}
}  // namespace update_client

#endif  // BUILDFLAG(IS_STARBOARD)
