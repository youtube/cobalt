// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_TEST_WITH_WEB_STATE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_TEST_WITH_WEB_STATE_H_

#import "ios/web/public/test/web_test_with_web_state.h"

namespace web {
class WebClient;
class WebFrame;
}  // namespace web

// A fixture to set up testing of Autofill methods.
class AutofillTestWithWebState : public web::WebTestWithWebState {
 protected:
  AutofillTestWithWebState(std::unique_ptr<web::WebClient> web_client);

  // Injects initial renderer is value into the |frame| and waits for
  // completion.
  void SetUpForUniqueIds(web::WebFrame* frame);

  // Toggles tracking form mutations in a |frame| and waits for completion.
  void TrackFormMutations(web::WebFrame* frame);

  // web::WebTestWithWebState:
  id ExecuteJavaScript(NSString* script) override;
};

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_TEST_WITH_WEB_STATE_H_
