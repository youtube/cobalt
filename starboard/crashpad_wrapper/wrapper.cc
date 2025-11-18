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

#include "starboard/crashpad_wrapper/wrapper.h"

#include <string.h>
#include <sys/stat.h>

#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "starboard/common/log.h"
#include "starboard/common/system_property.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/system.h"
#include "third_party/crashpad/crashpad/snapshot/sanitized/sanitization_information.h"

using starboard::kSystemPropertyMaxLength;

namespace crashpad {

const char kCrashpadVersionKey[] = "ver";
const char kCrashpadProductKey[] = "prod";
const char kCrashpadUserAgentStringKey[] = "user_agent_string";
const char kCrashpadCertScopeKey[] = "cert_scope";

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
  if (!SbSystemGetPath(kSbSystemPathExecutableFile, exe_path.data(),
                       kSbFileMaxPath)) {
    LOG(ERROR) << "Couldn't retrieve path to crashpad_handler binary.";
    return base::FilePath("");
  }
  base::FilePath exe_dir_path = base::FilePath(exe_path.data()).DirName();
  std::string handler_path(exe_dir_path.value());
  handler_path.push_back(kSbFileSepChar);
#if defined(OS_ANDROID)
  // Path to the extracted native library.
  handler_path.append("arm/libcrashpad_handler.so");
#else  // defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
  // TODO: b/406511608 - we probably want to be able to expect the binary to be
  // in the executable directory itself, without the native_target subdir.
  handler_path.append("native_target/crashpad_handler");
#else   // BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
  handler_path.append("crashpad_handler");
#endif  // BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
#endif  // defined(OS_ANDROID)
  return base::FilePath(handler_path.c_str());
}

base::FilePath GetDatabasePath() {
  std::vector<char> cache_directory_path(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_directory_path.data(),
                       kSbFileMaxPath)) {
    LOG(ERROR) << "Couldn't retrieve path to database directory";
    return base::FilePath("");
  }

  std::string crashpad_directory_path(cache_directory_path.data());
  crashpad_directory_path.push_back(kSbFileSepChar);
  crashpad_directory_path.append("crashpad_database");
  struct stat info;
  if (mkdir(crashpad_directory_path.c_str(), 0700) != 0 &&
      !(stat(crashpad_directory_path.c_str(), &info) == 0 &&
        S_ISDIR(info.st_mode))) {
    LOG(ERROR) << "Couldn't create directory for crashpad database";
    return base::FilePath("");
  }

  return base::FilePath(crashpad_directory_path.c_str());
}

bool InitializeCrashpadDatabase(const base::FilePath database_directory_path) {
  std::unique_ptr<::crashpad::CrashReportDatabase> database =
      ::crashpad::CrashReportDatabase::Initialize(database_directory_path);
  if (!database) {
    return false;
  }

  ::crashpad::Settings* settings = database->GetSettings();
  settings->SetUploadsEnabled(true);
  return true;
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

  std::vector<char> value(kSystemPropertyMaxLength);
  bool result;
  result = SbSystemGetProperty(kSbSystemPropertySystemIntegratorName,
                               value.data(), kSystemPropertyMaxLength);
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

  result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField, value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"aux_field", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber,
                               value.data(), kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"chipset_model_number", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyModelYear, value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"model_year", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"firmware_version", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyBrandName, value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"brand", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyModelName, value.data(),
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({"model", value.data()});
  }

  result = SbSystemGetProperty(kSbSystemPropertyCertificationScope,
                               value.data(), kSystemPropertyMaxLength);
  if (result) {
    platform_info.insert({kCrashpadCertScopeKey, value.data()});
  }

  return platform_info;
}

void RecordStatus(CrashpadInstallationStatus status) {
  auto metrics_extension =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));
  if (metrics_extension &&
      strcmp(metrics_extension->name,
             kStarboardExtensionLoaderAppMetricsName) == 0 &&
      metrics_extension->version >= 1) {
    metrics_extension->SetCrashpadInstallationStatus(status);
  }
}

}  // namespace

void InstallCrashpadHandler(const std::string& ca_certificates_path) {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();

  const base::FilePath handler_path = GetPathToCrashpadHandlerBinary();
  struct stat file_info;
  if (stat(handler_path.value().c_str(), &file_info) != 0) {
    LOG(ERROR) << "crashpad_handler not at expected location of "
               << handler_path.value();
    RecordStatus(
        CrashpadInstallationStatus::kFailedCrashpadHandlerBinaryNotFound);
    return;
  }

  const base::FilePath database_directory_path = GetDatabasePath();
  const base::FilePath default_metrics_dir;
  const std::string product_name = GetProductName();
  std::map<std::string, std::string> default_annotations = {
      {kCrashpadVersionKey, kCrashpadVersion},
      {kCrashpadProductKey, product_name}};

  // Without this argument the handler's report upload thread, when the handler
  // is started in response to a crash, will perform its first periodic scan for
  // pending reports before that crash is handled. This scan is not needed -
  // a scan is triggered via CrashReportUploadThread::ReportPending after the
  // crash is handled - and we can simplify the concurrency model and avoid
  // thread contention by skipping it, especially now that upload scans trigger
  // report pruning upon completion.
  const std::vector<std::string> default_arguments = {"--no-periodic-tasks"};

  const std::map<std::string, std::string> platform_info = GetPlatformInfo();
  default_annotations.insert(platform_info.begin(), platform_info.end());

  if (!InitializeCrashpadDatabase(database_directory_path)) {
    LOG(ERROR) << "Failed to initialize Crashpad database";
    RecordStatus(
        CrashpadInstallationStatus::kFailedDatabaseInitializationFailed);

    // As we investigate b/329458881 we may find that it's safe to continue with
    // installation of the Crashpad handler here with the hope that the handler,
    // when it runs, doesn't experience the same failure when initializing the
    // Crashpad database. For now it seems safer to just give up on crash
    // reporting for this particular Cobalt session.
    return;
  }

  client->SetUnhandledSignals({});

  if (!client->StartHandlerAtCrash(handler_path, database_directory_path,
                                   default_metrics_dir, kUploadUrl,
                                   base::FilePath(ca_certificates_path.c_str()),
                                   default_annotations, default_arguments)) {
    LOG(ERROR) << "Failed to install the signal handler";
    RecordStatus(
        CrashpadInstallationStatus::kFailedSignalHandlerInstallationFailed);
    return;
  }

// TODO: b/446933116 - pass sanitization information to the handler.
#if !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
  ::crashpad::SanitizationInformation sanitization_info = {0, 0, 0, 1};
  client->SendSanitizationInformationToHandler(sanitization_info);
#endif  // !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)

  // |InsertCrashpadAnnotation| is injected into the extension implementation
  // to avoid a build dependency from the extension implementation on this
  // wrapper library. Such a dependency would introduce a cycle because this
  // library indirectly depends on //base, and //base depends on //starboard.
  auto crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  if (crash_handler_extension && crash_handler_extension->version >= 3) {
    crash_handler_extension->RegisterSetStringCallback(
        &InsertCrashpadAnnotation);
  }

  RecordStatus(CrashpadInstallationStatus::kSucceeded);
}

bool AddEvergreenInfoToCrashpad(EvergreenInfo evergreen_info) {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();
  return client->SendEvergreenInfoToHandler(evergreen_info);
}

bool InsertCrashpadAnnotation(const char* key, const char* value) {
  ::crashpad::CrashpadClient* client = GetCrashpadClient();
  return client->InsertAnnotationForHandler(key, value);
}

}  // namespace crashpad
