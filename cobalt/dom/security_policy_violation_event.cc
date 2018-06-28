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

#include "cobalt/dom/security_policy_violation_event.h"

#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"

namespace cobalt {
namespace dom {

SecurityPolicyViolationEvent::SecurityPolicyViolationEvent(
    const std::string& type)
    : Event(type), status_code_(0), line_number_(0), column_number_(0) {}

SecurityPolicyViolationEvent::SecurityPolicyViolationEvent(
    const std::string& document_uri, const std::string& referrer,
    const std::string& blocked_uri, const std::string& violated_directive,
    const std::string& effective_directive, const std::string& original_policy,
    const std::string& source_file, int status_code, int line_number,
    int column_number)
    : Event(base::Tokens::securitypolicyviolation()),
      document_uri_(document_uri),
      referrer_(referrer),
      blocked_uri_(blocked_uri),
      violated_directive_(violated_directive),
      effective_directive_(effective_directive),
      original_policy_(original_policy),
      source_file_(source_file),
      status_code_(status_code),
      line_number_(line_number),
      column_number_(column_number) {}

}  // namespace dom
}  // namespace cobalt
