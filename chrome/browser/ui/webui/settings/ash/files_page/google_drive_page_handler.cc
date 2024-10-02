// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/files_page/google_drive_page_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/google_drive_handler.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash::settings {

using drivefs::pinning::Progress;

namespace {

google_drive::mojom::StatusPtr CreateStatusPtr(const Progress& progress) {
  auto status = google_drive::mojom::Status::New();
  status->required_space =
      base::UTF16ToUTF8(ui::FormatBytes(progress.required_space));
  status->remaining_space = base::UTF16ToUTF8(
      ui::FormatBytes(progress.free_space - progress.required_space));
  status->stage = progress.stage;
  status->is_error = progress.IsError();
  return status;
}

}  // namespace

GoogleDrivePageHandler::GoogleDrivePageHandler(
    mojo::PendingReceiver<google_drive::mojom::PageHandler> receiver,
    mojo::PendingRemote<google_drive::mojom::Page> page,
    Profile* profile)
    : profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {}

GoogleDrivePageHandler::~GoogleDrivePageHandler() {
  auto* const pin_manager = GetPinManager();
  if (!pin_manager || !pin_manager->HasObserver(this)) {
    return;
  }
  pin_manager->RemoveObserver(this);
}

void GoogleDrivePageHandler::CalculateRequiredSpace() {
  auto* pin_manager = GetPinManager();
  if (!pin_manager) {
    page_->OnServiceUnavailable();
    return;
  }

  NotifyProgress(pin_manager->GetProgress());
  if (!pin_manager->HasObserver(this)) {
    pin_manager->AddObserver(this);
  }
  pin_manager->CalculateRequiredSpace();
}

void GoogleDrivePageHandler::NotifyProgress(const Progress& progress) {
  page_->OnProgress(CreateStatusPtr(progress));
}

drive::DriveIntegrationService* GoogleDrivePageHandler::GetDriveService() {
  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!service || !service->IsMounted()) {
    return nullptr;
  }
  return service;
}

drivefs::pinning::PinManager* GoogleDrivePageHandler::GetPinManager() {
  drive::DriveIntegrationService* service = GetDriveService();
  if (!service || !service->GetPinManager()) {
    return nullptr;
  }
  return service->GetPinManager();
}

void GoogleDrivePageHandler::OnProgress(const Progress& progress) {
  auto* pin_manager = GetPinManager();
  if (!pin_manager) {
    page_->OnServiceUnavailable();
    return;
  }

  NotifyProgress(progress);
}

void GoogleDrivePageHandler::OnDrop() {
  page_->OnServiceUnavailable();
}

void GoogleDrivePageHandler::GetTotalPinnedSize(
    GetTotalPinnedSizeCallback callback) {
  if (!GetDriveService()) {
    page_->OnServiceUnavailable();
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // If Drive crashes, this callback may not get invoked so in that instance
  // ensure it gets invoked with "-1" to signal an error case.
  auto on_total_pinned_size_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&GoogleDrivePageHandler::OnGetTotalPinnedSize,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          /*size=*/-1);
  GetDriveService()->GetTotalPinnedSize(
      std::move(on_total_pinned_size_callback));
}

void GoogleDrivePageHandler::OnGetTotalPinnedSize(
    GetTotalPinnedSizeCallback callback,
    int64_t size) {
  if (size == -1) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(base::UTF16ToUTF8(ui::FormatBytes(size)));
}

void GoogleDrivePageHandler::ClearPinnedFiles(
    ClearPinnedFilesCallback callback) {
  if (!GetDriveService()) {
    std::move(callback).Run();
    page_->OnServiceUnavailable();
    return;
  }

  // If Drive crashes, this callback may not get invoked so in that instance
  // ensure it gets invoked with drve::FILE_ERROR_ABORT to indicate that the
  // call has been aborted.
  auto on_clear_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&GoogleDrivePageHandler::OnClearPinnedFiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      drive::FILE_ERROR_ABORT);
  GetDriveService()->ClearOfflineFiles(std::move(on_clear_callback));
}

void GoogleDrivePageHandler::OnClearPinnedFiles(
    ClearPinnedFilesCallback callback,
    drive::FileError error) {
  std::move(callback).Run();
}

}  // namespace ash::settings
