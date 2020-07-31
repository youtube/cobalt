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

#include "third_party/crashpad/wrapper/wrapper.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/system.h"

namespace third_party {
namespace crashpad {
namespace wrapper {

namespace {

// TODO: Get evergreen information from installation.
const std::string kCrashpadVersion = "1.0.0.0";
#if defined(STARBOARD_BUILD_TYPE_GOLD)
const std::string kUploadUrl("https://clients2.google.com/cr/report");
#else
const std::string kUploadUrl("https://clients2.google.com/cr/staging_report");
#endif

::crashpad::CrashpadClient* GetCrashpadClient() {
  static auto* crashpad_client = new ::crashpad::CrashpadClient();
  return crashpad_client;
}

base::FilePath GetPathToCrashpadHandlerBinary() {
  std::vector<char> exe_path(kSbFileMaxPath);
  if (!SbSystemGetPath(
          kSbSystemPathExecutableFile, exe_path.data(), kSbFileMaxPath)) {
    LOG(ERROR) << "Couldn't retrieve path to crashpad_handler binary.";
    return base::FilePath("");
  }
  base::FilePath exe_dir_path = base::FilePath(exe_path.data()).DirName();
  std::string handler_path(exe_dir_path.value());
  handler_path.push_back(kSbFileSepChar);
  handler_path.append("crashpad_handler");
  return base::FilePath(handler_path.c_str());
}

base::FilePath GetDatabasePath() {
  std::vector<char> temp_directory_path(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathTempDirectory,
                       temp_directory_path.data(),
                       kSbFileMaxPath)) {
    LOG(ERROR) << "Couldn't retrieve path to database directory";
    return base::FilePath("");
  }

  std::string crashpad_directory_path(temp_directory_path.data());
  crashpad_directory_path.push_back(kSbFileSepChar);
  crashpad_directory_path.append("crashpad_database");
  if (!SbDirectoryCreate(crashpad_directory_path.c_str())) {
    LOG(ERROR) << "Couldn't create directory for crashpad database";
    return base::FilePath("");
  }

  return base::FilePath(crashpad_directory_path.c_str());
}

void InitializeCrashpadDatabase(const base::FilePath database_directory_path) {
  std::unique_ptr<::crashpad::CrashReportDatabase> database =
      ::crashpad::CrashReportDatabase::Initialize(database_directory_path);
  ::crashpad::Settings* settings = database->GetSettings();
  settings->SetUploadsEnabled(true);
}

std::string GetProductName() {
#if SB_IS(EVERGREEN_COMPATIBLE)
  return "Cobalt_Evergreen";
#else
  return "Cobalt";
#endif
}

}  // namespace

void InstallCrashpadHandler() {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();

  const base::FilePath handler_path = GetPathToCrashpadHandlerBinary();
  const base::FilePath database_directory_path = GetDatabasePath();
  const base::FilePath default_metrics_dir;
  const std::string product_name = GetProductName();
  const std::map<std::string, std::string> default_annotations = {
      {"ver", kCrashpadVersion}, {"prod", product_name}};
  const std::vector<std::string> default_arguments = {};

  InitializeCrashpadDatabase(database_directory_path);
  client->SetUnhandledSignals({});
  client->StartHandler(handler_path,
                       database_directory_path,
                       default_metrics_dir,
                       kUploadUrl,
                       default_annotations,
                       default_arguments,
                       false,
                       false);
}

bool AddEvergreenInfoToCrashpad(EvergreenInfo evergreen_info) {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();
  return client->SendEvergreenInfoToHandler(evergreen_info);
}

}  // namespace wrapper
}  // namespace crashpad
}  // namespace third_party
