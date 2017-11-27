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
  enum ResourceType {
    kFont,
    kImage,
    kLocation,
    kMedia,
    kScript,
    kStyle,
    kXhr,
    kWebSocket,
  };

  CspDelegate();
  virtual ~CspDelegate();

  // Return |true| if the given resource type can be loaded from |url|.
  // Set |did_redirect| if url was the result of a redirect.
  virtual bool CanLoad(ResourceType type, const GURL& url,
                       bool did_redirect) const = 0;
  virtual bool IsValidNonce(ResourceType type,
                            const std::string& nonce) const = 0;

  virtual bool AllowInline(ResourceType type,
                           const base::SourceLocation& location,
                           const std::string& script_content) const = 0;

  // Return |true| if 'unsafe-eval' is set. No report will be generated in any
  // case. If eval_disabled_message is non-NULL, it will be set with a message
  // that should be reported when an application attempts to use eval().
  virtual bool AllowEval(std::string* eval_disabled_message) const = 0;

  // Report that code was generated from a string, such as through eval() or the
  // Function constructor. If eval() is not allowed, generate a violation
  // report. Otherwise if eval() is allowed this is a no-op.
  virtual void ReportEval() const = 0;

  // Signal to the CSP object that CSP policy directives have been received.
  // Return |true| if success, |false| if failure and load should be aborted.
  virtual bool OnReceiveHeaders(const csp::ResponseHeaders& headers) = 0;
  virtual void OnReceiveHeader(const std::string& header,
                               csp::HeaderType header_type,
                               csp::HeaderSource header_source) = 0;
  // Inform the policy that the document's origin has changed.
  virtual void NotifyUrlChanged(const GURL& url) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CspDelegate);
};

// This class is just a no-op implementation that allows everything.
class CspDelegateInsecure : public CspDelegate {
 public:
  CspDelegateInsecure() {}
  bool CanLoad(ResourceType, const GURL&, bool) const override { return true; }
  bool IsValidNonce(ResourceType, const std::string&) const override {
    return true;
  }
  bool AllowInline(ResourceType, const base::SourceLocation&,
                   const std::string&) const override {
    return true;
  }
  bool AllowEval(std::string*) const override { return true; }
  void ReportEval() const override {}
  bool OnReceiveHeaders(const csp::ResponseHeaders&) override { return true; }
  void OnReceiveHeader(const std::string&, csp::HeaderType,
                       csp::HeaderSource) override {}
  void NotifyUrlChanged(const GURL&) const override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CspDelegateInsecure);
};

class CspDelegateSecure : public CspDelegate {
 public:
  CspDelegateSecure(scoped_ptr<CspViolationReporter> violation_reporter,
                    const GURL& url, csp::CSPHeaderPolicy require_csp,
                    const base::Closure& policy_changed_callback);
  ~CspDelegateSecure();

  // Return |true| if the given resource type can be loaded from |url|.
  // Set |did_redirect| if url was the result of a redirect.
  bool CanLoad(ResourceType type, const GURL& url,
               bool did_redirect) const override;
  bool IsValidNonce(ResourceType type, const std::string& nonce) const override;

  bool AllowInline(ResourceType type, const base::SourceLocation& location,
                   const std::string& script_content) const override;

  // Return |true| if 'unsafe-eval' is set. No report will be generated in any
  // case. If eval_disabled_message is non-NULL, it will be set with a message
  // that should be reported when an application attempts to use eval().
  bool AllowEval(std::string* eval_disabled_message) const override;

  // Report that code was generated from a string, such as through eval() or the
  // Function constructor. If eval() is not allowed, generate a violation
  // report. Otherwise if eval() is allowed this is a no-op.
  void ReportEval() const override;

  // Signal to the CSP object that CSP policy directives have been received.
  // Return |true| if success, |false| if failure and load should be aborted.
  bool OnReceiveHeaders(const csp::ResponseHeaders& headers) override;
  void OnReceiveHeader(const std::string& header, csp::HeaderType header_type,
                       csp::HeaderSource header_source) override;
  void NotifyUrlChanged(const GURL& url) const override {
    return csp_->NotifyUrlChanged(url);
  }

 protected:
  void SetLocationPolicy(const std::string& policy);

  scoped_ptr<csp::ContentSecurityPolicy> csp_;

  // Helper class to send violation events to any reporting endpoints.
  scoped_ptr<CspViolationReporter> reporter_;

  // We disallow all loads if CSP headers weren't received. This tracks if we
  // did get a valid header.
  bool was_header_received_;

  // This should be called any time the CSP policy changes. For example, after
  // receiving (and parsing) the headers, or after encountering a CSP directive
  // in a <meta> tag.
  base::Closure policy_changed_callback_;

  // Whether Cobalt is forbidden to render without receiving CSP header.
  csp::CSPHeaderPolicy require_csp_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CspDelegateSecure);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSP_DELEGATE_H_
