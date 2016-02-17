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

#include "cobalt/base/source_location.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/csp_violation_reporter.h"

namespace cobalt {
namespace dom {

// Object that represents a Content Security Policy for a particular document.
// Owned by the Document. Objects wishing to enforce CSP need to query the
// delegate to decide if they are permitted to load a resource.
// Note that any thread may call CanLoad().
class CspDelegate {
 public:
  enum EnforcementType {
    // Require CSP policy to be delivered in HTTP headers, otherwise consider
    // it an error. The main web module should *require* CSP.
    kEnforcementRequire,
    // Enable CSP checks, if policy is received. This is the standard browser
    // policy (except for our custom Location restrictions).
    kEnforcementEnable,
#if !defined(COBALT_FORCE_CSP)
    // Do no CSP checks, regardless of policy from the server. This is
    // for testing, so CSP can be disabled on the command line.
    kEnforcementDisable,
#endif
  };

  enum ResourceType {
    kFont,
    kImage,
    kLocation,
    kMedia,
    kScript,
    kStyle,
    kXhr,
  };

  CspDelegate(scoped_ptr<CspViolationReporter> violation_reporter,
              const GURL& url, const std::string& location_policy,
              EnforcementType mode,
              const base::Closure& policy_changed_callback);
  virtual ~CspDelegate();

  // Return |true| if the given resource type can be loaded from |url|.
  // Set |did_redirect| if url was the result of a redirect.
  virtual bool CanLoad(ResourceType type, const GURL& url,
                       bool did_redirect) const;
  virtual bool IsValidNonce(ResourceType type, const std::string& nonce) const;

  virtual bool AllowInline(ResourceType type,
                           const base::SourceLocation& location,
                           const std::string& script_content) const;

  // Return |true| if 'unsafe-eval' is set. No report will be generated in any
  // case. If eval_disabled_message is non-NULL, it will be set with a message
  // that should be reported when an application attempts to use eval().
  virtual bool AllowEval(std::string* eval_disabled_message) const;

  // Report that code was generated from a string, such as through eval() or the
  // Function constructor. If eval() is not allowed, generate a violation
  // report. Otherwise if eval() is allowed this is a no-op.
  virtual void ReportEval() const;

  // Signal to the CSP object that CSP policy directives have been received.
  // Return |true| if success, |false| if failure and load should be aborted.
  virtual bool OnReceiveHeaders(const csp::ResponseHeaders& headers);
  virtual void OnReceiveHeader(const std::string& header,
                               csp::HeaderType header_type,
                               csp::HeaderSource header_source);

  // Inform the policy that the document's origin has changed.
  void NotifyUrlChanged(const GURL& url) { return csp_->NotifyUrlChanged(url); }

  const std::string& location_policy() const { return location_policy_; }

  EnforcementType enforcement_mode() const { return enforcement_mode_; }
  CspViolationReporter* reporter() const { return reporter_.get(); }
  const network_bridge::PostSender& post_sender() const {
    return reporter_->post_sender();
  }

  const base::Closure& policy_changed_callback() const {
    return policy_changed_callback_;
  }

 private:
  void SetLocationPolicy(const std::string& policy);

  scoped_ptr<csp::ContentSecurityPolicy> csp_;

  // Hardcoded policy to restrict navigation.
  std::string location_policy_;

  // Helper class to send violation events to any reporting endpoints.
  scoped_ptr<CspViolationReporter> reporter_;

  EnforcementType enforcement_mode_;
  // In "Require" enforcement mode, we disallow all loads if CSP headers
  // weren't received. This tracks if we did get a valid header.
  bool was_header_received_;

  // This should be called any time the CSP policy changes. For example, after
  // receiving (and parsing) the headers, or after encountering a CSP directive
  // in a <meta> tag.
  base::Closure policy_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(CspDelegate);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSP_DELEGATE_H_
