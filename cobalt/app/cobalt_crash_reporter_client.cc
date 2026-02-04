// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/app/cobalt_crash_reporter_client.h"

#include <string>

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"  // Needed for GetCacheDirectory
#endif                                // BUILDFLAG(IS_ANDROID)
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/version.h"

namespace {
constexpr char kProductName[] = "Cobalt_ATV";
constexpr char kCrashReportUrl[] = "https://clients2.google.com/cr/report";
}  // namespace

void CobaltCrashReporterClient::Create() {
  static base::NoDestructor<CobaltCrashReporterClient> crash_client;
  crash_reporter::SetCrashReporterClient(crash_client.get());
}

CobaltCrashReporterClient::CobaltCrashReporterClient() = default;

CobaltCrashReporterClient::~CobaltCrashReporterClient() = default;

void CobaltCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  CHECK(product_name);
  CHECK(version);
  *product_name = kProductName;
  *version = COBALT_VERSION;
}

void CobaltCrashReporterClient::GetProductNameAndVersion(
    std::string* product_name,
    std::string* version,
    std::string* channel) {
  *product_name = kProductName;
  *version = COBALT_VERSION;
  *channel = "";  // Or appropriate channel
}

bool CobaltCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
#if BUILDFLAG(IS_ANDROID)
  base::FilePath cache_dir;
  if (!base::android::GetCacheDirectory(&cache_dir)) {
    LOG(ERROR) << "Failed to get cache directory for Crashpad";
    return false;
  }
  *crash_dir = cache_dir.Append("crashpad");
  return true;
#else   // BUILDFLAG(IS_ANDROID)
  // [DUMMY] Provide a default path for non-Android platforms
  LOG(WARNING)
      << "GetCrashDumpLocation not fully implemented for this platform.";
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool CobaltCrashReporterClient::IsRunningUnattended() {
  return false;
}

std::string CobaltCrashReporterClient::GetUploadUrl() {
  return kCrashReportUrl;
}
