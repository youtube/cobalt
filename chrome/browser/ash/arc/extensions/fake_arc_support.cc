// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/extensions/fake_arc_support.h"

#include <string>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/extensions/arc_support_message_host.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

void SerializeAndSend(extensions::NativeMessageHost* native_message_host,
                      const base::Value::Dict& message) {
  DCHECK(native_message_host);
  std::string message_string;
  if (!base::JSONWriter::Write(message, &message_string)) {
    NOTREACHED();
    return;
  }
  native_message_host->OnMessage(message_string);
}

}  // namespace

namespace arc {

FakeArcSupport::FakeArcSupport(ArcSupportHost* support_host)
    : support_host_(support_host) {
  DCHECK(support_host_);
  support_host_->SetRequestOpenAppCallbackForTesting(base::BindRepeating(
      &FakeArcSupport::Open, weak_ptr_factory_.GetWeakPtr()));
}

FakeArcSupport::~FakeArcSupport() {
  // Ensure that message host is disconnected.
  if (!native_message_host_)
    return;
  UnsetMessageHost();
}

void FakeArcSupport::Open(Profile* profile) {
  DCHECK(!native_message_host_);
  native_message_host_ = ArcSupportMessageHost::Create(profile);
  native_message_host_->Start(this);
  support_host_->SetMessageHost(
      static_cast<ArcSupportMessageHost*>(native_message_host_.get()));
}

void FakeArcSupport::Close() {
  DCHECK(native_message_host_);
  native_message_host_->OnMessage("{\"event\": \"onWindowClosed\"}");
  UnsetMessageHost();
}

void FakeArcSupport::EmulateAuthSuccess() {
  DCHECK_EQ(ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH, ui_page_);
  base::Value::Dict message;
  message.Set("event", "onAuthSucceeded");
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::EmulateAuthFailure(const std::string& error_msg) {
  DCHECK(native_message_host_);
  DCHECK_EQ(ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH, ui_page_);
  base::Value::Dict message;
  message.Set("event", "onAuthFailed");
  message.Set("errorMessage", error_msg);
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::ClickAgreeButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::TERMS);
  base::Value::Dict message;
  message.Set("event", "onAgreed");
  message.Set("tosContent", tos_content_);
  message.Set("tosShown", tos_shown_);
  message.Set("isMetricsEnabled", metrics_mode_);
  message.Set("isBackupRestoreEnabled", backup_and_restore_mode_);
  message.Set("isBackupRestoreManaged", backup_and_restore_managed_);
  message.Set("isLocationServiceEnabled", location_service_mode_);
  message.Set("isLocationServiceManaged", location_service_managed_);
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::ClickCancelButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::TERMS);
  base::Value::Dict message;
  message.Set("event", "onCanceled");
  message.Set("tosContent", tos_content_);
  message.Set("tosShown", tos_shown_);
  message.Set("isMetricsEnabled", metrics_mode_);
  message.Set("isBackupRestoreEnabled", backup_and_restore_mode_);
  message.Set("isBackupRestoreManaged", backup_and_restore_managed_);
  message.Set("isLocationServiceEnabled", location_service_mode_);
  message.Set("isLocationServiceManaged", location_service_managed_);
  SerializeAndSend(native_message_host_.get(), message);
  // The cancel button closes the window.
  Close();
}

void FakeArcSupport::ClickAdAuthCancelButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH);
  // The cancel button closes the window.
  Close();
}

void FakeArcSupport::ClickRetryButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onRetryClicked\"}");
}

void FakeArcSupport::ClickSendFeedbackButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onSendFeedbackClicked\"}");
}

void FakeArcSupport::ClickRunNetworkTestsButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onRunNetworkTestsClicked\"}");
}

void FakeArcSupport::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeArcSupport::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeArcSupport::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

void FakeArcSupport::UnsetMessageHost() {
  support_host_->UnsetMessageHost(
      static_cast<ArcSupportMessageHost*>(native_message_host_.get()));
  native_message_host_.reset();
}

void FakeArcSupport::PostMessageFromNativeHost(
    const std::string& message_string) {
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(message_string);
  DCHECK(parsed_json);

  const base::Value::Dict& message = parsed_json->GetDict();
  const std::string* action = message.FindString("action");
  if (!action) {
    NOTREACHED() << message_string;
    return;
  }

  ArcSupportHost::UIPage prev_ui_page = ui_page_;
  if (*action == "initialize") {
    // Do nothing as emulation.
  } else if (*action == "showPage") {
    const std::string* page = message.FindString("page");
    if (!page) {
      NOTREACHED() << message_string;
      return;
    }
    if (*page == "terms") {
      ui_page_ = ArcSupportHost::UIPage::TERMS;
    } else if (*page == "arc-loading") {
      ui_page_ = ArcSupportHost::UIPage::ARC_LOADING;
    } else if (*page == "active-directory-auth") {
      ui_page_ = ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH;
      const std::string* federation_url =
          message.FindStringByDottedPath("options.federationUrl");
      const std::string* device_management_url_prefix =
          message.FindStringByDottedPath("options.deviceManagementUrlPrefix");
      if (!federation_url || !device_management_url_prefix) {
        NOTREACHED() << message_string;
        return;
      }
      active_directory_auth_federation_url_ = *federation_url;
      active_directory_auth_device_management_url_prefix_ =
          *device_management_url_prefix;
    } else {
      NOTREACHED() << message_string;
    }
  } else if (*action == "showErrorPage") {
    ui_page_ = ArcSupportHost::UIPage::ERROR;
  } else if (*action == "setMetricsMode") {
    absl::optional<bool> opt = message.FindBool("enabled");
    if (!opt) {
      NOTREACHED() << message_string;
      return;
    }
    metrics_mode_ = opt.value();
  } else if (*action == "setBackupAndRestoreMode") {
    absl::optional<bool> opt = message.FindBool("enabled");
    if (!opt) {
      NOTREACHED() << message_string;
      return;
    }
    backup_and_restore_mode_ = opt.value();
  } else if (*action == "setLocationServiceMode") {
    absl::optional<bool> opt = message.FindBool("enabled");
    if (!opt) {
      NOTREACHED() << message_string;
      return;
    }
    location_service_mode_ = opt.value();
  } else if (*action == "closeWindow") {
    // Do nothing as emulation.
  } else if (*action == "setWindowBounds") {
    // Do nothing as emulation.
  } else {
    // Unknown or unsupported action.
    NOTREACHED() << message_string;
  }
  if (prev_ui_page != ui_page_) {
    for (auto& observer : observer_list_)
      observer.OnPageChanged(ui_page_);
  }
}

void FakeArcSupport::CloseChannel(const std::string& error_message) {
  NOTREACHED();
}

}  // namespace arc
