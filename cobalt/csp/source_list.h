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

#ifndef COBALT_CSP_SOURCE_LIST_H_
#define COBALT_CSP_SOURCE_LIST_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/csp/source.h"
#include "cobalt/network/local_network.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

class SourceList {
 public:
  struct LocalNetworkCheckerInterface {
    virtual bool IsIPInLocalNetwork(
        const SbSocketAddress& destination) const = 0;
    virtual bool IsIPInPrivateRange(
        const SbSocketAddress& destination) const = 0;

    virtual ~LocalNetworkCheckerInterface() {}
  };

  SourceList(const LocalNetworkCheckerInterface* checker,
             ContentSecurityPolicy* policy, const std::string& directive_name);
  void Parse(const base::StringPiece& begin);

  bool Matches(const GURL& url,
               ContentSecurityPolicy::RedirectStatus =
                   ContentSecurityPolicy::kDidNotRedirect) const;
  bool AllowInline() const;
  bool AllowEval() const;
  bool AllowNonce(const std::string& nonce) const;
  bool AllowHash(const HashValue& hash_value) const;
  uint8 hash_algorithms_used() const { return hash_algorithms_used_; }
  bool hash_or_nonce_present() const;

  static bool ParseHash(const char* begin, const char* end, DigestValue* hash,
                        HashAlgorithm* hash_algorithm);

 private:
  bool ParseSource(const char* begin, const char* end,
                   SourceConfig* source_config);
  bool ParseScheme(const char* begin, const char* end, std::string* scheme);
  bool ParseHost(const char* begin, const char* end, std::string* host,
                 SourceConfig::WildcardDisposition* host_disposition);
  bool ParsePort(const char* begin, const char* end, int* port,
                 SourceConfig::WildcardDisposition* port_disposition);
  bool ParsePath(const char* begin, const char* end, std::string* path);
  bool ParseNonce(const char* begin, const char* end, std::string* nonce);

  void AddSourceLocalhost();
  void AddSourceLocalNetwork();
  void AddSourcePrivateRange();
  void AddSourceSelf();
  void AddSourceStar();
  void AddSourceUnsafeInline();
  void AddSourceUnsafeEval();
  void AddSourceNonce(const std::string& nonce);
  void AddSourceHash(const HashAlgorithm&, const DigestValue& hash);

  ContentSecurityPolicy* policy_;
  std::vector<Source> list_;
  std::string directive_name_;
  bool allow_self_;
  bool allow_star_;
  bool allow_inline_;
  bool allow_eval_;
  bool allow_insecure_connections_to_local_network_;
  bool allow_insecure_connections_to_localhost_;
  bool allow_insecure_connections_to_private_range_;
  base::hash_set<std::string> nonces_;
  // TODO: This is a hash_set in blink. Need to implement
  // a hash for HashValue.
  std::set<HashValue> hashes_;
  uint8 hash_algorithms_used_;

  const LocalNetworkCheckerInterface* local_network_checker_;

  FRIEND_TEST_ALL_PREFIXES(SourceListTest, TestInsecureLocalNetworkDefault);

  DISALLOW_COPY_AND_ASSIGN(SourceList);
};

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_SOURCE_LIST_H_
