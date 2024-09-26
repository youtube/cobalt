// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/echo_private_ash.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ash/notifications/echo_dialog_view.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace crosapi {

namespace {

// Gets the Oobe timestamp on a sequence that allows file-access.
std::string GetOobeTimestampBackground() {
  const char kOobeTimestampFile[] = "/home/chronos/.oobe_completed";
  std::string timestamp;
  base::File::Info fileInfo;
  if (base::GetFileInfo(base::FilePath(kOobeTimestampFile), &fileInfo)) {
    base::Time::Exploded ctime;
    fileInfo.creation_time.UTCExplode(&ctime);
    timestamp += base::StringPrintf("%u-%u-%u", ctime.year, ctime.month,
                                    ctime.day_of_month);
  }
  return timestamp;
}

}  // namespace

EchoPrivateAsh::EchoPrivateAsh() = default;
EchoPrivateAsh::~EchoPrivateAsh() = default;

void EchoPrivateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::EchoPrivate> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void EchoPrivateAsh::CheckRedeemOffersAllowed(aura::Window* window,
                                              const std::string& service_name,
                                              const std::string& origin,
                                              BoolCallback callback) {
  if (in_flight_callback_) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }
  in_flight_callback_ = std::move(callback);

  ash::CrosSettingsProvider::TrustedStatus status =
      ash::CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &EchoPrivateAsh::DidPrepareTrustedValues, weak_factory_.GetWeakPtr(),
          window, service_name, origin));

  // Callback was dropped in this case. Manually invoke.
  if (status == ash::CrosSettingsProvider::TRUSTED)
    DidPrepareTrustedValues(window, service_name, origin);
}

void EchoPrivateAsh::CheckRedeemOffersAllowed(const std::string& window_id,
                                              const std::string& service_name,
                                              const std::string& origin,
                                              BoolCallback callback) {
  gfx::NativeWindow window = crosapi::GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }
  CheckRedeemOffersAllowed(window, service_name, origin, std::move(callback));
}

void EchoPrivateAsh::GetOobeTimestamp(GetOobeTimestampCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetOobeTimestampBackground), std::move(callback));
}

void EchoPrivateAsh::GetRegistrationCode(mojom::RegistrationCodeType type,
                                         GetRegistrationCodeCallback callback) {
  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  std::string result;
  switch (type) {
    case mojom::RegistrationCodeType::kCoupon:
      if (const absl::optional<base::StringPiece> offers_code =
              provider->GetMachineStatistic(
                  ash::system::kOffersCouponCodeKey)) {
        result = std::string(offers_code.value());
      }
      break;
    case mojom::RegistrationCodeType::kGroup:
      if (const absl::optional<base::StringPiece> offers_code =
              provider->GetMachineStatistic(ash::system::kOffersGroupCodeKey)) {
        result = std::string(offers_code.value());
      }
      break;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void EchoPrivateAsh::DidPrepareTrustedValues(aura::Window* window,
                                             const std::string& service_name,
                                             const std::string& origin) {
  ash::CrosSettingsProvider::TrustedStatus status =
      ash::CrosSettings::Get()->PrepareTrustedValues(base::NullCallback());
  if (status != ash::CrosSettingsProvider::TRUSTED) {
    std::move(in_flight_callback_).Run(/*allowed=*/false);
    return;
  }

  bool allow = true;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kAllowRedeemChromeOsRegistrationOffers, &allow);

  // Create and show the dialog.
  ash::EchoDialogView::Params dialog_params;
  dialog_params.echo_enabled = allow;
  if (allow) {
    dialog_params.service_name = base::UTF8ToUTF16(service_name);
    dialog_params.origin = base::UTF8ToUTF16(origin);
  }

  ash::EchoDialogView* dialog = new ash::EchoDialogView(this, dialog_params);
  dialog->Show(window);
}

void EchoPrivateAsh::OnAccept() {
  std::move(in_flight_callback_).Run(/*allowed=*/true);
}

void EchoPrivateAsh::OnCancel() {
  std::move(in_flight_callback_).Run(/*allowed=*/false);
}

void EchoPrivateAsh::OnMoreInfoLinkClicked() {
  NavigateParams params(ProfileManager::GetPrimaryUserProfile(),
                        GURL(chrome::kEchoLearnMoreURL),
                        ui::PAGE_TRANSITION_LINK);
  // Open the link in a new window. The echo dialog is modal, so the current
  // window is useless until the dialog is closed.
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  Navigate(&params);
}

}  // namespace crosapi
