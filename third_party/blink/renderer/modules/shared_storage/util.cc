// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/util.h"

#include "base/memory/scoped_refptr.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/aggregation_service/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_private_aggregation_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

bool StringFromV8(v8::Isolate* isolate, v8::Local<v8::Value> val, String* out) {
  DCHECK(out);

  if (!val->IsString()) {
    return false;
  }

  v8::Local<v8::String> str = v8::Local<v8::String>::Cast(val);
  wtf_size_t length = str->Utf8Length(isolate);
  LChar* buffer;
  *out = String::CreateUninitialized(length, buffer);

  str->WriteUtf8(isolate, reinterpret_cast<char*>(buffer), length, nullptr,
                 v8::String::NO_NULL_TERMINATION);

  return true;
}

bool CheckBrowsingContextIsValid(ScriptState& script_state,
                                 ExceptionState& exception_state) {
  if (!script_state.ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "context is not valid");
    return false;
  }

  ExecutionContext* execution_context = ExecutionContext::From(&script_state);
  if (execution_context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "context has been destroyed");
    return false;
  }

  return true;
}

bool CheckSharedStoragePermissionsPolicy(ScriptState& script_state,
                                         ExecutionContext& execution_context,
                                         ScriptPromiseResolver& resolver) {
  // The worklet scope has to be created from the Window scope, thus the
  // shared-storage permissions policy feature must have been enabled. Besides,
  // the `SharedStorageWorkletGlobalScope` is currently given a null
  // `PermissionsPolicy`, so we shouldn't attempt to check the permissions
  // policy.
  //
  // TODO(crbug.com/1414951): When the `PermissionsPolicy` is properly set for
  // `SharedStorageWorkletGlobalScope`, we can remove this.
  if (execution_context.IsSharedStorageWorkletGlobalScope()) {
    return true;
  }

  if (!execution_context.IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kSharedStorage)) {
    resolver.Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state.GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"shared-storage\" Permissions Policy denied the method on "
        "window.sharedStorage."));

    return false;
  }

  return true;
}

bool CheckPrivateAggregationConfig(
    const SharedStorageRunOperationMethodOptions& options,
    ScriptState& script_state,
    ScriptPromiseResolver& resolver,
    WTF::String& out_context_id,
    scoped_refptr<SecurityOrigin>& out_aggregation_coordinator_origin) {
  out_context_id = WTF::String();
  out_aggregation_coordinator_origin.reset();

  if (!options.hasPrivateAggregationConfig()) {
    return true;
  }

  if (options.privateAggregationConfig()->hasContextId()) {
    if (options.privateAggregationConfig()->contextId().length() >
        kPrivateAggregationApiContextIdMaxLength) {
      resolver.Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state.GetIsolate(), DOMExceptionCode::kDataError,
          "contextId length cannot be larger than 64"));
      return false;
    }
    out_context_id = options.privateAggregationConfig()->contextId();
  }

  if (options.privateAggregationConfig()->hasAggregationCoordinatorOrigin() &&
      base::FeatureList::IsEnabled(
          features::kPrivateAggregationApiMultipleCloudProviders) &&
      base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders)) {
    scoped_refptr<SecurityOrigin> parsed_coordinator =
        SecurityOrigin::CreateFromString(
            options.privateAggregationConfig()->aggregationCoordinatorOrigin());
    CHECK(parsed_coordinator);
    if (parsed_coordinator->IsOpaque()) {
      resolver.Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state.GetIsolate(), DOMExceptionCode::kSyntaxError,
          "aggregationCoordinatorOrigin must be a valid origin"));
      return false;
    }
    if (!aggregation_service::IsAggregationCoordinatorOriginAllowed(
            parsed_coordinator->ToUrlOrigin())) {
      resolver.Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state.GetIsolate(), DOMExceptionCode::kDataError,
          "aggregationCoordinatorOrigin must be on the allowlist"));
      return false;
    }
    out_aggregation_coordinator_origin = parsed_coordinator;
  }

  return true;
}

}  // namespace blink
