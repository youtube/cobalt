// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UTILS_H_
#define COMPONENTS_UPDATE_CLIENT_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "components/update_client/update_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace update_client {

class Component;
struct CrxComponent;

extern const char kArchAmd64[];
extern const char kArchIntel[];
extern const char kArchArm64[];

#if BUILDFLAG(IS_STARBOARD)
enum class UpdaterStatus {
  kNewUpdate,
  kChecking,
  kUpdateAvailable,
  kDownloadingDiff,
  kDownloading,
  kSlotLocked,
  kDownloaded,
  kUpdatingDiff,
  kUpdating,
  kUpdated,
  kRolledForward,
  kUpToDate,
  kUpdateError,
  kUninstalled,
  kRun
};

// Mapping a component state to an updater status.
// clang-format off
const std::map<ComponentState, UpdaterStatus> component_to_updater_status_map = {
        // clang-format on
        {ComponentState::kNew, UpdaterStatus::kNewUpdate},
        {ComponentState::kChecking, UpdaterStatus::kChecking},
        {ComponentState::kCanUpdate, UpdaterStatus::kUpdateAvailable},
        {ComponentState::kDownloadingDiff, UpdaterStatus::kDownloadingDiff},
        {ComponentState::kDownloading, UpdaterStatus::kDownloading},
        {ComponentState::kDownloaded, UpdaterStatus::kDownloaded},
        {ComponentState::kUpdatingDiff, UpdaterStatus::kUpdatingDiff},
        {ComponentState::kUpdating, UpdaterStatus::kUpdating},
        {ComponentState::kUpdated, UpdaterStatus::kUpdated},
        {ComponentState::kUpToDate, UpdaterStatus::kUpToDate},
        {ComponentState::kUpdateError, UpdaterStatus::kUpdateError},
        {ComponentState::kUninstalled, UpdaterStatus::kUninstalled},
        {ComponentState::kRun, UpdaterStatus::kRun},
};

// Translating an updater status to a status string.
const std::map<UpdaterStatus, const char*> updater_status_string_map = {
    {UpdaterStatus::kNewUpdate, "Will check for update soon"},
    {UpdaterStatus::kChecking, "Checking for update"},
    {UpdaterStatus::kUpdateAvailable, "Update is available"},
    {UpdaterStatus::kDownloadingDiff, "Downloading delta update"},
    {UpdaterStatus::kDownloading, "Downloading update"},
    {UpdaterStatus::kSlotLocked, "Slot is locked"},
    {UpdaterStatus::kDownloaded, "Update is downloaded"},
    {UpdaterStatus::kUpdatingDiff, "Installing delta update"},
    {UpdaterStatus::kUpdating, "Installing update"},
    {UpdaterStatus::kUpdated, "Update installed, pending restart"},
    {UpdaterStatus::kRolledForward, "Updated locally, pending restart"},
    {UpdaterStatus::kUpToDate, "App is up to date"},
    {UpdaterStatus::kUpdateError, "Failed to update"},
    {UpdaterStatus::kUninstalled, "Update uninstalled"},
    {UpdaterStatus::kRun, "Transitioning..."},
};
#endif // BUILDFLAG(IS_STARBOARD)

// Defines a name-value pair that represents an installer attribute.
// Installer attributes are component-specific metadata, which may be serialized
// in an update check request.
using InstallerAttribute = std::pair<std::string, std::string>;

// Returns true if the |component| contains a valid differential update url.
bool HasDiffUpdate(const Component& component);

// Returns true if the |status_code| represents a server error 5xx.
bool IsHttpServerError(int status_code);

// Deletes the file and its directory, if the directory is empty. If the
// parent directory is not empty, the function ignores deleting the directory.
// Returns true if the file and the empty directory are deleted,
// or if the file was deleted and the directory was not empty.
bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath);

// Deletes the given directory, if the directory is empty. If the
// directory is not empty, the function ignores deleting the directory.
// Returns true if the directory is not empty or if the directory was empty
// and successfully deleted.
bool DeleteEmptyDirectory(const base::FilePath& filepath);

// Returns the component id of the |component|. The component id is either the
// app_id, if the member is set, or a string value derived from the public
// key hash with a format similar with the format of an extension id.
std::string GetCrxComponentID(const CrxComponent& component);

// Returns a CRX id from a public key hash.
std::string GetCrxIdFromPublicKeyHash(const std::vector<uint8_t>& pk_hash);

#if defined(IN_MEMORY_UPDATES)
// Returns true if the actual SHA-256 hash of |content| matches the
// |expected_hash|.
// |content| must refer to a valid string.
bool VerifyHash256(const std::string* content,
                   const std::string& expected_hash);
#else
// Returns true if the actual SHA-256 hash of the |filepath| matches the
// |expected_hash|.
bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash);
#endif   

#if BUILDFLAG(IS_STARBOARD)
// Reads the Evergreen version of the installation dir.
base::Version ReadEvergreenVersion(base::FilePath installation_dir);
#endif  // BUILDFLAG(IS_STARBOARD)

// Returns true if the |brand| parameter matches ^[a-zA-Z]{4}?$ .
bool IsValidBrand(const std::string& brand);

// Returns true if the name part of the |attr| parameter matches
// ^[-_a-zA-Z0-9]{1,256}$ and the value part of the |attr| parameter
// matches ^[-.,;+_=$a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttribute(const InstallerAttribute& attr);

// Removes the unsecure urls in the |urls| parameter.
void RemoveUnsecureUrls(std::vector<GURL>* urls);

// Adapter function for the old definitions of CrxInstaller::Install until the
// component installer code is migrated to use a Result instead of bool.
CrxInstaller::Result InstallFunctionWrapper(
    base::OnceCallback<bool()> callback);

// Deserializes the CRX manifest. The top level must be a dictionary.
// Returns a base::Value::Dict object of type dictionary on success, or nullopt
// on failure.
absl::optional<base::Value::Dict> ReadManifest(
    const base::FilePath& unpack_path);

// Converts a custom, specific installer error (and optionally extended error)
// to an installer result.
template <typename T>
CrxInstaller::Result ToInstallerResult(const T& error, int extended_error = 0) {
  static_assert(std::is_enum<T>::value,
                "Use an enum class to define custom installer errors");
  return CrxInstaller::Result(
      static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE) +
          static_cast<int>(error),
      extended_error);
}

// Returns a string representation of the processor architecture. Uses
// `base::win::OSInfo::IsWowX86OnARM64` and
// `base::win::OSInfo::IsWowAMD64OnARM64` if available on Windows (more
// accurate).
// If not, or not Windows, falls back to
// `base::SysInfo().OperatingSystemArchitecture`.
std::string GetArchitecture();

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UTILS_H_
