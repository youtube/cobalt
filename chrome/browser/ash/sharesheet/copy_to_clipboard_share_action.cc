// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sharesheet/copy_to_clipboard_share_action.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/sharesheet/constants.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {
const char kToastId[] = "copy_to_clipboard_share_action";

void RecordFormFactorMetric() {
  auto form_factor = ::sharesheet::SharesheetMetrics::GetFormFactorForMetrics();
  ::sharesheet::SharesheetMetrics::RecordCopyToClipboardShareActionFormFactor(
      form_factor);
}

void RecordMimeTypes(
    const base::flat_set<::sharesheet::SharesheetMetrics::MimeType>&
        mime_types_to_record) {
  for (auto& mime_type : mime_types_to_record) {
    ::sharesheet::SharesheetMetrics::RecordCopyToClipboardShareActionMimeType(
        mime_type);
  }
}

}  // namespace

namespace ash {
namespace sharesheet {

using ::sharesheet::SharesheetMetrics;

CopyToClipboardShareAction::CopyToClipboardShareAction(Profile* profile)
    : profile_(profile) {}

CopyToClipboardShareAction::~CopyToClipboardShareAction() = default;

const std::u16string CopyToClipboardShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(
      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL);
}

const gfx::VectorIcon& CopyToClipboardShareAction::GetActionIcon() {
  return kCopyIcon;
}

void CopyToClipboardShareAction::LaunchAction(
    ::sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::IntentPtr intent) {
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);

  if (intent->share_text.has_value()) {
    apps_util::SharedText extracted_text =
        apps_util::ExtractSharedText(intent->share_text.value());

    if (!extracted_text.text.empty()) {
      clipboard_writer.WriteText(base::UTF8ToUTF16(extracted_text.text));
    }

    if (!extracted_text.url.is_empty()) {
      std::string anchor_text;
      if (intent->share_title.has_value() &&
          !(intent->share_title.value().empty())) {
        anchor_text = intent->share_title.value();
      }
      clipboard_writer.WriteText(base::UTF8ToUTF16(extracted_text.url.spec()));
    }
  }

  if (!intent->files.empty()) {
    std::vector<ui::FileInfo> file_infos;
    for (const auto& file : intent->files) {
      auto file_url = apps::GetFileSystemURL(profile_, file->url);
      // TODO(crbug.com/1274983) : Add support for copying from MTP and
      // FileSystemProviders.
      if (!file_manager::util::IsNonNativeFileSystemType(file_url.type())) {
        file_infos.emplace_back(
            ui::FileInfo(file_url.path(), base::FilePath()));
      }
    }
    clipboard_writer.WriteFilenames(ui::FileInfosToURIList(file_infos));
  }

  RecordFormFactorMetric();
  RecordMimeTypes(
      ::sharesheet::SharesheetMetrics::GetMimeTypesFromIntentForMetrics(
          intent));

  if (controller) {
    controller->CloseBubble(::sharesheet::SharesheetResult::kSuccess);
  }

  ToastData toast(kToastId, ToastCatalogName::kCopyToClipboardShareAction,
                  l10n_util::GetStringUTF16(
                      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SUCCESS_TOAST_LABEL));
  ShowToast(std::move(toast));
}

void CopyToClipboardShareAction::OnClosing(
    ::sharesheet::SharesheetController* controller) {}

bool CopyToClipboardShareAction::ShouldShowAction(
    const apps::IntentPtr& intent,
    bool contains_hosted_document) {
  bool contains_uncopyable_file = false;
  if (!intent->files.empty()) {
    for (const auto& file : intent->files) {
      auto file_url = apps::GetFileSystemURL(profile_, file->url);
      contains_uncopyable_file =
          file_manager::util::IsNonNativeFileSystemType(file_url.type());
      if (contains_uncopyable_file) {
        break;
      }
    }
  }
  // If |intent| contains a file we can't copy, don't show this action.
  // Files that are not in a native local file system (e.g. MTP, documents
  // providers) do not currently support paste outside of the Files app.
  return !contains_uncopyable_file &&
         ShareAction::ShouldShowAction(intent, contains_hosted_document);
}

void CopyToClipboardShareAction::ShowToast(ash::ToastData toast_data) {
  ToastManager::Get()->Show(std::move(toast_data));
}

}  // namespace sharesheet
}  // namespace ash
