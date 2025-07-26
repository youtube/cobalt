// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/version/version_loader.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

VersionHandlerChromeOS::VersionHandlerChromeOS() = default;

VersionHandlerChromeOS::~VersionHandlerChromeOS() = default;

void VersionHandlerChromeOS::OnJavascriptDisallowed() {
  VersionHandler::OnJavascriptDisallowed();
  weak_factory_.InvalidateWeakPtrs();
}

void VersionHandlerChromeOS::HandleRequestVersionInfo(
    const base::Value::List& args) {
  VersionHandler::HandleRequestVersionInfo(args);

  // Start the asynchronous load of the versions.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&VersionHandlerChromeOS::OnPlatformVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetFirmware),
      base::BindOnce(&VersionHandlerChromeOS::OnFirmwareVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&VersionHandlerChromeOS::GetArcAndArcAndroidSdkVersions),
      base::BindOnce(&VersionHandlerChromeOS::OnArcAndArcAndroidSdkVersions,
                     weak_factory_.GetWeakPtr()));
}

void VersionHandlerChromeOS::OnPlatformVersion(
    const std::optional<std::string>& version) {
  FireWebUIListener("return-platform-version",
                    base::Value(version.value_or("0.0.0.0")));
}

void VersionHandlerChromeOS::OnFirmwareVersion(const std::string& version) {
  FireWebUIListener("return-firmware-version", base::Value(version));
}

void VersionHandlerChromeOS::OnArcAndArcAndroidSdkVersions(
    const std::string& version) {
  FireWebUIListener("return-arc-and-arc-android-sdk-versions",
                    base::Value(version));
}

// static
std::string VersionHandlerChromeOS::GetArcAndArcAndroidSdkVersions() {
  std::string arc_version = chromeos::version_loader::GetArcVersion();
  std::optional<std::string> arc_android_sdk_version =
      chromeos::version_loader::GetArcAndroidSdkVersion();
  if (!arc_android_sdk_version.has_value()) {
    arc_android_sdk_version = base::UTF16ToUTF8(
        l10n_util::GetStringUTF16(IDS_ARC_SDK_VERSION_UNKNOWN));
  }
  std::string sdk_label =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ARC_SDK_VERSION_LABEL));
  std::string labeled_version =
      base::StringPrintf("%s %s %s", arc_version.c_str(), sdk_label.c_str(),
                         arc_android_sdk_version->c_str());
  return labeled_version;
}
