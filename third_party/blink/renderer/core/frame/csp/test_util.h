// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_TEST_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Simple CSP delegate that stores the console messages logged by the
// ContentSecurityPolicy context and allows retrieving them.
class TestCSPDelegate final : public GarbageCollected<TestCSPDelegate>,
                              public ContentSecurityPolicyDelegate {
 public:
  Vector<String>& console_messages() { return console_messages_; }

  // ContentSecurityPolicyDelegate override
  const SecurityOrigin* GetSecurityOrigin() override {
    return security_origin_.get();
  }
  const KURL& Url() const override { return url_; }
  void SetSandboxFlags(network::mojom::blink::WebSandboxFlags) override {}
  void SetRequireTrustedTypes() override {}
  void AddInsecureRequestPolicy(mojom::blink::InsecureRequestPolicy) override {}
  std::unique_ptr<SourceLocation> GetSourceLocation() override {
    return nullptr;
  }
  absl::optional<uint16_t> GetStatusCode() override { return absl::nullopt; }
  String GetDocumentReferrer() override { return ""; }
  void DispatchViolationEvent(const SecurityPolicyViolationEventInit&,
                              Element*) override {}
  void PostViolationReport(const SecurityPolicyViolationEventInit&,
                           const String& stringified_report,
                           bool is_frame_ancestors_violation,
                           const Vector<String>& report_endpoints,
                           bool use_reporting_api) override {}
  void Count(WebFeature) override {}
  void AddConsoleMessage(ConsoleMessage* message) override {
    console_messages_.push_back(message->Message());
  }
  void AddInspectorIssue(AuditsIssue) override {}
  void DisableEval(const String& error_message) override {}
  void SetWasmEvalErrorMessage(const String& error_message) override {}
  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) override {}
  void DidAddContentSecurityPolicies(
      WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>) override {}

  void Trace(Visitor*) const override {}

 private:
  const KURL url_ = KURL("https://example.test/index.html");
  const scoped_refptr<SecurityOrigin> security_origin_ =
      SecurityOrigin::Create(url_);
  Vector<String> console_messages_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_TEST_UTIL_H_
