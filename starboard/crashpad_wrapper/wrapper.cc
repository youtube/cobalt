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
#include <optional>
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
#include "starboard/extension/native_stability.h"
#include "starboard/system.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/snapshot/sanitized/sanitization_information.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"
#include "util/misc/capture_context.h"

using starboard::kSystemPropertyMaxLength;

namespace crashpad {

const char kCrashpadVersionKey[] = "ver";
const char kCrashpadProductKey[] = "prod";
const char kCrashpadUserAgentStringKey[] = "user_agent_string";
const char kCrashpadCertScopeKey[] = "cert_scope";
const char kNativeStabilityCrashUuidKey[] = "native_stability_crash_uuid";
const char kNativeStabilityHangUuidKey[] = "native_stability_hang_uuid";

namespace {

base::FilePath* g_database_path_override_for_testing = nullptr;

// TODO: Get evergreen information from installation.
const std::string kCrashpadVersion = "1.0.0.0";
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
const std::string kUploadUrl("https://clients2.google.com/cr/report");
#else
const std::string kUploadUrl("https://clients2.google.com/cr/staging_report");
#endif

static constexpr ::crashpad::SanitizationInformation kSanitizationInfo = {
    /*allowed_annotations_address=*/0,
    /*target_module_address=*/0,
    /*allowed_memory_ranges_address=*/0,
    /*sanitize_stacks=*/1};

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
#else   // defined(OS_ANDROID)
  handler_path.append("native_target/crashpad_handler");
#endif  // defined(OS_ANDROID)
  return base::FilePath(handler_path.c_str());
}

base::FilePath GetDatabasePath() {
  if (g_database_path_override_for_testing) {
    return *g_database_path_override_for_testing;
  }

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
#if BUILDFLAG(IS_STARBOARD)
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

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  platform_info.insert({"build_configuration", "gold"});
#elif defined(OFFICIAL_BUILD)
  platform_info.insert({"build_configuration", "qa"});
#elif defined(NDEBUG)
  platform_info.insert({"build_configuration", "devel"});
#else
  // Note: |debug| builds will incorrectly be caught by the previous condition
  // until the todo comment in cobalt/build/gn.py, attached to b/423038377, is
  // addressed.
  platform_info.insert({"build_configuration", "debug"});
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

std::optional<SbNativeStabilityReport> ParseReportFromMinidump(
    const ::crashpad::CrashReportDatabase::Report& report) {
  ::crashpad::FileReader reader;
  if (!reader.Open(report.file_path)) {
    return std::nullopt;
  }

  ::crashpad::ProcessSnapshotMinidump snapshot;
  if (!snapshot.Initialize(&reader)) {
    return std::nullopt;
  }

  const std::map<std::string, std::string>& annotations =
      snapshot.AnnotationsSimpleMap();
  const ::crashpad::ExceptionSnapshot* exception = snapshot.Exception();
  // Non-crashing minidumps captured for hangs via DumpWithoutCrashing have the
  // exception code set to kSimulatedSigno, distinguishing them from actual
  // signal-driven process crashes.
  bool is_hang = exception && exception->Exception() ==
                                  static_cast<uint32_t>(
                                      ::crashpad::Signals::kSimulatedSigno);

  std::string event_uuid;
  SbNativeStabilityReportType report_type = kSbNativeStabilityReportUnknown;
  if (is_hang) {
    report_type = kSbNativeStabilityReportHang;
    auto hang_uuid_it = annotations.find(kNativeStabilityHangUuidKey);
    if (hang_uuid_it != annotations.end()) {
      event_uuid = hang_uuid_it->second;
    }
  } else {
    report_type = kSbNativeStabilityReportCrash;
    auto crash_uuid_it = annotations.find(kNativeStabilityCrashUuidKey);
    if (crash_uuid_it != annotations.end()) {
      event_uuid = crash_uuid_it->second;
    }
  }

  constexpr size_t kExpectedUuidLength =
      sizeof(SbNativeStabilityReport::native_stability_event_uuid) - 1;
  if (event_uuid.size() != kExpectedUuidLength) {
    return std::nullopt;
  }

  timeval snapshot_time{};
  snapshot.SnapshotTime(&snapshot_time);

  SbNativeStabilityReport native_stability_report{};
  native_stability_report.event_time_s =
      static_cast<int64_t>(snapshot_time.tv_sec);
  native_stability_report.report_type = report_type;

  memcpy(native_stability_report.native_stability_event_uuid,
         event_uuid.c_str(), kExpectedUuidLength);
  native_stability_report.native_stability_event_uuid[kExpectedUuidLength] =
      '\0';
  return native_stability_report;
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

  const std::vector<std::string> default_arguments = {
      // Without this argument the handler's report upload thread, when the
      // handler is started in response to a crash, will perform its first
      // periodic scan for pending reports before that crash is handled. This
      // scan is not needed - a scan is triggered via
      // CrashReportUploadThread::ReportPending after the crash is handled - and
      // we can simplify the concurrency model and avoid thread contention by
      // skipping it, especially now that upload scans trigger report pruning
      // upon completion.
      "--no-periodic-tasks",
      base::StringPrintf("--sanitization-information=%p", &kSanitizationInfo)};

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

  // |InsertCrashpadAnnotation| is injected into the extension implementation
  // to avoid a build dependency from the extension implementation on this
  // wrapper library. Such a dependency would introduce a cycle because this
  // library indirectly depends on //base, and //base depends on //starboard.
  auto crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  if (crash_handler_extension && crash_handler_extension->version >= 3 &&
      crash_handler_extension->RegisterSetStringCallback) {
    crash_handler_extension->RegisterSetStringCallback(
        &InsertCrashpadAnnotation);
  }
  if (crash_handler_extension && crash_handler_extension->version >= 4 &&
      crash_handler_extension->RegisterDumpWithoutCrashingCallback) {
    crash_handler_extension->RegisterDumpWithoutCrashingCallback(
        &DumpWithoutCrashingWrapper);
  }

  auto native_stability_extension =
      static_cast<const CobaltExtensionNativeStabilityApi*>(
          SbSystemGetExtension(kCobaltExtensionNativeStabilityName));
  if (native_stability_extension && native_stability_extension->version >= 1 &&
      native_stability_extension->RegisterReadReportsCallback) {
    native_stability_extension->RegisterReadReportsCallback(&ReadReports);
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

void DumpWithoutCrashingWrapper() {
  ::crashpad::NativeCPUContext context;
  ::crashpad::CaptureContext(&context);
  ::crashpad::CrashpadClient::DumpWithoutCrash(&context);
}

int ReadReports(SbNativeStabilityReport* reports, int max_num_reports) {
  const base::FilePath database_directory_path = GetDatabasePath();
  if (!reports || max_num_reports <= 0 || database_directory_path.empty()) {
    return -1;
  }

  std::unique_ptr<::crashpad::CrashReportDatabase> database =
      ::crashpad::CrashReportDatabase::Initialize(database_directory_path);
  if (!database) {
    return -1;
  }

  std::vector<::crashpad::CrashReportDatabase::Report> all_reports;

  // We generally expect to find zero pending reports and certainly don't expect
  // to find many: just after the Crashpad handler snapshots a crash, it
  // attempts to upload the report - or declines to do so because of client-side
  // throttling - and in all cases, including throttled or failed uploads, then
  // moves the report from |pending| to |completed|.
  std::vector<::crashpad::CrashReportDatabase::Report> pending_reports;
  if (database->GetPendingReports(&pending_reports) ==
      ::crashpad::CrashReportDatabase::kNoError) {
    all_reports.insert(all_reports.end(), pending_reports.begin(),
                       pending_reports.end());
  }

  std::vector<::crashpad::CrashReportDatabase::Report> completed_reports;
  if (database->GetCompletedReports(&completed_reports) ==
      ::crashpad::CrashReportDatabase::kNoError) {
    all_reports.insert(all_reports.end(), completed_reports.begin(),
                       completed_reports.end());
  }

  int count = 0;
  for (const auto& report : all_reports) {
    if (count >= max_num_reports) {
      break;
    }
    std::optional<SbNativeStabilityReport> sb_native_stability_report =
        ParseReportFromMinidump(report);
    if (sb_native_stability_report.has_value()) {
      reports[count++] = sb_native_stability_report.value();
    }
  }

  return count;
}

namespace internal {

void SetDatabasePathForTesting(const base::FilePath& path) {
  delete g_database_path_override_for_testing;
  if (path.empty()) {
    g_database_path_override_for_testing = nullptr;
  } else {
    g_database_path_override_for_testing = new base::FilePath(path);
  }
}

}  // namespace internal

}  // namespace crashpad
