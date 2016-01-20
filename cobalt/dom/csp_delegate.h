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

#ifndef COBALT_DOM_CSP_DELEGATE_H_
#define COBALT_DOM_CSP_DELEGATE_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/network_bridge/net_poster.h"

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
  CSPDelegate(const network_bridge::NetPosterFactory& net_poster_factory,
              const std::string& default_security_policy, bool disable_csp);
  ~CSPDelegate() OVERRIDE;

  enum ResourceType {
    kFont,
    kImage,
    kLocation,
    kMedia,
    kScript,
    kStyle,
    kXhr,
  };

  // virtual for overriding by mocks.
  virtual void SetDocument(Document* document);

  // Return |true| if the given resource type can be loaded from |url|.
  // Set |did_redirect| if url was the result of a redirect.
  virtual bool CanLoad(ResourceType type, const GURL& url,
                       bool did_redirect) const;
  // Signal to the CSP object that CSP policy directives have been received.
  virtual void OnReceiveHeaders(const csp::ResponseHeaders& headers);
  virtual void OnReceiveHeader(const std::string& header,
                               csp::HeaderType header_type,
                               csp::HeaderSource header_source);

  // From csp::ContentSecurityPolicyDelegate
  GURL url() const OVERRIDE;
  void ReportViolation(const std::string& directive_text,
                       const std::string& effective_directive,
                       const std::string& console_message,
                       const GURL& blocked_url,
                       const std::vector<std::string>& endpoints,
                       const std::string& header) OVERRIDE;
  void SetReferrerPolicy(csp::ReferrerPolicy policy) OVERRIDE;

  const network_bridge::NetPosterFactory& net_poster_factory() const {
    return net_poster_factory_;
  }
  const std::string& default_security_policy() const {
    return default_security_policy_;
  }
#if !defined(COBALT_BUILD_TYPE_GOLD)
  bool disable_csp() const { return disable_csp_; }
#else
  bool disable_csp() const { return false; }
#endif

 private:
  void SendViolationReports(const std::vector<std::string>& endpoints,
                            const std::string& report);
  void SetLocationPolicy(const std::string& policy);

  Document* document_;
  scoped_ptr<csp::ContentSecurityPolicy> csp_;
  base::hash_set<uint32> violation_reports_sent_;
  network_bridge::NetPosterFactory net_poster_factory_;

  std::string default_security_policy_;
#if !defined(COBALT_BUILD_TYPE_GOLD)
  bool disable_csp_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CSPDelegate);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSP_DELEGATE_H_
