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

#ifndef COBALT_CSP_DIRECTIVE_H_
#define COBALT_CSP_DIRECTIVE_H_

#include <string>

#include "base/basictypes.h"

namespace cobalt {
namespace csp {

class ContentSecurityPolicy;

class Directive {
 public:
  Directive(const std::string& name, const std::string& value,
            ContentSecurityPolicy* policy);

  const std::string& text() const { return text_; }

 protected:
  ContentSecurityPolicy* policy() const { return policy_; }

 private:
  std::string text_;
  ContentSecurityPolicy* policy_;

  DISALLOW_COPY_AND_ASSIGN(Directive);
};

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_DIRECTIVE_H_
