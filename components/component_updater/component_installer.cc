// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_installer.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_query_params.h"
#include "components/update_client/utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/backup_util.h"
#endif

namespace component_updater {

const char kNullVersion[] = "0.0.0.0";

namespace {
using Result = update_client::CrxInstaller::Result;
using InstallError = update_client::InstallError;
}  // namespace

ComponentInstallerPolicy::~ComponentInstallerPolicy() = default;

ComponentInstaller::RegistrationInfo::RegistrationInfo()
    : version(kNullVersion) {}

ComponentInstaller::RegistrationInfo::~RegistrationInfo() = default;

ComponentInstaller::ComponentInstaller(
    std::unique_ptr<ComponentInstallerPolicy> installer_policy,
    scoped_refptr<update_client::ActionHandler> action_handler)
    : current_version_(kNullVersion),
      installer_policy_(std::move(installer_policy)),
      action_handler_(action_handler),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

ComponentInstaller::~ComponentInstaller() = default;

void ComponentInstaller::Register(ComponentUpdateService* cus,
                                  base::OnceClosure callback,
                                  base::TaskPriority task_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cus);

  std::vector<uint8_t> public_key_hash;
  installer_policy_->GetHash(&public_key_hash);
  Register(base::BindOnce(&ComponentUpdateService::RegisterComponent,
                          base::Unretained(cus)),
           std::move(callback), task_priority,
           cus->GetRegisteredVersion(
               update_client::GetCrxIdFromPublicKeyHash(public_key_hash)));
}

void ComponentInstaller::Register(RegisterCallback register_callback,
                                  base::OnceClosure callback,
                                  base::TaskPriority task_priority,
                                  const base::Version& registered_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), task_priority,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  if (!installer_policy_) {
    LOG(ERROR) << "A ComponentInstaller has been created but "
               << "has no installer policy.";
    return;
  }

  auto registration_info = base::MakeRefCounted<RegistrationInfo>();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ComponentInstaller::StartRegistration, this,
                     registered_version, registration_info),
      base::BindOnce(&ComponentInstaller::FinishRegistration, this,
                     registration_info, std::move(register_callback),
                     std::move(callback)));
}

void ComponentInstaller::OnUpdateError(int error) {
  LOG(ERROR) << "Component update error: " << error;
}

Result ComponentInstaller::InstallHelper(const base::FilePath& unpack_path,
                                         base::Value::Dict* manifest,
                                         base::Version* version,
                                         base::FilePath* install_path) {
  absl::optional<base::Value::Dict> local_manifest =
      update_client::ReadManifest(unpack_path);
  if (!local_manifest) {
    return Result(InstallError::BAD_MANIFEST);
  }

  const std::string* version_ascii = local_manifest->FindString("version");
  if (!version_ascii || !base::IsStringASCII(*version_ascii))
    return Result(InstallError::INVALID_VERSION);

  const base::Version manifest_version(*version_ascii);

  VLOG(1) << "Install: version=" << manifest_version.GetString()
          << " current version=" << current_version_.GetString();

  if (!manifest_version.IsValid())
    return Result(InstallError::INVALID_VERSION);
  base::FilePath local_install_path;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &local_install_path))
    return Result(InstallError::NO_DIR_COMPONENT_USER);
  local_install_path =
      local_install_path.Append(installer_policy_->GetRelativeInstallDir())
          .AppendASCII(manifest_version.GetString());
  if (base::PathExists(local_install_path)) {
    if (!base::DeletePathRecursively(local_install_path))
      return Result(InstallError::CLEAN_INSTALL_DIR_FAILED);
  }

  VLOG(1) << "unpack_path=" << unpack_path.AsUTF8Unsafe()
          << " install_path=" << local_install_path.AsUTF8Unsafe();

  if (!base::Move(unpack_path, local_install_path)) {
    PLOG(ERROR) << "Move failed.";
    base::DeletePathRecursively(local_install_path);
    return Result(InstallError::MOVE_FILES_ERROR);
  }

  // Acquire the ownership of the |local_install_path|.
  base::ScopedTempDir install_path_owner;
  std::ignore = install_path_owner.Set(local_install_path);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::SetPosixFilePermissions(local_install_path, 0755)) {
    PLOG(ERROR) << "SetPosixFilePermissions failed: "
                << local_install_path.value();
    return Result(InstallError::SET_PERMISSIONS_FAILED);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DCHECK(!base::PathExists(unpack_path));
  DCHECK(base::PathExists(local_install_path));

#if BUILDFLAG(IS_APPLE)
  // Since components can be large and can be re-downloaded when needed, they
  // are excluded from backups.
  base::mac::SetBackupExclusion(local_install_path);
#endif

  const Result result =
      installer_policy_->OnCustomInstall(*local_manifest, local_install_path);
  if (result.error)
    return result;

  if (!installer_policy_->VerifyInstallation(*local_manifest,
                                             local_install_path)) {
    return Result(InstallError::INSTALL_VERIFICATION_FAILED);
  }

  *manifest = std::move(local_manifest.value());
  *version = manifest_version;
  *install_path = install_path_owner.Take();

  return Result(InstallError::NONE);
}

void ComponentInstaller::Install(
    const base::FilePath& unpack_path,
    const std::string& /*public_key*/,
    std::unique_ptr<InstallParams> /*install_params*/,
    ProgressCallback /*progress_callback*/,
    Callback callback) {
  base::Value::Dict manifest;
  base::Version version;
  base::FilePath install_path;
  const Result result =
      InstallHelper(unpack_path, &manifest, &version, &install_path);
  base::DeletePathRecursively(unpack_path);
  if (result.error) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), result));
    return;
  }

  current_version_ = version;
  current_install_dir_ = install_path;

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ComponentInstaller::ComponentReady, this,
                                std::move(manifest)));
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
}

bool ComponentInstaller::GetInstalledFile(const std::string& file,
                                          base::FilePath* installed_file) {
  if (current_version_ == base::Version(kNullVersion))
    return false;  // No component has been installed yet.
  *installed_file = current_install_dir_.AppendASCII(file);
  return true;
}

bool ComponentInstaller::Uninstall() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ComponentInstaller::UninstallOnTaskRunner, this));
  return true;
}

bool ComponentInstaller::FindPreinstallation(
    const base::FilePath& root,
    scoped_refptr<RegistrationInfo> registration_info) {
  base::FilePath path = root.Append(installer_policy_->GetRelativeInstallDir());
  if (!base::PathExists(path)) {
    DVLOG(1) << "Relative install dir does not exist: " << path.MaybeAsASCII();
    return false;
  }

  absl::optional<base::Value::Dict> manifest =
      update_client::ReadManifest(path);
  if (!manifest) {
    DVLOG(1) << "Manifest does not exist: " << path.MaybeAsASCII();
    return false;
  }

  if (!installer_policy_->VerifyInstallation(*manifest, path)) {
    DVLOG(1) << "Installation verification failed: " << path.MaybeAsASCII();
    return false;
  }

  std::string* version_lexical = manifest->FindString("version");
  if (!version_lexical || !base::IsStringASCII(*version_lexical)) {
    DVLOG(1) << "Failed to get component version from the manifest.";
    return false;
  }

  const base::Version version(*version_lexical);
  if (!version.IsValid()) {
    DVLOG(1) << "Version in the manifest is invalid:" << *version_lexical;
    return false;
  }

  VLOG(1) << "Preinstalled component found for " << installer_policy_->GetName()
          << " at " << path.MaybeAsASCII() << " with version " << version
          << ".";

  registration_info->install_dir = path;
  registration_info->version = version;
  registration_info->manifest = std::move(manifest);

  return true;
}

// Checks to see if the installation found in |path| is valid, and returns
// its manifest if it is.
absl::optional<base::Value::Dict>
ComponentInstaller::GetValidInstallationManifest(const base::FilePath& path) {
  absl::optional<base::Value::Dict> manifest =
      update_client::ReadManifest(path);
  if (!manifest) {
    PLOG(ERROR) << "Failed to read manifest for "
                << installer_policy_->GetName() << " (" << path.MaybeAsASCII()
                << ").";
    return absl::nullopt;
  }

  if (!installer_policy_->VerifyInstallation(*manifest, path)) {
    PLOG(ERROR) << "Failed to verify installation for "
                << installer_policy_->GetName() << " (" << path.MaybeAsASCII()
                << ").";
    return absl::nullopt;
  }

  const base::Value::List* accept_archs = manifest->FindList("accept_arch");
  if (accept_archs != nullptr &&
      base::ranges::none_of(*accept_archs, [](const base::Value& v) {
        static const char* current_arch =
            update_client::UpdateQueryParams::GetArch();
        return v.is_string() && v.GetString() == current_arch;
      })) {
    return absl::nullopt;
  }

  return manifest;
}

// Processes the user component directory to select an appropriate component
// version, and saves its data to |registration_info|.
absl::optional<base::Version> ComponentInstaller::SelectComponentVersion(
    const base::Version& registered_version,
    const base::FilePath& base_dir,
    scoped_refptr<RegistrationInfo> registration_info) {
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);

  absl::optional<base::Version> selected_version;
  base::FilePath selected_path;
  absl::optional<base::Value::Dict> selected_manifest;

  const base::Version bundled_version = registration_info->version.IsValid()
                                            ? registration_info->version
                                            : base::Version(kNullVersion);

  // Only look for a previously registered version if it is higher than the
  // bundled version, else default to the highest version.
  const absl::optional<base::Version> target_version =
      (registered_version > bundled_version)
          ? absl::optional<base::Version>(registered_version)
          : absl::nullopt;

  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    // Ignore folders that don't have valid version names. These
    // are not managed by component installer.
    base::Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid()) {
      continue;
    }

    if (!selected_version || version > *selected_version ||
        (target_version && version == *target_version)) {
      absl::optional<base::Value::Dict> candidate_manifest =
          GetValidInstallationManifest(path);
      if (candidate_manifest) {
        selected_version = version;
        selected_path = path;
        selected_manifest = std::move(*candidate_manifest);
      }
    }
    // Stop searching if |target_version| is located.
    if (selected_version && target_version &&
        *selected_version == *target_version) {
      break;
    }
  }

  // No suitable version was found.
  if (!selected_version || bundled_version >= *selected_version) {
    return absl::nullopt;
  }

  registration_info->version = selected_version.value();
  registration_info->manifest = std::move(*selected_manifest);
  registration_info->install_dir = selected_path;
  base::ReadFileToString(selected_path.AppendASCII("manifest.fingerprint"),
                         &registration_info->fingerprint);

  return selected_version;
}

void ComponentInstaller::DeleteUnselectedComponentVersions(
    const base::FilePath& base_dir,
    const absl::optional<base::Version>& selected_version) {
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);

  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());
    // Delete any component version directory that was not selected.
    if (version.IsValid() &&
        !(selected_version && version == *selected_version)) {
      base::DeletePathRecursively(path);
    }
  }
}

absl::optional<base::FilePath> ComponentInstaller::GetComponentDirectory() {
  base::FilePath base_component_dir;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &base_component_dir))
    return absl::nullopt;
  base::FilePath base_dir =
      base_component_dir.Append(installer_policy_->GetRelativeInstallDir());
  if (!base::CreateDirectory(base_dir)) {
    PLOG(ERROR) << "Could not create the base directory for "
                << installer_policy_->GetName() << " ("
                << base_dir.MaybeAsASCII() << ").";
    return absl::nullopt;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath base_dir_ = base_component_dir;
  for (const base::FilePath::StringType& component :
       installer_policy_->GetRelativeInstallDir().GetComponents()) {
    base_dir_ = base_dir_.Append(component);
    if (!base::SetPosixFilePermissions(base_dir_, 0755)) {
      PLOG(ERROR) << "SetPosixFilePermissions failed: " << base_dir.value();
      return absl::nullopt;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return base_dir;
}

void ComponentInstaller::StartRegistration(
    const base::Version& registered_version,
    scoped_refptr<RegistrationInfo> registration_info) {
  VLOG(1) << __func__ << " for " << installer_policy_->GetName();
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // First check for an installation set up alongside Chrome itself.
  base::FilePath root;
  if (base::PathService::Get(DIR_COMPONENT_PREINSTALLED, &root) &&
      FindPreinstallation(root, registration_info)) {
  }

  // If there is a distinct alternate root, check there as well, and override
  // anything found in the basic root.
  base::FilePath root_alternate;
  if (base::PathService::Get(DIR_COMPONENT_PREINSTALLED_ALT, &root_alternate) &&
      root != root_alternate &&
      FindPreinstallation(root_alternate, registration_info)) {
  }

  absl::optional<base::FilePath> base_dir = GetComponentDirectory();

  if (!base_dir) {
    return;
  }

  DeleteUnselectedComponentVersions(
      base_dir.value(),
      SelectComponentVersion(registered_version, base_dir.value(),
                             registration_info));
}

void ComponentInstaller::UninstallOnTaskRunner() {
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Only try to delete any files that are in our user-level install path.
  base::FilePath userInstallPath;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &userInstallPath))
    return;
  if (!userInstallPath.IsParent(current_install_dir_))
    return;

  const base::FilePath base_dir = current_install_dir_.DirName();
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by the component installer, so do not try to remove them.
    if (!version.IsValid())
      continue;

    if (!base::DeletePathRecursively(path))
      DLOG(ERROR) << "Couldn't delete " << path.value();
  }

  // Delete the base directory if it's empty now.
  if (base::IsDirectoryEmpty(base_dir)) {
    if (!base::DeleteFile(base_dir))
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
  }

  // Customized operations for individual component.
  installer_policy_->OnCustomUninstall();
}

void ComponentInstaller::FinishRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    RegisterCallback register_callback,
    base::OnceClosure callback) {
  VLOG(1) << __func__ << " for " << installer_policy_->GetName();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_install_dir_ = registration_info->install_dir;
  current_version_ = registration_info->version;
  current_fingerprint_ = registration_info->fingerprint;

  std::vector<uint8_t> public_key_hash;
  installer_policy_->GetHash(&public_key_hash);

  if (!std::move(register_callback)
           .Run(ComponentRegistration(
               update_client::GetCrxIdFromPublicKeyHash(public_key_hash),
               installer_policy_->GetName(), public_key_hash, current_version_,
               current_fingerprint_,
               installer_policy_->GetInstallerAttributes(), action_handler_,
               this, installer_policy_->RequiresNetworkEncryption(),
               installer_policy_
                   ->SupportsGroupPolicyEnabledComponentUpdates()))) {
    LOG(ERROR) << "Component registration failed for "
               << installer_policy_->GetName();
    if (!callback.is_null())
      std::move(callback).Run();
    return;
  }

  if (registration_info->manifest) {
    ComponentReady(std::move(*registration_info->manifest));
  } else {
    DVLOG(1) << "No component found for " << installer_policy_->GetName();
  }

  if (!callback.is_null())
    std::move(callback).Run();
}

void ComponentInstaller::ComponentReady(base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << current_version_.GetString()
          << " in " << current_install_dir_.value();
  installer_policy_->ComponentReady(current_version_, current_install_dir_,
                                    std::move(manifest));
}

}  // namespace component_updater
