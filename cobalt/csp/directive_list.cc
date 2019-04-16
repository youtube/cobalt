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

#include "cobalt/csp/directive_list.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "cobalt/csp/crypto.h"
#include "cobalt/csp/media_list_directive.h"
#include "cobalt/csp/source_list_directive.h"

namespace cobalt {
namespace csp {

namespace {

std::string GetSha256String(const std::string& content) {
  DigestValue digest;
  if (!ComputeDigest(kHashAlgorithmSha256, content.c_str(), content.length(),
                     &digest) ||
      digest.size() == 0) {
    return "sha256-...";
  }

  base::StringPiece digest_piece(reinterpret_cast<char*>(&digest[0]),
                                 digest.size());
  std::string encoded;
  base::Base64Encode(digest_piece, &encoded);
  if (!encoded.empty() || digest_piece.empty()) {
    return "'sha256-" + encoded + "'";
  } else {
    DLOG(WARNING) << "Base64Encode failed on " << content;
    return "sha256-...";
  }
}

std::string ElidedUrl(const GURL& url) {
  // Emulate KURL::elidedString() in Blink.

  std::string url_spec = url.spec();
  const size_t len = url_spec.length();
  if (len < 1024) {
    return url_spec;
  } else {
    return url_spec.substr(0, 511) + "..." + url_spec.substr(len - 510, 510);
  }
}
}  // namespace

DirectiveList::DirectiveList(ContentSecurityPolicy* policy,
                             const base::StringPiece& text, HeaderType type,
                             HeaderSource source)
    : policy_(policy),
      header_type_(type),
      header_source_(source),
      report_only_(type == kHeaderTypeReport),
      has_sandbox_policy_(false),
      has_suborigin_policy_(false),
      reflected_xss_disposition_(kReflectedXSSUnset),
      did_set_referrer_policy_(false),
      referrer_policy_(kReferrerPolicyDefault),
      strict_mixed_content_checking_enforced_(false),
      upgrade_insecure_requests_(false) {
  Parse(text);
  if (!CheckEval(OperativeDirective(script_src_.get()))) {
    std::string message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        OperativeDirective(script_src_.get())->text() + "\".\n";
    set_eval_disabled_error_message(message);
  }
  if (report_only() && report_endpoints_.empty()) {
    policy->ReportMissingReportURI(text.as_string());
  }
}

DirectiveList::~DirectiveList() {}

void DirectiveList::ReportViolation(const std::string& directive_text,
                                    const std::string& effective_directive,
                                    const std::string& console_message,
                                    const GURL& blocked_url) const {
  std::string message =
      report_only_ ? "[Report Only] " + console_message : console_message;
  policy_->ReportViolation(directive_text, effective_directive, message,
                           blocked_url, report_endpoints_, header_);
}

void DirectiveList::ReportViolationWithLocation(
    const std::string& directive_text, const std::string& effective_directive,
    const std::string& console_message, const GURL& blocked_url,
    const std::string& context_url, int context_line) const {
  std::string message = base::StringPrintf(
      "%s%s %s:%d", report_only_ ? "[Report Only] " : "",
      console_message.c_str(), context_url.c_str(), context_line);
  policy_->ReportViolation(directive_text, effective_directive, message,
                           blocked_url, report_endpoints_, header_);
}

bool DirectiveList::CheckEval(SourceListDirective* directive) const {
  return !directive || directive->AllowEval();
}

bool DirectiveList::CheckInline(SourceListDirective* directive) const {
  return !directive ||
         (directive->AllowInline() && !directive->hash_or_nonce_present());
}

bool DirectiveList::CheckNonce(SourceListDirective* directive,
                               const std::string& nonce) const {
  return !directive || directive->AllowNonce(nonce);
}

bool DirectiveList::CheckHash(SourceListDirective* directive,
                              const HashValue& hashValue) const {
  return !directive || directive->AllowHash(hashValue);
}

bool DirectiveList::CheckSource(
    SourceListDirective* directive, const GURL& url,
    ContentSecurityPolicy::RedirectStatus redirect_status) const {
  return !directive || directive->Allows(url, redirect_status);
}

bool DirectiveList::CheckMediaType(MediaListDirective* directive,
                                   const std::string& type,
                                   const std::string& type_attribute) const {
  if (!directive) {
    return true;
  }
  std::string trimmed_type_attribute;
  TrimWhitespaceASCII(type_attribute, base::TRIM_ALL, &trimmed_type_attribute);

  if (type_attribute.empty() || trimmed_type_attribute != type) {
    return false;
  }
  return directive->Allows(type);
}

SourceListDirective* DirectiveList::OperativeDirective(
    SourceListDirective* directive) const {
  return directive ? directive : default_src_.get();
}

SourceListDirective* DirectiveList::OperativeDirective(
    SourceListDirective* directive,
    SourceListDirective* directive_override) const {
  return directive ? directive : directive_override;
}

bool DirectiveList::CheckEvalAndReportViolation(
    SourceListDirective* directive, const std::string& console_message) const {
  if (CheckEval(directive)) {
    return true;
  }

  std::string suffix;
  if (directive == default_src_.get()) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  ReportViolation(
      directive->text(), ContentSecurityPolicy::kScriptSrc,
      console_message + "\"" + directive->text() + "\"." + suffix + "\n",
      GURL());
  if (report_only_) {
    return true;
  } else {
    return false;
  }
}

bool DirectiveList::CheckMediaTypeAndReportViolation(
    MediaListDirective* directive, const std::string& type,
    const std::string& type_attribute,
    const std::string& console_message) const {
  if (CheckMediaType(directive, type, type_attribute)) {
    return true;
  }

  std::string message = console_message + "\'" + directive->text() + "\'.";
  if (type_attribute.empty()) {
    message +=
        " When enforcing the 'plugin-types' directive, the plugin's "
        "media type must be explicitly declared with a 'type' attribute "
        "on the containing element (e.g. '<object type=\"[TYPE GOES "
        "HERE]\" ...>').";
  }

  ReportViolation(directive->text(), ContentSecurityPolicy::kPluginTypes,
                  message + "\n", GURL());
  return deny_if_enforcing_policy();
}

bool DirectiveList::CheckInlineAndReportViolation(
    SourceListDirective* directive, const std::string& console_message,
    const std::string& context_url, int context_line, bool is_script,
    const std::string& hash_value) const {
  if (CheckInline(directive)) {
    return true;
  }

  std::string suffix;
  if (directive->AllowInline() && directive->hash_or_nonce_present()) {
    // If inline is allowed, but a hash or nonce is present, we ignore
    // 'unsafe-inline'. Throw a reasonable error.
    suffix =
        " Note that 'unsafe-inline' is ignored if either a hash or nonce value "
        "is present in the source list.";
  } else if (directive->hash_or_nonce_present()) {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash (" + hash_value +
        "), or a nonce ('nonce-...') is required to enable inline execution.";
    DigestValue digest_value;
    HashAlgorithm hash_algorithm;
    SourceList::ParseHash(hash_value.c_str(),
                          hash_value.c_str() + hash_value.length(),
                          &digest_value, &hash_algorithm);
    if (directive->AllowHash(HashValue(hash_algorithm, digest_value))) {
      return true;
    }
  } else {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash (" + hash_value +
        "), or a nonce ('nonce-...') is required to enable inline execution.";
    if (directive == default_src_.get())
      suffix = suffix + " Note also that '" +
               std::string(is_script ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
  }

  ReportViolationWithLocation(
      directive->text(), is_script ? ContentSecurityPolicy::kScriptSrc
                                   : ContentSecurityPolicy::kStyleSrc,
      console_message + "\"" + directive->text() + "\"." + suffix + "\n",
      GURL(), context_url, context_line);

  if (!report_only_) {
    if (is_script) {
      // policy_->ReportBlockedScriptExecutionToInspector(directive->text());
    }
    return false;
  }
  return true;
}

bool DirectiveList::CheckSourceAndReportViolation(
    SourceListDirective* directive, const GURL& url,
    const std::string& effective_directive,
    ContentSecurityPolicy::RedirectStatus redirect_status) const {
  if (CheckSource(directive, url, redirect_status)) {
    return true;
  }

  std::string prefix;
  if (ContentSecurityPolicy::kBaseURI == effective_directive) {
    prefix = "Refused to set the document's base URI to '";
  } else if (ContentSecurityPolicy::kChildSrc == effective_directive) {
    prefix = "Refused to create a child context containing '";
  } else if (ContentSecurityPolicy::kConnectSrc == effective_directive) {
    prefix = "Refused to connect to '";
  } else if (ContentSecurityPolicy::kFontSrc == effective_directive) {
    prefix = "Refused to load the font '";
  } else if (ContentSecurityPolicy::kFormAction == effective_directive) {
    prefix = "Refused to send form data to '";
  } else if (ContentSecurityPolicy::kFrameSrc == effective_directive) {
    prefix = "Refused to frame '";
  } else if (ContentSecurityPolicy::kImgSrc == effective_directive) {
    prefix = "Refused to load the image '";
  } else if (ContentSecurityPolicy::kLocationSrc == effective_directive) {
    prefix = "Refused to navigate to '";
  } else if (ContentSecurityPolicy::kMediaSrc == effective_directive) {
    prefix = "Refused to load media from '";
  } else if (ContentSecurityPolicy::kManifestSrc == effective_directive) {
    prefix = "Refused to load manifest from '";
  } else if (ContentSecurityPolicy::kObjectSrc == effective_directive) {
    prefix = "Refused to load plugin data from '";
  } else if (ContentSecurityPolicy::kScriptSrc == effective_directive) {
    prefix = "Refused to load the script '";
  } else if (ContentSecurityPolicy::kStyleSrc == effective_directive) {
    prefix = "Refused to load the stylesheet '";
  }

  std::string suffix = std::string();
  if (directive == default_src_.get())
    suffix =
        " Note that '" + effective_directive +
        "' was not explicitly set, so 'default-src' is used as a fallback.";

  ReportViolation(directive->text(), effective_directive,
                  prefix + ElidedUrl(url) +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      directive->text() + "\"." + suffix + "\n",
                  url);
  return deny_if_enforcing_policy();
}

bool DirectiveList::AllowJavaScriptURLs(
    const std::string& context_url, int context_line,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  if (reporting_status == ContentSecurityPolicy::kSendReport) {
    return CheckInlineAndReportViolation(
        OperativeDirective(script_src_.get()),
        "Refused to execute JavaScript URL because it violates the following "
        "Content Security Policy directive: ",
        context_url, context_line, true, "sha256-...");
  }
  return CheckInline(OperativeDirective(script_src_.get()));
}

bool DirectiveList::AllowInlineEventHandlers(
    const std::string& context_url, int context_line,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  if (reporting_status == ContentSecurityPolicy::kSendReport) {
    return CheckInlineAndReportViolation(
        OperativeDirective(script_src_.get()),
        "Refused to execute inline event handler because it violates the "
        "following Content Security Policy directive: ",
        context_url, context_line, true, "sha256-...");
  }
  return CheckInline(OperativeDirective(script_src_.get()));
}

bool DirectiveList::AllowInlineScript(
    const std::string& context_url, int context_line,
    ContentSecurityPolicy::ReportingStatus reporting_status,
    const std::string& content) const {
  if (reporting_status == ContentSecurityPolicy::kSendReport) {
    return CheckInlineAndReportViolation(
        OperativeDirective(script_src_.get()),
        "Refused to execute inline script because it violates the following "
        "Content Security Policy directive: ",
        context_url, context_line, true, GetSha256String(content));
  }
  return CheckInline(OperativeDirective(script_src_.get()));
}

bool DirectiveList::AllowInlineStyle(
    const std::string& context_url, int context_line,
    ContentSecurityPolicy::ReportingStatus reporting_status,
    const std::string& content) const {
  if (reporting_status == ContentSecurityPolicy::kSendReport) {
    return CheckInlineAndReportViolation(
        OperativeDirective(style_src_.get()),
        "Refused to apply inline style because it violates the following "
        "Content Security Policy directive: ",
        context_url, context_line, false, GetSha256String(content));
  }
  return CheckInline(OperativeDirective(style_src_.get()));
}

bool DirectiveList::AllowEval(
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  if (reporting_status == ContentSecurityPolicy::kSendReport) {
    return CheckEvalAndReportViolation(
        OperativeDirective(script_src_.get()),
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ");
  }
  return CheckEval(OperativeDirective(script_src_.get()));
}

bool DirectiveList::AllowScriptFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(script_src_.get()), url,
                   ContentSecurityPolicy::kScriptSrc, redirect_status)
             : CheckSource(OperativeDirective(script_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowObjectFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(object_src_.get()), url,
                   ContentSecurityPolicy::kObjectSrc, redirect_status)
             : CheckSource(OperativeDirective(object_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowImageFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(img_src_.get()), url,
                   ContentSecurityPolicy::kImgSrc, redirect_status)
             : CheckSource(OperativeDirective(img_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowStyleFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(style_src_.get()), url,
                   ContentSecurityPolicy::kStyleSrc, redirect_status)
             : CheckSource(OperativeDirective(style_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowFontFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(font_src_.get()), url,
                   ContentSecurityPolicy::kFontSrc, redirect_status)
             : CheckSource(OperativeDirective(font_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowMediaFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(media_src_.get()), url,
                   ContentSecurityPolicy::kMediaSrc, redirect_status)
             : CheckSource(OperativeDirective(media_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowManifestFromSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(manifest_src_.get()), url,
                   ContentSecurityPolicy::kManifestSrc, redirect_status)
             : CheckSource(OperativeDirective(manifest_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowConnectToSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(connect_src_.get()), url,
                   ContentSecurityPolicy::kConnectSrc, redirect_status)
             : CheckSource(OperativeDirective(connect_src_.get()), url,
                           redirect_status);
}

bool DirectiveList::AllowNavigateToSource(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  // No fallback to default for h5vcc-location-src policy, so we don't use
  // OperativeDirective() in this case.
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(
                   location_src_.get(), url,
                   ContentSecurityPolicy::kLocationSrc, redirect_status)
             : CheckSource(location_src_.get(), url, redirect_status);
}

bool DirectiveList::AllowFormAction(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(form_action_.get(), url,
                                             ContentSecurityPolicy::kFormAction,
                                             redirect_status)
             : CheckSource(form_action_.get(), url, redirect_status);
}

bool DirectiveList::AllowBaseURI(
    const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
    ContentSecurityPolicy::ReportingStatus reporting_status) const {
  return reporting_status == ContentSecurityPolicy::kSendReport
             ? CheckSourceAndReportViolation(base_uri_.get(), url,
                                             ContentSecurityPolicy::kBaseURI,
                                             redirect_status)
             : CheckSource(base_uri_.get(), url, redirect_status);
}

bool DirectiveList::AllowScriptNonce(const std::string& nonce) const {
  return CheckNonce(OperativeDirective(script_src_.get()), nonce);
}

bool DirectiveList::AllowStyleNonce(const std::string& nonce) const {
  return CheckNonce(OperativeDirective(style_src_.get()), nonce);
}

bool DirectiveList::AllowScriptHash(const HashValue& hash_value) const {
  return CheckHash(OperativeDirective(script_src_.get()), hash_value);
}

bool DirectiveList::AllowStyleHash(const HashValue& hash_value) const {
  return CheckHash(OperativeDirective(style_src_.get()), hash_value);
}

const std::string& DirectiveList::PluginTypesText() const {
  DCHECK_EQ(has_plugin_types(), true);
  return plugin_types_->text();
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void DirectiveList::Parse(const base::StringPiece& text) {
  const char* begin = text.begin();
  const char* end = text.end();
  header_ = ToString(begin, end);

  if (begin == end) {
    return;
  }

  const char* position = begin;
  while (position < end) {
    const char* directive_begin = position;
    SkipUntil(&position, end, ';');

    std::string name, value;
    if (ParseDirective(directive_begin, position, &name, &value)) {
      DCHECK(!name.empty());
      AddDirective(name, value);
    }

    DCHECK(position == end || *position == ';');
    SkipExactly(&position, end, ';');
  }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool DirectiveList::ParseDirective(const char* begin, const char* end,
                                   std::string* name, std::string* value) {
  DCHECK(name && name->empty());
  DCHECK(value && value->empty());

  const char* position = begin;
  SkipWhile<base::IsAsciiWhitespace>(&position, end);

  // Empty directive (e.g. ";;;"). Exit early.
  if (position == end) {
    return false;
  }

  const char* name_begin = position;
  SkipWhile<IsCSPDirectiveNameCharacter>(&position, end);

  // The directive-name must be non-empty.
  if (name_begin == position) {
    SkipWhile<IsNotAsciiWhitespace>(&position, end);
    policy_->ReportUnsupportedDirective(ToString(name_begin, position));
    return false;
  }

  *name = ToString(name_begin, position);

  if (position == end) {
    return true;
  }

  if (!SkipExactly<base::IsAsciiWhitespace>(&position, end)) {
    SkipWhile<IsNotAsciiWhitespace>(&position, end);
    policy_->ReportUnsupportedDirective(ToString(name_begin, position));
    return false;
  }

  SkipWhile<base::IsAsciiWhitespace>(&position, end);

  const char* value_begin = position;
  SkipWhile<IsCSPDirectiveValueCharacter>(&position, end);

  if (position != end) {
    policy_->ReportInvalidDirectiveValueCharacter(*name,
                                                  ToString(value_begin, end));
    return false;
  }

  // The directive-value may be empty.
  if (value_begin == position) {
    return true;
  }

  *value = ToString(value_begin, position);
  return true;
}

void DirectiveList::ParseReportURI(const std::string& name,
                                   const std::string& value) {
  if (header_source_ == kHeaderSourceMeta) {
    // The report-uri, frame-ancestors, and sandbox directives are not supported
    // inside a meta element.
    // https://w3c.github.io/webappsec-csp/#meta-element
    policy_->ReportDirectiveNotSupportedInsideMeta(name);
    return;
  }
  if (!report_endpoints_.empty()) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  base::StringPiece characters(value);
  const char* position = characters.data();
  const char* end = position + characters.size();

  while (position < end) {
    SkipWhile<base::IsAsciiWhitespace>(&position, end);

    const char* url_begin = position;
    SkipWhile<IsNotAsciiWhitespace>(&position, end);

    if (url_begin < position) {
      std::string url = ToString(url_begin, position);
      report_endpoints_.push_back(url);
    }
  }
}

void DirectiveList::SetCSPDirective(
    const std::string& name, const std::string& value,
    std::unique_ptr<SourceListDirective>* directive) {
  DCHECK(directive);
  if (*directive) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  directive->reset(new SourceListDirective(name, value, policy_));
}

void DirectiveList::SetCSPDirective(
    const std::string& name, const std::string& value,
    std::unique_ptr<MediaListDirective>* directive) {
  DCHECK(directive);
  if (*directive) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  directive->reset(new MediaListDirective(name, value, policy_));
}

void DirectiveList::EnforceStrictMixedContentChecking(
    const std::string& name, const std::string& value) {
  if (report_only_) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (strict_mixed_content_checking_enforced_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  strict_mixed_content_checking_enforced_ = true;
  policy_->set_enforce_strict_mixed_content_checking();
  if (!value.empty()) {
    policy_->ReportValueForEmptyDirective(name, value);
  }
}

void DirectiveList::EnableInsecureRequestsUpgrade(const std::string& name,
                                                  const std::string& value) {
  if (report_only_) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (upgrade_insecure_requests_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  upgrade_insecure_requests_ = true;

  // TODO: Do something with this.
  NOTIMPLEMENTED() << "SetInsecureRequestsPolicy()";

  if (!value.empty()) {
    policy_->ReportValueForEmptyDirective(name, value);
  }
}

void DirectiveList::ParseReflectedXSS(const std::string& name,
                                      const std::string& value) {
  if (reflected_xss_disposition_ != kReflectedXSSUnset) {
    policy_->ReportDuplicateDirective(name);
    reflected_xss_disposition_ = kReflectedXSSInvalid;
    return;
  }

  if (value.empty()) {
    reflected_xss_disposition_ = kReflectedXSSInvalid;
    policy_->ReportInvalidReflectedXSS(value);
    return;
  }

  base::StringPiece characters(value);
  const char* position = characters.data();
  const char* end = position + characters.size();

  SkipWhile<base::IsAsciiWhitespace>(&position, end);
  const char* begin = position;
  SkipWhile<IsNotAsciiWhitespace>(&position, end);

  // value1
  //       ^

  if (base::LowerCaseEqualsASCII(begin, position, "allow")) {
    reflected_xss_disposition_ = kAllowReflectedXSS;
  } else if (base::LowerCaseEqualsASCII(begin, position, "filter")) {
    reflected_xss_disposition_ = kFilterReflectedXSS;
  } else if (base::LowerCaseEqualsASCII(begin, position, "block")) {
    reflected_xss_disposition_ = kBlockReflectedXSS;
  } else {
    reflected_xss_disposition_ = kReflectedXSSInvalid;
    policy_->ReportInvalidReflectedXSS(value);
    return;
  }

  SkipWhile<base::IsAsciiWhitespace>(&position, end);
  if (position == end && reflected_xss_disposition_ != kReflectedXSSUnset) {
    return;
  }

  // value1 value2
  //        ^
  reflected_xss_disposition_ = kReflectedXSSInvalid;
  policy_->ReportInvalidReflectedXSS(value);
}

void DirectiveList::ParseReferrer(const std::string& name,
                                  const std::string& value) {
  if (did_set_referrer_policy_) {
    policy_->ReportDuplicateDirective(name);
    referrer_policy_ = kReferrerPolicyNoReferrer;
    return;
  }

  did_set_referrer_policy_ = true;

  if (value.empty()) {
    policy_->ReportInvalidReferrer(value);
    referrer_policy_ = kReferrerPolicyNoReferrer;
    return;
  }

  base::StringPiece characters(value);

  const char* position = characters.begin();
  const char* end = characters.end();

  SkipWhile<base::IsAsciiWhitespace>(&position, end);
  const char* begin = position;
  SkipWhile<IsNotAsciiWhitespace>(&position, end);

  // value1
  //       ^
  if (base::LowerCaseEqualsASCII(begin, position, "unsafe-url")) {
    referrer_policy_ = kReferrerPolicyUnsafeUrl;
  } else if (base::LowerCaseEqualsASCII(begin, position, "no-referrer")) {
    referrer_policy_ = kReferrerPolicyNoReferrer;
  } else if (base::LowerCaseEqualsASCII(begin, position,
                                        "no-referrer-when-downgrade")) {
    referrer_policy_ = kReferrerPolicyDefault;
  } else if (base::LowerCaseEqualsASCII(begin, position, "origin")) {
    referrer_policy_ = kReferrerPolicyOrigin;
  } else if (base::LowerCaseEqualsASCII(begin, position,
                                        "origin-when-cross-origin") ||
             base::LowerCaseEqualsASCII(begin, position,
                                        "origin-when-crossorigin")) {
    referrer_policy_ = kReferrerPolicyOriginWhenCrossOrigin;
  } else {
    policy_->ReportInvalidReferrer(value);
    return;
  }

  SkipWhile<base::IsAsciiWhitespace>(&position, end);
  if (position == end) {
    return;
  }

  // value1 value2
  //        ^
  referrer_policy_ = kReferrerPolicyNoReferrer;
  policy_->ReportInvalidReferrer(value);
}

std::string DirectiveList::ParseSuboriginName(const std::string& policy) {
  base::StringPiece characters(policy);

  const char* position = characters.begin();
  const char* end = characters.end();

  // Parse the name of the suborigin (no spaces, single string)
  SkipWhile<base::IsAsciiWhitespace>(&position, end);
  if (position == end) {
    policy_->ReportInvalidSuboriginFlags("No suborigin name specified.");
    return std::string();
  }

  const char* begin = position;

  SkipWhile<IsAsciiAlphanumeric>(&position, end);
  if (position != end && !base::IsAsciiWhitespace(*position)) {
    policy_->ReportInvalidSuboriginFlags(
        "Invalid character \'" + std::string(position, 1) + "\' in suborigin.");
    return std::string();
  }
  size_t length = static_cast<size_t>(position - begin);
  SkipWhile<base::IsAsciiWhitespace>(&position, end);
  if (position != end) {
    policy_->ReportInvalidSuboriginFlags(
        "Whitespace is not allowed in suborigin names.");
    return std::string();
  }

  return std::string(begin, length);
}

void DirectiveList::AddDirective(const std::string& name,
                                 const std::string& value) {
  DCHECK(!name.empty());
  std::string lower_name = base::ToLowerASCII(name);
  if (lower_name == ContentSecurityPolicy::kDefaultSrc) {
    SetCSPDirective(name, value, &default_src_);
  } else if (lower_name == ContentSecurityPolicy::kScriptSrc) {
    SetCSPDirective(name, value, &script_src_);
    policy_->set_uses_script_hash_algorithms(
        script_src_->hash_algorithms_used());
  } else if (lower_name == ContentSecurityPolicy::kObjectSrc) {
    SetCSPDirective(name, value, &object_src_);
  } else if (lower_name == ContentSecurityPolicy::kFrameAncestors) {
    SetCSPDirective(name, value, &frame_ancestors_);
  } else if (lower_name == ContentSecurityPolicy::kFrameSrc) {
    SetCSPDirective(name, value, &frame_src_);
  } else if (lower_name == ContentSecurityPolicy::kImgSrc) {
    SetCSPDirective(name, value, &img_src_);
  } else if (lower_name == ContentSecurityPolicy::kStyleSrc) {
    SetCSPDirective(name, value, &style_src_);
    policy_->set_uses_style_hash_algorithms(style_src_->hash_algorithms_used());
  } else if (lower_name == ContentSecurityPolicy::kFontSrc) {
    SetCSPDirective(name, value, &font_src_);
  } else if (lower_name == ContentSecurityPolicy::kLocationSrc) {
    SetCSPDirective(name, value, &location_src_);
  } else if (lower_name == ContentSecurityPolicy::kMediaSrc) {
    SetCSPDirective(name, value, &media_src_);
  } else if (lower_name == ContentSecurityPolicy::kConnectSrc) {
    SetCSPDirective(name, value, &connect_src_);
  } else if (lower_name == ContentSecurityPolicy::kSandbox) {
    // TODO: ApplySandboxPolicy().
    // ApplySandboxPolicy(name, value);
    NOTIMPLEMENTED() << ContentSecurityPolicy::kSandbox;
  } else if (lower_name == ContentSecurityPolicy::kReportURI) {
    ParseReportURI(name, value);
  } else if (lower_name == ContentSecurityPolicy::kBaseURI) {
    SetCSPDirective(name, value, &base_uri_);
  } else if (lower_name == ContentSecurityPolicy::kChildSrc) {
    SetCSPDirective(name, value, &child_src_);
  } else if (lower_name == ContentSecurityPolicy::kFormAction) {
    SetCSPDirective(name, value, &form_action_);
  } else if (lower_name == ContentSecurityPolicy::kPluginTypes) {
    SetCSPDirective(name, value, &plugin_types_);
  } else if (lower_name == ContentSecurityPolicy::kReflectedXSS) {
    ParseReflectedXSS(name, value);
  } else if (lower_name == ContentSecurityPolicy::kReferrer) {
    ParseReferrer(name, value);
  } else if (lower_name == ContentSecurityPolicy::kUpgradeInsecureRequests) {
    EnableInsecureRequestsUpgrade(name, value);
  } else if (lower_name == ContentSecurityPolicy::kBlockAllMixedContent) {
    EnforceStrictMixedContentChecking(name, value);
  } else if (lower_name == ContentSecurityPolicy::kManifestSrc) {
    SetCSPDirective(name, value, &manifest_src_);
  } else if (lower_name == ContentSecurityPolicy::kSuborigin) {
    // TODO: ApplySuboriginPolicy.
    // ApplySuboriginPolicy(name, value);
    NOTIMPLEMENTED() << ContentSecurityPolicy::kSuborigin;
  } else {
    policy_->ReportUnsupportedDirective(name);
  }
}

}  // namespace csp
}  // namespace cobalt
