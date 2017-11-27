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

#ifndef COBALT_CSP_SOURCE_LIST_DIRECTIVE_H_
#define COBALT_CSP_SOURCE_LIST_DIRECTIVE_H_

#include <string>

#include "cobalt/csp/content_security_policy.h"
#include "cobalt/csp/directive.h"
#include "cobalt/csp/source_list.h"

namespace cobalt {
namespace csp {

class SourceListDirective : public Directive {
 public:
  class LocalNetworkChecker : public SourceList::LocalNetworkCheckerInterface {
   public:
    bool IsIPInLocalNetwork(const SbSocketAddress& destination) const override;
    bool IsIPInPrivateRange(const SbSocketAddress& destination) const override;
  };

  SourceListDirective(const std::string& name, const std::string& value,
                      ContentSecurityPolicy* policy);

  bool Allows(const GURL& url,
              ContentSecurityPolicy::RedirectStatus redirect_status) const;
  bool AllowInline() const;
  bool AllowEval() const;
  bool AllowNonce(const std::string& nonce) const;
  bool AllowHash(const HashValue& hash_value) const;
  bool hash_or_nonce_present() const;
  uint8 hash_algorithms_used() const;

 private:
  // Note: Ensure |local_network_checker_| is initialized before |source_list_|.
  LocalNetworkChecker local_network_checker_;
  SourceList source_list_;
  DISALLOW_COPY_AND_ASSIGN(SourceListDirective);
};

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_SOURCE_LIST_DIRECTIVE_H_
