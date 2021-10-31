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

#include "cobalt/csp/source_list_directive.h"

namespace cobalt {
namespace csp {

bool SourceListDirective::LocalNetworkChecker::IsIPInLocalNetwork(
    const SbSocketAddress& destination) const {
  return ::cobalt::network::IsIPInLocalNetwork(destination);
}

bool SourceListDirective::LocalNetworkChecker::IsIPInPrivateRange(
    const SbSocketAddress& destination) const {
  return ::cobalt::network::IsIPInPrivateRange(destination);
}

SourceListDirective::SourceListDirective(const std::string& name,
                                         const std::string& value,
                                         ContentSecurityPolicy* policy)
    : Directive(name, value, policy),
      source_list_(&local_network_checker_, policy, name) {
  source_list_.Parse(base::StringPiece(value));
}

bool SourceListDirective::Allows(
    const GURL& url,
    ContentSecurityPolicy::RedirectStatus redirectStatus) const {
  return source_list_.Matches(url.is_empty() ? policy()->url() : url,
                              redirectStatus);
}

bool SourceListDirective::AllowInline() const {
  return source_list_.AllowInline();
}

bool SourceListDirective::AllowEval() const { return source_list_.AllowEval(); }

bool SourceListDirective::AllowNonce(const std::string& nonce) const {
  std::string trimmed_nonce;
  TrimWhitespaceASCII(nonce, base::TRIM_ALL, &trimmed_nonce);
  return source_list_.AllowNonce(trimmed_nonce);
}

bool SourceListDirective::AllowHash(const HashValue& hashValue) const {
  return source_list_.AllowHash(hashValue);
}

bool SourceListDirective::hash_or_nonce_present() const {
  return source_list_.hash_or_nonce_present();
}

uint8 SourceListDirective::hash_algorithms_used() const {
  return source_list_.hash_algorithms_used();
}

}  // namespace csp
}  // namespace cobalt
