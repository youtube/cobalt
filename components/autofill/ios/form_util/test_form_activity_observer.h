// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_OBSERVER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_OBSERVER_H_

#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/autofill/ios/form_util/form_activity_params.h"

namespace web {
class WebState;
}

namespace autofill {
// Arguments passed to |DocumentSubmitted|.
struct TestSubmitDocumentInfo {
  TestSubmitDocumentInfo();
  web::WebState* web_state = nullptr;
  web::WebFrame* sender_frame = nullptr;
  std::string form_name;
  std::string form_data;
  bool has_user_gesture;
};

// Arguments passed to |FormActivityRegistered|.
struct TestFormActivityInfo {
  web::WebState* web_state = nullptr;
  web::WebFrame* sender_frame = nullptr;
  FormActivityParams form_activity;
};

// Arguments passed to |FormRemovalRegistered|.
struct TestFormRemovalInfo {
  web::WebState* web_state = nullptr;
  web::WebFrame* sender_frame = nullptr;
  FormRemovalParams form_removal_params;
};

class TestFormActivityObserver : public autofill::FormActivityObserver {
 public:
  explicit TestFormActivityObserver(web::WebState* web_state);

  TestFormActivityObserver(const TestFormActivityObserver&) = delete;
  TestFormActivityObserver& operator=(const TestFormActivityObserver&) = delete;

  ~TestFormActivityObserver() override;

  // Arguments passed to |DocumentSubmitted|.
  TestSubmitDocumentInfo* submit_document_info();

  // Arguments passed to |FormActivityRegistered|.
  TestFormActivityInfo* form_activity_info();

  // Arguments passed to |FormRemoved|.
  TestFormRemovalInfo* form_removal_info();

  void DocumentSubmitted(web::WebState* web_state,
                         web::WebFrame* sender_frame,
                         const std::string& form_name,
                         const std::string& form_data,
                         bool has_user_gesture) override;

  void FormActivityRegistered(web::WebState* web_state,
                              web::WebFrame* sender_frame,
                              const FormActivityParams& params) override;

  void FormRemoved(web::WebState* web_state,
                   web::WebFrame* sender_frame,
                   const FormRemovalParams& params) override;

 private:
  web::WebState* web_state_ = nullptr;
  std::unique_ptr<TestSubmitDocumentInfo> submit_document_info_;
  std::unique_ptr<TestFormActivityInfo> form_activity_info_;
  std::unique_ptr<TestFormRemovalInfo> form_removal_info_;
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_OBSERVER_H_
