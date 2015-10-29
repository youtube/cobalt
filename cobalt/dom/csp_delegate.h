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

#ifndef DOM_CSP_DELEGATE_H_
#define DOM_CSP_DELEGATE_H_

#include <string>
#include <vector>

#include "cobalt/csp/content_security_policy.h"

namespace cobalt {
namespace dom {

class Document;

// Object that represents a Content Security Policy for a particular document.
// Owned by the Document. Objects wishing to enforce CSP need to query the
// delegate to decide if they are permitted to load a resource.
// This also is responsible for reporting violations, i.e. posting JSON
// reports to any reporting endpoints described by the policy.
class CSPDelegate : public csp::ContentSecurityPolicy::Delegate {
 public:
  explicit CSPDelegate(Document* document);
  ~CSPDelegate() OVERRIDE;

  csp::ContentSecurityPolicy* csp() const;

  bool CanLoadImage(const GURL& url) const;

  // From csp::ContentSecurityPolicyDelegate
  GURL url() const OVERRIDE;
  void ReportViolation(const std::string& directive_text,
                       const std::string& effective_directive,
                       const std::string& console_message,
                       const GURL& blocked_url,
                       const std::vector<std::string>& endpoints,
                       const std::string& header) OVERRIDE;
  void SetReferrerPolicy(csp::ReferrerPolicy policy) OVERRIDE;

 private:
  Document* document_;
  scoped_ptr<csp::ContentSecurityPolicy> csp_;

  DISALLOW_COPY_AND_ASSIGN(CSPDelegate);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_CSP_DELEGATE_H_
