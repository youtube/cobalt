// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace bruschetta {

struct BruschettaLauncher::Files {
  base::ScopedFD firmware;
  absl::optional<base::ScopedFD> pflash;
};

namespace {

// TODO(b/233289313): Once we have an installer and multiple Bruschettas this
// needs to be dynamic, but for now we hardcode the same path that the go/brua
// instructions have people using for the alpha, and the same disk name that
// people following the instructions will have (base64 encoded "bru").
const char kDiskName[] = "YnJ1.img";

const char kOldBiosPath[] = "Downloads/bios";

// We currently support three different paths here, for backwards compatibility.
// 1) A firmware image at kOldBiosPath with flash data embedded in the firmware
// 2) A firmware image at kBiosPath with flash data at kPflashPath
// 3) A firmware image at kBiosPath, with flash data handled by concierge
//
// TODO(b/265096855): Remove support for options 1&2 once they're no longer in
// use.
std::unique_ptr<BruschettaLauncher::Files> OpenFdsBlocking(
    base::FilePath profile_path) {
  base::File firmware(profile_path.Append(kBiosPath),
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!firmware.IsValid()) {
    firmware = base::File(profile_path.Append(kOldBiosPath),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!firmware.IsValid()) {
      PLOG(ERROR) << "Failed to open firmware";
      return nullptr;
    }
    BruschettaLauncher::Files files = {
        .firmware = base::ScopedFD(firmware.TakePlatformFile()),
        .pflash = absl::nullopt,
    };
    return std::make_unique<BruschettaLauncher::Files>(std::move(files));
  }

  base::File pflash(profile_path.Append(kPflashPath),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);

  BruschettaLauncher::Files files = {
      .firmware = base::ScopedFD(firmware.TakePlatformFile()),
      .pflash = absl::nullopt,
  };
  if (pflash.IsValid()) {
    files.pflash = base::ScopedFD(pflash.TakePlatformFile());
  }

  return std::make_unique<BruschettaLauncher::Files>(std::move(files));
}

}  // namespace

BruschettaLauncher::BruschettaLauncher(std::string vm_name, Profile* profile)
    : vm_name_(vm_name), profile_(profile) {}
BruschettaLauncher::~BruschettaLauncher() = default;

void BruschettaLauncher::EnsureRunning(
    base::OnceCallback<void(BruschettaResult)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool launch_in_progress = false;
  if (!callbacks_.empty()) {
    launch_in_progress = true;
  }
  callbacks_.AddUnsafe(std::move(callback));
  if (!launch_in_progress) {
    EnsureDlcInstalled();
    // If we're not complete after 4 minutes time out the entire launch.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BruschettaLauncher::OnTimeout,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(240));
  }
}

void BruschettaLauncher::EnsureDlcInstalled() {
  in_progress_dlc_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kToolsDlc, /*retry=*/false,
      base::BindOnce(&BruschettaLauncher::OnMountDlc,
                     weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaLauncher::OnMountDlc(
    guest_os::GuestOsDlcInstallation::Result install_result) {
  in_progress_dlc_.reset();
  if (!install_result.has_value()) {
    LOG(ERROR) << "Error installing DLC: " << install_result.error();
    Finish(BruschettaResult::kDlcInstallError);
    return;
  }

  // TODO(b/264495837, b/264495396): Eventually we should stop storing these
  // files in the user's Downloads directory.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&OpenFdsBlocking, profile_->GetPath()),
      base::BindOnce(&BruschettaLauncher::StartVm, weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::StartVm(
    std::unique_ptr<BruschettaLauncher::Files> files) {
  if (!files) {
    LOG(ERROR) << "Error opening BIOS or pflash files";
    Finish(BruschettaResult::kBiosNotAccessible);
    return;
  }

  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Error connecting to concierge. Client is NULL.";
    Finish(BruschettaResult::kStartVmFailed);
    return;
  }

  const std::string config_id =
      GetContainerPrefValue(profile_, MakeBruschettaId(vm_name_),
                            guest_os::prefs::kBruschettaConfigId)
          ->GetString();
  RunningVmPolicy launch_policy;
  auto opt = GetLaunchPolicyForConfig(profile_, config_id);
  if (!opt.has_value()) {
    // Policy prohibits starting the VM, so don't.
    LOG(ERROR) << "Starting VM prohibited by policy";
    Finish(BruschettaResult::kForbiddenByPolicy);
    return;
  } else {
    launch_policy = *opt;
  }

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);
  std::string vm_username = GetVmUsername(profile_);
  vm_tools::concierge::StartVmRequest request;
  request.set_start_termina(false);
  request.set_name(vm_name_);
  *request.mutable_vm()->mutable_tools_dlc_id() = kToolsDlc;
  *request.mutable_owner_id() = user_hash;
  request.set_vm_username(vm_username);
  request.set_start_termina(false);
  request.set_timeout(240);
  request.set_vtpm_proxy(launch_policy.vtpm_enabled);

  // fds and request.fds must have the same order.
  std::vector<base::ScopedFD> fds;
  request.add_fds(vm_tools::concierge::StartVmRequest::BIOS);
  fds.push_back(std::move(files->firmware));
  if (files->pflash) {
    request.add_fds(vm_tools::concierge::StartVmRequest::PFLASH);
    fds.push_back(std::move(*files->pflash));
  }
  files.reset();

  auto* disk = request.mutable_disks()->Add();
  *disk->mutable_path() =
      base::StrCat({"/run/daemon-store/crosvm/", user_hash, "/", kDiskName});
  disk->set_writable(true);
  disk->set_do_mount(false);

  client->StartVmWithFds(
      std::move(fds), request,
      base::BindOnce(&BruschettaLauncher::OnStartVm, weak_factory_.GetWeakPtr(),
                     launch_policy));
}

void BruschettaLauncher::OnStartVm(
    RunningVmPolicy launch_policy,
    absl::optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response || !response->success()) {
    if (response) {
      LOG(ERROR) << "Error starting VM, got status: " << response->status()
                 << " and reason " << response->failure_reason();
    } else {
      LOG(ERROR) << "Error starting VM: no response from Concierge";
    }
    Finish(BruschettaResult::kStartVmFailed);
    return;
  }

  BruschettaService::GetForProfile(profile_)->RegisterVmLaunch(vm_name_,
                                                               launch_policy);

  auto* tracker = guest_os::GuestOsSessionTracker::GetForProfile(profile_);
  subscription_ = tracker->RunOnceContainerStarted(
      guest_os::GuestId{guest_os::VmType::BRUSCHETTA, vm_name_, "penguin"},
      base::BindOnce(&BruschettaLauncher::OnContainerRunning,
                     weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::OnContainerRunning(guest_os::GuestInfo info) {
  Finish(BruschettaResult::kSuccess);
}

void BruschettaLauncher::OnTimeout() {
  // These are no-ops if empty so safe to always call.
  subscription_.reset();
  Finish(BruschettaResult::kTimeout);

  // We don't actually abort or cancel the launch, let it keep going in the
  // background in case it's really slow for some reason then the next time they
  // try it might succeed.
}

void BruschettaLauncher::Finish(BruschettaResult result) {
  base::UmaHistogramEnumeration("Bruschetta.LaunchResult", result);
  callbacks_.Notify(result);
}

base::WeakPtr<BruschettaLauncher> BruschettaLauncher::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace bruschetta
