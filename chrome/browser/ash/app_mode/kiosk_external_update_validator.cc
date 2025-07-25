// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_external_update_validator.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace ash {

KioskExternalUpdateValidator::KioskExternalUpdateValidator(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const extensions::CRXFileInfo& file,
    const base::FilePath& crx_unpack_dir,
    const base::WeakPtr<KioskExternalUpdateValidatorDelegate>& delegate)
    : backend_task_runner_(backend_task_runner),
      crx_file_(file),
      crx_unpack_dir_(crx_unpack_dir),
      delegate_(delegate) {}

KioskExternalUpdateValidator::~KioskExternalUpdateValidator() = default;

void KioskExternalUpdateValidator::Start() {
  auto unpacker = base::MakeRefCounted<extensions::SandboxedUnpacker>(
      extensions::mojom::ManifestLocation::kExternalPref,
      extensions::Extension::NO_FLAGS, crx_unpack_dir_,
      backend_task_runner_.get(), this);
  if (!backend_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&extensions::SandboxedUnpacker::StartWithCrx,
                         unpacker.get(), crx_file_))) {
    NOTREACHED();
  }
}

void KioskExternalUpdateValidator::OnUnpackFailure(
    const extensions::CrxInstallError& error) {
  LOG(ERROR) << "Failed to unpack external kiosk crx file: "
             << crx_file_.extension_id << " " << error.message();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &KioskExternalUpdateValidatorDelegate::OnExternalUpdateUnpackFailure,
          delegate_, crx_file_.extension_id));
}

void KioskExternalUpdateValidator::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_dir,
    std::unique_ptr<base::Value::Dict> original_manifest,
    const extensions::Extension* extension,
    const SkBitmap& install_icon,
    extensions::declarative_net_request::RulesetInstallPrefs
        ruleset_install_prefs) {
  DCHECK(crx_file_.extension_id == extension->id());

  std::string minimum_browser_version;
  if (const std::string* temp = extension->manifest()->FindStringPath(
          extensions::manifest_keys::kMinimumChromeVersion)) {
    minimum_browser_version = *temp;
  } else {
    LOG(ERROR) << "Can't find minimum browser version for app "
               << crx_file_.extension_id;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &KioskExternalUpdateValidatorDelegate::OnExternalUpdateUnpackSuccess,
          delegate_, crx_file_.extension_id, extension->VersionString(),
          minimum_browser_version, temp_dir));
}

}  // namespace ash
