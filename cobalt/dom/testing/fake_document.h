// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_FAKE_DOCUMENT_H_
#define COBALT_DOM_TESTING_FAKE_DOCUMENT_H_

#include <memory>
#include <utility>

#include "cobalt/dom/document.h"

#include "cobalt/web/csp_delegate_factory.h"

namespace cobalt {
namespace dom {
namespace testing {

class FakeDocument : public dom::Document {
 public:
  explicit FakeDocument(dom::HTMLElementContext* html_element_context)
      : dom::Document(html_element_context) {
    web::WindowOrWorkerGlobalScope::Options options(
        base::ApplicationState::kApplicationStateStarted);
    std::unique_ptr<web::CspViolationReporter> violation_reporter(
        new web::CspViolationReporter(nullptr, options.post_sender));
    csp_delegate_ = web::CspDelegateFactory::GetInstance()->Create(
        options.csp_enforcement_mode, std::move(violation_reporter),
        environment_settings()->creation_url(), options.require_csp,
        options.csp_policy_changed_callback,
        options.csp_insecure_allowed_token);
  }

  FakeDocument(dom::HTMLElementContext* html_element_context,
               dom::Document::Options doc_options)
      : dom::Document(html_element_context, doc_options) {
    web::WindowOrWorkerGlobalScope::Options options(
        base::ApplicationState::kApplicationStateStarted);
    std::unique_ptr<web::CspViolationReporter> violation_reporter(
        new web::CspViolationReporter(nullptr, options.post_sender));
    csp_delegate_ = web::CspDelegateFactory::GetInstance()->Create(
        options.csp_enforcement_mode, std::move(violation_reporter),
        environment_settings()->creation_url(), options.require_csp,
        options.csp_policy_changed_callback,
        options.csp_insecure_allowed_token);
  }

  web::CspDelegate* GetCSPDelegate() const override {
    return csp_delegate_.get();
  }

 private:
  std::unique_ptr<web::CspDelegate> csp_delegate_;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_FAKE_DOCUMENT_H_
