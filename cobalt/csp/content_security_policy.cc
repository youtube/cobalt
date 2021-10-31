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

#include <memory>

#include "cobalt/csp/content_security_policy.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "cobalt/csp/directive_list.h"
#include "cobalt/csp/source.h"

namespace cobalt {
namespace csp {

namespace {

ReferrerPolicy MergeReferrerPolicies(ReferrerPolicy a, ReferrerPolicy b) {
  // If there are conflicting policies, err on the side of security, i.e.
  // send no referrer. This could happen if there is a CSP policy in the
  // response headers from the server and then a different one in a <meta>
  // in the document.
  if (a != b) {
    return kReferrerPolicyNoReferrer;
  } else {
    return a;
  }
}

// Macros for iterating over the CSP's policies_ list and
// calling a given function.
#define FOR_ALL_POLICIES_1(AllowFunc, arg0)               \
  for (PolicyList::const_iterator it = policies_.begin(); \
       it != policies_.end(); ++it) {                     \
    if (!(*it)->AllowFunc((arg0))) return false;          \
  }                                                       \
  return true

#define FOR_ALL_POLICIES_3(AllowFunc, arg0, arg1, arg2)                  \
  for (PolicyList::const_iterator it = policies_.begin();                \
       it != policies_.end(); ++it) {                                    \
    if ((*it)->AllowFunc((arg0), (arg1), (arg2)) == false) return false; \
  }                                                                      \
  return true

#define FOR_ALL_POLICIES_4(AllowFunc, arg0, arg1, arg2, arg3)        \
  for (PolicyList::const_iterator it = policies_.begin();            \
       it != policies_.end(); ++it) {                                \
    if ((*it)->AllowFunc((arg0), (arg1), (arg2), (arg3)) == false) { \
      return false;                                                  \
    }                                                                \
  }                                                                  \
  return true

// Similar to above but templatized on a member variable.
// Used by CheckDigest().
template <bool (DirectiveList::*allowed)(const HashValue&) const>
bool IsAllowedByAllWithHash(const ContentSecurityPolicy::PolicyList& policies,
                            const HashValue& hash_value) {
  for (ContentSecurityPolicy::PolicyList::const_iterator it = policies.begin();
       it != policies.end(); ++it) {
    if (!((*it).get()->*allowed)(hash_value)) {
      return false;
    }
  }
  return true;
}

template <bool (DirectiveList::*allowed)(const HashValue&) const>
bool CheckDigest(const std::string& source, uint8 hash_algorithms_used,
                 const ContentSecurityPolicy::PolicyList& policies) {
  if (hash_algorithms_used == kHashAlgorithmNone) {
    return false;
  }

  HashAlgorithm valid_hash_algorithms[] = {
      kHashAlgorithmSha256, kHashAlgorithmSha384, kHashAlgorithmSha512,
  };

  for (size_t i = 0; i < arraysize(valid_hash_algorithms); ++i) {
    DigestValue digest;
    if (valid_hash_algorithms[i] & hash_algorithms_used) {
      bool digest_success = ComputeDigest(
          valid_hash_algorithms[i], source.c_str(), source.length(), &digest);
      if (digest_success &&
          IsAllowedByAllWithHash<allowed>(
              policies, HashValue(valid_hash_algorithms[i], digest))) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

ResponseHeaders::ResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& response) {
  response->GetNormalizedHeader("Content-Security-Policy",
                                &content_security_policy_);
  response->GetNormalizedHeader("Content-Security-Policy-Report-Only",
                                &content_security_policy_report_only_);
}

// CSP Level 1 Directives
const char ContentSecurityPolicy::kConnectSrc[] = "connect-src";
const char ContentSecurityPolicy::kDefaultSrc[] = "default-src";
const char ContentSecurityPolicy::kFontSrc[] = "font-src";
const char ContentSecurityPolicy::kFrameSrc[] = "frame-src";
const char ContentSecurityPolicy::kImgSrc[] = "img-src";
const char ContentSecurityPolicy::kMediaSrc[] = "media-src";
const char ContentSecurityPolicy::kObjectSrc[] = "object-src";
const char ContentSecurityPolicy::kReportURI[] = "report-uri";
const char ContentSecurityPolicy::kSandbox[] = "sandbox";
const char ContentSecurityPolicy::kScriptSrc[] = "script-src";
const char ContentSecurityPolicy::kStyleSrc[] = "style-src";

// CSP Level 2 Directives
const char ContentSecurityPolicy::kBaseURI[] = "base-uri";
const char ContentSecurityPolicy::kChildSrc[] = "child-src";
const char ContentSecurityPolicy::kFormAction[] = "form-action";
const char ContentSecurityPolicy::kFrameAncestors[] = "frame-ancestors";
const char ContentSecurityPolicy::kPluginTypes[] = "plugin-types";
const char ContentSecurityPolicy::kReflectedXSS[] = "reflected-xss";
const char ContentSecurityPolicy::kReferrer[] = "referrer";

// Custom Cobalt directive to enforce navigation restrictions.
const char ContentSecurityPolicy::kLocationSrc[] = "h5vcc-location-src";

// CSP Editor's Draft:
// https://w3c.github.io/webappsec/specs/content-security-policy
const char ContentSecurityPolicy::kManifestSrc[] = "manifest-src";

// Mixed Content Directive
// https://w3c.github.io/webappsec/specs/mixedcontent/#strict-mode
const char ContentSecurityPolicy::kBlockAllMixedContent[] =
    "block-all-mixed-content";

// https://w3c.github.io/webappsec/specs/upgrade/
const char ContentSecurityPolicy::kUpgradeInsecureRequests[] =
    "upgrade-insecure-requests";

// Suborigin Directive
// https://metromoxie.github.io/webappsec/specs/suborigins/index.html
const char ContentSecurityPolicy::kSuborigin[] = "suborigin";

// clang-format off
bool ContentSecurityPolicy::IsDirectiveName(const std::string& name) {
  std::string lower_name = base::ToLowerASCII(name);
  return (lower_name == kConnectSrc ||
          lower_name == kDefaultSrc ||
          lower_name == kFontSrc ||
          lower_name == kFrameSrc ||
          lower_name == kImgSrc ||
          lower_name == kLocationSrc ||
          lower_name == kMediaSrc ||
          lower_name == kObjectSrc ||
          lower_name == kReportURI ||
          lower_name == kSandbox ||
          lower_name == kSuborigin ||
          lower_name == kScriptSrc ||
          lower_name == kStyleSrc ||
          lower_name == kBaseURI ||
          lower_name == kChildSrc ||
          lower_name == kFormAction ||
          lower_name == kFrameAncestors ||
          lower_name == kPluginTypes ||
          lower_name == kReflectedXSS ||
          lower_name == kReferrer ||
          lower_name == kManifestSrc ||
          lower_name == kBlockAllMixedContent ||
          lower_name == kUpgradeInsecureRequests);
}
// clang-format on

ContentSecurityPolicy::ContentSecurityPolicy(
    const GURL& url, const ViolationCallback& violation_callback)
    : violation_callback_(violation_callback),
      script_hash_algorithms_used_(0),
      style_hash_algorithms_used_(0),
      enforce_strict_mixed_content_checking_(false),
      referrer_policy_(kReferrerPolicyDefault) {
  NotifyUrlChanged(url);
}

ContentSecurityPolicy::~ContentSecurityPolicy() {}

void ContentSecurityPolicy::OnReceiveHeaders(const ResponseHeaders& headers) {
  if (!headers.content_security_policy().empty()) {
    AddPolicyFromHeaderValue(headers.content_security_policy(),
                             kHeaderTypeEnforce, kHeaderSourceHTTP);
  }
  if (!headers.content_security_policy_report_only().empty()) {
    AddPolicyFromHeaderValue(headers.content_security_policy_report_only(),
                             kHeaderTypeReport, kHeaderSourceHTTP);
  }
}

void ContentSecurityPolicy::OnReceiveHeader(const std::string& header,
                                            HeaderType type,
                                            HeaderSource source) {
  AddPolicyFromHeaderValue(header, type, source);
}

bool ContentSecurityPolicy::UrlMatchesSelf(const GURL& url) const {
  return self_source_->Matches(url, kDidNotRedirect);
}

bool ContentSecurityPolicy::SchemeMatchesSelf(const GURL& url) const {
  // https://www.w3.org/TR/CSP2/#match-source-expression, section 4.5.1
  // Allow "upgrade" to https if our document is http.
  if (base::LowerCaseEqualsASCII(self_scheme_, "http")) {
    return url.SchemeIs("http") || url.SchemeIs("https");
  } else {
    return self_scheme_ == url.scheme();
  }
}

void ContentSecurityPolicy::ReportViolation(
    const std::string& directive_text, const std::string& effective_directive,
    const std::string& console_message, const GURL& blocked_url,
    const std::vector<std::string>& report_endpoints,
    const std::string& header) {
  if (violation_callback_.is_null()) {
    return;
  }

  ViolationInfo violation_info;
  violation_info.directive_text = directive_text;
  violation_info.effective_directive = effective_directive;
  violation_info.console_message = console_message;
  violation_info.blocked_url = blocked_url;
  violation_info.endpoints = report_endpoints;
  violation_info.header = header;
  violation_callback_.Run(violation_info);
}

void ContentSecurityPolicy::ReportInvalidReferrer(
    const std::string& invalid_value) {
  std::string message =
      "The 'referrer' Content Security Policy directive has the invalid value "
      "\"" +
      invalid_value +
      "\". Valid values are \"no-referrer\", \"no-referrer-when-downgrade\", "
      "\"origin\", \"origin-when-cross-origin\", and \"unsafe-url\".";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidPluginTypes(
    const std::string& plugin_type) {
  std::string message;
  if (plugin_type.empty()) {
    message =
        "'plugin-types' Content Security Policy directive is empty; all "
        "plugins will be blocked.\n";
  } else if (plugin_type == "'none'") {
    message =
        "Invalid plugin type in 'plugin-types' Content Security Policy "
        "directive: '";
    message += plugin_type;
    message += "'. Did you mean to set the object-src directive to 'none'?\n";
  } else {
    message =
        "Invalid plugin type in 'plugin-types' Content Security Policy "
        "directive: '";
    message += plugin_type;
    message += "'.\n";
  }
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportMetaOutsideHead(const std::string& header) {
  std::string message = "The Content Security Policy '" + header +
                        "' was delivered via a <meta> element outside the "
                        "document's <head>, which is disallowed. The policy "
                        "has been ignored.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportValueForEmptyDirective(
    const std::string& name, const std::string& value) {
  std::string message =
      "The Content Security Policy directive '" + name +
      "' should be empty, but was delivered with a value of '" + value +
      "'. The directive has been applied, and the value ignored.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportDirectiveAsSourceExpression(
    const std::string& directive_name, const std::string& source_expression) {
  std::string message = "The Content Security Policy directive '" +
                        directive_name + "' contains '" + source_expression +
                        "' as a source expression. Did you mean '" +
                        directive_name + " ...; " + source_expression +
                        "...' (note the semicolon)?";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidSourceExpression(
    const std::string& directive_name, const std::string& source) {
  std::string message =
      "The source list for Content Security Policy directive '" +
      directive_name + "' contains an invalid source: '" + source +
      "'. It will be ignored.";
  if (base::LowerCaseEqualsASCII(source.c_str(), "'none'")) {
    message = message +
              " Note that 'none' has no effect unless it is the only "
              "expression in the source list.";
  }
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidPathCharacter(
    const std::string& directive_name, const std::string& value,
    char invalid_char) {
  DCHECK(invalid_char == '#' || invalid_char == '?');
  std::string message =
      "The source list for Content Security Policy directive '";
  message += directive_name;
  message += "' contains a source with an invalid path: '";
  message += value;
  message += "'. ";
  message +=
      invalid_char == '?'
          ? "The query component, including the '?', will be ignored."
          : "The fragment identifier, including the '#', will be ignored.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportDuplicateDirective(const std::string& name) {
  std::string message =
      "Ignoring duplicate Content-Security-Policy directive '" + name + "'.\n";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidDirectiveValueCharacter(
    const std::string& directive_name, const std::string& value) {
  std::string message =
      "The value for Content Security Policy directive '" + directive_name +
      "' contains an invalid character: '" + value +
      "'. Non-whitespace characters outside ASCII 0x21-0x7E must be "
      "percent-encoded, as described in RFC 3986, section 2.1: "
      "http://tools.ietf.org/html/rfc3986#section-2.1.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidReflectedXSS(
    const std::string& invalid_value) {
  std::string message =
      "The 'reflected-xss' Content Security Policy directive has the invalid "
      "value \"" +
      invalid_value +
      "\". Valid values are \"allow\", \"filter\", and \"block\".";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportMissingReportURI(const std::string& policy) {
  std::string message = "The Content Security Policy '";
  message += policy;
  message +=
      "' was delivered in report-only mode, but does not specify a "
      "'report-uri'; the policy will have no effect. Please either add a "
      "'report-uri' directive, or deliver the policy via the "
      "'Content-Security-Policy' header.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportReportOnlyInMeta(const std::string& header) {
  std::string message = "The report-only Content Security Policy '" + header +
                        "' was delivered via a <meta> element, which is "
                        "disallowed. The policy has been ignored.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidSuboriginFlags(
    const std::string& invalid_flags) {
  std::string message =
      "Error while parsing the 'suborigin' Content Security Policy "
      "directive: " +
      invalid_flags;
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportUnsupportedDirective(
    const std::string& name) {
  std::string lower_name = base::ToLowerASCII(name);
  std::string message;
  if (lower_name == "allow") {
    message =
        "The 'allow' directive has been replaced with 'default-src'. Please "
        "use that directive instead, as 'allow' has no effect.";
  } else if (lower_name == "options") {
    message =
        "The 'options' directive has been replaced with 'unsafe-inline' and "
        "'unsafe-eval' source expressions for the 'script-src' and 'style-src' "
        "directives. Please use those directives instead, as 'options' has no "
        "effect.";
  } else if (lower_name == "policy-uri") {
    message =
        "The 'policy-uri' directive has been removed from the specification. "
        "Please specify a complete policy via the Content-Security-Policy "
        "header.";
  } else if (IsDirectiveName(name)) {
    message = "The Content-Security-Policy directive '" + name +
              "' is implemented behind a flag which is currently disabled.\n";
  } else {
    message =
        "Unrecognized Content-Security-Policy directive '" + name + "'.\n";
  }
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportInvalidInReportOnly(const std::string& name) {
  std::string message = "The Content Security Policy directive '" + name +
                        "' is ignored when delivered in a report-only policy.";
  DLOG(WARNING) << message;
}

void ContentSecurityPolicy::ReportDirectiveNotSupportedInsideMeta(
    const std::string& name) {
  DLOG(WARNING) << "The " << name
                << " directive is not supported inside a <meta> element.";
}

bool ContentSecurityPolicy::AllowJavaScriptURLs(const std::string& context_url,
                                                int context_line,
                                                ReportingStatus status) const {
  FOR_ALL_POLICIES_3(AllowJavaScriptURLs, context_url, context_line, status);
}

bool ContentSecurityPolicy::AllowInlineEventHandlers(
    const std::string& context_url, int context_line,
    ReportingStatus status) const {
  FOR_ALL_POLICIES_3(AllowInlineEventHandlers, context_url, context_line,
                     status);
}
bool ContentSecurityPolicy::AllowInlineScript(const std::string& context_url,
                                              int context_line,
                                              const std::string& script_content,
                                              ReportingStatus status) const {
  FOR_ALL_POLICIES_4(AllowInlineScript, context_url, context_line, status,
                     script_content);
}
bool ContentSecurityPolicy::AllowInlineStyle(const std::string& context_url,
                                             int context_line,
                                             const std::string& style_content,
                                             ReportingStatus status) const {
  FOR_ALL_POLICIES_4(AllowInlineStyle, context_url, context_line, status,
                     style_content);
}

bool ContentSecurityPolicy::AllowEval(ReportingStatus status) const {
  FOR_ALL_POLICIES_1(AllowEval, status);
}

bool ContentSecurityPolicy::AllowScriptFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowScriptFromSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowObjectFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowObjectFromSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowImageFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowImageFromSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowNavigateToSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  // Note that this is a Cobalt-specific policy to prevent navigation
  // to any unexpected URLs.
  FOR_ALL_POLICIES_3(AllowNavigateToSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowStyleFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowStyleFromSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowFontFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowFontFromSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowMediaFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowMediaFromSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowConnectToSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowConnectToSource, url, redirect_status,
                     reporting_status);
}

bool ContentSecurityPolicy::AllowFormAction(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowFormAction, url, redirect_status, reporting_status);
}

bool ContentSecurityPolicy::AllowBaseURI(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowBaseURI, url, redirect_status, reporting_status);
}

bool ContentSecurityPolicy::AllowManifestFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  FOR_ALL_POLICIES_3(AllowManifestFromSource, url, redirect_status,
                     reporting_status);
}
bool ContentSecurityPolicy::AllowScriptWithNonce(
    const std::string& nonce) const {
  FOR_ALL_POLICIES_1(AllowScriptNonce, nonce);
}

bool ContentSecurityPolicy::AllowStyleWithNonce(
    const std::string& nonce) const {
  FOR_ALL_POLICIES_1(AllowStyleNonce, nonce);
}

bool ContentSecurityPolicy::AllowScriptWithHash(
    const std::string& source) const {
  return CheckDigest<&DirectiveList::AllowScriptHash>(
      source, script_hash_algorithms_used_, policies_);
}

bool ContentSecurityPolicy::AllowStyleWithHash(
    const std::string& source) const {
  return CheckDigest<&DirectiveList::AllowStyleHash>(
      source, style_hash_algorithms_used_, policies_);
}

void ContentSecurityPolicy::NotifyUrlChanged(const GURL& url) {
  url_ = url;
  CreateSelfSource();
}

bool ContentSecurityPolicy::DidSetReferrerPolicy() const {
  for (PolicyList::const_iterator it = policies_.begin(); it != policies_.end();
       ++it) {
    if ((*it)->did_set_referrer_policy()) {
      return true;
    }
  }
  return false;
}

void ContentSecurityPolicy::CreateSelfSource() {
  // Ensure that 'self' processes correctly.
  self_scheme_ = url_.scheme();
  SourceConfig config;
  config.scheme = self_scheme_;
  config.host = url_.host();
  config.path.clear();
  config.port = url_.IntPort();
  config.host_wildcard = SourceConfig::kNoWildcard;
  config.port_wildcard = SourceConfig::kNoWildcard;
  self_source_.reset(new Source(this, config));
}

void ContentSecurityPolicy::AddPolicyFromHeaderValue(const std::string& header,
                                                     HeaderType type,
                                                     HeaderSource source) {
  // If this is a report-only header inside a <meta> element, bail out.
  if (source == kHeaderSourceMeta && type == kHeaderTypeReport) {
    ReportReportOnlyInMeta(header);
    return;
  }

  base::StringPiece characters(header);

  const char* begin = characters.begin();
  const char* end = characters.end();

  // RFC2616, section 4.2 specifies that headers appearing multiple times can
  // be combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  const char* position = begin;
  while (position < end) {
    SkipUntil(&position, end, ',');

    // header1,header2 OR header1
    //        ^                  ^
    base::StringPiece begin_piece(begin, static_cast<size_t>(position - begin));
    std::unique_ptr<DirectiveList> policy(
        new DirectiveList(this, begin_piece, type, source));
    if (type != kHeaderTypeReport && policy->did_set_referrer_policy()) {
      // FIXME: We need a 'ReferrerPolicyUnset' enum to avoid confusing code
      // like this.
      referrer_policy_ = DidSetReferrerPolicy()
                             ? MergeReferrerPolicies(referrer_policy_,
                                                     policy->referrer_policy())
                             : policy->referrer_policy();
    }

    if (!policy->AllowEval(kSuppressReport) &&
        disable_eval_error_message_.empty()) {
      disable_eval_error_message_ = policy->eval_disabled_error_message();
    }

    policies_.emplace_back(policy.release());

    // Skip the comma, and begin the next header from the current position.
    DCHECK(position == end || *position == ',');
    SkipExactly(&position, end, ',');
    begin = position;
  }
}

}  // namespace csp
}  // namespace cobalt
