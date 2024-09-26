// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_INTERVENTION_POLICY_DATABASE_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_INTERVENTION_POLICY_DATABASE_COMPONENT_INSTALLER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace resource_coordinator {
class InterventionPolicyDatabase;
}

namespace component_updater {

class ComponentUpdateService;

// Component for receiving the intervention policy database. The database
// consists in a proto, defined in
// chrome/browser/resource_coordinator/intervention_policy_database.proto.
class InterventionPolicyDatabaseComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  InterventionPolicyDatabaseComponentInstallerPolicy(
      resource_coordinator::InterventionPolicyDatabase* database);
  InterventionPolicyDatabaseComponentInstallerPolicy(
      const InterventionPolicyDatabaseComponentInstallerPolicy&) = delete;
  InterventionPolicyDatabaseComponentInstallerPolicy& operator=(
      const InterventionPolicyDatabaseComponentInstallerPolicy&) = delete;
  ~InterventionPolicyDatabaseComponentInstallerPolicy() override = default;

 private:
  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  raw_ptr<resource_coordinator::InterventionPolicyDatabase> database_;
};

// Call once to make the component update service aware of the Intervention
// Policy Database component.
void RegisterInterventionPolicyDatabaseComponent(
    ComponentUpdateService* cus,
    resource_coordinator::InterventionPolicyDatabase* database);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_INTERVENTION_POLICY_DATABASE_COMPONENT_INSTALLER_H_
