// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/version/version_handler.h"
#include "chromeos/version/version_loader.h"

// VersionHandlerChromeOS is responsible for loading the Chrome OS
// version.
class VersionHandlerChromeOS : public VersionHandler {
 public:
  VersionHandlerChromeOS();

  VersionHandlerChromeOS(const VersionHandlerChromeOS&) = delete;
  VersionHandlerChromeOS& operator=(const VersionHandlerChromeOS&) = delete;

  ~VersionHandlerChromeOS() override;

  // VersionHandler overrides:
  void OnJavascriptDisallowed() override;
  void HandleRequestVersionInfo(const base::Value::List& args) override;
  void RegisterMessages() override;

  // Callbacks from chromeos::VersionLoader.
  void OnVersion(const absl::optional<std::string>& version);
  void OnOSFirmware(const std::string& version);
  void OnArcAndArcAndroidSdkVersions(const std::string& version);

  // Callback for the "crosUrlVersionRedirect" message.
  void HandleCrosUrlVersionRedirect(const base::Value::List& args);

 private:
  base::WeakPtrFactory<VersionHandlerChromeOS> weak_factory_{this};
  static std::string GetArcAndArcAndroidSdkVersions();
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_CHROMEOS_H_
