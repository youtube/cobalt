// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/shared_storage/module_script_downloader.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group_size.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_protected_audience_private_aggregation_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_run_function_for_shared_storage_run_operation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_run_function_for_shared_storage_select_url_operation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_interest_group.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_auctionad_longlong.h"
#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/modules/crypto/crypto.h"
#include "third_party/blink/renderer/modules/shared_storage/private_aggregation.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_operation_definition.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_navigator.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InterestGroupsResultStatus {
  kFailureDuringAddModule = 0,
  kFailurePermissionsPolicyDenied = 1,
  kFailureBrowserDenied = 2,
  kSuccess = 3,

  kMaxValue = kSuccess,
};

void RecordInterestGroupsResultStatusUma(InterestGroupsResultStatus status) {
  base::UmaHistogramEnumeration(
      "Storage.SharedStorage.InterestGroups.ResultStatus", status);
}

void ScriptValueToObject(ScriptState* script_state,
                         ScriptValue value,
                         v8::Local<v8::Object>* object) {
  auto* isolate = script_state->GetIsolate();
  DCHECK(!value.IsEmpty());
  auto v8_value = value.V8Value();
  // All the object parameters in the standard are default-initialised to an
  // empty object.
  if (v8_value->IsUndefined()) {
    *object = v8::Object::New(isolate);
    return;
  }
  std::ignore = v8_value->ToObject(script_state->GetContext()).ToLocal(object);
}

ScriptValue JsonStringToScriptValue(ScriptState* script_state,
                                    const String& json_string) {
  DCHECK(script_state->ContextIsValid());
  ScriptState::Scope scope(script_state);
  return ScriptValue(script_state->GetIsolate(),
                     FromJSONString(script_state, json_string));
}

Member<AuctionAd> ConvertMojomAdToIDLAd(
    ScriptState* script_state,
    const mojom::blink::InterestGroupAdPtr& mojom_ad) {
  AuctionAd* ad = AuctionAd::Create();
  ad->setRenderURL(mojom_ad->render_url);
  ad->setRenderUrlDeprecated(mojom_ad->render_url);
  if (mojom_ad->size_group) {
    ad->setSizeGroup(mojom_ad->size_group);
  }
  if (mojom_ad->buyer_reporting_id) {
    ad->setBuyerReportingId(mojom_ad->buyer_reporting_id);
  }
  if (mojom_ad->buyer_and_seller_reporting_id) {
    ad->setBuyerAndSellerReportingId(mojom_ad->buyer_and_seller_reporting_id);
  }
  if (mojom_ad->selectable_buyer_and_seller_reporting_ids) {
    ad->setSelectableBuyerAndSellerReportingIds(
        *mojom_ad->selectable_buyer_and_seller_reporting_ids);
  }
  if (mojom_ad->metadata) {
    ad->setMetadata(JsonStringToScriptValue(script_state, mojom_ad->metadata));
  }
  if (mojom_ad->ad_render_id) {
    ad->setAdRenderId(mojom_ad->ad_render_id);
  }
  if (mojom_ad->allowed_reporting_origins) {
    Vector<String> allowed_reporting_origins;
    allowed_reporting_origins.reserve(
        mojom_ad->allowed_reporting_origins->size());
    for (const scoped_refptr<const blink::SecurityOrigin>& origin :
         *mojom_ad->allowed_reporting_origins) {
      allowed_reporting_origins.push_back(origin->ToString());
    }
    ad->setAllowedReportingOrigins(std::move(allowed_reporting_origins));
  }

  return ad;
}

HeapVector<Member<AuctionAd>> ConvertMojomAdsToIDLAds(
    ScriptState* script_state,
    const Vector<mojom::blink::InterestGroupAdPtr>& mojom_ads) {
  HeapVector<Member<AuctionAd>> ads;
  ads.reserve(mojom_ads.size());
  for (const mojom::blink::InterestGroupAdPtr& mojom_ad : mojom_ads) {
    ads.push_back(ConvertMojomAdToIDLAd(script_state, mojom_ad));
  }
  return ads;
}

std::optional<ScriptValue> Deserialize(
    v8::Isolate* isolate,
    ExecutionContext* execution_context,
    const BlinkCloneableMessage& serialized_data) {
  if (!serialized_data.message->CanDeserializeIn(execution_context)) {
    return std::nullopt;
  }

  Member<UnpackedSerializedScriptValue> unpacked =
      SerializedScriptValue::Unpack(serialized_data.message);
  if (!unpacked) {
    return std::nullopt;
  }

  return ScriptValue(isolate, unpacked->Deserialize(isolate));
}

// We try to use .stack property so that the error message contains a stack
// trace, but otherwise fallback to .toString().
String ExceptionToString(ScriptState* script_state,
                         v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = script_state->GetIsolate();

  if (!exception.IsEmpty()) {
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Value> value =
        v8::TryCatch::StackTrace(context, exception).FromMaybe(exception);
    v8::Local<v8::String> value_string;
    if (value->ToString(context).ToLocal(&value_string)) {
      return String(gin::V8ToString(isolate, value_string));
    }
  }

  return "Unknown Failure";
}

struct UnresolvedSelectURLRequest final
    : public GarbageCollected<UnresolvedSelectURLRequest> {
  UnresolvedSelectURLRequest(size_t urls_size,
                             blink::mojom::blink::SharedStorageWorkletService::
                                 RunURLSelectionOperationCallback callback)
      : urls_size(urls_size), callback(std::move(callback)) {}
  ~UnresolvedSelectURLRequest() = default;

  void Trace(Visitor* visitor) const {}

  size_t urls_size;
  blink::mojom::blink::SharedStorageWorkletService::
      RunURLSelectionOperationCallback callback;
};

struct UnresolvedRunRequest final
    : public GarbageCollected<UnresolvedRunRequest> {
  explicit UnresolvedRunRequest(
      blink::mojom::blink::SharedStorageWorkletService::RunOperationCallback
          callback)
      : callback(std::move(callback)) {}
  ~UnresolvedRunRequest() = default;

  void Trace(Visitor* visitor) const {}

  blink::mojom::blink::SharedStorageWorkletService::RunOperationCallback
      callback;
};

class SelectURLResolutionSuccessCallback final
    : public ThenCallable<IDLAny, SelectURLResolutionSuccessCallback> {
 public:
  explicit SelectURLResolutionSuccessCallback(
      UnresolvedSelectURLRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ThenCallable<IDLAny, SelectURLResolutionSuccessCallback>::Trace(visitor);
  }

  void React(ScriptState* script_state, ScriptValue value) {
    ScriptState::Scope scope(script_state);

    v8::Local<v8::Context> context = value.GetIsolate()->GetCurrentContext();
    v8::Local<v8::Value> v8_value = value.V8Value();

    v8::Local<v8::Uint32> v8_result_index;
    if (!v8_value->ToUint32(context).ToLocal(&v8_result_index)) {
      std::move(request_->callback)
          .Run(/*success=*/false, kSharedStorageReturnValueToIntErrorMessage,
               /*index=*/0);
    } else {
      uint32_t result_index = v8_result_index->Value();
      if (result_index >= request_->urls_size) {
        std::move(request_->callback)
            .Run(/*success=*/false,
                 kSharedStorageReturnValueOutOfRangeErrorMessage,
                 /*index=*/0);
      } else {
        std::move(request_->callback)
            .Run(/*success=*/true,
                 /*error_message=*/g_empty_string, result_index);
      }
    }
  }

 private:
  Member<UnresolvedSelectURLRequest> request_;
};

class SelectURLResolutionFailureCallback final
    : public ThenCallable<IDLAny, SelectURLResolutionFailureCallback> {
 public:
  explicit SelectURLResolutionFailureCallback(
      UnresolvedSelectURLRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ThenCallable<IDLAny, SelectURLResolutionFailureCallback>::Trace(visitor);
  }

  void React(ScriptState* script_state, ScriptValue value) {
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Value> v8_value = value.V8Value();
    std::move(request_->callback)
        .Run(/*success=*/false, ExceptionToString(script_state, v8_value),
             /*index=*/0);
  }

 private:
  Member<UnresolvedSelectURLRequest> request_;
};

class RunResolutionSuccessCallback final
    : public ThenCallable<IDLAny, RunResolutionSuccessCallback> {
 public:
  explicit RunResolutionSuccessCallback(UnresolvedRunRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ThenCallable<IDLAny, RunResolutionSuccessCallback>::Trace(visitor);
  }

  void React(ScriptState*, ScriptValue) {
    std::move(request_->callback)
        .Run(/*success=*/true,
             /*error_message=*/g_empty_string);
  }

 private:
  Member<UnresolvedRunRequest> request_;
};

class RunResolutionFailureCallback final
    : public ThenCallable<IDLAny, RunResolutionFailureCallback> {
 public:
  explicit RunResolutionFailureCallback(UnresolvedRunRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ThenCallable<IDLAny, RunResolutionFailureCallback>::Trace(visitor);
  }

  void React(ScriptState* script_state, ScriptValue value) {
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Value> v8_value = value.V8Value();
    std::move(request_->callback)
        .Run(/*success=*/false, ExceptionToString(script_state, v8_value));
  }

 private:
  Member<UnresolvedRunRequest> request_;
};

}  // namespace

SharedStorageWorkletGlobalScope::SharedStorageWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {
  ContextFeatureSettings::From(
      this, ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->EnablePrivateAggregationInSharedStorage(
          ShouldDefinePrivateAggregationInSharedStorage());
}

SharedStorageWorkletGlobalScope::~SharedStorageWorkletGlobalScope() = default;

void SharedStorageWorkletGlobalScope::BindSharedStorageWorkletService(
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler) {
  receiver_.Bind(std::move(receiver),
                 GetTaskRunner(blink::TaskType::kMiscPlatformAPI));

  // When `SharedStorageWorkletHost` is destroyed, the disconnect handler will
  // be called, and we rely on this explicit signal to clean up the worklet
  // environment.
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

void SharedStorageWorkletGlobalScope::Register(
    const String& name,
    V8NoArgumentConstructor* operation_ctor,
    ExceptionState& exception_state) {
  if (name.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Operation name cannot be empty.");
    return;
  }

  if (operation_definition_map_.Contains(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Operation name already registered.");
    return;
  }

  // If the result of Type(argument=prototype) is not Object, throw a TypeError.
  CallbackMethodRetriever retriever(operation_ctor);
  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException()) {
    return;
  }

  v8::Local<v8::Function> v8_run =
      retriever.GetMethodOrThrow("run", exception_state);
  if (exception_state.HadException()) {
    return;
  }

  auto* operation_definition =
      MakeGarbageCollected<SharedStorageOperationDefinition>(
          ScriptController()->GetScriptState(), name, operation_ctor, v8_run);

  operation_definition_map_.Set(name, operation_definition);
}

void SharedStorageWorkletGlobalScope::OnConsoleApiMessage(
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  WorkerOrWorkletGlobalScope::OnConsoleApiMessage(level, message, location);

  client_->DidAddMessageToConsole(level, message);
}

void SharedStorageWorkletGlobalScope::NotifyContextDestroyed() {
  if (private_aggregation_) {
    CHECK(ShouldDefinePrivateAggregationInSharedStorage());
    private_aggregation_->OnWorkletDestroyed();
  }

  WorkletGlobalScope::NotifyContextDestroyed();
}

void SharedStorageWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(shared_storage_);
  visitor->Trace(private_aggregation_);
  visitor->Trace(crypto_);
  visitor->Trace(navigator_);
  visitor->Trace(operation_definition_map_);
  visitor->Trace(client_);
  WorkletGlobalScope::Trace(visitor);
  Supplementable<SharedStorageWorkletGlobalScope>::Trace(visitor);
}

void SharedStorageWorkletGlobalScope::Initialize(
    mojo::PendingAssociatedRemote<
        mojom::blink::SharedStorageWorkletServiceClient> client,
    mojom::blink::SharedStorageWorkletPermissionsPolicyStatePtr
        permissions_policy_state,
    const String& embedder_context) {
  client_.Bind(std::move(client),
               GetTaskRunner(blink::TaskType::kMiscPlatformAPI));

  permissions_policy_state_ = std::move(permissions_policy_state);
  embedder_context_ = embedder_context;
}

void SharedStorageWorkletGlobalScope::AddModule(
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
        pending_url_loader_factory,
    const KURL& script_source_url,
    AddModuleCallback callback) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>(
          std::move(pending_url_loader_factory)));

  module_script_downloader_ = std::make_unique<ModuleScriptDownloader>(
      url_loader_factory.get(), GURL(script_source_url),
      WTF::BindOnce(&SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded,
                    WrapWeakPersistent(this), script_source_url,
                    std::move(callback)));

  // Create a ResourceRequest and populate only the fields needed by
  // `CodeCacheFetcher`.
  //
  // TODO(yaoxia): Move `code_cache_fetcher_` to `ModuleScriptDownloader` to
  // avoid replicating the ResourceRequest here. This isn't viable today because
  // `ModuleScriptDownloader` lives in blink/public/common, due to its use of
  // `network::SimpleURLLoader`.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(script_source_url);
  resource_request->destination =
      network::mojom::RequestDestination::kSharedStorageWorklet;

  CHECK(GetCodeCacheHost());
  code_cache_fetcher_ = CodeCacheFetcher::TryCreateAndStart(
      *resource_request, *GetCodeCacheHost(),
      WTF::BindOnce(&SharedStorageWorkletGlobalScope::DidReceiveCachedCode,
                    WrapWeakPersistent(this)));
}

void SharedStorageWorkletGlobalScope::RunURLSelectionOperation(
    const String& name,
    const Vector<KURL>& urls,
    BlinkCloneableMessage serialized_data,
    mojom::blink::PrivateAggregationOperationDetailsPtr pa_operation_details,
    RunURLSelectionOperationCallback callback) {
  String error_message;
  SharedStorageOperationDefinition* operation_definition = nullptr;
  if (!PerformCommonOperationChecks(name, error_message,
                                    operation_definition)) {
    std::move(callback).Run(
        /*success=*/false, error_message,
        /*length=*/0);
    return;
  }

  base::OnceClosure operation_completion_cb =
      StartOperation(std::move(pa_operation_details));
  RunURLSelectionOperationCallback combined_operation_completion_cb =
      std::move(callback).Then(std::move(operation_completion_cb));

  DCHECK(operation_definition);

  ScriptState* script_state = operation_definition->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  TraceWrapperV8Reference<v8::Value> instance =
      operation_definition->GetInstance();
  V8RunFunctionForSharedStorageSelectURLOperation* registered_run_function =
      operation_definition->GetRunFunctionForSharedStorageSelectURLOperation();

  Vector<String> urls_param;
  base::ranges::transform(urls, std::back_inserter(urls_param),
                          [](const KURL& url) { return url.GetString(); });

  base::ElapsedTimer deserialization_timer;

  std::optional<ScriptValue> data_param =
      Deserialize(isolate, /*execution_context=*/this, serialized_data);
  if (!data_param) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, kSharedStorageCannotDeserializeDataErrorMessage,
             /*index=*/0);
    return;
  }

  base::UmaHistogramTimes(
      "Storage.SharedStorage.SelectURL.DataDeserialization.Time",
      deserialization_timer.Elapsed());

  v8::Maybe<ScriptPromise<IDLAny>> result = registered_run_function->Invoke(
      instance.Get(isolate), urls_param, *data_param);

  if (try_catch.HasCaught()) {
    v8::Local<v8::Value> exception = try_catch.Exception();
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, ExceptionToString(script_state, exception),
             /*index=*/0);
    return;
  }

  if (result.IsNothing()) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, kSharedStorageEmptyScriptResultErrorMessage,
             /*index=*/0);
    return;
  }

  auto* unresolved_request = MakeGarbageCollected<UnresolvedSelectURLRequest>(
      urls.size(), std::move(combined_operation_completion_cb));

  ScriptPromise<IDLAny> promise = result.FromJust();

  auto* success_callback =
      MakeGarbageCollected<SelectURLResolutionSuccessCallback>(
          unresolved_request);
  auto* failure_callback =
      MakeGarbageCollected<SelectURLResolutionFailureCallback>(
          unresolved_request);

  promise.React(script_state, success_callback, failure_callback);
}

void SharedStorageWorkletGlobalScope::RunOperation(
    const String& name,
    BlinkCloneableMessage serialized_data,
    mojom::blink::PrivateAggregationOperationDetailsPtr pa_operation_details,
    RunOperationCallback callback) {
  String error_message;
  SharedStorageOperationDefinition* operation_definition = nullptr;
  if (!PerformCommonOperationChecks(name, error_message,
                                    operation_definition)) {
    std::move(callback).Run(
        /*success=*/false, error_message);
    return;
  }

  base::OnceClosure operation_completion_cb =
      StartOperation(std::move(pa_operation_details));
  mojom::blink::SharedStorageWorkletService::RunOperationCallback
      combined_operation_completion_cb =
          std::move(callback).Then(std::move(operation_completion_cb));

  DCHECK(operation_definition);

  ScriptState* script_state = operation_definition->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  TraceWrapperV8Reference<v8::Value> instance =
      operation_definition->GetInstance();
  V8RunFunctionForSharedStorageRunOperation* registered_run_function =
      operation_definition->GetRunFunctionForSharedStorageRunOperation();

  base::ElapsedTimer deserialization_timer;

  std::optional<ScriptValue> data_param =
      Deserialize(isolate, /*execution_context=*/this, serialized_data);
  if (!data_param) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false,
             kSharedStorageCannotDeserializeDataErrorMessage);
    return;
  }

  base::UmaHistogramTimes("Storage.SharedStorage.Run.DataDeserialization.Time",
                          deserialization_timer.Elapsed());

  v8::Maybe<ScriptPromise<IDLAny>> result =
      registered_run_function->Invoke(instance.Get(isolate), *data_param);

  if (try_catch.HasCaught()) {
    v8::Local<v8::Value> exception = try_catch.Exception();
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, ExceptionToString(script_state, exception));
    return;
  }

  if (result.IsNothing()) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, kSharedStorageEmptyScriptResultErrorMessage);
    return;
  }

  auto* unresolved_request = MakeGarbageCollected<UnresolvedRunRequest>(
      std::move(combined_operation_completion_cb));

  ScriptPromise<IDLAny> promise = result.FromJust();

  auto* success_callback =
      MakeGarbageCollected<RunResolutionSuccessCallback>(unresolved_request);
  auto* failure_callback =
      MakeGarbageCollected<RunResolutionFailureCallback>(unresolved_request);

  promise.React(script_state, success_callback, failure_callback);
}

SharedStorage* SharedStorageWorkletGlobalScope::sharedStorage(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!add_module_finished_) {
    CHECK(!shared_storage_);

    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "sharedStorage cannot be accessed during addModule().");

    return nullptr;
  }

  // As long as `addModule()` has finished, it should be fine to expose
  // `sharedStorage`: on the browser side, we already enforce that `addModule()`
  // can only be called once, so there's no way to expose the storage data to
  // the associated `Document`.
  if (!shared_storage_) {
    shared_storage_ = MakeGarbageCollected<SharedStorage>();
  }

  return shared_storage_.Get();
}

PrivateAggregation* SharedStorageWorkletGlobalScope::privateAggregation(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  CHECK(ShouldDefinePrivateAggregationInSharedStorage());

  if (!add_module_finished_) {
    CHECK(!private_aggregation_);

    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "privateAggregation cannot be accessed during addModule().");

    return nullptr;
  }

  return GetOrCreatePrivateAggregation();
}

Crypto* SharedStorageWorkletGlobalScope::crypto(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!crypto_) {
    crypto_ = MakeGarbageCollected<Crypto>();
  }

  return crypto_.Get();
}

ScriptPromise<IDLSequence<StorageInterestGroup>>
SharedStorageWorkletGlobalScope::interestGroups(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!add_module_finished_) {
    RecordInterestGroupsResultStatusUma(
        InterestGroupsResultStatus::kFailureDuringAddModule);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "interestGroups() cannot be called during addModule().");
    return EmptyPromise();
  }

  if (!permissions_policy_state_->join_ad_interest_group_allowed) {
    RecordInterestGroupsResultStatusUma(
        InterestGroupsResultStatus::kFailurePermissionsPolicyDenied);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The \"join-ad-interest-group\" Permissions Policy denied the "
        "interestGroups() method.");
    return EmptyPromise();
  }

  if (!permissions_policy_state_->run_ad_auction_allowed) {
    RecordInterestGroupsResultStatusUma(
        InterestGroupsResultStatus::kFailurePermissionsPolicyDenied);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The \"run-ad-auction\" Permissions Policy denied the interestGroups() "
        "method.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<StorageInterestGroup>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  GetSharedStorageWorkletServiceClient()->GetInterestGroups(
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](base::ElapsedTimer timer,
             ScriptPromiseResolver<IDLSequence<StorageInterestGroup>>* resolver,
             mojom::blink::GetInterestGroupsResultPtr result) {
            ScriptState* script_state = resolver->GetScriptState();
            DCHECK(script_state->ContextIsValid());

            if (result->is_error_message()) {
              RecordInterestGroupsResultStatusUma(
                  InterestGroupsResultStatus::kFailureBrowserDenied);
              ScriptState::Scope scope(script_state);
              resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                  script_state->GetIsolate(), DOMExceptionCode::kOperationError,
                  result->get_error_message()));
              return;
            }

            CHECK(result->is_groups());

            Vector<mojom::blink::StorageInterestGroupPtr>& mojom_groups =
                result->get_groups();

            base::Time now = base::Time::Now();

            HeapVector<Member<StorageInterestGroup>> groups;
            groups.reserve(mojom_groups.size());

            for (const auto& mojom_group : mojom_groups) {
              StorageInterestGroup* group = StorageInterestGroup::Create();

              group->setOwner(mojom_group->interest_group->owner->ToString());
              group->setName(mojom_group->interest_group->name);
              group->setPriority(mojom_group->interest_group->priority);

              group->setEnableBiddingSignalsPrioritization(
                  mojom_group->interest_group
                      ->enable_bidding_signals_prioritization);

              if (mojom_group->interest_group->priority_vector) {
                Vector<std::pair<String, double>> priority_vector;
                priority_vector.reserve(
                    mojom_group->interest_group->priority_vector->size());
                for (const auto& entry :
                     *mojom_group->interest_group->priority_vector) {
                  priority_vector.emplace_back(entry.key, entry.value);
                }
                group->setPriorityVector(std::move(priority_vector));
              }

              if (mojom_group->interest_group->priority_signals_overrides) {
                Vector<std::pair<String, double>> priority_signals_overrides;
                priority_signals_overrides.reserve(
                    mojom_group->interest_group->priority_signals_overrides
                        ->size());
                for (const auto& entry :
                     *mojom_group->interest_group->priority_signals_overrides) {
                  priority_signals_overrides.emplace_back(entry.key,
                                                          entry.value);
                }
                group->setPrioritySignalsOverrides(
                    std::move(priority_signals_overrides));
              }

              if (mojom_group->interest_group->seller_capabilities) {
                Vector<std::pair<String, Vector<String>>> seller_capabilities;
                seller_capabilities.reserve(
                    mojom_group->interest_group->seller_capabilities->size());
                for (const auto& entry :
                     *mojom_group->interest_group->seller_capabilities) {
                  Vector<String> capabilities;
                  capabilities.reserve(2);
                  if (entry.value->allows_interest_group_counts) {
                    capabilities.push_back("interest-group-counts");
                  }
                  if (entry.value->allows_latency_stats) {
                    capabilities.push_back("latency-stats");
                  }

                  seller_capabilities.emplace_back(entry.key->ToString(),
                                                   std::move(capabilities));
                }
                group->setSellerCapabilities(std::move(seller_capabilities));
              }

              String execution_mode_string;
              switch (mojom_group->interest_group->execution_mode) {
                case mojom::blink::InterestGroup::ExecutionMode::
                    kGroupedByOriginMode:
                  execution_mode_string = "group-by-origin";
                  break;
                case mojom::blink::InterestGroup::ExecutionMode::kFrozenContext:
                  execution_mode_string = "frozen-context";
                  break;
                case mojom::blink::InterestGroup::ExecutionMode::
                    kCompatibilityMode:
                  execution_mode_string = "compatibility";
                  break;
              }

              group->setExecutionMode(execution_mode_string);

              if (mojom_group->interest_group->bidding_url) {
                group->setBiddingLogicURL(
                    *mojom_group->interest_group->bidding_url);
                group->setBiddingLogicUrlDeprecated(
                    *mojom_group->interest_group->bidding_url);
              }

              if (mojom_group->interest_group->bidding_wasm_helper_url) {
                group->setBiddingWasmHelperURL(
                    *mojom_group->interest_group->bidding_wasm_helper_url);
                group->setBiddingWasmHelperUrlDeprecated(
                    *mojom_group->interest_group->bidding_wasm_helper_url);
              }

              if (mojom_group->interest_group->update_url) {
                group->setUpdateURL(*mojom_group->interest_group->update_url);
                group->setUpdateUrlDeprecated(
                    *mojom_group->interest_group->update_url);
              }

              if (mojom_group->interest_group->trusted_bidding_signals_url) {
                group->setTrustedBiddingSignalsURL(
                    *mojom_group->interest_group->trusted_bidding_signals_url);
                group->setTrustedBiddingSignalsUrlDeprecated(
                    *mojom_group->interest_group->trusted_bidding_signals_url);
              }

              if (mojom_group->interest_group->trusted_bidding_signals_keys) {
                group->setTrustedBiddingSignalsKeys(
                    *mojom_group->interest_group->trusted_bidding_signals_keys);
              }

              String trusted_bidding_signals_slot_size_mode_string;
              switch (mojom_group->interest_group
                          ->trusted_bidding_signals_slot_size_mode) {
                case mojom::blink::InterestGroup::
                    TrustedBiddingSignalsSlotSizeMode ::kNone:
                  trusted_bidding_signals_slot_size_mode_string = "none";
                  break;
                case mojom::blink::InterestGroup::
                    TrustedBiddingSignalsSlotSizeMode::kSlotSize:
                  trusted_bidding_signals_slot_size_mode_string = "slot-size";
                  break;
                case mojom::blink::InterestGroup::
                    TrustedBiddingSignalsSlotSizeMode::kAllSlotsRequestedSizes:
                  trusted_bidding_signals_slot_size_mode_string =
                      "all-slots-requested-sizes";
                  break;
              }

              group->setTrustedBiddingSignalsSlotSizeMode(
                  trusted_bidding_signals_slot_size_mode_string);

              group->setMaxTrustedBiddingSignalsURLLength(
                  mojom_group->interest_group
                      ->max_trusted_bidding_signals_url_length);

              if (mojom_group->interest_group
                      ->trusted_bidding_signals_coordinator) {
                group->setTrustedBiddingSignalsCoordinator(
                    mojom_group->interest_group
                        ->trusted_bidding_signals_coordinator->ToString());
              }

              if (mojom_group->interest_group->user_bidding_signals) {
                group->setUserBiddingSignals(JsonStringToScriptValue(
                    resolver->GetScriptState(),
                    mojom_group->interest_group->user_bidding_signals));
              }

              if (mojom_group->interest_group->ads) {
                group->setAds(
                    ConvertMojomAdsToIDLAds(resolver->GetScriptState(),
                                            *mojom_group->interest_group->ads));
              }

              if (mojom_group->interest_group->ad_components) {
                group->setAdComponents(ConvertMojomAdsToIDLAds(
                    resolver->GetScriptState(),
                    *mojom_group->interest_group->ad_components));
              }

              if (mojom_group->interest_group->ad_sizes) {
                HeapVector<
                    std::pair<String, Member<AuctionAdInterestGroupSize>>>
                    ad_sizes;
                ad_sizes.reserve(mojom_group->interest_group->ad_sizes->size());

                for (const auto& entry :
                     *mojom_group->interest_group->ad_sizes) {
                  const mojom::blink::AdSizePtr& mojom_ad_size = entry.value;
                  AuctionAdInterestGroupSize* ad_size =
                      AuctionAdInterestGroupSize::Create();
                  ad_size->setWidth(String(ConvertAdDimensionToString(
                      mojom_ad_size->width, mojom_ad_size->width_units)));
                  ad_size->setHeight(String(ConvertAdDimensionToString(
                      mojom_ad_size->height, mojom_ad_size->height_units)));

                  ad_sizes.emplace_back(entry.key, ad_size);
                }

                group->setAdSizes(std::move(ad_sizes));
              }

              if (mojom_group->interest_group->size_groups) {
                Vector<std::pair<String, Vector<String>>> size_groups;
                size_groups.reserve(
                    mojom_group->interest_group->size_groups->size());
                for (const auto& entry :
                     *mojom_group->interest_group->size_groups) {
                  size_groups.emplace_back(entry.key, entry.value);
                }
                group->setSizeGroups(std::move(size_groups));
              }

              Vector<String> auction_server_request_flags;
              auction_server_request_flags.reserve(3);
              if (mojom_group->interest_group->auction_server_request_flags
                      ->omit_ads) {
                auction_server_request_flags.push_back("omit-ads");
              }
              if (mojom_group->interest_group->auction_server_request_flags
                      ->include_full_ads) {
                auction_server_request_flags.push_back("include-full-ads");
              }
              if (mojom_group->interest_group->auction_server_request_flags
                      ->omit_user_bidding_signals) {
                auction_server_request_flags.push_back(
                    "omit-user-bidding-signals");
              }
              group->setAuctionServerRequestFlags(
                  std::move(auction_server_request_flags));

              if (mojom_group->interest_group->additional_bid_key) {
                Vector<char> original_additional_bid_key;
                WTF::Base64Encode(
                    base::make_span(
                        *mojom_group->interest_group->additional_bid_key),
                    original_additional_bid_key);

                group->setAdditionalBidKey(String(original_additional_bid_key));
              }

              ProtectedAudiencePrivateAggregationConfig* pa_config =
                  ProtectedAudiencePrivateAggregationConfig::Create();
              if (mojom_group->interest_group->aggregation_coordinator_origin) {
                pa_config->setAggregationCoordinatorOrigin(
                    mojom_group->interest_group->aggregation_coordinator_origin
                        ->ToString());
              }
              group->setPrivateAggregationConfig(pa_config);

              group->setJoinCount(
                  mojom_group->bidding_browser_signals->join_count);
              group->setBidCount(
                  mojom_group->bidding_browser_signals->bid_count);

              HeapVector<HeapVector<Member<V8UnionAuctionAdOrLongLong>>>
                  previous_wins;
              previous_wins.reserve(
                  mojom_group->bidding_browser_signals->prev_wins.size());

              for (const auto& mojom_previous_win :
                   mojom_group->bidding_browser_signals->prev_wins) {
                ScriptValue ad_script_value = JsonStringToScriptValue(
                    resolver->GetScriptState(), mojom_previous_win->ad_json);

                ScriptState::Scope scope(resolver->GetScriptState());
                auto* isolate = resolver->GetScriptState()->GetIsolate();

                // If the 'metadata' field is set, update it with the parsed
                // JSON object.
                {
                  auto context = resolver->GetScriptState()->GetContext();

                  v8::Local<v8::Object> ad_dict;
                  ScriptValueToObject(resolver->GetScriptState(),
                                      ad_script_value, &ad_dict);

                  v8::Local<v8::Value> v8_metadata_string;
                  if (ad_dict->Get(context, V8AtomicString(isolate, "metadata"))
                          .ToLocal(&v8_metadata_string)) {
                    ScriptValue metadata_script_value = JsonStringToScriptValue(
                        resolver->GetScriptState(),
                        String(gin::V8ToString(isolate, v8_metadata_string)));

                    v8::MicrotasksScope microtasks(
                        context, v8::MicrotasksScope::kDoNotRunMicrotasks);

                    std::ignore = ad_dict->Set(
                        context, V8AtomicString(isolate, "metadata"),
                        metadata_script_value.V8Value());
                  }
                }

                AuctionAd* ad = AuctionAd::Create(
                    isolate, ad_script_value.V8Value(), ASSERT_NO_EXCEPTION);

                HeapVector<Member<V8UnionAuctionAdOrLongLong>> previous_win;
                previous_wins.reserve(2);
                previous_win.push_back(
                    MakeGarbageCollected<V8UnionAuctionAdOrLongLong>(
                        (now - mojom_previous_win->time).InMilliseconds()));
                previous_win.push_back(
                    MakeGarbageCollected<V8UnionAuctionAdOrLongLong>(ad));

                previous_wins.push_back(std::move(previous_win));
              }

              group->setPrevWinsMs(std::move(previous_wins));

              group->setJoiningOrigin(mojom_group->joining_origin->ToString());

              group->setTimeSinceGroupJoinedMs(
                  (now - mojom_group->join_time).InMilliseconds());
              group->setLifetimeRemainingMs(
                  (mojom_group->interest_group->expiry - now).InMilliseconds());
              group->setTimeSinceLastUpdateMs(
                  (now - mojom_group->last_updated).InMilliseconds());
              group->setTimeUntilNextUpdateMs(
                  (mojom_group->next_update_after - now).InMilliseconds());

              group->setEstimatedSize(mojom_group->estimated_size);

              groups.push_back(group);
            }

            base::UmaHistogramTimes(
                "Storage.SharedStorage.InterestGroups.TimeToResolve",
                timer.Elapsed());

            RecordInterestGroupsResultStatusUma(
                InterestGroupsResultStatus::kSuccess);
            resolver->Resolve(groups);
          },
          base::ElapsedTimer())));

  return promise;
}

SharedStorageWorkletNavigator* SharedStorageWorkletGlobalScope::Navigator(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!add_module_finished_) {
    CHECK(!navigator_);

    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "navigator cannot be accessed during addModule().");

    return nullptr;
  }

  if (!navigator_) {
    navigator_ = MakeGarbageCollected<SharedStorageWorkletNavigator>(
        GetExecutionContext());
  }
  return navigator_.Get();
}

// Returns the unique ID for the currently running operation.
int64_t SharedStorageWorkletGlobalScope::GetCurrentOperationId() {
  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  v8::Local<v8::Value> data =
      script_state->GetIsolate()->GetContinuationPreservedEmbedderData();
  return data.As<v8::BigInt>()->Int64Value();
}

void SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded(
    const KURL& script_source_url,
    mojom::blink::SharedStorageWorkletService::AddModuleCallback callback,
    std::unique_ptr<std::string> response_body,
    std::string error_message,
    network::mojom::URLResponseHeadPtr response_head) {
  module_script_downloader_.reset();

  // If we haven't received the code cache data, defer handing the response.
  if (code_cache_fetcher_ && code_cache_fetcher_->is_waiting()) {
    handle_script_download_response_after_code_cache_response_ = WTF::BindOnce(
        &SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded,
        WrapPersistent(this), script_source_url, std::move(callback),
        std::move(response_body), std::move(error_message),
        std::move(response_head));
    return;
  }

  // Note: There's no need to check the `cached_metadata` param from
  // `URLLoaderClient::OnReceiveResponse`. This param is only set for data
  // fetched from ServiceWorker caches. Today, shared storage script fetch
  // cannot be intercepted by service workers.

  std::optional<mojo_base::BigBuffer> cached_metadata =
      (code_cache_fetcher_ && response_head)
          ? code_cache_fetcher_->TakeCodeCacheForResponse(*response_head)
          : std::nullopt;
  code_cache_fetcher_.reset();

  mojom::blink::SharedStorageWorkletService::AddModuleCallback
      add_module_finished_callback = std::move(callback).Then(WTF::BindOnce(
          &SharedStorageWorkletGlobalScope::RecordAddModuleFinished,
          WrapPersistent(this)));

  if (!response_body) {
    std::move(add_module_finished_callback)
        .Run(false, String(error_message.c_str()));
    return;
  }

  DCHECK(error_message.empty());
  DCHECK(response_head);

  if (!ScriptController()) {
    std::move(add_module_finished_callback)
        .Run(false, /*error_message=*/"Worklet is being destroyed.");
    return;
  }

  WebURLResponse response =
      WebURLResponse::Create(script_source_url, *response_head.get(),
                             /*report_security_info=*/false, /*request_id=*/0);

  const ResourceResponse& resource_response = response.ToResourceResponse();

  // Create a `ScriptCachedMetadataHandler` for http family URLs. This
  // replicates the core logic from `ScriptResource::ResponseReceived`,
  // simplified since shared storage doesn't require
  // `ScriptCachedMetadataHandlerWithHashing` which is only used for certain
  // schemes.
  ScriptCachedMetadataHandler* cached_metadata_handler = nullptr;

  if (script_source_url.ProtocolIsInHTTPFamily()) {
    std::unique_ptr<CachedMetadataSender> sender = CachedMetadataSender::Create(
        resource_response, mojom::blink::CodeCacheType::kJavascript,
        GetSecurityOrigin());

    cached_metadata_handler = MakeGarbageCollected<ScriptCachedMetadataHandler>(
        WTF::TextEncoding(response_head->charset.c_str()), std::move(sender));

    if (cached_metadata) {
      cached_metadata_handler->SetSerializedCachedMetadata(
          std::move(*cached_metadata));
    }
  }

  // TODO(crbug.com/1419253): Using a classic script with the custom script
  // loader is tentative. Eventually, this should migrate to the blink-worklet's
  // script loading infrastructure.
  ClassicScript* worker_script = ClassicScript::Create(
      String(*response_body),
      /*source_url=*/script_source_url,
      /*base_url=*/KURL(), ScriptFetchOptions(),
      ScriptSourceLocationType::kUnknown, SanitizeScriptErrors::kSanitize,
      cached_metadata_handler);

  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  v8::HandleScope handle_scope(script_state->GetIsolate());
  ScriptEvaluationResult result =
      worker_script->RunScriptOnScriptStateAndReturnValue(script_state);

  if (result.GetResultType() ==
      ScriptEvaluationResult::ResultType::kException) {
    v8::Local<v8::Value> exception = result.GetExceptionForWorklet();

    std::move(add_module_finished_callback)
        .Run(false,
             /*error_message=*/ExceptionToString(script_state, exception));
    return;
  } else if (result.GetResultType() !=
             ScriptEvaluationResult::ResultType::kSuccess) {
    std::move(add_module_finished_callback)
        .Run(false, /*error_message=*/"Internal Failure");
    return;
  }

  std::move(add_module_finished_callback)
      .Run(true, /*error_message=*/g_empty_string);
}

void SharedStorageWorkletGlobalScope::DidReceiveCachedCode() {
  if (handle_script_download_response_after_code_cache_response_) {
    std::move(handle_script_download_response_after_code_cache_response_).Run();
  }
}

void SharedStorageWorkletGlobalScope::RecordAddModuleFinished() {
  add_module_finished_ = true;
}

bool SharedStorageWorkletGlobalScope::PerformCommonOperationChecks(
    const String& operation_name,
    String& error_message,
    SharedStorageOperationDefinition*& operation_definition) {
  DCHECK(error_message.empty());
  DCHECK_EQ(operation_definition, nullptr);

  if (!add_module_finished_) {
    // TODO(http://crbug/1249581): if this operation comes while fetching the
    // module script, we might want to queue the operation to be handled later
    // after addModule completes.
    error_message = kSharedStorageModuleScriptNotLoadedErrorMessage;
    return false;
  }

  auto it = operation_definition_map_.find(operation_name);
  if (it == operation_definition_map_.end()) {
    error_message = kSharedStorageOperationNotFoundErrorMessage;
    return false;
  }

  operation_definition = it->value;

  ScriptState* script_state = operation_definition->GetScriptState();

  ScriptState::Scope scope(script_state);

  TraceWrapperV8Reference<v8::Value> instance =
      operation_definition->GetInstance();
  if (instance.IsEmpty()) {
    error_message = kSharedStorageEmptyOperationDefinitionInstanceErrorMessage;
    return false;
  }

  return true;
}

base::OnceClosure SharedStorageWorkletGlobalScope::StartOperation(
    mojom::blink::PrivateAggregationOperationDetailsPtr pa_operation_details) {
  CHECK(add_module_finished_);
  CHECK_EQ(!!pa_operation_details,
           ShouldDefinePrivateAggregationInSharedStorage());

  int64_t operation_id = operation_counter_++;

  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  isolate->SetContinuationPreservedEmbedderData(
      v8::BigInt::New(isolate, operation_id));

  if (ShouldDefinePrivateAggregationInSharedStorage()) {
    GetOrCreatePrivateAggregation()->OnOperationStarted(
        operation_id, std::move(pa_operation_details));
  }

  return WTF::BindOnce(&SharedStorageWorkletGlobalScope::FinishOperation,
                       WrapPersistent(this), operation_id);
}

void SharedStorageWorkletGlobalScope::FinishOperation(int64_t operation_id) {
  if (ShouldDefinePrivateAggregationInSharedStorage()) {
    CHECK(private_aggregation_);
    private_aggregation_->OnOperationFinished(operation_id);
  }
}

PrivateAggregation*
SharedStorageWorkletGlobalScope::GetOrCreatePrivateAggregation() {
  CHECK(ShouldDefinePrivateAggregationInSharedStorage());
  CHECK(add_module_finished_);

  if (!private_aggregation_) {
    private_aggregation_ = MakeGarbageCollected<PrivateAggregation>(this);
  }

  return private_aggregation_.Get();
}

}  // namespace blink
