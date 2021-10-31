// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_SECURITY_POLICY_VIOLATION_EVENT_H_
#define COBALT_DOM_SECURITY_POLICY_VIOLATION_EVENT_H_

#include <string>

#include "cobalt/dom/event.h"

namespace cobalt {
namespace dom {

// https://www.w3.org/TR/CSP/#securitypolicyviolationevent-interface
class SecurityPolicyViolationEvent : public Event {
 public:
  explicit SecurityPolicyViolationEvent(const std::string& type);

  SecurityPolicyViolationEvent(const std::string& document_uri,
                               const std::string& referrer,
                               const std::string& blocked_uri,
                               const std::string& violated_directive,
                               const std::string& effective_directive,
                               const std::string& original_policy,
                               const std::string& source_file, int status_code,
                               int line_number, int column_number);

  const std::string& document_uri() const { return document_uri_; }
  const std::string& referrer() const { return referrer_; }
  const std::string& blocked_uri() const { return blocked_uri_; }
  const std::string& violated_directive() const { return violated_directive_; }
  const std::string& effective_directive() const {
    return effective_directive_;
  }
  const std::string& original_policy() const { return original_policy_; }
  const std::string& source_file() const { return source_file_; }
  int status_code() const { return status_code_; }
  int line_number() const { return line_number_; }
  int column_number() const { return column_number_; }

  DEFINE_WRAPPABLE_TYPE(SecurityPolicyViolationEvent);

 private:
  std::string document_uri_;
  std::string referrer_;
  std::string blocked_uri_;
  std::string violated_directive_;
  std::string effective_directive_;
  std::string original_policy_;
  std::string source_file_;
  int status_code_;
  int line_number_;
  int column_number_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SECURITY_POLICY_VIOLATION_EVENT_H_
