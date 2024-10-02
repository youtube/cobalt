// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_FETCH_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_FETCH_OPTIONS_H_

#include "services/network/public/mojom/referrer_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/render_blocking_behavior.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMWrapperWorld;
class KURL;
class SecurityOrigin;

// ScriptFetchOptions corresponds to the spec concept "script fetch options".
// https://html.spec.whatwg.org/C/#script-fetch-options
class PLATFORM_EXPORT ScriptFetchOptions final {
  DISALLOW_NEW();

 public:
  enum class AttributionReportingEligibility {
    kIneligible,
    kEligible,
  };

  // https://html.spec.whatwg.org/C/#default-classic-script-fetch-options
  // "The default classic script fetch options are a script fetch options whose
  // cryptographic nonce is the empty string, integrity metadata is the empty
  // string, parser metadata is "not-parser-inserted", credentials mode is
  // "same-origin", and referrer policy is the empty string." [spec text]
  ScriptFetchOptions();
  ScriptFetchOptions(const String& nonce,
                     const IntegrityMetadataSet& integrity_metadata,
                     const String& integrity_attribute,
                     ParserDisposition parser_state,
                     network::mojom::CredentialsMode credentials_mode,
                     network::mojom::ReferrerPolicy referrer_policy,
                     mojom::blink::FetchPriorityHint fetch_priority_hint,
                     RenderBlockingBehavior render_blocking_behavior,
                     RejectCoepUnsafeNone reject_coep_unsafe_none =
                         RejectCoepUnsafeNone(false));
  ~ScriptFetchOptions();

  const String& Nonce() const { return nonce_; }
  const IntegrityMetadataSet& GetIntegrityMetadata() const {
    return integrity_metadata_;
  }
  const String& GetIntegrityAttributeValue() const {
    return integrity_attribute_;
  }
  const ParserDisposition& ParserState() const { return parser_state_; }
  network::mojom::CredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  network::mojom::ReferrerPolicy GetReferrerPolicy() const {
    return referrer_policy_;
  }
  mojom::blink::FetchPriorityHint FetchPriorityHint() const {
    return fetch_priority_hint_;
  }
  RejectCoepUnsafeNone GetRejectCoepUnsafeNone() const {
    return reject_coep_unsafe_none_;
  }
  RenderBlockingBehavior GetRenderBlockingBehavior() const {
    return render_blocking_behavior_;
  }

  void SetAttributionReportingEligibility(
      AttributionReportingEligibility eligibility) {
    attribution_reporting_eligibility_ = eligibility;
  }

  // https://html.spec.whatwg.org/C/#fetch-a-classic-script
  // Steps 1 and 3.
  FetchParameters CreateFetchParameters(
      const KURL&,
      const SecurityOrigin*,
      scoped_refptr<const DOMWrapperWorld> world,
      CrossOriginAttributeValue,
      const WTF::TextEncoding&,
      FetchParameters::DeferOption) const;

 private:
  // https://html.spec.whatwg.org/C/#concept-script-fetch-options-nonce
  const String nonce_;

  // https://html.spec.whatwg.org/C/#concept-script-fetch-options-integrity
  const IntegrityMetadataSet integrity_metadata_;
  const String integrity_attribute_;

  // https://html.spec.whatwg.org/C/#concept-script-fetch-options-parser
  const ParserDisposition parser_state_;

  // https://html.spec.whatwg.org/C/#concept-script-fetch-options-credentials
  const network::mojom::CredentialsMode credentials_mode_;

  // https://html.spec.whatwg.org/C/#concept-script-fetch-options-referrer-policy
  const network::mojom::ReferrerPolicy referrer_policy_;

  // https://wicg.github.io/priority-hints/#script
  const mojom::blink::FetchPriorityHint fetch_priority_hint_;

  const RenderBlockingBehavior render_blocking_behavior_ =
      RenderBlockingBehavior::kUnset;
  // True when we should reject a response with COEP: none.
  // https://wicg.github.io/cross-origin-embedder-policy/#integration-html
  // This is for dedicated workers.
  // TODO(crbug.com/1064920): Remove this once PlzDedicatedWorker ships.
  const RejectCoepUnsafeNone reject_coep_unsafe_none_ =
      RejectCoepUnsafeNone(false);

  // https://wicg.github.io/attribution-reporting-api
  // TODO(crbug.com/1338976): make this member const once the attributionsrc
  // spec is drafted.
  AttributionReportingEligibility attribution_reporting_eligibility_ =
      AttributionReportingEligibility::kIneligible;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_FETCH_OPTIONS_H_
