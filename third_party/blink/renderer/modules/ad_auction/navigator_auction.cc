// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/navigator_auction.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/aggregation_service/features.h"
#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_fencedframeconfig_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_usvstringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_auction_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_auction_data_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_properties.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_request_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_targeting.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ads.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group_size.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_additional_bid_signature.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_report_buyers_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_protected_audience_private_aggregation_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_adproperties_adpropertiessequence.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/ad_auction/ads.h"
#include "third_party/blink/renderer/modules/ad_auction/join_leave_queue.h"
#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_coordinates.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/url_constants.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

// Helper to manage runtime of abort + promise resolution pipe.
// Can interface to AbortController itself, and has helper classes that can be
// connected to promises via Then and ScriptFunction.
class NavigatorAuction::AuctionHandle final : public AbortSignal::Algorithm {
 public:
  class AuctionHandleFunction : public ScriptFunction::Callable {
   public:
    explicit AuctionHandleFunction(AuctionHandle* auction_handle);

    void Trace(Visitor* visitor) const override;
    ScriptValue Call(ScriptState* script_state, ScriptValue value) final;

    AuctionHandle* auction_handle() { return auction_handle_.Get(); }

   private:
    virtual ScriptValue CallImpl(ScriptState* script_state,
                                 ScriptValue value) = 0;
    Member<AuctionHandle> auction_handle_;
  };

  class JsonResolved : public AuctionHandleFunction {
   public:
    // `field_name` is expected to point to a literal.
    JsonResolved(AuctionHandle* auction_handle,
                 mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
                 mojom::blink::AuctionAdConfigField field,
                 const String& seller_name,
                 const char* field_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const mojom::blink::AuctionAdConfigField field_;
    const String seller_name_;
    const char* const field_name_;
  };

  class PerBuyerSignalsResolved : public AuctionHandleFunction {
   public:
    PerBuyerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  // This is used for perBuyerTimeouts and perBuyerCumulativeTimeouts, with
  // `field` indicating which of the two fields an object is being used for.
  class BuyerTimeoutsResolved : public AuctionHandleFunction {
   public:
    BuyerTimeoutsResolved(AuctionHandle* auction_handle,
                          mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
                          mojom::blink::AuctionAdConfigBuyerTimeoutField field,
                          const String& seller_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const mojom::blink::AuctionAdConfigBuyerTimeoutField field_;
    const String seller_name_;
  };

  class BuyerCurrenciesResolved : public AuctionHandleFunction {
   public:
    BuyerCurrenciesResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  class DirectFromSellerSignalsResolved : public AuctionHandleFunction {
   public:
    DirectFromSellerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name,
        const scoped_refptr<const SecurityOrigin>& seller_origin,
        const absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>&
            interest_group_buyers);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
    const scoped_refptr<const SecurityOrigin> seller_origin_;
    absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>
        interest_group_buyers_;
  };

  class DirectFromSellerSignalsHeaderAdSlotResolved
      : public AuctionHandleFunction {
   public:
    DirectFromSellerSignalsHeaderAdSlotResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  class ServerResponseResolved : public AuctionHandleFunction {
   public:
    ServerResponseResolved(AuctionHandle* auction_handle,
                           mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
                           const String& seller_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  class AdditionalBidsResolved : public AuctionHandleFunction {
   public:
    AdditionalBidsResolved(AuctionHandle* auction_handle,
                           mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
                           const String& seller_name);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;

   private:
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  class ResolveToConfigResolved : public AuctionHandleFunction {
   public:
    ResolveToConfigResolved(AuctionHandle* auction_handle);

    ScriptValue CallImpl(ScriptState* script_state, ScriptValue value) override;
  };

  class Rejected : public AuctionHandleFunction {
   public:
    explicit Rejected(AuctionHandle* auction_handle);

    ScriptValue CallImpl(ScriptState*, ScriptValue) override;
  };

  AuctionHandle(ExecutionContext* context,
                mojo::PendingRemote<mojom::blink::AbortableAdAuction> remote)
      : abortable_ad_auction_(context) {
    abortable_ad_auction_.Bind(
        std::move(remote), context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  }

  ~AuctionHandle() override = default;

  void QueueAttachPromiseHandler(ScriptPromise promise,
                                 ScriptFunction::Callable* success_helper) {
    queued_promises_.emplace_back(promise, success_helper);
  }

  void AttachQueuedPromises(ScriptState& script_state) {
    for (auto& [promise, success_helper] : queued_promises_) {
      promise.Then(
          MakeGarbageCollected<ScriptFunction>(&script_state, success_helper),
          MakeGarbageCollected<ScriptFunction>(
              &script_state,
              MakeGarbageCollected<NavigatorAuction::AuctionHandle::Rejected>(
                  this)));
    }
    queued_promises_.clear();
  }

  void Abort() { abortable_ad_auction_->Abort(); }

  // AbortSignal::Algorithm implementation:
  void Run() override { Abort(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(abortable_ad_auction_);
    visitor->Trace(auction_resolver_);
    visitor->Trace(queued_promises_);
    AbortSignal::Algorithm::Trace(visitor);
  }

  void AuctionComplete(
      ScriptPromiseResolver*,
      std::unique_ptr<ScopedAbortState>,
      bool aborted_by_script,
      const absl::optional<FencedFrame::RedactedFencedFrameConfig>&);

  void MaybeResolveAuction();

  void SetResolveToConfig(bool value) { resolve_to_config_ = value; }

  mojom::blink::AbortableAdAuction* mojo_pipe() {
    return abortable_ad_auction_.get();
  }

 private:
  VectorOfPairs<ScriptPromise, ScriptFunction::Callable> queued_promises_;
  HeapMojoRemote<mojom::blink::AbortableAdAuction> abortable_ad_auction_;

  absl::optional<bool> resolve_to_config_;
  Member<ScriptPromiseResolver> auction_resolver_;
  absl::optional<FencedFrame::RedactedFencedFrameConfig> auction_config_;
};

namespace {

// The maximum number of active cross-site joins and leaves. Once these are hit,
// cross-site joins/leaves are queued until they drop below this number. Queued
// pending operations are dropped on destruction / navigation away.
const int kMaxActiveCrossSiteJoins = 20;
const int kMaxActiveCrossSiteLeaves = 20;
const int kMaxActiveCrossSiteClears = 20;

// Error string builders.
String ErrorInvalidInterestGroup(const AuctionAdInterestGroup& group,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  StringBuilder error_builder;
  if (!field_name.empty()) {
    error_builder.AppendFormat("%s '%s' for ", field_name.Utf8().c_str(),
                               field_value.Utf8().c_str());
  }
  error_builder.AppendFormat(
      "AuctionAdInterestGroup with owner '%s' and name '%s' ",
      group.owner().Utf8().c_str(), group.name().Utf8().c_str());
  error_builder.Append(error);
  return error_builder.ReleaseString();
}

String ErrorInvalidInterestGroupJson(const AuctionAdInterestGroup& group,
                                     const String& field_name) {
  return String::Format(
      "%s for AuctionAdInterestGroup with owner '%s' and name '%s' must be a "
      "JSON-serializable object.",
      field_name.Utf8().c_str(), group.owner().Utf8().c_str(),
      group.name().Utf8().c_str());
}

String ErrorInvalidAuctionConfigSeller(const String& seller_name,
                                       const String& field_name,
                                       const String& field_value,
                                       const String& error) {
  return String::Format("%s '%s' for AuctionAdConfig with seller '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        seller_name.Utf8().c_str(), error.Utf8().c_str());
}

String ErrorInvalidAuctionConfig(const AuctionAdConfig& config,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  return ErrorInvalidAuctionConfigSeller(config.seller(), field_name,
                                         field_value, error);
}

String ErrorInvalidAuctionConfigSellerJson(const String& seller_name,
                                           const String& field_name) {
  return String::Format(
      "%s for AuctionAdConfig with seller '%s' must be a JSON-serializable "
      "object.",
      field_name.Utf8().c_str(), seller_name.Utf8().c_str());
}

String ErrorInvalidAdRequestConfig(const AdRequestConfig& config,
                                   const String& field_name,
                                   const String& field_value,
                                   const String& error) {
  return String::Format("%s '%s' for AdRequestConfig with URL '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        config.adRequestUrl().Utf8().c_str(),
                        error.Utf8().c_str());
}

String ErrorInvalidAuctionConfigUint128(const AuctionAdConfig& config,
                                        const String& field_name,
                                        const String& error) {
  return String::Format("%s for AuctionAdConfig with seller '%s': %s",
                        field_name.Utf8().c_str(),
                        config.seller().Utf8().c_str(), error.Utf8().c_str());
}

String ErrorRenameMismatch(const String& old_field_name,
                           const String& old_field_value,
                           const String& new_field_name,
                           const String& new_field_value) {
  return String::Format(
      "%s doesn't have the same value as %s ('%s' vs '%s')",
      old_field_name.Utf8().c_str(), new_field_name.Utf8().c_str(),
      old_field_value.Utf8().c_str(), new_field_value.Utf8().c_str());
}

String ErrorMissingRequired(const String& required_field_name) {
  return String::Format("Missing required field %s",
                        required_field_name.Utf8().c_str());
}

String WarningPermissionsPolicy(const String& feature, const String& api) {
  return String::Format(
      "In the future, Permissions Policy feature %s will not be enabled by "
      "default in cross-origin iframes or same-origin iframes nested in "
      "cross-origin iframes. Calling %s will be rejected with NotAllowedError "
      "if it is not explicitly enabled",
      feature.Utf8().c_str(), api.Utf8().c_str());
}

// Console warnings.

void AddWarningMessageToConsole(const ExecutionContext& execution_context,
                                const String& message) {
  auto* window = To<LocalDOMWindow>(&execution_context);
  WebLocalFrameImpl::FromFrame(window->GetFrame())
      ->AddMessageToConsole(
          WebConsoleMessage(mojom::blink::ConsoleMessageLevel::kWarning,
                            message),
          /*discard_duplicates=*/true);
}

void ConsoleWarnDeprecatedEnum(const ExecutionContext& execution_context,
                               String enum_name,
                               String deprecated_value) {
  AddWarningMessageToConsole(
      execution_context,
      String::Format("Enum %s used deprecated value %s -- \"dashed-naming\" "
                     "should be used instead of \"camelCase\".",
                     enum_name.Utf8().c_str(),
                     deprecated_value.Utf8().c_str()));
}

// JSON and Origin conversion helpers.

bool Jsonify(const ScriptState& script_state,
             const v8::Local<v8::Value>& value,
             String& output) {
  v8::Local<v8::String> v8_string;
  // v8::JSON throws on certain inputs that can't be converted to JSON (like
  // recursive structures). Use TryCatch to consume them. Otherwise, they'd take
  // precedence over the returned ExtensionState for methods that return
  // ScriptPromises, since ExceptionState is used to generate a rejected
  // promise, which V8 exceptions take precedence over.
  v8::TryCatch try_catch(script_state.GetIsolate());
  if (!v8::JSON::Stringify(script_state.GetContext(), value)
           .ToLocal(&v8_string) ||
      try_catch.HasCaught()) {
    return false;
  }

  output = ToCoreString(v8_string);
  // JSON.stringify can fail to produce a string value in one of two ways: it
  // can throw an exception (as with unserializable objects), or it can return
  // `undefined` (as with e.g. passing a function). If JSON.stringify returns
  // `undefined`, the v8 API then coerces it to the string value "undefined".
  // Check for this, and consider it a failure (since we didn't properly
  // serialize a value, and v8::JSON::Parse() rejects "undefined").
  return output != "undefined";
}

base::expected<absl::uint128, String> CopyBigIntToUint128(
    const BigInt& bigint) {
  if (!bigint.FitsIn128Bits()) {
    return base::unexpected("Too large BigInt; Must fit in 128 bits");
  }
  if (bigint.IsNegative()) {
    return base::unexpected("Negative BigInt cannot be converted to uint128");
  }
  return *bigint.ToUInt128();
}

// Returns nullptr if |origin_string| couldn't be parsed into an acceptable
// origin.
scoped_refptr<const SecurityOrigin> ParseOrigin(const String& origin_string) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(origin_string);
  if (origin->Protocol() != url::kHttpsScheme) {
    return nullptr;
  }
  return origin;
}

// WebIDL -> Mojom copy functions -- each return true if successful (including
// the not present, nothing to copy case), returns false and throws JS exception
// for invalid input.

// joinAdInterestGroup() copy functions.

// TODO(crbug.com/1451034): Remove method when old expiration is removed.
bool CopyLifetimeIdlToMojo(ExceptionState& exception_state,
                           absl::optional<double> lifetime_seconds,
                           const AuctionAdInterestGroup& input,
                           mojom::blink::InterestGroup& output) {
  absl::optional<base::TimeDelta> lifetime_old =
      lifetime_seconds
          ? absl::optional<base::TimeDelta>(base::Seconds(*lifetime_seconds))
          : absl::nullopt;
  absl::optional<base::TimeDelta> lifetime_new =
      input.hasLifetimeMs() ? absl::optional<base::TimeDelta>(
                                  base::Milliseconds(input.lifetimeMs()))
                            : absl::nullopt;
  if (lifetime_old && !lifetime_new) {
    lifetime_new = lifetime_old;
  }
  if (!lifetime_new) {
    exception_state.ThrowTypeError(ErrorMissingRequired("lifetimeMs"));
    return false;
  }
  output.expiry = base::Time::Now() + *lifetime_new;
  return true;
}

bool CopyOwnerFromIdlToMojo(const ExecutionContext& execution_context,
                            ExceptionState& exception_state,
                            const AuctionAdInterestGroup& input,
                            mojom::blink::InterestGroup& output) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(input.owner());
  if (!owner) {
    exception_state.ThrowTypeError(String::Format(
        "owner '%s' for AuctionAdInterestGroup with name '%s' must be a valid "
        "https origin.",
        input.owner().Utf8().c_str(), input.name().Utf8().c_str()));
    return false;
  }

  output.owner = std::move(owner);
  return true;
}

// Converts a sparse vector used in `priority_vector` and
// `priority_signals_overrides` to a WTF::HashMap, as is used in mojom structs.
// Has no failure cases.
WTF::HashMap<WTF::String, double> ConvertSparseVectorIdlToMojo(
    const Vector<std::pair<WTF::String, double>>& priority_signals_in) {
  WTF::HashMap<WTF::String, double> priority_signals_out;
  for (const auto& key_value_pair : priority_signals_in) {
    priority_signals_out.insert(key_value_pair.first, key_value_pair.second);
  }
  return priority_signals_out;
}

mojom::blink::SellerCapabilitiesPtr ConvertSellerCapabilitiesTypeFromIdlToMojo(
    const ExecutionContext& execution_context,
    const Vector<String>& capabilities_vector) {
  auto seller_capabilities = mojom::blink::SellerCapabilities::New();
  for (const String& capability_str : capabilities_vector) {
    const bool used_deprecated_names =
        capability_str == "interestGroupCounts" ||
        capability_str == "latencyStats";
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.EnumNaming.Renderer.SellerCapabilities",
        used_deprecated_names);
    if (used_deprecated_names) {
      ConsoleWarnDeprecatedEnum(execution_context, "SellerCapabilities",
                                capability_str);
    }
    if (capability_str == "interest-group-counts" ||
        capability_str == "interestGroupCounts") {
      seller_capabilities->allows_interest_group_counts = true;
    } else if (capability_str == "latency-stats" ||
               capability_str == "latencyStats") {
      seller_capabilities->allows_latency_stats = true;
    } else {
      // For forward compatibility with new values, don't throw.
      continue;
    }
  }
  return seller_capabilities;
}

bool CopySellerCapabilitiesFromIdlToMojo(
    const ExecutionContext& execution_context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  output.all_sellers_capabilities = mojom::blink::SellerCapabilities::New();
  if (!input.hasSellerCapabilities()) {
    return true;
  }

  for (const auto& [origin_string, capabilities_vector] :
       input.sellerCapabilities()) {
    mojom::blink::SellerCapabilitiesPtr seller_capabilities =
        ConvertSellerCapabilitiesTypeFromIdlToMojo(execution_context,
                                                   capabilities_vector);
    if (origin_string == "*") {
      output.all_sellers_capabilities = std::move(seller_capabilities);
    } else {
      if (!output.seller_capabilities) {
        output.seller_capabilities.emplace();
      }
      output.seller_capabilities->insert(
          SecurityOrigin::CreateFromString(origin_string),
          std::move(seller_capabilities));
    }
  }

  return true;
}

bool CopyExecutionModeFromIdlToMojo(const ExecutionContext& execution_context,
                                    ExceptionState& exception_state,
                                    const AuctionAdInterestGroup& input,
                                    mojom::blink::InterestGroup& output) {
  if (!input.hasExecutionMode()) {
    return true;
  }
  const bool used_deprecated_names = input.executionMode() == "groupByOrigin";
  base::UmaHistogramBoolean(
      "Ads.InterestGroup.EnumNaming.Renderer.WorkletExecutionMode",
      used_deprecated_names);
  if (used_deprecated_names) {
    ConsoleWarnDeprecatedEnum(execution_context, "executionMode",
                              input.executionMode());
  }

  // TODO(crbug.com/1330341): Support "frozen-context".
  if (input.executionMode() == "compatibility") {
    output.execution_mode =
        mojom::blink::InterestGroup::ExecutionMode::kCompatibilityMode;
  } else if (input.executionMode() == "group-by-origin" ||
             input.executionMode() == "groupByOrigin") {
    output.execution_mode =
        mojom::blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  } else if (input.executionMode() == "frozen-context") {
    output.execution_mode =
        mojom::blink::InterestGroup::ExecutionMode::kFrozenContext;
  }
  // For forward compatibility with new values, don't throw if unrecognized enum
  // values encountered.
  return true;
}

bool CopyBiddingLogicUrlFromIdlToMojo(const ExecutionContext& context,
                                      ExceptionState& exception_state,
                                      const AuctionAdInterestGroup& input,
                                      mojom::blink::InterestGroup& output) {
  if (!input.hasBiddingLogicURL()) {
    return true;
  }
  KURL bidding_url = context.CompleteURL(input.biddingLogicURL());
  if (!bidding_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "biddingLogicURL", input.biddingLogicURL(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  output.bidding_url = bidding_url;
  return true;
}

bool CopyWasmHelperUrlFromIdlToMojo(const ExecutionContext& context,
                                    ExceptionState& exception_state,
                                    const AuctionAdInterestGroup& input,
                                    mojom::blink::InterestGroup& output) {
  if (!input.hasBiddingWasmHelperURL()) {
    return true;
  }
  KURL wasm_url = context.CompleteURL(input.biddingWasmHelperURL());
  if (!wasm_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "biddingWasmHelperURL", input.biddingWasmHelperURL(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // ValidateBlinkInterestGroup will checks whether this follows all the rules.
  output.bidding_wasm_helper_url = wasm_url;
  return true;
}

bool CopyUpdateUrlFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AuctionAdInterestGroup& input,
                                mojom::blink::InterestGroup& output) {
  if (input.hasUpdateURL()) {
    if (input.hasDailyUpdateUrl() &&
        input.updateURL() != input.dailyUpdateUrl()) {
      exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
          input, "updateURL", input.updateURL(),
          "must match dailyUpdateUrl, when both are present."));
      return false;
    }
    KURL update_url = context.CompleteURL(input.updateURL());
    if (!update_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "updateURL", input.updateURL(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    output.update_url = update_url;
    return true;
  }
  if (input.hasDailyUpdateUrl()) {
    KURL daily_update_url = context.CompleteURL(input.dailyUpdateUrl());
    if (!daily_update_url.IsValid()) {
      exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
          input, "dailyUpdateUrl", input.dailyUpdateUrl(),
          "cannot be resolved to a valid URL."));
      return false;
    }
    output.update_url = daily_update_url;
  }
  return true;
}

bool CopyTrustedBiddingSignalsUrlFromIdlToMojo(
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasTrustedBiddingSignalsURL()) {
    return true;
  }
  KURL trusted_bidding_signals_url =
      context.CompleteURL(input.trustedBiddingSignalsURL());
  if (!trusted_bidding_signals_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "trustedBiddingSignalsURL", input.trustedBiddingSignalsURL(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  output.trusted_bidding_signals_url = trusted_bidding_signals_url;
  return true;
}

bool CopyTrustedBiddingSignalsKeysFromIdlToMojo(
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasTrustedBiddingSignalsKeys()) {
    return true;
  }
  output.trusted_bidding_signals_keys.emplace();
  for (const auto& key : input.trustedBiddingSignalsKeys()) {
    output.trusted_bidding_signals_keys->push_back(key);
  }
  return true;
}

bool CopyUserBiddingSignalsFromIdlToMojo(const ScriptState& script_state,
                                         ExceptionState& exception_state,
                                         const AuctionAdInterestGroup& input,
                                         mojom::blink::InterestGroup& output) {
  if (!input.hasUserBiddingSignals()) {
    return true;
  }
  if (!Jsonify(script_state, input.userBiddingSignals().V8Value(),
               output.user_bidding_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidInterestGroupJson(input, "userBiddingSignals"));
    return false;
  }

  return true;
}

bool CopyAdsFromIdlToMojo(const ExecutionContext& context,
                          const ScriptState& script_state,
                          ExceptionState& exception_state,
                          const AuctionAdInterestGroup& input,
                          mojom::blink::InterestGroup& output) {
  if (!input.hasAds()) {
    return true;
  }
  output.ads.emplace();
  for (const auto& ad : input.ads()) {
    auto mojo_ad = mojom::blink::InterestGroupAd::New();
    KURL render_url = context.CompleteURL(ad->renderURL());
    if (!render_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "ad renderURL", ad->renderURL(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    mojo_ad->render_url = render_url;
    if (ad->hasSizeGroup()) {
      mojo_ad->size_group = ad->sizeGroup();
    }
    if (ad->hasBuyerReportingId()) {
      mojo_ad->buyer_reporting_id = ad->buyerReportingId();
    }
    if (ad->hasBuyerAndSellerReportingId()) {
      mojo_ad->buyer_and_seller_reporting_id = ad->buyerAndSellerReportingId();
    }
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value(), mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    if (ad->hasAdRenderId()) {
      mojo_ad->ad_render_id = ad->adRenderId();
    }
    if (ad->hasAllowedReportingOrigins()) {
      mojo_ad->allowed_reporting_origins.emplace();
      for (const String& origin_string : ad->allowedReportingOrigins()) {
        scoped_refptr<const SecurityOrigin> origin = ParseOrigin(origin_string);
        if (origin) {
          mojo_ad->allowed_reporting_origins->push_back(std::move(origin));
        } else {
          exception_state.ThrowTypeError(
              ErrorInvalidInterestGroup(input, "ad allowedReportingOrigins", "",
                                        "must all be https origins."));
          return false;
        }
      }
    }
    output.ads->push_back(std::move(mojo_ad));
  }
  return true;
}

bool CopyAdComponentsFromIdlToMojo(const ExecutionContext& context,
                                   const ScriptState& script_state,
                                   ExceptionState& exception_state,
                                   const AuctionAdInterestGroup& input,
                                   mojom::blink::InterestGroup& output) {
  if (!input.hasAdComponents()) {
    return true;
  }
  output.ad_components.emplace();
  for (const auto& ad : input.adComponents()) {
    auto mojo_ad = mojom::blink::InterestGroupAd::New();
    KURL render_url = context.CompleteURL(ad->renderURL());
    if (!render_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "ad renderURL", ad->renderURL(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    mojo_ad->render_url = render_url;
    if (ad->hasSizeGroup()) {
      mojo_ad->size_group = ad->sizeGroup();
    }
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value(), mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    if (ad->hasAdRenderId()) {
      mojo_ad->ad_render_id = ad->adRenderId();
    }
    output.ad_components->push_back(std::move(mojo_ad));
  }
  return true;
}

bool CopyAdSizesFromIdlToMojo(const ExecutionContext& context,
                              const ScriptState& script_state,
                              ExceptionState& exception_state,
                              const AuctionAdInterestGroup& input,
                              mojom::blink::InterestGroup& output) {
  if (!input.hasAdSizes()) {
    return true;
  }
  output.ad_sizes.emplace();
  for (const auto& [name, size] : input.adSizes()) {
    auto [width_val, width_units] =
        blink::ParseAdSizeString(size->width().Ascii());
    auto [height_val, height_units] =
        blink::ParseAdSizeString(size->height().Ascii());

    output.ad_sizes->insert(
        name, mojom::blink::AdSize::New(width_val, width_units, height_val,
                                        height_units));
  }
  return true;
}

bool CopySizeGroupsFromIdlToMojo(const ExecutionContext& context,
                                 const ScriptState& script_state,
                                 ExceptionState& exception_state,
                                 const AuctionAdInterestGroup& input,
                                 mojom::blink::InterestGroup& output) {
  if (!input.hasSizeGroups()) {
    return true;
  }
  output.size_groups.emplace();
  for (const auto& group : input.sizeGroups()) {
    output.size_groups->insert(group.first, group.second);
  }
  return true;
}

bool CopyAuctionServerRequestFlagsFromIdlToMojo(
    const ExecutionContext& execution_context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  output.auction_server_request_flags =
      mojom::blink::AuctionServerRequestFlags::New();
  if (!input.hasAuctionServerRequestFlags()) {
    return true;
  }

  for (const String& flag : input.auctionServerRequestFlags()) {
    if (flag == "omit-ads") {
      output.auction_server_request_flags->omit_ads = true;
    } else if (flag == "include-full-ads") {
      output.auction_server_request_flags->include_full_ads = true;
    }
  }
  return true;
}

bool CopyAdditionalBidKeyFromIdlToMojo(
    const ExecutionContext& execution_context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasAdditionalBidKey()) {
    return true;
  }
  WTF::Vector<char> decoded_key;
  if (!WTF::Base64Decode(input.additionalBidKey(), decoded_key,
                         WTF::Base64DecodePolicy::kForgiving)) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "additionalBidKey", input.additionalBidKey(),
        "cannot be base64 decoded."));
    return false;
  }
  if (decoded_key.size() != ED25519_PUBLIC_KEY_LEN) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "additionalBidKey", input.additionalBidKey(),
        String::Format("must be exactly %d' bytes, was %u.",
                       ED25519_PUBLIC_KEY_LEN, decoded_key.size())));
    return false;
  }
  output.additional_bid_key.emplace(ED25519_PUBLIC_KEY_LEN);
  std::copy(decoded_key.begin(), decoded_key.end(),
            output.additional_bid_key->begin());
  return true;
}

bool GetAggregationCoordinatorFromConfig(
    ExceptionState& exception_state,
    const ProtectedAudiencePrivateAggregationConfig& config,
    scoped_refptr<const SecurityOrigin>& aggregation_coordinator_origin_out) {
  if (!config.hasAggregationCoordinatorOrigin()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiMultipleCloudProviders) ||
      !base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders)) {
    // Ignore the specified aggregation coordinator unless the feature is
    // enabled.
    return true;
  }

  scoped_refptr<const SecurityOrigin> aggregation_coordinator_origin =
      ParseOrigin(config.aggregationCoordinatorOrigin());
  if (!aggregation_coordinator_origin) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        String::Format(
            "aggregationCoordinatorOrigin '%s' must be a valid https origin.",
            config.aggregationCoordinatorOrigin().Utf8().c_str()));
    return false;
  }
  if (!aggregation_service::IsAggregationCoordinatorOriginAllowed(
          aggregation_coordinator_origin->ToUrlOrigin())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::Format("aggregationCoordinatorOrigin '%s' is not a recognized "
                       "coordinator origin.",
                       config.aggregationCoordinatorOrigin().Utf8().c_str()));
    return false;
  }
  aggregation_coordinator_origin_out =
      std::move(aggregation_coordinator_origin);
  return true;
}

bool CopyAggregationCoordinatorOriginFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasPrivateAggregationConfig()) {
    return true;
  }

  return GetAggregationCoordinatorFromConfig(
      exception_state, *input.privateAggregationConfig(),
      output.aggregation_coordinator_origin);
}

// createAdRequest copy functions.
bool CopyAdRequestUrlFromIdlToMojo(const ExecutionContext& context,
                                   ExceptionState& exception_state,
                                   const AdRequestConfig& input,
                                   mojom::blink::AdRequestConfig& output) {
  KURL ad_request_url = context.CompleteURL(input.adRequestUrl());
  if (!ad_request_url.IsValid() ||
      (ad_request_url.Protocol() != url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        String::Format("adRequestUrl '%s' for AdRequestConfig must "
                       "be a valid https origin.",
                       input.adRequestUrl().Utf8().c_str()));
    return false;
  }
  output.ad_request_url = ad_request_url;
  return true;
}

bool CopyAdPropertiesFromIdlToMojo(const ExecutionContext& context,
                                   ExceptionState& exception_state,
                                   const AdRequestConfig& input,
                                   mojom::blink::AdRequestConfig& output) {
  if (!input.hasAdProperties()) {
    exception_state.ThrowTypeError(
        ErrorInvalidAdRequestConfig(input, "adProperties", input.adRequestUrl(),
                                    "must be provided to createAdRequest."));
    return false;
  }

  // output.ad_properties = mojom::blink::AdProperties::New();
  switch (input.adProperties()->GetContentType()) {
    case V8UnionAdPropertiesOrAdPropertiesSequence::ContentType::
        kAdProperties: {
      const auto* ad_properties = input.adProperties()->GetAsAdProperties();
      auto mojo_ad_properties = mojom::blink::AdProperties::New();
      mojo_ad_properties->width =
          ad_properties->hasWidth() ? ad_properties->width() : "";
      mojo_ad_properties->height =
          ad_properties->hasHeight() ? ad_properties->height() : "";
      mojo_ad_properties->slot =
          ad_properties->hasSlot() ? ad_properties->slot() : "";
      mojo_ad_properties->lang =
          ad_properties->hasLang() ? ad_properties->lang() : "";
      mojo_ad_properties->ad_type =
          ad_properties->hasAdtype() ? ad_properties->adtype() : "";
      mojo_ad_properties->bid_floor =
          ad_properties->hasBidFloor() ? ad_properties->bidFloor() : 0.0;

      output.ad_properties.push_back(std::move(mojo_ad_properties));
      break;
    }
    case V8UnionAdPropertiesOrAdPropertiesSequence::ContentType::
        kAdPropertiesSequence: {
      if (input.adProperties()->GetAsAdPropertiesSequence().size() <= 0) {
        exception_state.ThrowTypeError(ErrorInvalidAdRequestConfig(
            input, "adProperties", input.adRequestUrl(),
            "must be non-empty to createAdRequest."));
        return false;
      }

      for (const auto& ad_properties :
           input.adProperties()->GetAsAdPropertiesSequence()) {
        auto mojo_ad_properties = mojom::blink::AdProperties::New();
        mojo_ad_properties->width =
            ad_properties->hasWidth() ? ad_properties->width() : "";
        mojo_ad_properties->height =
            ad_properties->hasHeight() ? ad_properties->height() : "";
        mojo_ad_properties->slot =
            ad_properties->hasSlot() ? ad_properties->slot() : "";
        mojo_ad_properties->lang =
            ad_properties->hasLang() ? ad_properties->lang() : "";
        mojo_ad_properties->ad_type =
            ad_properties->hasAdtype() ? ad_properties->adtype() : "";
        mojo_ad_properties->bid_floor =
            ad_properties->hasBidFloor() ? ad_properties->bidFloor() : 0.0;

        output.ad_properties.push_back(std::move(mojo_ad_properties));
      }
      break;
    }
  }
  return true;
}

bool CopyTargetingFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AdRequestConfig& input,
                                mojom::blink::AdRequestConfig& output) {
  if (!input.hasTargeting()) {
    // Targeting information is not required.
    return true;
  }

  output.targeting = mojom::blink::AdTargeting::New();

  if (input.targeting()->hasInterests()) {
    output.targeting->interests.emplace();
    for (const auto& interest : input.targeting()->interests()) {
      output.targeting->interests->push_back(interest);
    }
  }

  if (input.targeting()->hasGeolocation()) {
    output.targeting->geolocation = mojom::blink::AdGeolocation::New();
    output.targeting->geolocation->latitude =
        input.targeting()->geolocation()->latitude();
    output.targeting->geolocation->longitude =
        input.targeting()->geolocation()->longitude();
  }

  return true;
}

bool CopyAdSignalsFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AdRequestConfig& input,
                                mojom::blink::AdRequestConfig& output) {
  if (!input.hasAnonymizedProxiedSignals()) {
    // AdSignals information is not required.
    return true;
  }

  output.anonymized_proxied_signals.emplace();

  for (const auto& signal : input.anonymizedProxiedSignals()) {
    if (signal == "coarse-geolocation") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kCourseGeolocation);
    } else if (signal == "coarse-ua") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kCourseUserAgent);
    } else if (signal == "targeting") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kTargeting);
    } else if (signal == "user-ad-interests") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kUserAdInterests);
    }
  }
  return true;
}

bool CopyFallbackSourceFromIdlToMojo(const ExecutionContext& context,
                                     ExceptionState& exception_state,
                                     const AdRequestConfig& input,
                                     mojom::blink::AdRequestConfig& output) {
  if (!input.hasFallbackSource()) {
    // FallbackSource information is not required.
    return true;
  }

  KURL fallback_source = context.CompleteURL(input.fallbackSource());
  if (!fallback_source.IsValid() ||
      (fallback_source.Protocol() != url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        String::Format("fallbackSource '%s' for AdRequestConfig must "
                       "be a valid https origin.",
                       input.fallbackSource().Utf8().c_str()));
    return false;
  }
  output.fallback_source = fallback_source;
  return true;
}

// runAdAuction() copy functions.

bool CopySellerFromIdlToMojo(ExceptionState& exception_state,
                             const AuctionAdConfig& input,
                             mojom::blink::AuctionAdConfig& output) {
  scoped_refptr<const SecurityOrigin> seller = ParseOrigin(input.seller());
  if (!seller) {
    exception_state.ThrowTypeError(String::Format(
        "seller '%s' for AuctionAdConfig must be a valid https origin.",
        input.seller().Utf8().c_str()));
    return false;
  }
  output.seller = seller;
  return true;
}

bool CopyServerResponseFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasServerResponse()) {
    if (input.hasRequestId()) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "requestId", input.requestId(),
          "should not be specified without 'serverResponse'"));
      return false;
    }
    // If it has neither field we have nothing to do, so return success.
    return true;
  }
  if (!input.hasRequestId()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "serverResponse", "Promise",
        "should not be specified without 'requestId'"));
    return false;
  }
  output.server_response = mojom::blink::AuctionAdServerResponseConfig::New();
  base::Uuid request_id = base::Uuid::ParseLowercase(input.requestId().Ascii());
  if (!request_id.is_valid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "serverResponse.requestId", input.requestId(),
        "must be a valid request ID provided by the API."));
    return false;
  }
  output.server_response->request_id = std::move(request_id);

  if (!auction_handle) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "serverResponse for AuctionAdConfig is not supported");
    return false;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.serverResponse(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::ServerResponseResolved>(
          auction_handle, auction_id->Clone(), input.seller()));
  return true;
}

bool CopyDecisionLogicUrlFromIdlToMojo(const ExecutionContext& context,
                                       ExceptionState& exception_state,
                                       const AuctionAdConfig& input,
                                       mojom::blink::AuctionAdConfig& output) {
  if (!input.hasDecisionLogicURL()) {
    return true;
  }
  KURL decision_logic_url = context.CompleteURL(input.decisionLogicURL());
  if (!decision_logic_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "decisionLogicURL", input.decisionLogicURL(),
        "cannot be resolved to a valid URL."));
    return false;
  }

  // Need to check scheme of the URL in addition to comparing origins because
  // FLEDGE currently only supports HTTPS URLs, and some non-HTTPS URLs can have
  // HTTPS origins.
  if (decision_logic_url.Protocol() != url::kHttpsScheme ||
      !output.seller->IsSameOriginWith(
          SecurityOrigin::Create(decision_logic_url).get())) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "decisionLogicURL", input.decisionLogicURL(),
        "must match seller origin."));
    return false;
  }

  output.decision_logic_url = decision_logic_url;
  return true;
}

bool CopyTrustedScoringSignalsFromIdlToMojo(
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasTrustedScoringSignalsURL()) {
    return true;
  }
  KURL trusted_scoring_signals_url =
      context.CompleteURL(input.trustedScoringSignalsURL());
  if (!trusted_scoring_signals_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "trustedScoringSignalsURL", input.trustedScoringSignalsURL(),
        "cannot be resolved to a valid URL."));
    return false;
  }

  // Need to check scheme of the URL in addition to comparing origins because
  // FLEDGE currently only supports HTTPS URLs, and some non-HTTPS URLs can have
  // HTTPS origins.
  if (trusted_scoring_signals_url.Protocol() != url::kHttpsScheme ||
      !output.seller->IsSameOriginWith(
          SecurityOrigin::Create(trusted_scoring_signals_url).get())) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "trustedScoringSignalsURL", input.trustedScoringSignalsURL(),
        "must match seller origin."));
    return false;
  }

  output.trusted_scoring_signals_url = trusted_scoring_signals_url;
  return true;
}

bool CopyInterestGroupBuyersFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  DCHECK(!output.auction_ad_config_non_shared_params->interest_group_buyers);

  if (!input.hasInterestGroupBuyers()) {
    return true;
  }

  Vector<scoped_refptr<const SecurityOrigin>> buyers;
  for (const auto& buyer_str : input.interestGroupBuyers()) {
    scoped_refptr<const SecurityOrigin> buyer = ParseOrigin(buyer_str);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "interestGroupBuyers buyer", buyer_str,
          "must be a valid https origin."));
      return false;
    }
    buyers.push_back(buyer);
  }
  output.auction_ad_config_non_shared_params->interest_group_buyers =
      std::move(buyers);
  return true;
}

mojom::blink::AuctionAdConfigMaybePromiseJsonPtr
ConvertJsonPromiseFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    const ScriptPromise& promise,
    mojom::blink::AuctionAdConfigField field,
    const char* field_name) {
  auction_handle->QueueAttachPromiseHandler(
      promise,
      MakeGarbageCollected<NavigatorAuction::AuctionHandle::JsonResolved>(
          auction_handle, auction_id->Clone(), field, input.seller(),
          field_name));
  return mojom::blink::AuctionAdConfigMaybePromiseJson::NewPromise(0);
}

// null `auction_handle` disables promise handling.
// `auction_id` should be null iff `auction_handle` is.
void CopyAuctionSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  DCHECK_EQ(auction_id == nullptr, auction_handle == nullptr);

  if (!input.hasAuctionSignals()) {
    output.auction_ad_config_non_shared_params->auction_signals =
        mojom::blink::AuctionAdConfigMaybePromiseJson::NewValue(String());
    return;
  }

  output.auction_ad_config_non_shared_params->auction_signals =
      ConvertJsonPromiseFromIdlToMojo(
          auction_handle, auction_id, input, input.auctionSignals(),
          mojom::blink::AuctionAdConfigField::kAuctionSignals,
          "auctionSignals");
}

void CopySellerSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasSellerSignals()) {
    output.auction_ad_config_non_shared_params->seller_signals =
        mojom::blink::AuctionAdConfigMaybePromiseJson::NewValue(String());
    return;
  }

  output.auction_ad_config_non_shared_params->seller_signals =
      ConvertJsonPromiseFromIdlToMojo(
          auction_handle, auction_id, input, input.sellerSignals(),
          mojom::blink::AuctionAdConfigField::kSellerSignals, "sellerSignals");
}

// Attempts to build a DirectFromSellerSignalsSubresource. If there is no
// registered subresource URL `subresource_url` returns nullptr -- processing
// may continue with the next `subresource_url`.
mojom::blink::DirectFromSellerSignalsSubresourcePtr
TryToBuildDirectFromSellerSignalsSubresource(
    const KURL& subresource_url,
    const SecurityOrigin& seller,
    ExceptionState& exception_state,
    const ResourceFetcher& resource_fetcher) {
  DCHECK(subresource_url.IsValid());
  DCHECK(
      subresource_url.ProtocolIs(url::kHttpsScheme) &&
      seller.IsSameOriginWith(SecurityOrigin::Create(subresource_url).get()));
  // NOTE: If subresource bundles are disabled, GetSubresourceBundleToken() will
  // always return absl::nullopt.
  absl::optional<base::UnguessableToken> token =
      resource_fetcher.GetSubresourceBundleToken(subresource_url);
  if (!token) {
    return nullptr;
  }
  absl::optional<KURL> bundle_url =
      resource_fetcher.GetSubresourceBundleSourceUrl(subresource_url);
  DCHECK(bundle_url->ProtocolIs(url::kHttpsScheme));
  DCHECK(seller.IsSameOriginWith(SecurityOrigin::Create(*bundle_url).get()));
  auto mojo_bundle = mojom::blink::DirectFromSellerSignalsSubresource::New();
  mojo_bundle->token = *token;
  mojo_bundle->bundle_url = *bundle_url;
  return mojo_bundle;
}

mojom::blink::DirectFromSellerSignalsPtr
ConvertDirectFromSellerSignalsFromV8ToMojo(
    const ScriptState& script_state,
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const ResourceFetcher& resource_fetcher,
    const String& seller_name,
    const SecurityOrigin& seller_origin,
    const absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>&
        interest_group_buyers,
    v8::Local<v8::Value> value) {
  String prefix_string = NativeValueTraits<IDLUSVString>::NativeValue(
      script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  const KURL direct_from_seller_signals_prefix =
      context.CompleteURL(prefix_string);
  if (!direct_from_seller_signals_prefix.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
        seller_name, "directFromSellerSignals", prefix_string,
        "cannot be resolved to a valid URL."));
    return nullptr;
  }
  if (!direct_from_seller_signals_prefix.ProtocolIs(url::kHttpsScheme) ||
      !seller_origin.IsSameOriginWith(
          SecurityOrigin::Create(direct_from_seller_signals_prefix).get())) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
        seller_name, "directFromSellerSignals", prefix_string,
        "must match seller origin; only https scheme is supported."));
    return nullptr;
  }
  if (!direct_from_seller_signals_prefix.Query().empty()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
        seller_name, "directFromSellerSignals", prefix_string,
        "URL prefix must not have a query string."));
    return nullptr;
  }
  auto mojo_direct_from_seller_signals =
      mojom::blink::DirectFromSellerSignals::New();
  mojo_direct_from_seller_signals->prefix = direct_from_seller_signals_prefix;

  if (interest_group_buyers) {
    for (scoped_refptr<const SecurityOrigin> buyer : *interest_group_buyers) {
      // Replace "/" with "%2F" to match the behavior of
      // base::EscapeQueryParamValue(). Also, the subresource won't be found if
      // the URL doesn't match.
      const KURL subresource_url(
          direct_from_seller_signals_prefix.GetString() + "?perBuyerSignals=" +
          EncodeWithURLEscapeSequences(buyer->ToString()).Replace("/", "%2F"));
      mojom::blink::DirectFromSellerSignalsSubresourcePtr maybe_mojo_bundle =
          TryToBuildDirectFromSellerSignalsSubresource(
              subresource_url, seller_origin, exception_state,
              resource_fetcher);
      if (!maybe_mojo_bundle) {
        continue;  // The bundle wasn't found, try the next one.
      }
      mojo_direct_from_seller_signals->per_buyer_signals.insert(
          buyer, std::move(maybe_mojo_bundle));
    }
  }

  {
    const KURL subresource_url(direct_from_seller_signals_prefix.GetString() +
                               "?sellerSignals");
    mojom::blink::DirectFromSellerSignalsSubresourcePtr maybe_mojo_bundle =
        TryToBuildDirectFromSellerSignalsSubresource(
            subresource_url, seller_origin, exception_state, resource_fetcher);
    // May be null if the signals weren't found.
    mojo_direct_from_seller_signals->seller_signals =
        std::move(maybe_mojo_bundle);
  }

  {
    const KURL subresource_url(direct_from_seller_signals_prefix.GetString() +
                               "?auctionSignals");
    mojom::blink::DirectFromSellerSignalsSubresourcePtr maybe_mojo_bundle =
        TryToBuildDirectFromSellerSignalsSubresource(
            subresource_url, seller_origin, exception_state, resource_fetcher);
    // May be null if the signals weren't found.
    mojo_direct_from_seller_signals->auction_signals =
        std::move(maybe_mojo_bundle);
  }

  return mojo_direct_from_seller_signals;
}

String ConvertIDLStringFromV8ToMojo(const ScriptState& script_state,
                                    ExceptionState& exception_state,
                                    v8::Local<v8::Value> value) {
  String result = NativeValueTraits<IDLString>::NativeValue(
      script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return String();
  }

  return result;
}

void CopyDirectFromSellerSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasDirectFromSellerSignals()) {
    output.direct_from_seller_signals = mojom::blink::
        AuctionAdConfigMaybePromiseDirectFromSellerSignals::NewValue(nullptr);
    return;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.directFromSellerSignals(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved>(
          auction_handle, auction_id->Clone(), input.seller(), output.seller,
          output.auction_ad_config_non_shared_params->interest_group_buyers));
  output.direct_from_seller_signals = mojom::blink::
      AuctionAdConfigMaybePromiseDirectFromSellerSignals::NewPromise(0);
}

void CopyDirectFromSellerSignalsHeaderAdSlotFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasDirectFromSellerSignalsHeaderAdSlot()) {
    output.expects_direct_from_seller_signals_header_ad_slot = false;
    return;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.directFromSellerSignalsHeaderAdSlot(),
      MakeGarbageCollected<NavigatorAuction::AuctionHandle::
                               DirectFromSellerSignalsHeaderAdSlotResolved>(
          auction_handle, auction_id->Clone(), input.seller()));
  output.expects_direct_from_seller_signals_header_ad_slot = true;
}

bool CopyAdditionalBidsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAdditionalBids()) {
    output.expects_additional_bids = false;
    return true;
  }

  if (!input.hasAuctionNonce()) {
    exception_state.ThrowTypeError(
        String::Format("additionalBids specified for AuctionAdConfig with "
                       "seller '%s' which does not have an auctionNonce.",
                       input.seller().Utf8().c_str()));
    return false;
  }

  if (!input.hasInterestGroupBuyers() || input.interestGroupBuyers().empty()) {
    exception_state.ThrowTypeError(
        String::Format("additionalBids specified for AuctionAdConfig with "
                       "seller '%s' which has no interestGroupBuyers. All "
                       "additionalBid buyers must be in interestGroupBuyers.",
                       input.seller().Utf8().c_str()));
    return false;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.additionalBids(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::AdditionalBidsResolved>(
          auction_handle, auction_id->Clone(), input.seller()));
  output.expects_additional_bids = true;
  return true;
}

bool CopyAggregationCoordinatorOriginFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPrivateAggregationConfig()) {
    return true;
  }

  return GetAggregationCoordinatorFromConfig(
      exception_state, *input.privateAggregationConfig(),
      output.aggregation_coordinator_origin);
}

// Returns nullopt + sets exception on failure, or returns a concrete value.
absl::optional<HashMap<scoped_refptr<const SecurityOrigin>, String>>
ConvertNonPromisePerBuyerSignalsFromV8ToMojo(const ScriptState& script_state,
                                             ExceptionState& exception_state,
                                             const String& seller_name,
                                             v8::Local<v8::Value> value) {
  HeapVector<std::pair<WTF::String, blink::ScriptValue>> decoded =
      NativeValueTraits<IDLRecord<IDLUSVString, IDLAny>>::NativeValue(
          script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return absl::nullopt;
  }

  absl::optional<HashMap<scoped_refptr<const SecurityOrigin>, String>>
      per_buyer_signals;

  per_buyer_signals.emplace();
  for (const auto& per_buyer_signal : decoded) {
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_signal.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, "perBuyerSignals buyer", per_buyer_signal.first,
          "must be a valid https origin."));
      return absl::nullopt;
    }
    String buyer_signals_str;
    if (!Jsonify(script_state, per_buyer_signal.second.V8Value(),
                 buyer_signals_str)) {
      exception_state.ThrowTypeError(
          ErrorInvalidAuctionConfigSellerJson(seller_name, "perBuyerSignals"));
      return absl::nullopt;
    }
    per_buyer_signals->insert(buyer, std::move(buyer_signals_str));
  }

  return per_buyer_signals;
}

void CopyPerBuyerSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerSignals()) {
    output.auction_ad_config_non_shared_params->per_buyer_signals =
        mojom::blink::AuctionAdConfigMaybePromisePerBuyerSignals::NewValue(
            absl::nullopt);
    return;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.perBuyerSignals(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved>(
          auction_handle, auction_id->Clone(), input.seller()));
  output.auction_ad_config_non_shared_params->per_buyer_signals =
      mojom::blink::AuctionAdConfigMaybePromisePerBuyerSignals::NewPromise(0);
}

// Returns nullptr + sets exception on failure, or returns a concrete value.
//
// This is shared logic for `perBuyerTimeouts` and `perBuyerCumulativeTimeouts`,
// with `field` indicating which name to use in error messages. The logic is
// identical in both cases.
mojom::blink::AuctionAdConfigBuyerTimeoutsPtr
ConvertNonPromisePerBuyerTimeoutsFromV8ToMojo(
    const ScriptState& script_state,
    ExceptionState& exception_state,
    const String& seller_name,
    v8::Local<v8::Value> value,
    mojom::blink::AuctionAdConfigBuyerTimeoutField field) {
  Vector<std::pair<String, uint64_t>> decoded =
      NativeValueTraits<IDLRecord<IDLUSVString, IDLUnsignedLongLong>>::
          NativeValue(script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_timeouts =
      mojom::blink::AuctionAdConfigBuyerTimeouts::New();
  buyer_timeouts->per_buyer_timeouts.emplace();
  for (const auto& per_buyer_timeout : decoded) {
    if (per_buyer_timeout.first == "*") {
      buyer_timeouts->all_buyers_timeout =
          base::Milliseconds(per_buyer_timeout.second);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_timeout.first);
    if (!buyer) {
      String field_name;
      switch (field) {
        case mojom::blink::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts:
          field_name = "perBuyerTimeouts buyer";
          break;
        case mojom::blink::AuctionAdConfigBuyerTimeoutField::
            kPerBuyerCumulativeTimeouts:
          field_name = "perBuyerCumulativeTimeouts buyer";
          break;
      }
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, field_name, per_buyer_timeout.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return nullptr;
    }
    buyer_timeouts->per_buyer_timeouts->insert(
        buyer, base::Milliseconds(per_buyer_timeout.second));
  }

  return buyer_timeouts;
}

void CopyPerBuyerTimeoutsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerTimeouts()) {
    output.auction_ad_config_non_shared_params->buyer_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewValue(
            mojom::blink::AuctionAdConfigBuyerTimeouts::New());
    return;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.perBuyerTimeouts(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved>(
          auction_handle, auction_id->Clone(),
          mojom::blink::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
          input.seller()));
  output.auction_ad_config_non_shared_params->buyer_timeouts =
      mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewPromise(0);
}

void CopyPerBuyerCumulativeTimeoutsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerCumulativeTimeouts()) {
    output.auction_ad_config_non_shared_params->buyer_cumulative_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewValue(
            mojom::blink::AuctionAdConfigBuyerTimeouts::New());
    return;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.perBuyerCumulativeTimeouts(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved>(
          auction_handle, auction_id->Clone(),
          mojom::blink::AuctionAdConfigBuyerTimeoutField::
              kPerBuyerCumulativeTimeouts,
          input.seller()));
  output.auction_ad_config_non_shared_params->buyer_cumulative_timeouts =
      mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewPromise(0);
}

// Returns nullptr + sets exception on failure, or returns a concrete value.
mojom::blink::AuctionAdConfigBuyerCurrenciesPtr
ConvertNonPromisePerBuyerCurrenciesFromV8ToMojo(const ScriptState& script_state,
                                                ExceptionState& exception_state,
                                                const String& seller_name,
                                                v8::Local<v8::Value> value) {
  Vector<std::pair<String, String>> decoded =
      NativeValueTraits<IDLRecord<IDLUSVString, IDLUSVString>>::NativeValue(
          script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  mojom::blink::AuctionAdConfigBuyerCurrenciesPtr buyer_currencies =
      mojom::blink::AuctionAdConfigBuyerCurrencies::New();
  buyer_currencies->per_buyer_currencies.emplace();
  for (const auto& per_buyer_currency : decoded) {
    std::string per_buyer_currency_str = per_buyer_currency.second.Ascii();
    if (!IsValidAdCurrencyCode(per_buyer_currency_str)) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, "perBuyerCurrencies currency", per_buyer_currency.second,
          "must be a 3-letter uppercase currency code."));
      return nullptr;
    }

    if (per_buyer_currency.first == "*") {
      buyer_currencies->all_buyers_currency =
          blink::AdCurrency::From(per_buyer_currency_str);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_currency.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, "perBuyerCurrencies buyer", per_buyer_currency.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return nullptr;
    }
    buyer_currencies->per_buyer_currencies->insert(
        buyer, blink::AdCurrency::From(per_buyer_currency_str));
  }

  return buyer_currencies;
}

void CopyPerBuyerCurrenciesFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerCurrencies()) {
    output.auction_ad_config_non_shared_params->buyer_currencies =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerCurrencies::NewValue(
            mojom::blink::AuctionAdConfigBuyerCurrencies::New());
    return;
  }

  auction_handle->QueueAttachPromiseHandler(
      input.perBuyerCurrencies(),
      MakeGarbageCollected<
          NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved>(
          auction_handle, auction_id->Clone(), input.seller()));
  output.auction_ad_config_non_shared_params->buyer_currencies =
      mojom::blink::AuctionAdConfigMaybePromiseBuyerCurrencies::NewPromise(0);
}

bool CopyPerBuyerExperimentIdsFromIdlToMojo(
    const ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerExperimentGroupIds()) {
    return true;
  }
  for (const auto& per_buyer_experiment_id :
       input.perBuyerExperimentGroupIds()) {
    if (per_buyer_experiment_id.first == "*") {
      output.has_all_buyer_experiment_group_id = true;
      output.all_buyer_experiment_group_id = per_buyer_experiment_id.second;
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_experiment_id.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerExperimentGroupIds buyer",
          per_buyer_experiment_id.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.per_buyer_experiment_group_ids.insert(
        buyer, per_buyer_experiment_id.second);
  }

  return true;
}

bool CopyPerBuyerGroupLimitsFromIdlToMojo(
    const ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerGroupLimits()) {
    return true;
  }
  for (const auto& per_buyer_group_limit : input.perBuyerGroupLimits()) {
    if (per_buyer_group_limit.second <= 0) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerGroupLimits value",
          String::Number(per_buyer_group_limit.second),
          "must be greater than 0."));
      return false;
    }
    if (per_buyer_group_limit.first == "*") {
      output.auction_ad_config_non_shared_params->all_buyers_group_limit =
          per_buyer_group_limit.second;
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_group_limit.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerGroupLimits buyer", per_buyer_group_limit.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.auction_ad_config_non_shared_params->per_buyer_group_limits.insert(
        buyer, per_buyer_group_limit.second);
  }

  return true;
}

bool ConvertAuctionConfigPrioritySignalsFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    const Vector<std::pair<WTF::String, double>>& priority_signals_in,
    WTF::HashMap<WTF::String, double>& priority_signals_out) {
  for (const auto& key_value_pair : priority_signals_in) {
    if (key_value_pair.first.StartsWith("browserSignals.")) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerPrioritySignals key", key_value_pair.first,
          "must not start with reserved \"browserSignals.\" prefix."));
      return false;
    }
    priority_signals_out.insert(key_value_pair.first, key_value_pair.second);
  }
  return true;
}

bool CopyPerBuyerPrioritySignalsFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerPrioritySignals()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->per_buyer_priority_signals
      .emplace();
  for (const auto& per_buyer_priority_signals :
       input.perBuyerPrioritySignals()) {
    WTF::HashMap<WTF::String, double> signals;
    if (!ConvertAuctionConfigPrioritySignalsFromIdlToMojo(
            exception_state, input, per_buyer_priority_signals.second,
            signals)) {
      return false;
    }
    if (per_buyer_priority_signals.first == "*") {
      output.auction_ad_config_non_shared_params->all_buyers_priority_signals =
          std::move(signals);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_priority_signals.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerPrioritySignals buyer",
          per_buyer_priority_signals.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.auction_ad_config_non_shared_params->per_buyer_priority_signals
        ->insert(buyer, std::move(signals));
  }

  return true;
}

// TODO(caraitto): Consider validating keys -- no bucket base + offset
// conflicts, no overflow, etc.
bool CopyAuctionReportBuyerKeysFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAuctionReportBuyerKeys()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->auction_report_buyer_keys
      .emplace();
  for (const BigInt& value : input.auctionReportBuyerKeys()) {
    ASSIGN_OR_RETURN(
        auto bucket, CopyBigIntToUint128(value), [&](String error) {
          exception_state.ThrowTypeError(ErrorInvalidAuctionConfigUint128(
              input, "auctionReportBuyerKeys", std::move(error)));
          return false;
        });
    output.auction_ad_config_non_shared_params->auction_report_buyer_keys
        ->push_back(std::move(bucket));
  }

  return true;
}

bool CopyAuctionReportBuyersFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAuctionReportBuyers()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->auction_report_buyers.emplace();
  for (const auto& [report_type_string, report_config] :
       input.auctionReportBuyers()) {
    mojom::blink::AuctionAdConfigNonSharedParams::BuyerReportType report_type;
    if (report_type_string == "interestGroupCount") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kInterestGroupCount;
    } else if (report_type_string == "bidCount") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kBidCount;
    } else if (report_type_string == "totalGenerateBidLatency") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kTotalGenerateBidLatency;
    } else if (report_type_string == "totalSignalsFetchLatency") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kTotalSignalsFetchLatency;
    } else {
      // Don't throw an error if an unknown type is provided to provide forward
      // compatibility with new fields added later.
      continue;
    }
    ASSIGN_OR_RETURN(
        auto bucket, CopyBigIntToUint128(report_config->bucket()),
        [&](String error) {
          exception_state.ThrowTypeError(ErrorInvalidAuctionConfigUint128(
              input, "auctionReportBuyers", error));
          return false;
        });
    output.auction_ad_config_non_shared_params->auction_report_buyers->insert(
        report_type, mojom::blink::AuctionReportBuyersConfig::New(
                         std::move(bucket), report_config->scale()));
  }

  return true;
}

bool CopyRequiredSellerSignalsFromIdlToMojo(
    const ExecutionContext& execution_context,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  output.auction_ad_config_non_shared_params->required_seller_capabilities =
      mojom::blink::SellerCapabilities::New();
  if (!input.hasRequiredSellerCapabilities()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->required_seller_capabilities =
      ConvertSellerCapabilitiesTypeFromIdlToMojo(
          execution_context, input.requiredSellerCapabilities());
  return true;
}

bool CopyRequestedSizeFromIdlToMojo(const ExecutionContext& execution_context,
                                    ExceptionState& exception_state,
                                    const AuctionAdConfig& input,
                                    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasRequestedSize()) {
    return true;
  }
  auto [width_val, width_units] =
      blink::ParseAdSizeString(input.requestedSize()->width().Ascii());
  auto [height_val, height_units] =
      blink::ParseAdSizeString(input.requestedSize()->height().Ascii());
  if (width_units == blink::AdSize::LengthUnit::kInvalid) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize width", input.requestedSize()->width(),
        "must use units '', 'px', 'sw', or 'sh'."));
    return false;
  }
  if (height_units == blink::AdSize::LengthUnit::kInvalid) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize height", input.requestedSize()->height(),
        "must use units '', 'px', 'sw', or 'sh'."));
    return false;
  }
  if (width_val <= 0 || !std::isfinite(width_val)) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize width", input.requestedSize()->width(),
        "must be finite and positive."));
    return false;
  }
  if (height_val <= 0 || !std::isfinite(height_val)) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize height", input.requestedSize()->height(),
        "must be finite and positive."));
    return false;
  }
  output.auction_ad_config_non_shared_params->requested_size =
      mojom::blink::AdSize::New(width_val, width_units, height_val,
                                height_units);
  return true;
}

bool CopyAuctionNonceFromIdlToMojo(const ExecutionContext& execution_context,
                                   ExceptionState& exception_state,
                                   const AuctionAdConfig& input,
                                   mojom::blink::AuctionAdConfig& output) {
  // We don't validate that the UUID parsed successfully here. If it failed,
  // the returned Uuid is an empty string. When we look try to claim it from
  // the AuctionNonceManager, we won't find an empty string, and will fail
  // the auction or component auction then.
  if (input.hasAuctionNonce()) {
    output.auction_ad_config_non_shared_params->auction_nonce =
        base::Uuid::ParseLowercase(input.auctionNonce().Ascii());
  }
  return true;
}

// Attempts to convert the AuctionAdConfig `config`, passed in via Javascript,
// to a `mojom::blink::AuctionAdConfig`. Throws a Javascript exception and
// return null on failure. `auction_handle` is used for promise handling;
// if it's null, promise will not be accepted.
mojom::blink::AuctionAdConfigPtr IdlAuctionConfigToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    bool is_top_level,
    uint32_t nested_pos,
    ScriptState& script_state,
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const ResourceFetcher& resource_fetcher,
    const AuctionAdConfig& config) {
  auto mojo_config = mojom::blink::AuctionAdConfig::New();
  mojo_config->auction_ad_config_non_shared_params =
      mojom::blink::AuctionAdConfigNonSharedParams::New();
  mojom::blink::AuctionAdConfigAuctionIdPtr auction_id;
  if (is_top_level) {
    auction_id = mojom::blink::AuctionAdConfigAuctionId::NewMainAuction(0);
    // For single-level auctions we need either a server response or a decision
    // logic URL. For multi-level auctions we always need a decision logic URL.
    if (!config.hasDecisionLogicURL() &&
        !(config.hasServerResponse() && !config.hasComponentAuctions())) {
      exception_state.ThrowTypeError(ErrorMissingRequired(
          "ad auction config decisionLogicURL or serverResponse"));
      return nullptr;
    }
  } else {
    auction_id =
        mojom::blink::AuctionAdConfigAuctionId::NewComponentAuction(nested_pos);
  }

  if (!CopySellerFromIdlToMojo(exception_state, config, *mojo_config) ||
      !CopyServerResponseFromIdlToMojo(auction_handle, auction_id.get(),
                                       exception_state, config, *mojo_config) ||
      !CopyDecisionLogicUrlFromIdlToMojo(context, exception_state, config,
                                         *mojo_config) ||
      !CopyTrustedScoringSignalsFromIdlToMojo(context, exception_state, config,
                                              *mojo_config) ||
      !CopyInterestGroupBuyersFromIdlToMojo(exception_state, config,
                                            *mojo_config) ||
      !CopyPerBuyerExperimentIdsFromIdlToMojo(script_state, exception_state,
                                              config, *mojo_config) ||
      !CopyPerBuyerGroupLimitsFromIdlToMojo(script_state, exception_state,
                                            config, *mojo_config) ||
      !CopyPerBuyerPrioritySignalsFromIdlToMojo(exception_state, config,
                                                *mojo_config) ||
      !CopyAuctionReportBuyerKeysFromIdlToMojo(exception_state, config,
                                               *mojo_config) ||
      !CopyAuctionReportBuyersFromIdlToMojo(exception_state, config,
                                            *mojo_config) ||
      !CopyRequiredSellerSignalsFromIdlToMojo(context, exception_state, config,
                                              *mojo_config) ||
      !CopyRequestedSizeFromIdlToMojo(context, exception_state, config,
                                      *mojo_config) ||
      !CopyAuctionNonceFromIdlToMojo(context, exception_state, config,
                                     *mojo_config) ||
      !CopyAdditionalBidsFromIdlToMojo(auction_handle, auction_id.get(),
                                       exception_state, config, *mojo_config) ||
      !CopyAggregationCoordinatorOriginFromIdlToMojo(exception_state, config,
                                                     *mojo_config)) {
    return mojom::blink::AuctionAdConfigPtr();
  }

  if (config.hasDirectFromSellerSignals() &&
      config.hasDirectFromSellerSignalsHeaderAdSlot()) {
    exception_state.ThrowTypeError(
        "The auction config fields directFromSellerSignals and "
        "directFromSellerSignalsHeaderAdSlot must not both be specified for a "
        "given component auction, top-level "
        "auction, or non-component auction.");
    return mojom::blink::AuctionAdConfigPtr();
  }

  CopyAuctionSignalsFromIdlToMojo(auction_handle, auction_id.get(), config,
                                  *mojo_config);
  CopySellerSignalsFromIdlToMojo(auction_handle, auction_id.get(), config,
                                 *mojo_config);
  CopyDirectFromSellerSignalsFromIdlToMojo(auction_handle, auction_id.get(),
                                           config, *mojo_config);
  CopyDirectFromSellerSignalsHeaderAdSlotFromIdlToMojo(
      auction_handle, auction_id.get(), config, *mojo_config);
  CopyPerBuyerSignalsFromIdlToMojo(auction_handle, auction_id.get(), config,
                                   *mojo_config);
  CopyPerBuyerTimeoutsFromIdlToMojo(auction_handle, auction_id.get(), config,
                                    *mojo_config);
  CopyPerBuyerCumulativeTimeoutsFromIdlToMojo(auction_handle, auction_id.get(),
                                              config, *mojo_config);
  CopyPerBuyerCurrenciesFromIdlToMojo(auction_handle, auction_id.get(), config,
                                      *mojo_config);

  if (mojo_config->server_response && !is_top_level) {
    // TODO(1457241): Add support for multi-level auctions including server-side
    // auctions.
    exception_state.ThrowTypeError(
        "Only top-level auctions may have 'serverResponse'.");
    return mojom::blink::AuctionAdConfigPtr();
  }

  if (config.hasSellerTimeout()) {
    mojo_config->auction_ad_config_non_shared_params->seller_timeout =
        base::Milliseconds(config.sellerTimeout());
  }

  if (config.hasSellerCurrency()) {
    std::string seller_currency_str = config.sellerCurrency().Ascii();
    if (!IsValidAdCurrencyCode(seller_currency_str)) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          config.seller(), "sellerCurrency", config.sellerCurrency(),
          "must be a 3-letter uppercase currency code."));
      return mojom::blink::AuctionAdConfigPtr();
    }
    mojo_config->auction_ad_config_non_shared_params->seller_currency =
        blink::AdCurrency::From(seller_currency_str);
  }

  // Recursively handle component auctions, if there are any.
  if (config.hasComponentAuctions()) {
    if (config.componentAuctions().size() > 0 &&
        mojo_config->auction_ad_config_non_shared_params
            ->interest_group_buyers &&
        mojo_config->auction_ad_config_non_shared_params->interest_group_buyers
                ->size() > 0) {
      exception_state.ThrowTypeError(
          "Auctions may only have one of 'interestGroupBuyers' or "
          "'componentAuctions'.");
      return mojom::blink::AuctionAdConfigPtr();
    }

    if (config.componentAuctions().size() > 0 &&
        mojo_config->expects_additional_bids) {
      exception_state.ThrowTypeError(
          "Auctions may only specify 'additionalBids' if they do not have  "
          "'componentAuctions'.");
      return mojom::blink::AuctionAdConfigPtr();
    }

    for (uint32_t pos = 0; pos < config.componentAuctions().size(); ++pos) {
      const auto& idl_component_auction = config.componentAuctions()[pos];
      // Component auctions may not have their own nested component auctions.
      if (!is_top_level) {
        exception_state.ThrowTypeError(
            "Auctions listed in componentAuctions may not have their own "
            "nested componentAuctions.");
        return mojom::blink::AuctionAdConfigPtr();
      }
      // We need decision logic for component auctions even if they have a
      // server response. We perform reporting client-side for component
      // auctions.
      if (!idl_component_auction->hasDecisionLogicURL()) {
        exception_state.ThrowTypeError(ErrorMissingRequired(
            "ad auction config decisionLogicURL or serverResponse"));
        return mojom::blink::AuctionAdConfigPtr();
      }

      auto mojo_component_auction = IdlAuctionConfigToMojo(
          auction_handle, /*is_top_level=*/false, pos, script_state, context,
          exception_state, resource_fetcher, *idl_component_auction);
      if (!mojo_component_auction) {
        return mojom::blink::AuctionAdConfigPtr();
      }
      mojo_config->auction_ad_config_non_shared_params->component_auctions
          .emplace_back(std::move(mojo_component_auction));
    }
  }

  if (config.hasSellerExperimentGroupId()) {
    mojo_config->has_seller_experiment_group_id = true;
    mojo_config->seller_experiment_group_id = config.sellerExperimentGroupId();
  }

  return mojo_config;
}

// finalizeAd() validation methods
bool ValidateAdsObject(ExceptionState& exception_state, const Ads* ads) {
  if (!ads || !ads->IsValid()) {
    exception_state.ThrowTypeError(
        "Ads used for finalizeAds() must be a valid Ads object from "
        "navigator.createAdRequest.");
    return false;
  }
  return true;
}

// Modified from
// LocalFrame::CountUseIfFeatureWouldBeBlockedByPermissionsPolicy.
//
// Checks whether or not a policy-controlled feature would be blocked by our
// restricted permissions policy EnableForSelf.
// Under EnableForSelf policy, the features will not be available in
// cross-origin document unless explicitly enabled.
// Returns true if the frame is cross-origin relative to the top-level document,
// or if it is same-origin with the top level, but is embedded in any way
// through a cross-origin frame (A->B->A embedding).
bool FeatureWouldBeBlockedByRestrictedPermissionsPolicy(Navigator& navigator) {
  const Frame* frame = navigator.DomWindow()->GetFrame();

  // Fenced Frames block all permissions, so we shouldn't end up here because
  // the policy is checked before this method is called.
  DCHECK(!frame->IsInFencedFrameTree());

  // Get the origin of the top-level document.
  const SecurityOrigin* top_origin =
      frame->Tree().Top().GetSecurityContext()->GetSecurityOrigin();

  // Walk up the frame tree looking for any cross-origin embeds. Even if this
  // frame is same-origin with the top-level, if it is embedded by a cross-
  // origin frame (like A->B->A) it would be blocked without a permissions
  // policy.
  while (!frame->IsMainFrame()) {
    if (!frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            top_origin)) {
      return true;
    }
    frame = frame->Tree().Parent();
  }
  return false;
}

void RecordCommonFledgeUseCounters(Document* document) {
  if (!document) {
    return;
  }
  UseCounter::Count(document, mojom::blink::WebFeature::kFledge);
  // Only record the ads APIs counter if enabled in that manner.
  if (RuntimeEnabledFeatures::PrivacySandboxAdsAPIsEnabled(
          document->GetExecutionContext())) {
    UseCounter::Count(document,
                      mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  }
}

// Several dictionary members are being renamed -- to maintain compatibility
// with existing scripts, both the new names and the old names will need to be
// supported for a time before support for the older names are dropped.
//
// If both names are supplied, they must have the same value (this allows
// scripts to be compatible with newer and older browsers).
//
// Some fields that were "required" in WebIDL also get checked -- during the
// rename, these fields aren't marked as required in WebIDL, but at least one
// of the old or new name versions must be specified.
bool HandleOldDictNamesJoin(AuctionAdInterestGroup* group,
                            ExceptionState& exception_state) {
  if (group->hasAds()) {
    for (auto& ad : group->ads()) {
      if (ad->hasRenderUrlDeprecated()) {
        if (ad->hasRenderURL()) {
          if (ad->renderURL() != ad->renderUrlDeprecated()) {
            exception_state.ThrowTypeError(ErrorRenameMismatch(
                /*old_field_name=*/"ad renderUrl",
                /*old_field_value=*/ad->renderUrlDeprecated(),
                /*new_field_name=*/"ad renderURL",
                /*new_field_value=*/ad->renderURL()));
            return false;
          }
        } else {
          ad->setRenderURL(std::move(ad->renderUrlDeprecated()));
        }
      }
      if (!ad->hasRenderURL()) {
        exception_state.ThrowTypeError(ErrorMissingRequired("ad renderURL"));
        return false;
      }
    }
  }

  if (group->hasAdComponents()) {
    for (auto& ad : group->adComponents()) {
      if (ad->hasRenderUrlDeprecated()) {
        if (ad->hasRenderURL()) {
          if (ad->renderURL() != ad->renderUrlDeprecated()) {
            exception_state.ThrowTypeError(ErrorRenameMismatch(
                /*old_field_name=*/"ad component renderUrl",
                /*old_field_value=*/ad->renderUrlDeprecated(),
                /*new_field_name=*/"ad component renderURL",
                /*new_field_value=*/ad->renderURL()));
            return false;
          }
        } else {
          ad->setRenderURL(std::move(ad->renderUrlDeprecated()));
        }
      }
      if (!ad->hasRenderURL()) {
        exception_state.ThrowTypeError(
            ErrorMissingRequired("ad component renderURL"));
        return false;
      }
    }
  }

  if (group->hasBiddingLogicUrlDeprecated()) {
    if (group->hasBiddingLogicURL()) {
      if (group->biddingLogicURL() != group->biddingLogicUrlDeprecated()) {
        exception_state.ThrowTypeError(ErrorRenameMismatch(
            /*old_field_name=*/"interest group biddingLogicUrl",
            /*old_field_value=*/group->biddingLogicUrlDeprecated(),
            /*new_field_name=*/"interest group biddingLogicURL",
            /*new_field_value=*/group->biddingLogicURL()));
        return false;
      }
    } else {
      group->setBiddingLogicURL(std::move(group->biddingLogicUrlDeprecated()));
    }
  }

  if (group->hasBiddingWasmHelperUrlDeprecated()) {
    if (group->hasBiddingWasmHelperURL()) {
      if (group->biddingWasmHelperUrlDeprecated() !=
          group->biddingWasmHelperURL()) {
        exception_state.ThrowTypeError(ErrorRenameMismatch(
            /*old_field_name=*/"interest group biddingWasmHelperUrl",
            /*old_field_value=*/group->biddingWasmHelperUrlDeprecated(),
            /*new_field_name=*/"interest group biddingWasmHelperURL",
            /*new_field_value=*/group->biddingWasmHelperURL()));
        return false;
      }
    } else {
      group->setBiddingWasmHelperURL(
          std::move(group->biddingWasmHelperUrlDeprecated()));
    }
  }

  if (group->hasUpdateUrlDeprecated()) {
    if (group->hasUpdateURL()) {
      if (group->updateUrlDeprecated() != group->updateURL()) {
        exception_state.ThrowTypeError(ErrorRenameMismatch(
            /*old_field_name=*/"interest group updateUrl",
            /*old_field_value=*/group->updateUrlDeprecated(),
            /*new_field_name=*/"interest group updateURL",
            /*new_field_value=*/group->updateURL()));
        return false;
      }
    } else {
      group->setUpdateURL(std::move(group->updateUrlDeprecated()));
    }
  }

  if (group->hasTrustedBiddingSignalsUrlDeprecated()) {
    if (group->hasTrustedBiddingSignalsURL()) {
      if (group->trustedBiddingSignalsUrlDeprecated() !=
          group->trustedBiddingSignalsURL()) {
        exception_state.ThrowTypeError(ErrorRenameMismatch(
            /*old_field_name=*/"interest group trustedBiddingSignalsUrl",
            /*old_field_value=*/group->trustedBiddingSignalsUrlDeprecated(),
            /*new_field_name=*/"interest group trustedBiddingSignalsURL",
            /*new_field_value=*/group->trustedBiddingSignalsURL()));
        return false;
      }
    } else {
      group->setTrustedBiddingSignalsURL(
          std::move(group->trustedBiddingSignalsUrlDeprecated()));
    }
  }

  return true;
}

bool HandleOldDictNamesRun(AuctionAdConfig* config,
                           ExceptionState& exception_state) {
  if (config->hasComponentAuctions()) {
    for (AuctionAdConfig* component_auction : config->componentAuctions()) {
      HandleOldDictNamesRun(component_auction, exception_state);
    }
  }

  if (config->hasDecisionLogicUrlDeprecated()) {
    if (config->hasDecisionLogicURL()) {
      if (config->decisionLogicURL() != config->decisionLogicUrlDeprecated()) {
        exception_state.ThrowTypeError(ErrorRenameMismatch(
            /*old_field_name=*/"ad auction config decisionLogicUrl",
            /*old_field_value=*/config->decisionLogicUrlDeprecated(),
            /*new_field_name=*/"ad auction config decisionLogicURL",
            /*new_field_value=*/config->decisionLogicURL()));
        return false;
      }
    } else {
      config->setDecisionLogicURL(config->decisionLogicUrlDeprecated());
    }
  }

  if (config->hasTrustedScoringSignalsUrlDeprecated()) {
    if (config->hasTrustedScoringSignalsURL()) {
      if (config->trustedScoringSignalsURL() !=
          config->trustedScoringSignalsUrlDeprecated()) {
        exception_state.ThrowTypeError(ErrorRenameMismatch(
            /*old_field_name=*/"ad auction config trustedScoringSignalsUrl",
            /*old_field_value=*/config->trustedScoringSignalsUrlDeprecated(),
            /*new_field_name=*/"ad auction config trustedScoringSignalsURL",
            /*new_field_value=*/config->trustedScoringSignalsURL()));
        return false;
      }
    } else {
      config->setTrustedScoringSignalsURL(
          config->trustedScoringSignalsUrlDeprecated());
    }
  }

  return true;
}

// TODO(crbug.com/1451034): Remove indirection method
// JoinAdInterestGroupInternal() when old expiration is removed.
ScriptPromise JoinAdInterestGroupInternal(
    ScriptState* script_state,
    Navigator& navigator,
    AuctionAdInterestGroup* group,
    absl::optional<double> duration_seconds,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (context->GetSecurityOrigin()->Protocol() != url::kHttpsScheme) {
    exception_state.ThrowSecurityError(
        "May only joinAdInterestGroup from an https origin.");
    return ScriptPromise();
  }
  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(
        *context, WarningPermissionsPolicy("join-ad-interest-group",
                                           "joinAdInterestGroup"));
  }

  return NavigatorAuction::From(ExecutionContext::From(script_state), navigator)
      .joinAdInterestGroup(script_state, group, duration_seconds,
                           exception_state);
}

}  // namespace

NavigatorAuction::AuctionHandle::AuctionHandleFunction::AuctionHandleFunction(
    AuctionHandle* auction_handle)
    : auction_handle_(auction_handle) {}

void NavigatorAuction::AuctionHandle::AuctionHandleFunction::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

ScriptValue NavigatorAuction::AuctionHandle::AuctionHandleFunction::Call(
    ScriptState* script_state,
    ScriptValue value) {
  // We can end up here when the global associated with our `NavigatorAuction`
  // is detached, at which point the normal thing to do would be not to do any
  // work inside that frame, which includes Promise stuff (and associated type
  // conversions). On top of it, `auction_handle_` will be null/unbound at that
  // point, and most of our implementations need it to do something useful.
  if (!script_state->ContextIsValid()) {
    return ScriptValue();
  }
  return CallImpl(script_state, value);
}

NavigatorAuction::AuctionHandle::JsonResolved::JsonResolved(
    AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
    mojom::blink::AuctionAdConfigField field,
    const String& seller_name,
    const char* field_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      field_(field),
      seller_name_(seller_name),
      field_name_(field_name) {}

ScriptValue NavigatorAuction::AuctionHandle::JsonResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  String maybe_json;
  bool maybe_json_ok = false;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (v8_value->IsUndefined() || v8_value->IsNull()) {
      // `maybe_json` left as the null string here; that's the blink equivalent
      // of absl::nullopt for a string? in mojo.
      maybe_json_ok = true;
    } else {
      maybe_json_ok = Jsonify(*script_state, value.V8Value(), maybe_json);
      if (!maybe_json_ok) {
        exception_state.ThrowTypeError(
            ErrorInvalidAuctionConfigSellerJson(seller_name_, field_name_));
      }
    }
  }

  if (maybe_json_ok) {
    auction_handle()->mojo_pipe()->ResolvedPromiseParam(
        auction_id_->Clone(), field_, std::move(maybe_json));
  } else {
    auction_handle()->Abort();
  }

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved::
    PerBuyerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  absl::optional<WTF::HashMap<scoped_refptr<const SecurityOrigin>, String>>
      per_buyer_signals;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      per_buyer_signals = ConvertNonPromisePerBuyerSignalsFromV8ToMojo(
          *script_state, exception_state, seller_name_, v8_value);
    }
  }

  if (!exception_state.HadException()) {
    auction_handle()->mojo_pipe()->ResolvedPerBuyerSignalsPromise(
        auction_id_->Clone(), std::move(per_buyer_signals));
  } else {
    auction_handle()->Abort();
  }

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved::BuyerTimeoutsResolved(
    AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
    mojom::blink::AuctionAdConfigBuyerTimeoutField field,
    const String& seller_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      field_(field),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_timeouts;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      buyer_timeouts = ConvertNonPromisePerBuyerTimeoutsFromV8ToMojo(
          *script_state, exception_state, seller_name_, v8_value, field_);
    }
  }

  if (!buyer_timeouts) {
    buyer_timeouts = mojom::blink::AuctionAdConfigBuyerTimeouts::New();
  }

  if (!exception_state.HadException()) {
    auction_handle()->mojo_pipe()->ResolvedBuyerTimeoutsPromise(
        auction_id_->Clone(), field_, std::move(buyer_timeouts));
  } else {
    auction_handle()->Abort();
  }

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved::
    BuyerCurrenciesResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  mojom::blink::AuctionAdConfigBuyerCurrenciesPtr buyer_currencies;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      buyer_currencies = ConvertNonPromisePerBuyerCurrenciesFromV8ToMojo(
          *script_state, exception_state, seller_name_, v8_value);
    }
  }

  if (!buyer_currencies) {
    buyer_currencies = mojom::blink::AuctionAdConfigBuyerCurrencies::New();
  }

  if (!exception_state.HadException()) {
    auction_handle()->mojo_pipe()->ResolvedBuyerCurrenciesPromise(
        auction_id_->Clone(), std::move(buyer_currencies));
  } else {
    auction_handle()->Abort();
  }

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved::
    DirectFromSellerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name,
        const scoped_refptr<const SecurityOrigin>& seller_origin,
        const absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>&
            interest_group_buyers)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name),
      seller_origin_(seller_origin),
      interest_group_buyers_(interest_group_buyers) {}

ScriptValue
NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return ScriptValue();
  }

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  mojom::blink::DirectFromSellerSignalsPtr direct_from_seller_signals;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      direct_from_seller_signals = ConvertDirectFromSellerSignalsFromV8ToMojo(
          *script_state, *context, exception_state, *context->Fetcher(),
          seller_name_, *seller_origin_, interest_group_buyers_, v8_value);
    }
  }

  if (!exception_state.HadException()) {
    auction_handle()->mojo_pipe()->ResolvedDirectFromSellerSignalsPromise(
        auction_id_->Clone(), std::move(direct_from_seller_signals));
  } else {
    auction_handle()->Abort();
  }

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::DirectFromSellerSignalsHeaderAdSlotResolved::
    DirectFromSellerSignalsHeaderAdSlotResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::
    DirectFromSellerSignalsHeaderAdSlotResolved::CallImpl(
        ScriptState* script_state,
        ScriptValue value) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return ScriptValue();
  }

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  String direct_from_seller_signals_header_ad_slot;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      direct_from_seller_signals_header_ad_slot = ConvertIDLStringFromV8ToMojo(
          *script_state, exception_state, v8_value);
    }
  }

  if (!exception_state.HadException()) {
    auction_handle()
        ->mojo_pipe()
        ->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
            auction_id_->Clone(),
            std::move(direct_from_seller_signals_header_ad_slot));
  } else {
    auction_handle()->Abort();
  }

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::ServerResponseResolved::ServerResponseResolved(
    AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
    const String& seller_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::ServerResponseResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionContextType::kOperationInvoke,
                                 "NavigatorAuction", "runAdAuction");
  v8::Local<v8::Value> v8_value = value.V8Value();
  if (!v8_value->IsUint8Array()) {
    exception_state.ThrowTypeError("'serverResponse' should be a Uint8Array");
    auction_handle()->Abort();
    return ScriptValue();
  }

  v8::Local<v8::Uint8Array> typed_array = v8_value.As<v8::Uint8Array>();
  mojo_base::BigBuffer buffer(typed_array->ByteLength());
  typed_array->CopyContents(buffer.data(), buffer.size());
  auction_handle()->mojo_pipe()->ResolvedAuctionAdResponsePromise(
      auction_id_->Clone(), std::move(buffer));
  return ScriptValue();
}

NavigatorAuction::AuctionHandle::AdditionalBidsResolved::AdditionalBidsResolved(
    AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
    const String& seller_name)
    : AuctionHandleFunction(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::AdditionalBidsResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return ScriptValue();
  }

  auction_handle()->mojo_pipe()->ResolvedAdditionalBids(auction_id_->Clone());

  return ScriptValue();
}

NavigatorAuction::AuctionHandle::ResolveToConfigResolved::
    ResolveToConfigResolved(AuctionHandle* auction_handle)
    : AuctionHandleFunction(auction_handle) {}

ScriptValue NavigatorAuction::AuctionHandle::ResolveToConfigResolved::CallImpl(
    ScriptState* script_state,
    ScriptValue value) {
  v8::Local<v8::Value> v8_value = value.V8Value();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return ScriptValue();
  }

  if (!v8_value->IsBoolean()) {
    auction_handle()->SetResolveToConfig(false);
  } else {
    auction_handle()->SetResolveToConfig(
        v8_value->BooleanValue(script_state->GetIsolate()));
  }

  auction_handle()->MaybeResolveAuction();
  return ScriptValue();
}

NavigatorAuction::AuctionHandle::Rejected::Rejected(
    AuctionHandle* auction_handle)
    : AuctionHandleFunction(auction_handle) {}

ScriptValue NavigatorAuction::AuctionHandle::Rejected::CallImpl(ScriptState*,
                                                                ScriptValue) {
  // Abort the auction if any input promise rejects
  auction_handle()->Abort();
  return ScriptValue();
}

NavigatorAuction::NavigatorAuction(Navigator& navigator)
    : Supplement(navigator),
      queued_cross_site_joins_(kMaxActiveCrossSiteJoins,
                               WTF::BindRepeating(&NavigatorAuction::StartJoin,
                                                  WrapWeakPersistent(this))),
      queued_cross_site_leaves_(
          kMaxActiveCrossSiteLeaves,
          WTF::BindRepeating(&NavigatorAuction::StartLeave,
                             WrapWeakPersistent(this))),
      queued_cross_site_clears_(
          kMaxActiveCrossSiteClears,
          WTF::BindRepeating(&NavigatorAuction::StartClear,
                             WrapWeakPersistent(this))),
      ad_auction_service_(navigator.GetExecutionContext()) {
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      ad_auction_service_.BindNewPipeAndPassReceiver(
          navigator.GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
}

NavigatorAuction& NavigatorAuction::From(ExecutionContext* context,
                                         Navigator& navigator) {
  NavigatorAuction* supplement =
      Supplement<Navigator>::From<NavigatorAuction>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorAuction>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorAuction::kSupplementName[] = "NavigatorAuction";

ScriptPromise NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    AuctionAdInterestGroup* mutable_group,
    absl::optional<double> lifetime_seconds,
    ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);

  // TODO(crbug.com/1441988): Remove this code after rename is complete.
  if (!HandleOldDictNamesJoin(mutable_group, exception_state)) {
    return ScriptPromise();
  }
  const AuctionAdInterestGroup* group = mutable_group;

  auto mojo_group = mojom::blink::InterestGroup::New();
  if (!CopyLifetimeIdlToMojo(exception_state, lifetime_seconds, *group,
                             *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyOwnerFromIdlToMojo(*context, exception_state, *group, *mojo_group)) {
    return ScriptPromise();
  }
  mojo_group->name = group->name();
  mojo_group->priority = (group->hasPriority()) ? group->priority() : 0.0;

  mojo_group->enable_bidding_signals_prioritization =
      group->hasEnableBiddingSignalsPrioritization()
          ? group->enableBiddingSignalsPrioritization()
          : false;
  if (group->hasPriorityVector()) {
    mojo_group->priority_vector =
        ConvertSparseVectorIdlToMojo(group->priorityVector());
  }
  if (group->hasPrioritySignalsOverrides()) {
    mojo_group->priority_signals_overrides =
        ConvertSparseVectorIdlToMojo(group->prioritySignalsOverrides());
  }

  if (!CopySellerCapabilitiesFromIdlToMojo(*context, exception_state, *group,
                                           *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyExecutionModeFromIdlToMojo(*context, exception_state, *group,
                                      *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyBiddingLogicUrlFromIdlToMojo(*context, exception_state, *group,
                                        *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyWasmHelperUrlFromIdlToMojo(*context, exception_state, *group,
                                      *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyUpdateUrlFromIdlToMojo(*context, exception_state, *group,
                                  *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyTrustedBiddingSignalsUrlFromIdlToMojo(*context, exception_state,
                                                 *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyTrustedBiddingSignalsKeysFromIdlToMojo(*group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyUserBiddingSignalsFromIdlToMojo(*script_state, exception_state,
                                           *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdsFromIdlToMojo(*context, *script_state, exception_state, *group,
                            *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdComponentsFromIdlToMojo(*context, *script_state, exception_state,
                                     *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdSizesFromIdlToMojo(*context, *script_state, exception_state,
                                *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopySizeGroupsFromIdlToMojo(*context, *script_state, exception_state,
                                   *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAuctionServerRequestFlagsFromIdlToMojo(*context, exception_state,
                                                  *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdditionalBidKeyFromIdlToMojo(*context, exception_state, *group,
                                         *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAggregationCoordinatorOriginFromIdlToMojo(exception_state, *group,
                                                     *mojo_group)) {
    return ScriptPromise();
  }

  String error_field_name;
  String error_field_value;
  String error;
  if (!ValidateBlinkInterestGroup(*mojo_group, error_field_name,
                                  error_field_value, error)) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        *group, error_field_name, error_field_value, error));
    return ScriptPromise();
  }

  bool is_cross_origin =
      !context->GetSecurityOrigin()->IsSameOriginWith(mojo_group->owner.get());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  mojom::blink::AdAuctionService::JoinInterestGroupCallback callback =
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&NavigatorAuction::JoinComplete,
                        WrapWeakPersistent(this), is_cross_origin));

  PendingJoin pending_join{std::move(mojo_group), std::move(callback)};
  if (is_cross_origin) {
    queued_cross_site_joins_.Enqueue(std::move(pending_join));
  } else {
    StartJoin(std::move(pending_join));
  }

  return promise;
}

/* static */
ScriptPromise NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    AuctionAdInterestGroup* group,
    double duration_seconds,
    ExceptionState& exception_state) {
  return JoinAdInterestGroupInternal(script_state, navigator, group,
                                     duration_seconds, exception_state);
}

/* static */
ScriptPromise NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    AuctionAdInterestGroup* group,
    ExceptionState& exception_state) {
  return JoinAdInterestGroupInternal(script_state, navigator, group,
                                     /*duration_seconds=*/absl::nullopt,
                                     exception_state);
}

ScriptPromise NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    const AuctionAdInterestGroupKey* group_key,
    ExceptionState& exception_state) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(group_key->owner());
  if (!owner) {
    exception_state.ThrowTypeError("owner '" + group_key->owner() +
                                   "' for AuctionAdInterestGroup with name '" +
                                   group_key->name() +
                                   "' must be a valid https origin.");
    return ScriptPromise();
  }

  if (ExecutionContext::From(script_state)->GetSecurityOrigin()->Protocol() !=
      url::kHttpsScheme) {
    exception_state.ThrowSecurityError(
        "May only leaveAdInterestGroup from an https origin.");
    return ScriptPromise();
  }

  bool is_cross_origin = !ExecutionContext::From(script_state)
                              ->GetSecurityOrigin()
                              ->IsSameOriginWith(owner.get());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  mojom::blink::AdAuctionService::LeaveInterestGroupCallback callback =
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&NavigatorAuction::LeaveComplete,
                        WrapWeakPersistent(this), is_cross_origin));

  PendingLeave pending_leave{std::move(owner), std::move(group_key->name()),
                             std::move(callback)};
  if (is_cross_origin) {
    queued_cross_site_leaves_.Enqueue(std::move(pending_leave));
  } else {
    StartLeave(std::move(pending_leave));
  }

  return promise;
}

ScriptPromise NavigatorAuction::leaveAdInterestGroupForDocument(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window) {
    exception_state.ThrowSecurityError(
        "May not leaveAdInterestGroup from a Document that is not fully "
        "active");
    return ScriptPromise();
  }
  if (ExecutionContext::From(script_state)->GetSecurityOrigin()->Protocol() !=
      url::kHttpsScheme) {
    exception_state.ThrowSecurityError(
        "May only leaveAdInterestGroup from an https origin.");
    return ScriptPromise();
  }
  // The renderer does not have enough information to verify that this document
  // is the result of a FLEDGE auction. The browser will silently ignore
  // this request if this document is not the result of a FLEDGE auction.
  ad_auction_service_->LeaveInterestGroupForDocument();

  // Return resolved promise. The browser-side code doesn't do anything
  // meaningful in this case (no .well-known fetches), and if it ever does do
  // them, likely don't want to expose timing information to the fenced frame,
  // anyways.
  return ScriptPromise::CastUndefined(script_state);
}

/* static */
ScriptPromise NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    const AuctionAdInterestGroupKey* group_key,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(
        *context, WarningPermissionsPolicy("join-ad-interest-group",
                                           "leaveAdInterestGroup"));
  }

  return From(context, navigator)
      .leaveAdInterestGroup(script_state, group_key, exception_state);
}

/* static */
ScriptPromise NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  // According to the spec, implicit leave bypasses permission policy.
  return From(ExecutionContext::From(script_state), navigator)
      .leaveAdInterestGroupForDocument(script_state, exception_state);
}

ScriptPromise NavigatorAuction::clearOriginJoinedAdInterestGroups(
    ScriptState* script_state,
    const String owner_string,
    const Vector<String> interest_groups_to_keep,
    ExceptionState& exception_state) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(owner_string);
  if (!owner) {
    exception_state.ThrowTypeError("owner '" + owner_string +
                                   "' must be a valid https origin.");
    return ScriptPromise();
  }

  if (ExecutionContext::From(script_state)->GetSecurityOrigin()->Protocol() !=
      url::kHttpsScheme) {
    exception_state.ThrowSecurityError(
        "May only clearOriginJoinedAdInterestGroups from an https origin.");
    return ScriptPromise();
  }

  bool is_cross_origin = !ExecutionContext::From(script_state)
                              ->GetSecurityOrigin()
                              ->IsSameOriginWith(owner.get());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  mojom::blink::AdAuctionService::LeaveInterestGroupCallback callback =
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&NavigatorAuction::ClearComplete,
                        WrapWeakPersistent(this), is_cross_origin));

  PendingClear pending_clear{owner, interest_groups_to_keep,
                             std::move(callback)};
  if (is_cross_origin) {
    queued_cross_site_clears_.Enqueue(std::move(pending_clear));
  } else {
    StartClear(std::move(pending_clear));
  }

  return promise;
}

/* static */
ScriptPromise NavigatorAuction::clearOriginJoinedAdInterestGroups(
    ScriptState* script_state,
    Navigator& navigator,
    const String owner,
    ExceptionState& exception_state) {
  return clearOriginJoinedAdInterestGroups(script_state, navigator, owner,
                                           Vector<String>(), exception_state);
}

/* static */
ScriptPromise NavigatorAuction::clearOriginJoinedAdInterestGroups(
    ScriptState* script_state,
    Navigator& navigator,
    const String owner,
    const Vector<String> interest_groups_to_keep,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(
        *context,
        WarningPermissionsPolicy("join-ad-interest-group",
                                 "clearOriginJoinedAdInterestGroups"));
  }

  return From(context, navigator)
      .clearOriginJoinedAdInterestGroups(
          script_state, owner, interest_groups_to_keep, exception_state);
}

void NavigatorAuction::updateAdInterestGroups() {
  ad_auction_service_->UpdateAdInterestGroups();
}

/* static */
void NavigatorAuction::updateAdInterestGroups(ScriptState* script_state,
                                              Navigator& navigator,
                                              ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return;
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(
        *context, WarningPermissionsPolicy("join-ad-interest-group",
                                           "updateAdInterestGroups"));
  }

  return From(context, navigator).updateAdInterestGroups();
}

ScriptPromise NavigatorAuction::createAuctionNonce(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  ScriptPromise promise = resolver->Promise();

  ad_auction_service_->CreateAuctionNonce(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&NavigatorAuction::CreateAuctionNonceComplete,
                    WrapPersistent(this))));

  return promise;
}

/* static */
ScriptPromise NavigatorAuction::createAuctionNonce(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }

  return From(ExecutionContext::From(script_state), navigator)
      .createAuctionNonce(script_state, exception_state);
}

ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             AuctionAdConfig* mutable_config,
                                             ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!HandleOldDictNamesRun(mutable_config, exception_state)) {
    return ScriptPromise();
  }
  const AuctionAdConfig* config = mutable_config;

  mojo::PendingReceiver<mojom::blink::AbortableAdAuction> abort_receiver;
  auto* auction_handle = MakeGarbageCollected<AuctionHandle>(
      context, abort_receiver.InitWithNewPipeAndPassRemote());
  auto mojo_config = IdlAuctionConfigToMojo(
      auction_handle,
      /*is_top_level=*/true, /*nested_pos=*/0, *script_state, *context,
      exception_state,
      /*resource_fetcher=*/
      *GetSupplementable()->DomWindow()->document()->Fetcher(), *config);
  if (!mojo_config) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
  if (auto* signal = config->getSignalOr(nullptr)) {
    if (signal->aborted()) {
      resolver->Reject(signal->reason(script_state));
      return promise;
    }
    auto* abort_handle = signal->AddAlgorithm(auction_handle);
    scoped_abort_state =
        std::make_unique<ScopedAbortState>(signal, abort_handle);
  }

  if (config->hasResolveToConfig()) {
    auction_handle->QueueAttachPromiseHandler(
        config->resolveToConfig(),
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::ResolveToConfigResolved>(
            auction_handle));
  } else {
    auction_handle->SetResolveToConfig(false);
  }

  auction_handle->AttachQueuedPromises(*script_state);
  ad_auction_service_->RunAdAuction(
      std::move(mojo_config), std::move(abort_receiver),
      WTF::BindOnce(&NavigatorAuction::AuctionHandle::AuctionComplete,
                    WrapPersistent(auction_handle), WrapPersistent(resolver),
                    std::move(scoped_abort_state)));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             Navigator& navigator,
                                             AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature run-ad-auction is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(
        *context, WarningPermissionsPolicy("run-ad-auction", "runAdAuction"));
  }

  return From(ExecutionContext::From(script_state), navigator)
      .runAdAuction(script_state, config, exception_state);
}

/* static */
Vector<String> NavigatorAuction::adAuctionComponents(
    ScriptState* script_state,
    Navigator& navigator,
    uint16_t num_ad_components,
    ExceptionState& exception_state) {
  Vector<String> out;
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return out;
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const auto& ad_auction_components =
      navigator.DomWindow()->document()->Loader()->AdAuctionComponents();
  if (!ad_auction_components) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame was not loaded with the "
                                      "result of an interest group auction.");
    return out;
  }

  // Clamp the number of ad components at blink::kMaxAdAuctionAdComponents.
  if (num_ad_components >
      static_cast<int16_t>(blink::kMaxAdAuctionAdComponents)) {
    num_ad_components = blink::kMaxAdAuctionAdComponents;
  }

  DCHECK_EQ(kMaxAdAuctionAdComponents, ad_auction_components->size());

  for (int i = 0; i < num_ad_components; ++i) {
    out.push_back((*ad_auction_components)[i].GetString());
  }
  return out;
}

ScriptPromise NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    const String& uuid_url_string,
    bool send_reports,
    ExceptionState& exception_state) {
  KURL uuid_url(uuid_url_string);
  if (!blink::IsValidUrnUuidURL(GURL(uuid_url))) {
    exception_state.ThrowTypeError("Passed URL must be a valid URN URL.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->DeprecatedGetURLFromURN(
      std::move(uuid_url), send_reports,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::GetURLFromURNComplete, WrapPersistent(this))));
  return promise;
}

ScriptPromise NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    Navigator& navigator,
    const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
    bool send_reports,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  String uuid_url_string;
  switch (urn_or_config->GetContentType()) {
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kUSVString:
      uuid_url_string = urn_or_config->GetAsUSVString();
      break;
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kFencedFrameConfig:
      absl::optional<KURL> uuid_url_opt =
          urn_or_config->GetAsFencedFrameConfig()->urn_uuid(
              base::PassKey<NavigatorAuction>());
      if (!uuid_url_opt.has_value()) {
        exception_state.ThrowTypeError("Passed config must have a mapped URL.");
        return ScriptPromise();
      }
      uuid_url_string = uuid_url_opt->GetString();
      break;
  }
  return From(ExecutionContext::From(script_state), navigator)
      .deprecatedURNToURL(script_state, uuid_url_string, send_reports,
                          exception_state);
}

ScriptPromise NavigatorAuction::deprecatedReplaceInURN(
    ScriptState* script_state,
    const String& uuid_url_string,
    const Vector<std::pair<String, String>>& replacements,
    ExceptionState& exception_state) {
  KURL uuid_url(uuid_url_string);
  if (!blink::IsValidUrnUuidURL(GURL(uuid_url))) {
    exception_state.ThrowTypeError("Passed URL must be a valid URN URL.");
    return ScriptPromise();
  }
  Vector<mojom::blink::AdKeywordReplacementPtr> replacements_list;
  for (const auto& replacement : replacements) {
    if (!(replacement.first.StartsWith("${") &&
          replacement.first.EndsWith("}")) &&
        !(replacement.first.StartsWith("%%") &&
          replacement.first.EndsWith("%%"))) {
      exception_state.ThrowTypeError(
          "Replacements must be of the form '${...}' or '%%...%%'");
      return ScriptPromise();
    }
    replacements_list.push_back(mojom::blink::AdKeywordReplacement::New(
        replacement.first, replacement.second));
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->DeprecatedReplaceInURN(
      std::move(uuid_url), std::move(replacements_list),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::ReplaceInURNComplete, WrapPersistent(this))));
  return promise;
}

ScriptPromise NavigatorAuction::deprecatedReplaceInURN(
    ScriptState* script_state,
    Navigator& navigator,
    const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
    const Vector<std::pair<String, String>>& replacements,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  String uuid_url_string;
  switch (urn_or_config->GetContentType()) {
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kUSVString:
      uuid_url_string = urn_or_config->GetAsUSVString();
      break;
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kFencedFrameConfig:
      absl::optional<KURL> uuid_url_opt =
          urn_or_config->GetAsFencedFrameConfig()->urn_uuid(
              base::PassKey<NavigatorAuction>());
      if (!uuid_url_opt.has_value()) {
        exception_state.ThrowTypeError("Passed config must have a mapped URL.");
        return ScriptPromise();
      }
      uuid_url_string = uuid_url_opt->GetString();
      break;
  }
  return From(ExecutionContext::From(script_state), navigator)
      .deprecatedReplaceInURN(script_state, uuid_url_string,
                              std::move(replacements), exception_state);
}

ScriptPromise NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AdRequestConfig::New();

  if (!CopyAdRequestUrlFromIdlToMojo(*context, exception_state, *config,
                                     *mojo_config)) {
    return ScriptPromise();
  }

  if (!CopyAdPropertiesFromIdlToMojo(*context, exception_state, *config,
                                     *mojo_config)) {
    return ScriptPromise();
  }

  if (config->hasPublisherCode()) {
    mojo_config->publisher_code = config->publisherCode();
  }

  if (!CopyTargetingFromIdlToMojo(*context, exception_state, *config,
                                  *mojo_config)) {
    return ScriptPromise();
  }

  if (!CopyAdSignalsFromIdlToMojo(*context, exception_state, *config,
                                  *mojo_config)) {
    return ScriptPromise();
  }

  if (!CopyFallbackSourceFromIdlToMojo(*context, exception_state, *config,
                                       *mojo_config)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->CreateAdRequest(
      std::move(mojo_config),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::AdsRequested, WrapPersistent(this))));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    Navigator& navigator,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  return From(ExecutionContext::From(script_state), navigator)
      .createAdRequest(script_state, config, exception_state);
}

void NavigatorAuction::AdsRequested(ScriptPromiseResolver* resolver,
                                    const WTF::String&) {
  // TODO(https://crbug.com/1249186): Add full impl of methods.
  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
      resolver->GetScriptState()->GetIsolate(),
      DOMExceptionCode::kNotSupportedError,
      "createAdRequest API not yet implemented"));
}

ScriptPromise NavigatorAuction::finalizeAd(ScriptState* script_state,
                                           const Ads* ads,
                                           const AuctionAdConfig* config,
                                           ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AuctionAdConfig::New();

  // For finalizing an Ad PARAKEET only really cares about the decisionLogicURL,
  // auctionSignals, sellerSignals, and perBuyerSignals. Also need seller, since
  // it's used to validate the decision logic URL. We can ignore
  // copying/validating other fields on AuctionAdConfig.
  if (!CopySellerFromIdlToMojo(exception_state, *config, *mojo_config) ||
      !CopyDecisionLogicUrlFromIdlToMojo(*context, exception_state, *config,
                                         *mojo_config)) {
    return ScriptPromise();
  }

  // TODO(morlovich): These no longer work since promise-capable type handling
  // requires non-null auction_handle.
  CopyAuctionSignalsFromIdlToMojo(/*auction_handle=*/nullptr,
                                  /*auction_id=*/nullptr, *config,
                                  *mojo_config);
  CopySellerSignalsFromIdlToMojo(/*auction_handle=*/nullptr,
                                 /*auction_id=*/nullptr, *config, *mojo_config);
  CopyPerBuyerSignalsFromIdlToMojo(/*auction_handle=*/nullptr,
                                   /*auction_id=*/nullptr, *config,
                                   *mojo_config);

  if (!ValidateAdsObject(exception_state, ads)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->FinalizeAd(
      ads->GetGuid(), std::move(mojo_config),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::FinalizeAdComplete, WrapPersistent(this))));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::finalizeAd(ScriptState* script_state,
                                           Navigator& navigator,
                                           const Ads* ads,
                                           const AuctionAdConfig* config,
                                           ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  return From(ExecutionContext::From(script_state), navigator)
      .finalizeAd(script_state, ads, config, exception_state);
}

void NavigatorAuction::FinalizeAdComplete(
    ScriptPromiseResolver* resolver,
    const absl::optional<KURL>& creative_url) {
  if (creative_url) {
    resolver->Resolve(creative_url);
  } else {
    // TODO(https://crbug.com/1249186): Add full impl of methods.
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotSupportedError,
        "finalizeAd API not yet implemented"));
  }
}

void NavigatorAuction::StartJoin(PendingJoin&& pending_join) {
  ad_auction_service_->JoinInterestGroup(std::move(pending_join.interest_group),
                                         std::move(pending_join.callback));
}

void NavigatorAuction::JoinComplete(bool is_cross_origin,
                                    ScriptPromiseResolver* resolver,
                                    bool failed_well_known_check) {
  if (is_cross_origin) {
    queued_cross_site_joins_.OnComplete();
  }

  if (failed_well_known_check) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        "Permission to join interest group denied."));
    return;
  }
  resolver->Resolve();
}

void NavigatorAuction::StartLeave(PendingLeave&& pending_leave) {
  ad_auction_service_->LeaveInterestGroup(pending_leave.owner,
                                          pending_leave.name,
                                          std::move(pending_leave.callback));
}

void NavigatorAuction::LeaveComplete(bool is_cross_origin,
                                     ScriptPromiseResolver* resolver,
                                     bool failed_well_known_check) {
  if (is_cross_origin) {
    queued_cross_site_leaves_.OnComplete();
  }

  if (failed_well_known_check) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        "Permission to leave interest group denied."));
    return;
  }
  resolver->Resolve();
}

void NavigatorAuction::StartClear(PendingClear&& pending_clear) {
  ad_auction_service_->ClearOriginJoinedInterestGroups(
      pending_clear.owner, pending_clear.interest_groups_to_keep,
      std::move(pending_clear.callback));
}

void NavigatorAuction::ClearComplete(bool is_cross_origin,
                                     ScriptPromiseResolver* resolver,
                                     bool failed_well_known_check) {
  if (is_cross_origin) {
    queued_cross_site_clears_.OnComplete();
  }

  if (failed_well_known_check) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        "Permission to leave interest groups denied."));
    return;
  }
  resolver->Resolve();
}

void NavigatorAuction::CreateAuctionNonceComplete(
    ScriptPromiseResolver* resolver,
    const base::Uuid& nonce) {
  resolver->Resolve(String(nonce.AsLowercaseString()));
}

void NavigatorAuction::AuctionHandle::AuctionComplete(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    bool aborted_by_script,
    const absl::optional<FencedFrame::RedactedFencedFrameConfig>&
        result_config) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  AbortSignal* abort_signal =
      scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  if (aborted_by_script) {
    if (abort_signal && abort_signal->aborted()) {
      resolver->Reject(abort_signal->reason(script_state));
    } else {
      // TODO(morlovich): It would probably be better to wire something more
      // precise.
      resolver->Reject(
          "Promise argument rejected or resolved to invalid value.");
    }
  } else if (result_config) {
    DCHECK(result_config->mapped_url().has_value());
    DCHECK(!result_config->mapped_url()->potentially_opaque_value.has_value());

    auction_resolver_ = resolver;
    auction_config_ = result_config;

    MaybeResolveAuction();
  } else {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
  }
}

void NavigatorAuction::AuctionHandle::MaybeResolveAuction() {
  if (!resolve_to_config_.has_value() || !auction_resolver_ ||
      !auction_config_.has_value()) {
    // Once both the resolveToConfig promise is resolved and the auction is
    // completed, this function will be called again to actually
    // complete the auction.
    return;
  }

  if (resolve_to_config_.value() == true) {
    auction_resolver_->Resolve(
        FencedFrameConfig::From(auction_config_.value()));
  } else {
    auction_resolver_->Resolve(KURL(auction_config_->urn_uuid().value()));
  }
}

void NavigatorAuction::GetURLFromURNComplete(
    ScriptPromiseResolver* resolver,
    const absl::optional<KURL>& decoded_url) {
  if (decoded_url) {
    resolver->Resolve(*decoded_url);
  } else {
    resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
  }
}

void NavigatorAuction::ReplaceInURNComplete(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

bool NavigatorAuction::canLoadAdAuctionFencedFrame(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return false;
  }

  LocalFrame* frame_to_check = LocalDOMWindow::From(script_state)->GetFrame();
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(frame_to_check && context);

  ContentSecurityPolicy* csp = context->GetContentSecurityPolicy();
  DCHECK(csp);

  // "A fenced frame tree of one mode cannot contain a child fenced frame of
  // another mode."
  // See: https://github.com/WICG/fenced-frame/blob/master/explainer/modes.md
  if (frame_to_check->GetPage()->IsMainFrameFencedFrameRoot() &&
      frame_to_check->GetPage()->DeprecatedFencedFrameMode() !=
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds) {
    return false;
  }

  if (!context->IsSecureContext()) {
    return false;
  }

  // Check that the flags specified in kFencedFrameMandatoryUnsandboxedFlags
  // are not set in this context. Fenced frames loaded in a sandboxed document
  // require these flags to remain unsandboxed.
  if (context->IsSandboxed(kFencedFrameMandatoryUnsandboxedFlags)) {
    return false;
  }

  // Check the results of the browser checks for the current frame.
  // If the embedding frame is an iframe with CSPEE set, or any ancestor
  // iframes has CSPEE set, the fenced frame will not be allowed to load.
  // The renderer has no knowledge of CSPEE up the ancestor chain, so we defer
  // to the browser to determine the existence of CSPEE outside of the scope
  // we can see here.
  if (frame_to_check->AncestorOrSelfHasCSPEE()) {
    return false;
  }

  // Ensure that if any CSP headers are set that will affect a fenced frame,
  // they allow all https urls to load. Opaque-ads fenced frames do not support
  // allowing/disallowing specific hosts, as that could reveal information to
  // a fenced frame about its embedding page. See design doc for more info:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/interaction_with_content_security_policy.md
  // This is being checked in the renderer because processing of <meta> tags
  // (including CSP) happen in the renderer after navigation commit, so we can't
  // piggy-back off of the ancestor_or_self_has_cspee bit being sent from the
  // browser (which is sent at commit time) since it doesn't know about all the
  // CSP headers yet.
  for (const auto& policy : csp->GetParsedPolicies()) {
    CSPOperativeDirective directive = CSPDirectiveListOperativeDirective(
        *policy, network::mojom::CSPDirectiveName::FencedFrameSrc);
    if (directive.type != network::mojom::CSPDirectiveName::Unknown) {
      // "*" urls will cause the allow_star flag to set
      if (directive.source_list->allow_star) {
        continue;
      }

      // Check for "https:" or "https://*:*"
      bool found_matching_source = false;
      for (const auto& source : directive.source_list->sources) {
        if (source->scheme == url::kHttpsScheme && source->host == "") {
          found_matching_source = true;
          break;
        }
      }
      if (!found_matching_source) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool NavigatorAuction::canLoadAdAuctionFencedFrame(ScriptState* script_state,
                                                   Navigator& navigator) {
  if (!navigator.DomWindow()) {
    return false;
  }
  return From(ExecutionContext::From(script_state), navigator)
      .canLoadAdAuctionFencedFrame(script_state);
}

bool NavigatorAuction::deprecatedRunAdAuctionEnforcesKAnonymity(
    ScriptState* script_state,
    Navigator&) {
  return base::FeatureList::IsEnabled(
      blink::features::kFledgeEnforceKAnonymity);
}

ScriptPromise NavigatorAuction::getInterestGroupAdAuctionData(
    ScriptState* script_state,
    const AdAuctionDataConfig* config,
    ExceptionState& exception_state) {
  CHECK(config);
  if (!script_state->ContextIsValid()) {
    return ScriptPromise();
  }

  scoped_refptr<const SecurityOrigin> seller = ParseOrigin(config->seller());
  if (!seller) {
    exception_state.ThrowTypeError(String::Format(
        "seller '%s' for AdAuctionDataConfig must be a valid https origin.",
        config->seller().Utf8().c_str()));
    return ScriptPromise();
  }

  scoped_refptr<const SecurityOrigin> coordinator;
  if (config->hasCoordinatorOrigin()) {
    coordinator = ParseOrigin(config->coordinatorOrigin());
    if (!coordinator) {
      exception_state.ThrowTypeError(String::Format(
          "coordinatorOrigin '%s' for AdAuctionDataConfig must be "
          "a valid https origin.",
          config->coordinatorOrigin().Utf8().c_str()));
      return ScriptPromise();
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  ScriptPromise promise = resolver->Promise();

  ad_auction_service_->GetInterestGroupAdAuctionData(
      seller, coordinator,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::GetInterestGroupAdAuctionDataComplete,
          WrapPersistent(this))));
  return promise;
}

void NavigatorAuction::GetInterestGroupAdAuctionDataComplete(
    ScriptPromiseResolver* resolver,
    mojo_base::BigBuffer data,
    const absl::optional<base::Uuid>& request_id,
    const WTF::String& error_message) {
  if (!error_message.empty()) {
    CHECK(!request_id);
    resolver->RejectWithTypeError(error_message);
    return;
  }

  AdAuctionData* result = AdAuctionData::Create();
  auto not_shared =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(data.data(), data.size()));
  result->setRequest(std::move(not_shared));
  std::string request_id_str;
  if (request_id) {
    request_id_str = request_id->AsLowercaseString();
  }
  result->setRequestId(WebString::FromLatin1(request_id_str));
  resolver->Resolve(result);
}

/* static */
ScriptPromise NavigatorAuction::getInterestGroupAdAuctionData(
    ScriptState* script_state,
    Navigator& navigator,
    const AdAuctionDataConfig* config,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature run-ad-auction is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(
        *context, WarningPermissionsPolicy("run-ad-auction",
                                           "getInterestGroupAdAuctionData"));
  }

  return From(ExecutionContext::From(script_state), navigator)
      .getInterestGroupAdAuctionData(script_state, config, exception_state);
}

}  // namespace blink
