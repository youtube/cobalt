// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_constants.h"

namespace installer {

// Elements that make up install paths.
const wchar_t kChromeArchive[] = L"chrome.7z";
const wchar_t kChromeCompressedArchive[] = L"chrome.packed.7z";
const wchar_t kVisualElements[] = L"VisualElements";
const wchar_t kVisualElementsManifest[] = L"chrome.VisualElementsManifest.xml";

// Sub directory of install source package under install temporary directory.
const wchar_t kInstallSourceDir[] = L"source";
const wchar_t kInstallSourceChromeDir[] = L"Chrome-bin";

const wchar_t kMediaPlayerRegPath[] =
    L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList";

const char kCourgette[] = "courgette";
const char kBsdiff[] = "bsdiff";
#if BUILDFLAG(ZUCCHINI)
const char kZucchini[] = "zucchini";
#endif  // BUILDFLAG(ZUCCHINI)

namespace switches {

// Set the MSI-managed DisplayVersion in the registry to match Chrome's real
// version number. The parameter to this option specifies the product-id in
// the registry under HKLM.
const char kSetDisplayVersionProduct[] = "set-display-version-product";
const char kSetDisplayVersionValue[] = "set-display-version-value";

// A handle number for an event to be signaled when the process is ready for
// work.
const char kStartupEventHandle[] = "startup-event-handle";

// Run setup.exe to conduct a post-update experiment.
const char kUserExperiment[] = "user-experiment";

// Sets the operation to do for the downgrade cleanup. Only the values "revert"
// and "cleanup" are accepted. If the operation is "cleanup", cleans up the
// necessary data, if the operation is "revert", reverts any cleanup previously
// done. Any other value will have no effect and will generate an error.
const char kCleanupForDowngradeOperation[] = "cleanup-for-downgrade-operation";

// Indicates the version to which the browser is being downgraded. All state
// written by versions newer than that indicated will be cleaned.
const char kCleanupForDowngradeVersion[] = "cleanup-for-downgrade-version";

}  // namespace switches

}  // namespace installer
