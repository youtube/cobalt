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

#ifndef COBALT_CSP_SOURCE_H_
#define COBALT_CSP_SOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "cobalt/csp/content_security_policy.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

struct SourceConfig {
  enum WildcardDisposition {
    kHasWildcard,
    kNoWildcard,
  };
  SourceConfig()
      : port(url::PORT_UNSPECIFIED),
        host_wildcard(kNoWildcard),
        port_wildcard(kNoWildcard) {}

  std::string scheme;
  std::string host;
  std::string path;
  int port;
  WildcardDisposition host_wildcard;
  WildcardDisposition port_wildcard;
};

class Source {
 public:
  Source(ContentSecurityPolicy* policy, const SourceConfig& config);
  bool Matches(const GURL& url,
               ContentSecurityPolicy::RedirectStatus redirect_status =
                   ContentSecurityPolicy::kDidNotRedirect) const;

 private:
  bool SchemeMatches(const GURL& url) const;
  bool HostMatches(const GURL& url) const;
  bool PathMatches(const GURL& url) const;
  bool PortMatches(const GURL& url) const;
  bool IsSchemeOnly() const;

  ContentSecurityPolicy* policy_;
  SourceConfig config_;
};

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_SOURCE_H_
