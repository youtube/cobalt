// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/h5vcc_url_handler.h"

#include <string>

#include "base/bind.h"
#include "cobalt/browser/browser_module.h"

namespace cobalt {
namespace browser {

namespace {
// Utility functions to get info from a GURL. Because our scheme is
// non-standard, the methods in GURL will not parse the components for us.
std::string GetH5vccUrlType(const GURL& url) {
  const std::string path = url.path();
  size_t start = path.find_first_not_of("/");
  if (start == std::string::npos) {
    DLOG(WARNING) << "Cannot determine type of URL.";
    return "";
  }
  size_t end = path.find_first_of("/?&", start);
  if (end != std::string::npos) {
    end -= start;
  }
  return path.substr(start, end);
}

std::string GetH5vccUrlQuery(const GURL& url) {
  const std::string path = url.path();
  size_t start = path.find("?");
  if (start == std::string::npos) {
    DLOG(WARNING) << "Cannot find query string in URL.";
    return "";
  }
  return path.substr(start + 1);
}

std::string GetH5vccUrlQueryParam(const GURL& url, const std::string& name) {
  const std::string query = GetH5vccUrlQuery(url);
  size_t name_start = query.find(name);
  if (name_start == std::string::npos) {
    DLOG(WARNING) << "Query parameter " << name << " not found in URL.";
    return "";
  }
  size_t name_end = query.find("=", name_start);
  if (name_end == std::string::npos) {
    DLOG(WARNING) << "Value of " << name << " not found in URL.";
    return "";
  }
  size_t value_start = name_end - name_start + 1;
  size_t value_end = query.find_first_of("&", value_start);
  if (value_end != std::string::npos) {
    value_end -= value_start;
  }
  return query.substr(value_start, value_end);
}

const char kH5vccScheme[] = "h5vcc";
const char kNetworkFailure[] = "network-failure";
const char kSignedOut[] = "signed-out";
const char kAgeRestricted[] = "age-restricted";

const char kRetryParam[] = "retry-url";
}  // namespace

H5vccURLHandler::H5vccURLHandler(BrowserModule* browser_module,
                                 system_window::SystemWindow* system_window,
                                 account::AccountManager* account_manager)
    : ALLOW_THIS_IN_INITIALIZER_LIST(URLHandler(
          browser_module,
          base::Bind(&H5vccURLHandler::HandleURL, base::Unretained(this)))),
      system_window_(system_window),
      account_manager_(account_manager) {}

bool H5vccURLHandler::HandleURL(const GURL& url) {
  bool was_handled = false;
  if (url.SchemeIs(kH5vccScheme)) {
    url_ = url;
    const std::string type = GetH5vccUrlType(url);
    if (type == kNetworkFailure) {
      was_handled = HandleNetworkFailure();
    } else if (type == kSignedOut) {
      was_handled = HandleSignedOut();
    } else if (type == kAgeRestricted) {
      was_handled = HandleAgeRestricted();
    } else {
      LOG(WARNING) << "Unknown h5vcc URL type: " << type;
    }
  }
  return was_handled;
}

bool H5vccURLHandler::HandleNetworkFailure() {
  system_window::SystemWindow::DialogOptions dialog_options;
  dialog_options.message_code =
      system_window::SystemWindow::kDialogConnectionError;
  dialog_options.callback = base::Bind(
      &H5vccURLHandler::OnNetworkFailureDialogResponse, base::Unretained(this));
  system_window_->ShowDialog(dialog_options);
  return true;
}

bool H5vccURLHandler::HandleSignedOut() {
  system_window::SystemWindow::DialogOptions dialog_options;
  dialog_options.message_code =
      system_window::SystemWindow::kDialogUserSignedOut;
  dialog_options.callback = base::Bind(
      &H5vccURLHandler::OnSignedOutDialogResponse, base::Unretained(this));
  system_window_->ShowDialog(dialog_options);
  return true;
}

bool H5vccURLHandler::HandleAgeRestricted() {
  system_window::SystemWindow::DialogOptions dialog_options;
  dialog_options.message_code =
      system_window::SystemWindow::kDialogUserAgeRestricted;
  system_window_->ShowDialog(dialog_options);
  return true;
}

void H5vccURLHandler::OnNetworkFailureDialogResponse(
    system_window::SystemWindow::DialogResponse response) {
  UNREFERENCED_PARAMETER(response);
  const std::string retry_url = GetH5vccUrlQueryParam(url_, kRetryParam);
  if (retry_url.length() > 0) {
    GURL url(retry_url);
    if (url.is_valid()) {
      browser_module()->Navigate(GURL(retry_url));
    }
  }
}

void H5vccURLHandler::OnSignedOutDialogResponse(
    system_window::SystemWindow::DialogResponse response) {
  UNREFERENCED_PARAMETER(response);
  account_manager_->StartSignIn();
}

}  // namespace browser
}  // namespace cobalt
