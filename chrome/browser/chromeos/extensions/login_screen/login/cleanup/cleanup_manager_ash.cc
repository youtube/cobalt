// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_ash.h"

#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browser_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/clipboard_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/extension_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/lacros_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/print_jobs_cleanup_handler.h"

namespace chromeos {

namespace {

// Must kept in sync with the CleanupHandler variant in
// tools/metrics/histograms/metadata/enterprise/histograms.xml
constexpr char kBrowserCleanupHandlerHistogramName[] = "Browser";
constexpr char kClipboardCleanupHandlerHistogramName[] = "Clipboard";
constexpr char kExtensionCleanupHandlerHistogramName[] = "Extension";
constexpr char kFilesCleanupHandlerHistogramName[] = "Files";
constexpr char kLacrosCleanupHandlerHistogramName[] = "Lacros";
constexpr char kPrintJobsCleanupHandlerHistogramName[] = "PrintJobs";

}  // namespace

// static
CleanupManagerAsh* CleanupManagerAsh::Get() {
  static base::NoDestructor<CleanupManagerAsh> instance;
  return instance.get();
}

CleanupManagerAsh::CleanupManagerAsh() = default;

CleanupManagerAsh::~CleanupManagerAsh() = default;

void CleanupManagerAsh::InitializeCleanupHandlers() {
  cleanup_handlers_.insert({kBrowserCleanupHandlerHistogramName,
                            std::make_unique<BrowserCleanupHandler>()});
  cleanup_handlers_.insert({kFilesCleanupHandlerHistogramName,
                            std::make_unique<FilesCleanupHandler>()});
  cleanup_handlers_.insert({kLacrosCleanupHandlerHistogramName,
                            std::make_unique<LacrosCleanupHandler>()});
  cleanup_handlers_.insert({kClipboardCleanupHandlerHistogramName,
                            std::make_unique<ClipboardCleanupHandler>()});
  cleanup_handlers_.insert({kPrintJobsCleanupHandlerHistogramName,
                            std::make_unique<PrintJobsCleanupHandler>()});
  cleanup_handlers_.insert({kExtensionCleanupHandlerHistogramName,
                            std::make_unique<ExtensionCleanupHandler>()});
}

}  // namespace chromeos
