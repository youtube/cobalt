// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_language_model.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ai_language_model_prompt_input.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model_factory.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class CloneLanguageModelClient
    : public GarbageCollected<CloneLanguageModelClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public AIMojoClient<AILanguageModel> {
 public:
  CloneLanguageModelClient(ScriptState* script_state,
                           AILanguageModel* language_model,
                           ScriptPromiseResolver<AILanguageModel>* resolver,
                           AbortSignal* signal,
                           base::PassKey<AILanguageModel>)
      : AIMojoClient(script_state, language_model, resolver, signal),
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
    AIMojoClient::Trace(visitor);
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
    AILanguageModel* cloned_language_model =
        MakeGarbageCollected<AILanguageModel>(
            language_model_->GetExecutionContext(),
            std::move(language_model_remote), language_model_->GetTaskRunner(),
            std::move(info));
    GetResolver()->Resolve(cloned_language_model);
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
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
  Member<AILanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
                   CloneLanguageModelClient>
      receiver_;
};

class CountPromptTokensClient
    : public GarbageCollected<CountPromptTokensClient>,
      public mojom::blink::AILanguageModelCountPromptTokensClient,
      public AIMojoClient<IDLUnsignedLongLong> {
 public:
  CountPromptTokensClient(ScriptState* script_state,
                          AILanguageModel* language_model,
                          ScriptPromiseResolver<IDLUnsignedLongLong>* resolver,
                          AbortSignal* signal,
                          const WTF::String& input)
      : AIMojoClient(script_state, language_model, resolver, signal),
        language_model_(language_model),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AILanguageModelCountPromptTokensClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->CountPromptTokens(
        input, std::move(client_remote));
  }
  ~CountPromptTokensClient() override = default;

  CountPromptTokensClient(const CountPromptTokensClient&) = delete;
  CountPromptTokensClient& operator=(const CountPromptTokensClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(language_model_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AILanguageModelCountPromptTokensClient implementation.
  void OnResult(uint32_t number_of_tokens) override {
    if (!GetResolver()) {
      return;
    }

    GetResolver()->Resolve(number_of_tokens);
    Cleanup();
  }

 protected:
  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<AILanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AILanguageModelCountPromptTokensClient,
                   CountPromptTokensClient>
      receiver_;
};

}  // namespace

AILanguageModel::AILanguageModel(
    ExecutionContext* execution_context,
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::blink::AILanguageModelInstanceInfoPtr info)
    : ExecutionContextClient(execution_context),
      task_runner_(task_runner),
      language_model_remote_(execution_context) {
  language_model_remote_.Bind(std::move(pending_remote), task_runner);
  if (info) {
    max_tokens_ = info->max_tokens;
    current_tokens_ = info->current_tokens;
    top_k_ = info->sampling_params->top_k;
    temperature_ = info->sampling_params->temperature;
    if (info->expected_input_languages.has_value()) {
      expected_input_languages_ =
          ToStringLanguageCodes(info->expected_input_languages.value());
    }
  }
}

void AILanguageModel::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(language_model_remote_);
}

const AtomicString& AILanguageModel::InterfaceName() const {
  return event_target_names::kAILanguageModel;
}

ExecutionContext* AILanguageModel::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

ScriptPromise<IDLString> AILanguageModel::prompt(
    ScriptState* script_state,
    const V8AILanguageModelPromptInput* input,
    const AILanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  // The API impl only accepts a string for now, more to come soon!
  if (!input->IsString()) {
    resolver->RejectWithTypeError("Input type not supported");
    return promise;
  }
  const WTF::String& input_string = input->GetAsString();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPrompt);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kLanguageModel),
                             int(input_string.CharactersSizeInBytes()));

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return promise;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, resolver, task_runner_,
      AIMetrics::AISessionType::kLanguageModel,
      WTF::BindOnce(&AILanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&AILanguageModel::OnContextOverflow,
                         WrapWeakPersistent(this)));
  language_model_remote_->Prompt(input_string, std::move(pending_remote));
  return promise;
}

ReadableStream* AILanguageModel::promptStreaming(
    ScriptState* script_state,
    const V8AILanguageModelPromptInput* input,
    const AILanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  // The API impl only accepts a string for now, more to come soon!
  if (!input->IsString()) {
    exception_state.ThrowTypeError("Input type not supported");
    return nullptr;
  }
  const WTF::String& input_string = input->GetAsString();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPromptStreaming);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kLanguageModel),
                             int(input_string.CharactersSizeInBytes()));

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return nullptr;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return nullptr;
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kLanguageModel,
          WTF::BindOnce(&AILanguageModel::OnResponseComplete,
                        WrapWeakPersistent(this)),
          WTF::BindRepeating(&AILanguageModel::OnContextOverflow,
                             WrapWeakPersistent(this)));
  language_model_remote_->Prompt(input_string, std::move(pending_remote));
  return readable_stream;
}

ScriptPromise<AILanguageModel> AILanguageModel::clone(
    ScriptState* script_state,
    const AILanguageModelCloneOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AILanguageModel>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<AILanguageModel>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AILanguageModel>>(
          script_state);
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
      script_state, this, resolver, signal, base::PassKey<AILanguageModel>());

  return promise;
}

ScriptPromise<IDLUnsignedLongLong> AILanguageModel::countPromptTokens(
    ScriptState* script_state,
    const WTF::String& input,
    const AILanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLUnsignedLongLong>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionCountPromptTokens);

  ScriptPromiseResolver<IDLUnsignedLongLong>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUnsignedLongLong>>(
          script_state);
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

  MakeGarbageCollected<CountPromptTokensClient>(script_state, this, resolver,
                                                signal, input);

  return promise;
}

// TODO(crbug.com/355967885): reset the remote to destroy the session.
void AILanguageModel::destroy(ScriptState* script_state,
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

void AILanguageModel::OnResponseComplete(
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  if (context_info) {
    current_tokens_ = context_info->current_tokens;
  }
}

HeapMojoRemote<mojom::blink::AILanguageModel>&
AILanguageModel::GetAILanguageModelRemote() {
  return language_model_remote_;
}

scoped_refptr<base::SequencedTaskRunner> AILanguageModel::GetTaskRunner() {
  return task_runner_;
}

uint64_t AILanguageModel::GetCurrentTokens() {
  return current_tokens_;
}

void AILanguageModel::OnContextOverflow() {
  DispatchEvent(*Event::Create(event_type_names::kContextoverflow));
}

}  // namespace blink
