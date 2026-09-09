// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "crypto/hash.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_STARBOARD)
#include <map>
#include "base/no_destructor.h"
#endif

namespace update_client {

const char kArchAmd64[] = "x86_64";
const char kArchIntel[] = "x86";
const char kArchArm64[] = "arm64";

#if BUILDFLAG(IS_CHROMEOS)
// In ChromeOS, /tmp is a ramfs drive that can be too small
// for large downloads like Gemini Nano2v3. A larger tmpfiles.d
// mount has been created (see https://crrev.com/c/6810025) as a
// scratch space with access to the full stateful partition to
// handle these larger downloads.
const char kTempDir[] = "/var/lib/odml/chrome_component_updater";
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsHttpServerError(int status_code) {
  return 500 <= status_code && status_code < 600;
}

bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath) {
  if (!RetryFileOperation(&base::DeleteFile, filepath)) {
    return false;
  }

  return DeleteEmptyDirectory(filepath.DirName());
}

bool DeleteEmptyDirectory(const base::FilePath& dir_path) {
  if (!base::IsDirectoryEmpty(dir_path)) {
    return true;
  }

  return RetryFileOperation(&base::DeleteFile, dir_path);
}

std::string GetCrxComponentID(const CrxComponent& component) {
  return component.app_id.empty() ? GetCrxIdFromPublicKeyHash(component.pk_hash)
                                  : component.app_id;
}

std::string GetCrxIdFromPublicKeyHash(base::span<const uint8_t> pk_hash) {
  const std::string result = crx_file::id_util::GenerateIdFromHash(pk_hash);
  CHECK(crx_file::id_util::IdIsValid(result));
  return result;
}

#if BUILDFLAG(IS_STARBOARD)
#if defined(IN_MEMORY_UPDATES)
bool VerifyHash256(const std::string* content,
                   const std::string& expected_hash_str) {
  std::array<uint8_t, crypto::hash::kSha256Size> expected_hash;
  if (!base::HexStringToSpan(expected_hash_str, expected_hash)) {
    return false;
  }

  std::array<uint8_t, crypto::hash::kSha256Size> hash;
  crypto::hash::Hash(crypto::hash::kSha256, *content, hash);

  return base::span(hash) == base::span(expected_hash);
}
#else  // defined(IN_MEMORY_UPDATES)
bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::array<uint8_t, crypto::hash::kSha256Size> expected_hash;
  if (!base::HexStringToSpan(expected_hash_str, expected_hash)) {
    return false;
  }

  base::File source_file(filepath,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!source_file.IsValid()) {
    DPLOG(ERROR) << "VerifyFileHash256(): Unable to open source file: "
                 << filepath.value();
    return false;
  }

  std::array<uint8_t, crypto::hash::kSha256Size> hash;
  if (!crypto::hash::HashFile(crypto::hash::kSha256, &source_file, hash)) {
    return false;
  }

  return base::span(hash) == base::span(expected_hash);
}
#endif  // defined(IN_MEMORY_UPDATES)

base::Version ReadEvergreenVersion(base::FilePath installation_dir) {
  auto manifest = ReadManifest(installation_dir);
  if (manifest) {
    auto version = manifest->Find("version");
    if (version) {
      return base::Version(version->GetString());
    }
  }
  LOG(WARNING) << "ReadEvergreenVersion: unable to read version from "
               << installation_dir.value();
  return base::Version();
}

const std::map<ComponentState, UpdaterStatus>& GetComponentToUpdaterStatusMap() {
  static const base::NoDestructor<std::map<ComponentState, UpdaterStatus>> map({
      {ComponentState::kNew, UpdaterStatus::kNewUpdate},
      {ComponentState::kChecking, UpdaterStatus::kChecking},
      {ComponentState::kCanUpdate, UpdaterStatus::kUpdateAvailable},
      {ComponentState::kDownloading, UpdaterStatus::kDownloading},
      {ComponentState::kDecompressing, UpdaterStatus::kDecompressing},
      {ComponentState::kPatching, UpdaterStatus::kPatching},
      {ComponentState::kUpdating, UpdaterStatus::kUpdating},
      {ComponentState::kUpdated, UpdaterStatus::kUpdated},
      {ComponentState::kUpToDate, UpdaterStatus::kUpToDate},
      {ComponentState::kUpdateError, UpdaterStatus::kUpdateError},
      {ComponentState::kRun, UpdaterStatus::kRun},
  });
  return *map;
}

const std::map<UpdaterStatus, const char*>& GetUpdaterStatusStringMap() {
  static const base::NoDestructor<std::map<UpdaterStatus, const char*>> map({
      {UpdaterStatus::kNewUpdate, "Will check for update soon"},
      {UpdaterStatus::kChecking, "Checking for update"},
      {UpdaterStatus::kUpdateAvailable, "Update is available"},
      {UpdaterStatus::kDownloading, "Downloading update"},
      {UpdaterStatus::kSlotLocked, "Slot is locked"},
      {UpdaterStatus::kDownloaded, "Update is downloaded"},
      {UpdaterStatus::kDecompressing, "Update is being decompressed"},
      {UpdaterStatus::kPatching, "Patch is being applied"},
      {UpdaterStatus::kUpdating, "Installing update"},
      {UpdaterStatus::kUpdated, "Update installed, pending restart"},
      {UpdaterStatus::kRolledForward, "Updated locally, pending restart"},
      {UpdaterStatus::kUpToDate, "App is up to date"},
      {UpdaterStatus::kUpdateError, "Failed to update"},
      {UpdaterStatus::kUninstalled, "Update uninstalled"},
      {UpdaterStatus::kRun, "Transitioning..."},
  });
  return *map;
}
#else  // BUILDFLAG(IS_STARBOARD)

bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::array<uint8_t, crypto::hash::kSha256Size> expected_hash;
  if (!base::HexStringToSpan(expected_hash_str, expected_hash)) {
    return false;
  }

  base::File file(filepath, base::File::FLAG_OPEN |
                                base::File::FLAG_WIN_SEQUENTIAL_SCAN |
                                base::File::FLAG_READ);
  if (!file.IsValid()) {
    return false;
  }

  std::array<uint8_t, crypto::hash::kSha256Size> hash;
  if (!crypto::hash::HashFile(crypto::hash::kSha256, &file, hash)) {
    return false;
  }

  return base::span(hash) == base::span(expected_hash);
}

#endif  // BUILDFLAG(IS_STARBOARD)

bool IsValidBrand(const std::string& brand) {
  const size_t kMaxBrandSize = 4;
  return brand.empty() ||
         (brand.size() == kMaxBrandSize &&
          std::ranges::all_of(brand, &base::IsAsciiAlpha<char>));
}

// Helper function.
// Returns true if |part| matches the expression
// ^[<special_chars>a-zA-Z0-9]{min_length,max_length}$
bool IsValidInstallerAttributePart(const std::string& part,
                                   const std::string& special_chars,
                                   size_t min_length,
                                   size_t max_length) {
  return part.size() >= min_length && part.size() <= max_length &&
         std::ranges::all_of(part, [&special_chars](char ch) {
           return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) ||
                  base::Contains(special_chars, ch);
         });
}

// Returns true if the |name| parameter matches ^[-_a-zA-Z0-9]{1,256}$ .
bool IsValidInstallerAttributeName(const std::string& name) {
  return IsValidInstallerAttributePart(name, "-_", 1, 256);
}

// Returns true if the |value| parameter matches ^[-.,;+_=$a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttributeValue(const std::string& value) {
  return IsValidInstallerAttributePart(value, "-.,;+_=$", 0, 256);
}

bool IsValidInstallerAttribute(const InstallerAttribute& attr) {
  return IsValidInstallerAttributeName(attr.first) &&
         IsValidInstallerAttributeValue(attr.second);
}

void RemoveUnsecureUrls(std::vector<GURL>* urls) {
  CHECK(urls);
  std::erase_if(*urls,
                [](const GURL& url) { return !url.SchemeIsCryptographic(); });
}

CrxInstaller::Result InstallFunctionWrapper(
    base::OnceCallback<bool()> callback) {
  return CrxInstaller::Result(std::move(callback).Run()
                                  ? InstallError::NONE
                                  : InstallError::GENERIC_ERROR);
}

std::optional<base::Value::Dict> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest)) {
    return std::nullopt;
  }
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root || !root->is_dict()) {
    return std::nullopt;
  }
  return std::move(root->GetDict());
}

std::string GetArchitecture() {
#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  return (os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64())
             ? kArchArm64
             : base::SysInfo().OperatingSystemArchitecture();
#else   // BUILDFLAG(IS_WIN)
  return base::SysInfo().OperatingSystemArchitecture();
#endif  // BUILDFLAG(IS_WIN)
}

bool RetryFileOperation(
    base::FunctionRef<bool(const base::FilePath&)> file_operation,
    const base::FilePath& path,
    size_t tries,
    base::TimeDelta time_between_tries) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  while (!file_operation(path) && --tries) {
    base::PlatformThread::Sleep(time_between_tries);
  }
  return tries;
}

bool CreateTempDirectory(const base::FilePath::StringType& prefix,
                         base::FilePath* new_temp_path) {
#if BUILDFLAG(IS_CHROMEOS)
  const base::FilePath largerTmpDir(kTempDir);
  if (base::DirectoryExists(largerTmpDir)) {
    return base::CreateTemporaryDirInDir(largerTmpDir, prefix, new_temp_path);
  }
#endif
  return base::CreateNewTempDirectory(prefix, new_temp_path);
}

bool CreateScopedTempDirectory(base::ScopedTempDir& dir) {
#if BUILDFLAG(IS_CHROMEOS)
  const base::FilePath largerTmpDir(kTempDir);
  if (base::DirectoryExists(largerTmpDir)) {
    return dir.CreateUniqueTempDirUnderPath(largerTmpDir);
  }
#endif
  return dir.CreateUniqueTempDir();
}

}  // namespace update_client
