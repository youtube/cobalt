// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_request_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "third_party/re2/src/re2/re2.h"

using base::Value;
using blink::mojom::RequestDigitalIdentityStatus;
using InterstitialType = content::DigitalIdentityInterstitialType;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;
using blink::mojom::GetRequestFormat;

namespace content {
namespace {

constexpr char kOpenid4vpProtocol[] = "openid4vp";
constexpr char kOpenid4vp10Protocol[] = "openid4vp1.0";
constexpr char kPreviewProtocol[] = "preview";

constexpr char kMdlDocumentType[] = "org.iso.18013.5.1.mDL";
constexpr char kMdlNamespace[] = "org.iso.18013.5.1";

constexpr char kOpenid4vpPathRegex[] =
    R"(\$\['org\.iso\.18013\.5\.1'\]\['([^\)]*)'\])";
constexpr char kMdocAgeOverDataElementRegex[] = R"(age_over_\d\d)";
constexpr char kMdocAgeInYearsDataElement[] = "age_in_years";
constexpr char kMdocAgeBirthYearDataElement[] = "age_birth_year";
constexpr char kMdocBirthDateDataElement[] = "birth_date";

constexpr char kDigitalIdentityDialogParam[] = "dialog";
constexpr char kDigitalIdentityNoDialogParamValue[] = "no_dialog";
constexpr char kDigitalIdentityLowRiskDialogParamValue[] = "low_risk";
constexpr char kDigitalIdentityHighRiskDialogParamValue[] = "high_risk";

// Returns entry if `dict` has a list with a single dict element for key
// `list_key`.
const Value::Dict* FindSingleElementListEntry(const Value::Dict& dict,
                                              const std::string& list_key) {
  const Value::List* list = dict.FindList(list_key);
  if (!list || list->size() != 1u) {
    return nullptr;
  }
  return list->front().GetIfDict();
}

// Returns whether an intertitial should be shown for a request which solely
// requests the passed-in mdoc data element.
bool CanMdocDataElementBypassInterstitial(const std::string& data_element) {
  if (re2::RE2::FullMatch(data_element,
                          re2::RE2(kMdocAgeOverDataElementRegex))) {
    return true;
  }

  const std::string kDataElementsCanBypassInterstitial[] = {
      kMdocAgeInYearsDataElement,
      kMdocAgeBirthYearDataElement,
      kMdocBirthDateDataElement,
  };
  return std::find(std::begin(kDataElementsCanBypassInterstitial),
                   std::end(kDataElementsCanBypassInterstitial),
                   data_element) !=
         std::end(kDataElementsCanBypassInterstitial);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithPresentationDefition(
    const Value::Dict& request) {
  const Value::Dict* presentation_dict =
      request.FindDict("presentation_definition");
  if (!presentation_dict) {
    return false;
  }

  const Value::Dict* input_descriptor_dict =
      FindSingleElementListEntry(*presentation_dict, "input_descriptors");
  if (!input_descriptor_dict) {
    return false;
  }

  const std::string* input_descriptor_id =
      input_descriptor_dict->FindString("id");
  if (!input_descriptor_id || *input_descriptor_id != kMdlDocumentType) {
    return false;
  }

  const Value::Dict* constraints_dict =
      input_descriptor_dict->FindDict("constraints");
  if (!constraints_dict) {
    return false;
  }

  const Value::Dict* field_dict =
      FindSingleElementListEntry(*constraints_dict, "fields");
  if (!field_dict) {
    return false;
  }

  const Value::List* field_paths = field_dict->FindList("path");
  if (!field_paths) {
    return false;
  }

  if (!field_paths || field_paths->size() != 1u ||
      !field_paths->front().is_string()) {
    return false;
  }

  std::string mdoc_data_element;
  return re2::RE2::FullMatch(field_paths->front().GetString(),
                             re2::RE2(kOpenid4vpPathRegex),
                             &mdoc_data_element) &&
         CanMdocDataElementBypassInterstitial(mdoc_data_element);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithDCQL(
    const Value::Dict& request) {
  const Value::Dict* query_dict = request.FindDict("dcql_query");
  if (!query_dict) {
    return false;
  }
  auto credential_to_claims = [](const Value::Dict& credential)
      -> std::optional<std::vector<std::string>> {
    const Value::List* claims_list = credential.FindList("claims");
    if (!claims_list) {
      return std::nullopt;
    }
    std::vector<std::string> claims;
    for (const Value& claim : *claims_list) {
      const Value::Dict* claim_dict = claim.GetIfDict();
      if (!claim_dict) {
        return std::nullopt;
      }
      const std::string* namespace_str = claim_dict->FindString("namespace");
      if (!namespace_str || *namespace_str != kMdlNamespace) {
        return std::nullopt;
      }

      const std::string* claim_name = claim_dict->FindString("claim_name");
      if (!claim_name) {
        return std::nullopt;
      }
      claims.push_back(*claim_name);
    }
    return claims;
  };

  base::flat_set<std::string> all_claims;
  const Value::List* credentials = query_dict->FindList("credentials");
  if (!credentials) {
    return false;
  }
  for (const Value& credential : *credentials) {
    const Value::Dict* credential_dict = credential.GetIfDict();
    if (!credential_dict) {
      return false;
    }
    const Value::Dict* meta_dict = credential_dict->FindDict("meta");
    if (!meta_dict) {
      return false;
    }
    const std::string* doctype_value = meta_dict->FindString("doctype_value");
    if (!doctype_value || *doctype_value != kMdlDocumentType) {
      return false;
    }
    std::optional<std::vector<std::string>> credential_claims =
        credential_to_claims(*credential_dict);
    if (!credential_claims.has_value()) {
      return false;
    }
    all_claims.insert(credential_claims->begin(), credential_claims->end());
  }
  return std::ranges::all_of(all_claims, CanMdocDataElementBypassInterstitial);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(
    const Value& request) {
  CHECK(request.is_dict());
  const Value::Dict& request_dict = request.GetDict();
  if (request_dict.contains("presentation_definition")) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithPresentationDefition(
        request_dict);
  }

  if (request_dict.contains("dcql_query")) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithDCQL(
        request_dict);
  }
  return false;
}

bool CanRequestCredentialBypassInterstitialForPreviewProtocol(
    const Value& request) {
  CHECK(request.is_dict());
  const Value::Dict& request_dict = request.GetDict();
  const Value::Dict* selector_dict = request_dict.FindDict("selector");
  if (!selector_dict) {
    return false;
  }

  const std::string* doctype = selector_dict->FindString("doctype");
  if (!doctype || *doctype != kMdlDocumentType) {
    return false;
  }

  const Value::List* fields_list = selector_dict->FindList("fields");
  if (!fields_list || fields_list->size() != 1u) {
    return false;
  }

  const Value::Dict* field_dict = fields_list->front().GetIfDict();
  if (!field_dict) {
    return false;
  }
  const std::string* mdoc_data_element = field_dict->FindString("name");
  return mdoc_data_element &&
         CanMdocDataElementBypassInterstitial(*mdoc_data_element);
}

// Returns whether an interstitial should be shown based on the assertions being
// requested.
bool CanRequestCredentialBypassInterstitial(const std::string& protocol,
                                            const Value& request) {
  if (!request.is_dict()) {
    return false;
  }

  if (protocol == kOpenid4vpProtocol || protocol == kOpenid4vp10Protocol) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(request);
  }
  return protocol == kPreviewProtocol &&
         CanRequestCredentialBypassInterstitialForPreviewProtocol(request);
}

blink::mojom::RequestDigitalIdentityStatus ToRequestDigitalIdentityStatus(
    RequestStatusForMetrics status_for_metrics) {
  switch (status_for_metrics) {
    case RequestStatusForMetrics::kSuccess:
      return blink::mojom::RequestDigitalIdentityStatus::kSuccess;
    case RequestStatusForMetrics::kErrorAborted:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorCanceled;
    case RequestStatusForMetrics::kErrorNoRequests:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorNoRequests;
    case RequestStatusForMetrics::kErrorNoTransientUserActivation:
      return blink::mojom::RequestDigitalIdentityStatus::
          kErrorNoTransientUserActivation;
    case RequestStatusForMetrics::kErrorNoCredential:
    case RequestStatusForMetrics::kErrorUserDeclined:
    case RequestStatusForMetrics::kErrorOther:
      return blink::mojom::RequestDigitalIdentityStatus::kError;
    case RequestStatusForMetrics::kErrorInvalidJson:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorInvalidJson;
  }
}

}  // anonymous namespace

// static
base::WeakPtr<DigitalIdentityRequestImpl>
DigitalIdentityRequestImpl::CreateInstance(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver) {
  // DigitalIdentityRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  DigitalIdentityRequestImpl* instance =
      new DigitalIdentityRequestImpl(host, std::move(receiver));
  return instance->weak_ptr_factory_.GetWeakPtr();
}

// static
std::optional<InterstitialType>
DigitalIdentityRequestImpl::ComputeInterstitialType(
    const url::Origin& rp_origin,
    const DigitalIdentityProvider* provider,
    const std::string& protocol,
    const data_decoder::DataDecoder::ValueOrError& request_data) {
  std::string dialog_param_value = base::GetFieldTrialParamValueByFeature(
      features::kWebIdentityDigitalCredentials, kDigitalIdentityDialogParam);
  if (dialog_param_value == kDigitalIdentityNoDialogParamValue) {
    return std::nullopt;
  }

  if (dialog_param_value == kDigitalIdentityHighRiskDialogParamValue) {
    return InterstitialType::kHighRisk;
  }

  if (dialog_param_value == kDigitalIdentityLowRiskDialogParamValue) {
    return InterstitialType::kLowRisk;
  }

  if (provider->IsLowRiskOrigin(rp_origin)) {
    return std::nullopt;
  }

  return (request_data.has_value() &&
          CanRequestCredentialBypassInterstitial(protocol, *request_data))
             ? std::nullopt
             : std::optional<InterstitialType>(InterstitialType::kLowRisk);
}

DigitalIdentityRequestImpl::DigitalIdentityRequestImpl(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver)
    : DocumentService(host, std::move(receiver)) {}

DigitalIdentityRequestImpl::~DigitalIdentityRequestImpl() = default;

void DigitalIdentityRequestImpl::CompleteRequest(
    std::optional<std::string> protocol,
    const base::expected<std::string, RequestStatusForMetrics>& response) {
  CompleteRequestWithStatus(
      std::move(protocol),
      response.has_value() ? RequestDigitalIdentityStatus::kSuccess
                           : ToRequestDigitalIdentityStatus(response.error()),
      response);
}

void DigitalIdentityRequestImpl::CompleteRequestWithError(
    RequestStatusForMetrics status_for_metrics) {
  CompleteRequest(/*protocol=*/std::nullopt,
                  base::unexpected(status_for_metrics));
}

void DigitalIdentityRequestImpl::CompleteRequestWithStatus(
    std::optional<std::string> protocol,
    RequestDigitalIdentityStatus status,
    const base::expected<std::string, RequestStatusForMetrics>& response) {
  // Invalidate pending requests in case that the request gets aborted.
  weak_ptr_factory_.InvalidateWeakPtrs();

  provider_.reset();
  update_interstitial_on_abort_callback_.Reset();

  base::UmaHistogramEnumeration("Blink.DigitalIdentityRequest.Status",
                                response.has_value()
                                    ? RequestStatusForMetrics::kSuccess
                                    : response.error());

  std::move(callback_).Run(status, std::move(protocol),
                           base::OptionalFromExpected(response));
}

Value BuildGetRequest(blink::mojom::DigitalCredentialRequestPtr request,
                      GetRequestFormat format) {
  auto result = Value::Dict();
  result.Set("protocol", request->protocol);

  result.Set(format == GetRequestFormat::kModern ? "data" : "request",
             request->data);

  Value::Dict out = Value::Dict().Set(
      format == GetRequestFormat::kModern ? "requests" : "providers",
      Value::List().Append(std::move(result)));
  return Value(std::move(out));
}

Value BuildCreateRequest(blink::mojom::DigitalCredentialRequestPtr request) {
  auto result = Value::Dict();
  result.Set("protocol", request->protocol);
  result.Set("data", request->data);
  return Value(std::move(result));
}

void DigitalIdentityRequestImpl::Get(
    std::vector<blink::mojom::DigitalCredentialRequestPtr>
        digital_credential_requests,
    GetRequestFormat format,
    GetCallback callback) {
  if (!IsWebIdentityDigitalCredentialsEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  callback_ = std::move(callback);

  if (!render_frame_host().HasTransientUserActivation()) {
    CompleteRequestWithError(
        RequestStatusForMetrics::kErrorNoTransientUserActivation);
    return;
  }

  if (digital_credential_requests.empty()) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorNoRequests);
    return;
  }

  // TODO(https://crbug.com/40257092): make sure the Digital Credentials
  // API works well with multiple requests.
  if (digital_credential_requests.size() > 1u) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  blink::mojom::DigitalCredentialRequestPtr digital_credential_request =
      std::move(digital_credential_requests[0]);

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  std::string protocol = digital_credential_request->protocol;
  std::string request_json_string = digital_credential_request->data;
  Value request_to_send =
      BuildGetRequest(std::move(digital_credential_request), format);

  data_decoder::DataDecoder::ParseJsonIsolated(
      request_json_string,
      base::BindOnce(&DigitalIdentityRequestImpl::OnGetRequestJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(protocol),
                     std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::Create(
    blink::mojom::DigitalCredentialRequestPtr digital_credential_request,
    CreateCallback callback) {
  if (!IsWebIdentityDigitalCredentialsCreationEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  callback_ = std::move(callback);

  if (!render_frame_host().HasTransientUserActivation()) {
    CompleteRequestWithError(
        RequestStatusForMetrics::kErrorNoTransientUserActivation);
    return;
  }

  if (digital_credential_request.is_null()) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorNoRequests);
    return;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  std::string protocol = digital_credential_request->protocol;
  std::string request_json_string = digital_credential_request->data;

  Value request_to_send =
      BuildCreateRequest(std::move(digital_credential_request));

  // TODO(crbug.com/378330032): consider using Value over mojo instead of string
  // since the param is already a JSON object.
  data_decoder::DataDecoder::ParseJsonIsolated(
      std::move(request_json_string),
      base::BindOnce(&DigitalIdentityRequestImpl::OnCreateRequestJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(protocol),
                     std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::Abort() {
  if (!callback_) {
    // Renderer sent abort request after the browser sent the callback but
    // before the renderer received it.
    return;
  }

  if (update_interstitial_on_abort_callback_) {
    std::move(update_interstitial_on_abort_callback_).Run();
  }

  CompleteRequestWithStatus(
      /*protocol=*/std::nullopt, RequestDigitalIdentityStatus::kErrorCanceled,
      base::unexpected(RequestStatusForMetrics::kErrorAborted));
}

void DigitalIdentityRequestImpl::OnGetRequestJsonParsed(
    std::string protocol,
    Value request_to_send,
    data_decoder::DataDecoder::ValueOrError parsed_result) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort.
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(), std::move(protocol),
                       "fake_test_token"),
        base::Milliseconds(1));
    return;
  }

  provider_ = GetContentClient()->browser()->CreateDigitalIdentityProvider();
  if (!provider_) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  if (!render_frame_host().IsActive() ||
      render_frame_host().GetVisibilityState() !=
          content::PageVisibilityState::kVisible) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  std::optional<InterstitialType> interstitial_type = ComputeInterstitialType(
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin(),
      provider_.get(), protocol, parsed_result);

  if (!interstitial_type) {
    OnInterstitialDone(std::move(protocol), std::move(request_to_send),
                       RequestStatusForMetrics::kSuccess);
    return;
  }

  update_interstitial_on_abort_callback_ =
      provider_->ShowDigitalIdentityInterstitial(
          *WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
          *interstitial_type,
          base::BindOnce(&DigitalIdentityRequestImpl::OnInterstitialDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(protocol),
                         std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::OnCreateRequestJsonParsed(
    std::string protocol,
    Value request_to_send,
    data_decoder::DataDecoder::ValueOrError parsed_result) {
  if (!parsed_result.has_value()) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorInvalidJson);
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort+.
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(), std::move(protocol),
                       "fake_test_token"),
        base::Milliseconds(1));
    return;
  }

  provider_ = GetContentClient()->browser()->CreateDigitalIdentityProvider();
  if (!provider_) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  if (!render_frame_host().IsActive() ||
      render_frame_host().GetVisibilityState() !=
          content::PageVisibilityState::kVisible) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  provider_->Create(
      WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
      request_to_send,
      base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(protocol)));
}

void DigitalIdentityRequestImpl::OnInterstitialDone(
    std::string protocol,
    Value request_to_send,
    RequestStatusForMetrics status_after_interstitial) {
  if (status_after_interstitial != RequestStatusForMetrics::kSuccess) {
    CompleteRequestWithError(status_after_interstitial);
    return;
  }

  provider_->Get(
      WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
      request_to_send,
      base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(protocol)));
}

}  // namespace content
