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

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/extension/installation_manager.h"
#include "cobalt/updater/crash_client.h"
#include "cobalt/updater/crash_reporter.h"
#include "components/crx_file/crx_verifier.h"

namespace {

// The SHA256 hash of the "cobalt_evergreen_public" key.
constexpr uint8_t kCobaltPublicKeyHash[] = {
    0xfd, 0x81, 0xea, 0x59, 0xa2, 0xa3, 0x88, 0xf6, 0xf2, 0x20, 0x21,
    0xaa, 0x90, 0xda, 0x5a, 0x8e, 0x51, 0xdf, 0x80, 0x6e, 0x0a, 0x0a,
    0x24, 0x45, 0x38, 0x79, 0xd4, 0x95, 0xfc, 0x57, 0x2d, 0xab};

void QuitLoop(base::OnceClosure quit_closure) { std::move(quit_closure).Run(); }

void Return() {}

}  // namespace

namespace cobalt {
namespace updater {

UpdaterModule::UpdaterModule(base::MessageLoop* message_loop,
                             network::NetworkModule* network_module)
    : message_loop_(message_loop), network_module_(network_module) {
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
}

UpdaterModule::~UpdaterModule() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  update_client_->RemoveObserver(updater_observer_.get());
  update_client_ = nullptr;
}

void UpdaterModule::MarkSuccessful() {
  const CobaltExtensionInstallationManagerApi* installation_api =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_api) {
    SB_LOG(ERROR) << "Failed to get installation manager extension";
    return;
  }
  int index = installation_api->GetCurrentInstallationIndex();
  if (index != IM_EXT_ERROR) {
    if (installation_api->MarkInstallationSuccessful(index) != IM_EXT_SUCCESS) {
      SB_LOG(ERROR)
          << "Updater failed to mark the current installation successful";
    }
  }
}

void UpdaterModule::Update() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO: get SABI string, get correct appid for the channel
  const std::vector<std::string> app_ids = {
      updater_configurator_->GetAppGuid()};

  // TODO: get rid of this callback
  base::Closure closure_callback = base::Bind(&Return);

  update_client_->Update(
      app_ids,
      base::BindOnce(
          [](const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            update_client::CrxComponent component;
            component.name = "cobalt_test";
            component.app_id = ids[0];
            component.version = base::Version("1.0.0.0");
            component.pk_hash.assign(std::begin(kCobaltPublicKeyHash),
                                     std::end(kCobaltPublicKeyHash));
            component.requires_network_encryption = true;
            component.crx_format_requirement = crx_file::VerifierFormat::CRX3;
            return {component};
          }),
      false,
      base::BindOnce(
          [](base::OnceClosure closure, update_client::Error error) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(&QuitLoop, std::move(closure)));
          },
          closure_callback));

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
  message_loop_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&UpdaterModule::Update, base::Unretained(this)),
      base::TimeDelta::FromHours(kNextUpdateCheckHours));
}

}  // namespace updater
}  // namespace cobalt
