// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_zipfile_installer.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

ZipFileInstaller::DoneCallback MakeRegisterInExtensionServiceCallback(
    content::BrowserContext* context) {
  return base::BindOnce(
      [](base::WeakPtr<content::BrowserContext> context_weak,
         const base::FilePath& zip_file, const base::FilePath& unzip_dir,
         const std::string& error) {
        if (!context_weak) {
          return;
        }

        if (!unzip_dir.empty()) {
          DCHECK(error.empty());
          UnpackedInstaller::Create(context_weak.get())->Load(unzip_dir);
          return;
        }
        DCHECK(!error.empty());
        LoadErrorReporter::GetInstance()->ReportLoadError(
            zip_file, error, context_weak.get(),
            /*noisy_on_failure=*/true);
      },
      context->GetWeakPtr());
}

}  // namespace extensions
