// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_message_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/language_model_create_client.h"
#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_util.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using AILanguageModelPromptContentOrError =
    std::variant<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>;

void RejectResolver(ScriptPromiseResolverBase* resolver,
                    const ScriptValue& value) {
  resolver->Reject(value);
}

class CloneLanguageModelClient
    : public GarbageCollected<CloneLanguageModelClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public AIContextObserver<LanguageModel> {
 public:
  CloneLanguageModelClient(ScriptState* script_state,
                           LanguageModel* language_model,
                           ScriptPromiseResolver<LanguageModel>* resolver,
                           AbortSignal* signal,
                           base::PassKey<LanguageModel>)
      : AIContextObserver(script_state, language_model, resolver, signal),
        language_model_(language_model),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->Fork(std::move(client_remote));
  }
  ~CloneLanguageModelClient() override = default;

  CloneLanguageModelClient(const CloneLanguageModelClient&) = delete;
  CloneLanguageModelClient& operator=(const CloneLanguageModelClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(language_model_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AIManagerCreateLanguageModelClient implementation.
  void OnResult(
      mojo::PendingRemote<mojom::blink::AILanguageModel> language_model_remote,
      mojom::blink::AILanguageModelInstanceInfoPtr info) override {
    if (!GetResolver()) {
      return;
    }

    CHECK(info);
    LanguageModel* cloned_language_model = MakeGarbageCollected<LanguageModel>(
        language_model_->GetExecutionContext(),
        std::move(language_model_remote), language_model_->GetTaskRunner(),
        std::move(info));
    GetResolver()->Resolve(cloned_language_model);
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error,
               mojom::blink::QuotaErrorInfoPtr quota_error_info) override {
    if (!GetResolver()) {
      return;
    }

    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        kExceptionMessageUnableToCloneSession);
    Cleanup();
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<LanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
                   CloneLanguageModelClient>
      receiver_;
};

class AppendClient : public GarbageCollected<AppendClient>,
                     public mojom::blink::ModelStreamingResponder,
                     public AIContextObserver<IDLUndefined> {
 public:
  AppendClient(
      ScriptState* script_state,
      LanguageModel* language_model,
      ScriptPromiseResolver<IDLUndefined>* resolver,
      WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts,
      AbortSignal* signal,
      base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
          complete_callback,
      base::RepeatingClosure overflow_callback)
      : AIContextObserver(script_state, language_model, resolver, signal),
        language_model_(language_model),
        complete_callback_(std::move(complete_callback)),
        overflow_callback_(overflow_callback),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::ModelStreamingResponder> client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->Append(
        std::move(prompts), std::move(client_remote));
  }
  ~AppendClient() override = default;

  AppendClient(const AppendClient&) = delete;
  AppendClient& operator=(const AppendClient&) = delete;

  // Helper function to make MeasureInputUsageClient with no return type.
  static void Create(
      ScriptState* script_state,
      LanguageModel* language_model,
      ScriptPromiseResolver<IDLUndefined>* resolver,
      AbortSignal* signal,
      base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
          complete_callback,
      base::RepeatingClosure overflow_callback,
      WTF::Vector<mojom::blink::AILanguageModelPromptPtr> input) {
    MakeGarbageCollected<AppendClient>(
        std::move(script_state), std::move(language_model), std::move(resolver),
        std::move(input), std::move(signal), std::move(complete_callback),
        std::move(overflow_callback));
  }

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(language_model_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::ModelStreamingResponder implementation.
  void OnCompletion(
      mojom::blink::ModelExecutionContextInfoPtr context_info) override {
    if (!GetResolver()) {
      return;
    }

    if (complete_callback_) {
      std::move(complete_callback_).Run(std::move(context_info));
    }

    GetResolver()->Resolve();
    Cleanup();
  }

  void OnError(ModelStreamingResponseStatus status,
               mojom::blink::QuotaErrorInfoPtr quota_error_info) override {
    if (GetResolver()) {
      GetResolver()->Reject(ConvertModelStreamingResponseErrorToDOMException(
          status, std::move(quota_error_info)));
    }
    Cleanup();
  }

  void OnStreaming(const WTF::String& text) override {
    NOTREACHED() << "Append() should not invoke `OnStreaming()`";
  }

  void OnQuotaOverflow() override {
    if (overflow_callback_) {
      overflow_callback_.Run();
    }
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<LanguageModel> language_model_;
  base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
      complete_callback_;
  base::RepeatingClosure overflow_callback_;
  HeapMojoReceiver<mojom::blink::ModelStreamingResponder, AppendClient>
      receiver_;
};

// Parses `constraint` from `options` if available. On success or if no
// constraint is present returns true, returns false and throws an exception on
// failure.
bool ParseConstraint(
    ScriptState* script_state,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state,
    on_device_model::mojom::blink::ResponseConstraintPtr& constraint) {
  if (!options->hasResponseConstraint()) {
    return true;
  }

  if (!RuntimeEnabledFeatures::AIPromptAPIStructuredOutputEnabled(
          ExecutionContext::From(script_state))) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "responseConstraint is not supported");
    return false;
  }

  v8::Local<v8::Object> value = options->responseConstraint().V8Object();
  if (value->IsRegExp()) {
    constraint = on_device_model::mojom::blink::ResponseConstraint::NewRegex(
        ToBlinkString<String>(script_state->GetIsolate(),
                              value.As<v8::RegExp>()->GetSource(),
                              ExternalMode::kDoNotExternalize));
  } else if (value->IsObject()) {
    String stringified_schema = ValidateAndStringifyObject(
        options->responseConstraint(), script_state, exception_state);
    if (!stringified_schema) {
      // ValidateAndStringifyObject throws an exception when it returns
      // a null string.
      return false;
    }
    constraint =
        on_device_model::mojom::blink::ResponseConstraint::NewJsonSchema(
            stringified_schema);
  } else {
    exception_state.ThrowTypeError("Constraint type is not supported");
    return false;
  }
  return true;
}

}  // namespace

// static
mojom::blink::AILanguageModelPromptRole LanguageModel::ConvertRoleToMojo(
    V8LanguageModelMessageRole role) {
  switch (role.AsEnum()) {
    case V8LanguageModelMessageRole::Enum::kSystem:
      return mojom::blink::AILanguageModelPromptRole::kSystem;
    case V8LanguageModelMessageRole::Enum::kUser:
      return mojom::blink::AILanguageModelPromptRole::kUser;
    case V8LanguageModelMessageRole::Enum::kAssistant:
      return mojom::blink::AILanguageModelPromptRole::kAssistant;
  }
  NOTREACHED();
}

LanguageModel::LanguageModel(
    ExecutionContext* execution_context,
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::blink::AILanguageModelInstanceInfoPtr info)
    : ExecutionContextClient(execution_context),
      task_runner_(task_runner),
      language_model_remote_(execution_context) {
  language_model_remote_.Bind(std::move(pending_remote), task_runner);
  if (info) {
    input_quota_ = info->input_quota;
    input_usage_ = info->input_usage;
    top_k_ = info->sampling_params->top_k;
    temperature_ = info->sampling_params->temperature;
    if (info->input_types.has_value()) {
      for (const auto& input_type : *(info->input_types)) {
        input_types_.insert(input_type);
      }
    }
  }
}

void LanguageModel::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(language_model_remote_);
}

const AtomicString& LanguageModel::InterfaceName() const {
  return event_target_names::kAILanguageModel;
}

ExecutionContext* LanguageModel::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

// static
ScriptPromise<LanguageModel> LanguageModel::create(
    ScriptState* script_state,
    LanguageModelCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageModel>>(script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCreateSession);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(execution_context);
  if (!ai_manager_remote.is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  MakeGarbageCollected<LanguageModelCreateClient>(resolver, options)->Create();
  return promise;
}

// static
ScriptPromise<V8Availability> LanguageModel::availability(
    ScriptState* script_state,
    const LanguageModelCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCanCreateSession);
  mojom::blink::AILanguageModelSamplingParamsPtr sampling_params;
  auto sampling_params_or_exception = ResolveSamplingParamsOption(options);
  if (!sampling_params_or_exception.has_value()) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }
  sampling_params = std::move(sampling_params_or_exception.value());

  Vector<mojom::blink::AILanguageModelExpectedPtr> expected_in, expected_out;
  if (options && options->hasExpectedInputs()) {
    expected_in = ToMojoExpectations(options->expectedInputs());
  }
  if (options && options->hasExpectedOutputs()) {
    expected_out = ToMojoExpectations(options->expectedOutputs());
  }

  Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts;
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(
          ExecutionContext::From(script_state));
  ai_manager_remote->CanCreateLanguageModel(
      mojom::blink::AILanguageModelCreateOptions::New(
          std::move(sampling_params), std::move(initial_prompts),
          std::move(expected_in), std::move(expected_out)),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8Availability>* resolver,
             mojom::blink::ModelAvailabilityCheckResult check_result) {
            Availability availability = HandleModelAvailabilityCheckResult(
                resolver->GetExecutionContext(),
                AIMetrics::AISessionType::kLanguageModel, check_result);
            resolver->Resolve(AvailabilityToV8(availability));
          },
          WrapPersistent(resolver)));
  return promise;
}

// static
ScriptPromise<IDLNullable<LanguageModelParams>> LanguageModel::params(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<LanguageModelParams>>>(script_state);
  auto promise = resolver->Promise();

  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(
          ExecutionContext::From(script_state));
  ai_manager_remote->GetLanguageModelParams(WTF::BindOnce(
      [](ScriptPromiseResolver<IDLNullable<LanguageModelParams>>* resolver,
         mojom::blink::AILanguageModelParamsPtr language_model_params) {
        if (!language_model_params) {
          resolver->Resolve(nullptr);
          return;
        }
        auto* params = MakeGarbageCollected<LanguageModelParams>(
            language_model_params->default_sampling_params->top_k,
            language_model_params->max_sampling_params->top_k,
            language_model_params->default_sampling_params->temperature,
            language_model_params->max_sampling_params->temperature);
        resolver->Resolve(params);
      },
      WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLString> LanguageModel::prompt(
    ScriptState* script_state,
    const V8LanguageModelPrompt* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  std::optional<on_device_model::mojom::blink::ResponseConstraintPtr>
      processed_constraint = ValidateAndProcessPromptInput(
          script_state, input, options, exception_state);
  if (!processed_constraint.has_value()) {
    return EmptyPromise();
  }
  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPrompt);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  auto pending_remote = CreateModelExecutionResponder(
      script_state, options->getSignalOr(nullptr), resolver, task_runner_,
      AIMetrics::AISessionType::kLanguageModel,
      WTF::BindOnce(&LanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                         WrapWeakPersistent(this)));

  ConvertPromptInputsToMojo(
      script_state, options->getSignalOr(nullptr), input, input_types_,
      WTF::BindOnce(&LanguageModel::ExecutePrompt, WrapPersistent(this),
                    WrapPersistent(script_state), WrapPersistent(resolver),
                    std::move(*processed_constraint),
                    std::move(pending_remote)),
      WTF::BindOnce(&RejectResolver, WrapPersistent(resolver)));
  return promise;
}

ReadableStream* LanguageModel::promptStreaming(
    ScriptState* script_state,
    const V8LanguageModelPrompt* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  std::optional<on_device_model::mojom::blink::ResponseConstraintPtr>
      processed_constraint = ValidateAndProcessPromptInput(
          script_state, input, options, exception_state);
  if (!processed_constraint.has_value()) {
    return nullptr;
  }
  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPromptStreaming);

  auto [stream, remote] = CreateModelExecutionStreamingResponder(
      script_state, options->getSignalOr(nullptr), task_runner_,
      AIMetrics::AISessionType::kLanguageModel,
      WTF::BindOnce(&LanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                         WrapWeakPersistent(this)));

  ConvertPromptInputsToMojo(
      script_state, options->getSignalOr(nullptr), input, input_types_,
      WTF::BindOnce(&LanguageModel::ExecutePrompt, WrapPersistent(this),
                    WrapPersistent(script_state), WrapPersistent(stream),
                    std::move(*processed_constraint), std::move(remote)),
      WTF::BindOnce(
          [](ReadableStream* readable_stream, ScriptState* script_state,
             const ScriptValue& error) {
            ReadableStreamController* controller =
                readable_stream->GetController();
            // TODO(crbug.com/414906618): Make this more elegant (i.e.
            // crrev.com/c/6528669).
            CHECK(controller->IsDefaultController());
            MakeGarbageCollected<
                ReadableStreamDefaultControllerWithScriptScope>(
                script_state, To<ReadableStreamDefaultController>(controller))
                ->Error(error.V8ValueFor(script_state));
          },
          WrapPersistent(stream), WrapPersistent(script_state)));

  return stream;
}

void LanguageModel::ExecutePrompt(
    ScriptState* script_state,
    ResolverOrStream resolver_or_stream,
    on_device_model::mojom::blink::ResponseConstraintPtr constraint,
    mojo::PendingRemote<mojom::blink::ModelStreamingResponder>
        pending_responder,
    WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts) {
  if (!language_model_remote_) {
    if (std::holds_alternative<ScriptPromiseResolverBase*>(
            resolver_or_stream)) {
      ScriptPromiseResolverBase* resolver =
          std::get<ScriptPromiseResolverBase*>(resolver_or_stream);
      resolver->Reject(CreateSessionDestroyedException());
      return;
    }
    CHECK(std::holds_alternative<ReadableStream*>(resolver_or_stream));
    ReadableStream* stream = std::get<ReadableStream*>(resolver_or_stream);
    ReadableStreamController* controller = stream->GetController();
    // TODO(crbug.com/414906618): Make this more elegant (i.e.
    // crrev.com/c/6536457).
    CHECK(controller->IsDefaultController());
    MakeGarbageCollected<ReadableStreamDefaultControllerWithScriptScope>(
        script_state, To<ReadableStreamDefaultController>(controller))
        ->Error(CreateSessionDestroyedException()->Wrap(script_state));
    return;
  }
  language_model_remote_->Prompt(std::move(prompts), std::move(constraint),
                                 std::move(pending_responder));
}

void LanguageModel::ExecuteMeasureInputUsage(
    ScriptPromiseResolver<IDLDouble>* resolver,
    AbortSignal* signal,
    WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts) {
  language_model_remote_->MeasureInputUsage(
      std::move(prompts),
      WTF::BindOnce(
          [](ScriptPromiseResolver<IDLDouble>* resolver, AbortSignal* signal,
             std::optional<uint32_t> usage) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }
            if (signal && signal->aborted()) {
              resolver->Reject(signal->reason(resolver->GetScriptState()));
              return;
            }
            if (!usage.has_value()) {
              resolver->Reject(
                  DOMException::Create(kExceptionMessageUnableToCalculateUsage,
                                       DOMException::GetErrorName(
                                           DOMExceptionCode::kOperationError)));
              return;
            }
            resolver->Resolve(static_cast<double>(usage.value()));
          },
          WrapPersistent(resolver), WrapPersistent(signal)));
}

std::optional<on_device_model::mojom::blink::ResponseConstraintPtr>
LanguageModel::ValidateAndProcessPromptInput(
    ScriptState* script_state,
    const V8LanguageModelPrompt* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return std::nullopt;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return std::nullopt;
  }

  on_device_model::mojom::blink::ResponseConstraintPtr constraint;
  if (!ParseConstraint(script_state, options, exception_state, constraint)) {
    // ParseConstraint will throw an exception when false is returned.
    return std::nullopt;
  }

  // TODO(crbug.com/411470034): Aggregate other input type sizes for UMA.
  if (input->IsString()) {
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionRequestSizeMetricName(
            AIMetrics::AISessionType::kLanguageModel),
        static_cast<int>(input->GetAsString().CharactersSizeInBytes()));
  }

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return std::nullopt;
  }

  return constraint;
}

ScriptPromise<IDLUndefined> LanguageModel::append(
    ScriptState* script_state,
    const V8LanguageModelPrompt* input,
    const LanguageModelAppendOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLUndefined>();
  }

  ScriptPromiseResolver<IDLUndefined>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return promise;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return promise;
  }

  ConvertPromptInputsToMojo(
      script_state, options->getSignalOr(nullptr), input, input_types_,
      WTF::BindOnce(&AppendClient::Create, WrapPersistent(script_state),
                    WrapPersistent(this), WrapPersistent(resolver),
                    WrapPersistent(signal),
                    WTF::BindOnce(&LanguageModel::OnResponseComplete,
                                  WrapWeakPersistent(this)),
                    WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                                       WrapWeakPersistent(this))),
      WTF::BindOnce(&RejectResolver, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<LanguageModel> LanguageModel::clone(
    ScriptState* script_state,
    const LanguageModelCloneOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<LanguageModel>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageModel>>(script_state);
  auto promise = resolver->Promise();

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return promise;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  MakeGarbageCollected<CloneLanguageModelClient>(
      script_state, this, resolver, signal, base::PassKey<LanguageModel>());

  return promise;
}

ScriptPromise<IDLDouble> LanguageModel::measureInputUsage(
    ScriptState* script_state,
    const V8LanguageModelPrompt* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionCountPromptTokens);

  ScriptPromiseResolver<IDLDouble>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLDouble>>(script_state);
  auto promise = resolver->Promise();

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return promise;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  ConvertPromptInputsToMojo(
      script_state, options->getSignalOr(nullptr), input, input_types_,
      WTF::BindOnce(&LanguageModel::ExecuteMeasureInputUsage,
                    WrapPersistent(this), WrapPersistent(resolver),
                    WrapPersistent(signal)),
      WTF::BindOnce(&RejectResolver, WrapPersistent(resolver)));

  return promise;
}

// TODO(crbug.com/355967885): reset the remote to destroy the session.
void LanguageModel::destroy(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionDestroy);

  if (language_model_remote_) {
    language_model_remote_->Destroy();
    language_model_remote_.reset();
  }
}

void LanguageModel::OnResponseComplete(
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  if (context_info) {
    input_usage_ = context_info->current_tokens;
  }
}

HeapMojoRemote<mojom::blink::AILanguageModel>&
LanguageModel::GetAILanguageModelRemote() {
  return language_model_remote_;
}

scoped_refptr<base::SequencedTaskRunner> LanguageModel::GetTaskRunner() {
  return task_runner_;
}

void LanguageModel::OnQuotaOverflow() {
  DispatchEvent(*Event::Create(event_type_names::kQuotaoverflow));
}

}  // namespace blink
