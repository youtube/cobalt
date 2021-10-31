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

#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
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
  std::vector<char> cache_directory_path(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory,
                       cache_directory_path.data(),
                       kSbFileMaxPath)) {
    LOG(ERROR) << "Couldn't retrieve path to database directory";
    return base::FilePath("");
  }

  std::string crashpad_directory_path(cache_directory_path.data());
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

std::map<std::string, std::string> GetPlatformInfo() {
  std::map<std::string, std::string> platform_info;

  platform_info.insert({"starboard_version",
                        base::StringPrintf("Starboard/%d", SB_API_VERSION)});

  const size_t kSystemPropertyMaxLength = 1024;
  std::vector<char> value(kSystemPropertyMaxLength);
  bool result;

#if SB_API_VERSION >= 12
  result = SbSystemGetProperty(kSbSystemPropertySystemIntegratorName,
                               value.data(),
                               kSystemPropertyMaxLength);
#else
  result = SbSystemGetProperty(kSbSystemPropertyOriginalDesignManufacturerName,
                               value.data(),
                               kSystemPropertyMaxLength);
#endif
  if (result) {
    platform_info.insert({"system_integrator_name", value.data()});
  }

#if defined(STARBOARD_BUILD_TYPE_DEBUG)
  platform_info.insert({"build_configuration", "debug"});
#elif defined(STARBOARD_BUILD_TYPE_DEVEL)
  platform_info.insert({"build_configuration", "devel"});
#elif defined(STARBOARD_BUILD_TYPE_QA)
  platform_info.insert({"build_configuration", "qa"});
#elif defined(STARBOARD_BUILD_TYPE_GOLD)
  platform_info.insert({"build_configuration", "gold"});
#endif

  result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField,
                               value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"aux_field", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber,
                               value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"chipset_model_number", value.data()});
  }

  result = SbSystemGetProperty(
      kSbSystemPropertyModelYear, value.data(), kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"model_year", value.data()});
  }

  result = SbSystemGetProperty(
      kSbSystemPropertyFirmwareVersion, value.data(), kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"firmware_version", value.data()});
  }

  result = SbSystemGetProperty(
      kSbSystemPropertyBrandName, value.data(), kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"brand", value.data()});
  }

  result = SbSystemGetProperty(
      kSbSystemPropertyModelName, value.data(), kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"model", value.data()});
  }

  return platform_info;
}

}  // namespace

void InstallCrashpadHandler() {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();

  const base::FilePath handler_path = GetPathToCrashpadHandlerBinary();
  if (!SbFileExists(handler_path.value().c_str())) {
    LOG(WARNING) << "crashpad_handler not at expected location of "
                 << handler_path.value();
    return;
  }

  const base::FilePath database_directory_path = GetDatabasePath();
  const base::FilePath default_metrics_dir;
  const std::string product_name = GetProductName();
  std::map<std::string, std::string> default_annotations = {
      {"ver", kCrashpadVersion}, {"prod", product_name}};
  const std::vector<std::string> default_arguments = {};

  const std::map<std::string, std::string> platform_info = GetPlatformInfo();
  default_annotations.insert(platform_info.begin(), platform_info.end());

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

bool AddAnnotationsToCrashpad(CrashpadAnnotations annotations) {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();
  return client->SendAnnotationsToHandler(annotations);
}

}  // namespace wrapper
}  // namespace crashpad
}  // namespace third_party
