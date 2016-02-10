/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_BROWSER_H5VCC_URL_HANDLER_H_
#define COBALT_BROWSER_H5VCC_URL_HANDLER_H_

#include "cobalt/account/account_manager.h"
#include "cobalt/browser/url_handler.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace browser {

// Handler for URLs with the scheme h5vcc://. These URLs are not loaded into a
// web module using the usual loader/fetcher mechanism, but are instead
// handled separately, e.g. by showing a system dialog.
class H5vccURLHandler : public URLHandler {
 public:
  explicit H5vccURLHandler(BrowserModule* browser_module,
                           system_window::SystemWindow* system_window,
                           account::AccountManager* account_manager);
  ~H5vccURLHandler() {}

 private:
  bool HandleURL(const GURL& url);
  bool HandleNetworkFailure();
  bool HandleSignedOut();
  bool HandleAgeRestricted();

  // Dialog response handlers.
  void OnNetworkFailureDialogResponse(
      system_window::SystemWindow::DialogResponse response);
  void OnSignedOutDialogResponse(
      system_window::SystemWindow::DialogResponse response);

  system_window::SystemWindow* system_window_;
  account::AccountManager* account_manager_;
  GURL url_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_H5VCC_URL_HANDLER_H_
