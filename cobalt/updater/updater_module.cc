// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/updater/updater_module.h"

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "cobalt/updater/crash_client.h"
#include "cobalt/updater/crash_reporter.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/utils.h"
#include "starboard/configuration_constants.h"
#include "starboard/loader_app/installation_manager.h"

namespace {

// The SHA256 hash of the "cobalt_evergreen_public" key.
constexpr uint8_t kCobaltPublicKeyHash[] = {
    0xfd, 0x81, 0xea, 0x59, 0xa2, 0xa3, 0x88, 0xf6, 0xf2, 0x20, 0x21,
    0xaa, 0x90, 0xda, 0x5a, 0x8e, 0x51, 0xdf, 0x80, 0x6e, 0x0a, 0x0a,
    0x24, 0x45, 0x38, 0x79, 0xd4, 0x95, 0xfc, 0x57, 0x2d, 0xab};

void QuitLoop(base::OnceClosure quit_closure) { std::move(quit_closure).Run(); }

}  // namespace

namespace cobalt {
namespace updater {
UpdaterModule::UpdaterModule(network::NetworkModule* network_module)
    : updater_thread_("updater"), network_module_(network_module) {
  updater_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  DETACH_FROM_THREAD(thread_checker_);
  // Initialize the underlying update client.
  updater_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&UpdaterModule::Initialize, base::Unretained(this)));
}

UpdaterModule::~UpdaterModule() {
  updater_thread_.task_runner()->PostBlockingTask(
      FROM_HERE, base::Bind(&UpdaterModule::Finalize, base::Unretained(this)));
}

void UpdaterModule::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO: enable crash report with dependency on CrashPad
  // updater::crash_reporter::InitializeCrashKeys();

  // static crash_reporter::CrashKeyString<16> crash_key_process_type(
  //     "process_type");
  // crash_key_process_type.Set("updater");

  // if (CrashClient::GetInstance()->InitializeCrashReporting())
  //   VLOG(1) << "Crash reporting initialized.";
  // else
  //   VLOG(1) << "Crash reporting is not available.";

  // StartCrashReporter(UPDATER_VERSION_STRING);

  updater_configurator_ = base::MakeRefCounted<Configurator>(network_module_);
  update_client_ = update_client::UpdateClientFactory(updater_configurator_);

  updater_observer_.reset(new Observer(update_client_));
  update_client_->AddObserver(updater_observer_.get());

  installation_manager_ =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager_) {
    SB_LOG(ERROR) << "Updater failed to get installation manager extension.";
    return;
  }

  // Schedule the first update check.
  updater_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&UpdaterModule::Update, base::Unretained(this)));
}

void UpdaterModule::Finalize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  installation_manager_ = nullptr;
  update_client_->RemoveObserver(updater_observer_.get());
  updater_observer_.reset();
  update_client_ = nullptr;

  updater_configurator_->GetPrefService()->CommitPendingWrite(
      base::BindOnce(&QuitLoop, base::Bind(base::DoNothing::Repeatedly())));

  updater_configurator_ = nullptr;
}

void UpdaterModule::MarkSuccessful() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int index = installation_manager_->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Updater failed to get current installation index.";
    return;
  }
  if (installation_manager_->MarkInstallationSuccessful(index) !=
      IM_EXT_SUCCESS) {
    SB_LOG(ERROR)
        << "Updater failed to mark the current installation successful";
  }
}

void UpdaterModule::Update() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::vector<std::string> app_ids = {
      updater_configurator_->GetAppGuid()};

  // Get the update version from the manifest file under the current
  // installation path.
  int index = installation_manager_->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Updater failed to get current installation index.";
    return;
  }
  std::vector<char> installation_path(kSbFileMaxPath);
  if (installation_manager_->GetInstallationPath(
          index, installation_path.data(), kSbFileMaxPath) == IM_ERROR) {
    SB_LOG(ERROR) << "Updater failed to get installation path.";
    return;
  }
  auto manifest = update_client::ReadManifest(base::FilePath(
      std::string(installation_path.begin(), installation_path.end())));
  if (!manifest) {
    SB_LOG(ERROR) << "Updater failed to read the manifest file of the current "
                     "installation.";
    return;
  }
  auto* version_path = manifest->FindPath({"version"});
  if (!version_path) {
    SB_LOG(ERROR) << "Updater failed to find the version in the manifest.";
    return;
  }
  const base::Version manifest_version(version_path->GetString());
  if (!manifest_version.IsValid()) {
    SB_LOG(ERROR) << "Updater failed to get the current update version.";
    return;
  }

  update_client_->Update(
      app_ids,
      base::BindOnce(
          [](base::Version manifest_version,
             const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            update_client::CrxComponent component;
            component.name = "cobalt";
            component.app_id = ids[0];
            component.version = manifest_version;
            component.pk_hash.assign(std::begin(kCobaltPublicKeyHash),
                                     std::end(kCobaltPublicKeyHash));
            component.requires_network_encryption = true;
            component.crx_format_requirement = crx_file::VerifierFormat::CRX3;
            return {component};
          },
          manifest_version),
      false,
      base::BindOnce(
          [](base::OnceClosure closure, update_client::Error error) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(&QuitLoop, std::move(closure)));
          },
          base::Bind(base::DoNothing::Repeatedly())));

  // Mark the current installation as successful.
  updater_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&UpdaterModule::MarkSuccessful, base::Unretained(this)));

  IncrementUpdateCheckCount();

  int kNextUpdateCheckHours = 0;
  if (GetUpdateCheckCount() == 1) {
    // Update check ran once. The next check will be scheduled at a randomized
    // time between 1 and 24 hours.
    kNextUpdateCheckHours = base::RandInt(1, 24);
  } else {
    // Update check ran at least twice. The next check will be scheduled in 24
    // hours.
    kNextUpdateCheckHours = 24;
  }
  updater_thread_.task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&UpdaterModule::Update, base::Unretained(this)),
      base::TimeDelta::FromHours(kNextUpdateCheckHours));
}

}  // namespace updater
}  // namespace cobalt
