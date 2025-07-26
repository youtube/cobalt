// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: iebhnlpddlcpcfpfalldikcoeakpeoah
constexpr std::array<uint8_t, 32> kIwaKeyDistributionPublicKeySHA256 = {
    0x84, 0x17, 0xdb, 0xf3, 0x3b, 0x2f, 0x25, 0xf5, 0x0b, 0xb3, 0x8a,
    0x2e, 0x40, 0xaf, 0x4e, 0x07, 0x18, 0xfa, 0xae, 0x6e, 0x0e, 0xdb,
    0x46, 0xfc, 0xc9, 0x36, 0x50, 0xcf, 0x38, 0xfa, 0xf9, 0xab};

constexpr std::string_view kPreloadedKey = "is_preloaded";
constexpr std::string_view kIwaKdcExpCohortAttribute = "_iwa_kdc_exp_cohort";

void OnDemandUpdateCompleted(update_client::Error err) {
  VLOG(1) << "On-demand update for the "
             "Iwa Key Distribution Component "
             "finished with result "
          << base::to_underlying(err);
}

}  // namespace

namespace component_updater {

BASE_FEATURE(kIwaKeyDistributionComponent,
             "IwaKeyDistributionComponent",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else   // !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // !BUILDFLAG(IS_CHROMEOS)
);

IwaKeyDistributionComponentInstallerPolicy::
    IwaKeyDistributionComponentInstallerPolicy() = default;
IwaKeyDistributionComponentInstallerPolicy::
    ~IwaKeyDistributionComponentInstallerPolicy() = default;

// static
bool IwaKeyDistributionComponentInstallerPolicy::QueueOnDemandUpdate(
    base::PassKey<web_app::IwaKeyDistributionInfoProvider>) {
  // static
  if (!g_browser_process ||
      !base::FeatureList::IsEnabled(kIwaKeyDistributionComponent)) {
    return false;
  }

  VLOG(1) << "Queueing on-demand update for the Iwa Key Distribution Component";
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_file::id_util::GenerateIdFromHash(kIwaKeyDistributionPublicKeySHA256),
      OnDemandUpdater::Priority::BACKGROUND,
      base::BindOnce(&OnDemandUpdateCompleted));

  return true;
}

bool IwaKeyDistributionComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kDataFileName));
}

bool IwaKeyDistributionComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool IwaKeyDistributionComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
IwaKeyDistributionComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // No custom install.
  return update_client::CrxInstaller::Result(0);
}

void IwaKeyDistributionComponentInstallerPolicy::OnCustomUninstall() {}

void IwaKeyDistributionComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  if (install_dir.empty() || !version.IsValid()) {
    return;
  }

  VLOG(1) << "Iwa Key Distribution Component ready, version " << version
          << " in " << install_dir;
  web_app::IwaKeyDistributionInfoProvider::GetInstance()
      ->LoadKeyDistributionData(
          version, install_dir.Append(kDataFileName),
          /*is_preloaded=*/manifest.FindBool(kPreloadedKey).value_or(false));
}

base::FilePath
IwaKeyDistributionComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kRelativeInstallDirName);
}

void IwaKeyDistributionComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kIwaKeyDistributionPublicKeySHA256),
               std::end(kIwaKeyDistributionPublicKeySHA256));
}

std::string IwaKeyDistributionComponentInstallerPolicy::GetName() const {
  return kManifestName;
}

update_client::InstallerAttributes
IwaKeyDistributionComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kIwaKeyDistributionComponentExpCohort)) {
    attributes.emplace(
        kIwaKdcExpCohortAttribute,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kIwaKeyDistributionComponentExpCohort));
  }
  return attributes;
}

void RegisterIwaKeyDistributionComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(kIwaKeyDistributionComponent)) {
    return;
  }

  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<IwaKeyDistributionComponentInstallerPolicy>(),
      /*action_handler=*/nullptr, base::TaskPriority::USER_VISIBLE)
      ->Register(cus, base::DoNothing());
}

}  // namespace component_updater
