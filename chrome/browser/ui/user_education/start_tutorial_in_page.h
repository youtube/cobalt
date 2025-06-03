// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_START_TUTORIAL_IN_PAGE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_START_TUTORIAL_IN_PAGE_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Browser;

// Utility for opening a page and starting a tutorial. The object exists
// only as long as the operation is in progress.
//
// Example:
//
//    // Open security settings page and start a tutorial.
//    StartTutorialInPage::Params params;
//
//    // The page does not need to be a WebUI page. It could be any page
//    // that is contextually needed for the tutorial.
//    // components/user_education/webui/README.md
//    params.target_url = "chrome://settings/security";
//
//    // The tutorial to start.
//    params.tutorial_id = kEnhancedSecuritySettingElementId;
//
//    // Log when the tutorial has been triggered. The page navigation
//    // may not be complete yet.
//    params.callback = base::BindOnce(
//      &::MaybeLogEnhancedSecurityTutorialStarted);
//
//    // Start the procedure of showing the tutorial and triggering
//    // the navigation.
//    StartTutorialInPage::Start(browser_, std::move(params));
//
class StartTutorialInPage {
 public:
  // Called when the tutorial is started.
  using Callback = base::OnceCallback<void(
      user_education::TutorialService* tutorial_service)>;

  // Specifies how a page should be open to show a help bubble.
  struct Params {
    Params();
    ~Params();
    Params(Params&& other);
    Params& operator=(Params&& other);

    // The page to open. If not specified, the current page will be used.
    absl::optional<GURL> target_url = absl::nullopt;

    // See overwrite_active_tab in show_promo_in_page.h
    bool overwrite_active_tab = false;

    // Callback that notifies when the tutorial was triggered. The
    // callback is passed the TutorialService to use to log or check
    // if the tutorial was started successfully. Typically used for
    // testing or metrics collection.
    Callback callback = base::DoNothing();

    // The ID of the tutorial to start.
    absl::optional<user_education::TutorialIdentifier> tutorial_id =
        absl::nullopt;
  };

  StartTutorialInPage(const StartTutorialInPage&) = delete;
  virtual ~StartTutorialInPage();
  void operator=(const StartTutorialInPage&) = delete;

  // Opens the page in `browser` and starts a Tutorial as described by
  // `params`. This method must be called on the UI thread.
  static base::WeakPtr<StartTutorialInPage> Start(Browser* browser,
                                                  Params params);

 protected:
  StartTutorialInPage();
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_START_TUTORIAL_IN_PAGE_H_
