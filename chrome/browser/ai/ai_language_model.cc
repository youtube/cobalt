// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace features {

// Indicates the streaming behavior of this session.
// If it's true, each streaming response will contain the full content that's
// generated so far. e.g.
// - This is
// - This is a test
// - This is a test response.
// If it's false, the response will be streamed back chunk by chunk. e.g.
// - This is
// - a test
// - response.
BASE_FEATURE(kAILanguageModelForceStreamingFullResponse,
             "AILanguageModelForceStreamingFullResponse",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
namespace {

using optimization_guide::proto::PromptApiMetadata;
using optimization_guide::proto::PromptApiPrompt;
using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

PromptApiRole ConvertRole(blink::mojom::AILanguageModelInitialPromptRole role) {
  switch (role) {
    case blink::mojom::AILanguageModelInitialPromptRole::kSystem:
      return PromptApiRole::PROMPT_API_ROLE_SYSTEM;
    case blink::mojom::AILanguageModelInitialPromptRole::kUser:
      return PromptApiRole::PROMPT_API_ROLE_USER;
    case blink::mojom::AILanguageModelInitialPromptRole::kAssistant:
      return PromptApiRole::PROMPT_API_ROLE_ASSISTANT;
  }
}

PromptApiPrompt MakePrompt(PromptApiRole role, const std::string& content) {
  PromptApiPrompt prompt;
  prompt.set_role(role);
  prompt.set_content(content);
  return prompt;
}

}  // namespace

AILanguageModel::Context::ContextItem::ContextItem() = default;
AILanguageModel::Context::ContextItem::ContextItem(const ContextItem&) =
    default;
AILanguageModel::Context::ContextItem::ContextItem(ContextItem&&) = default;
AILanguageModel::Context::ContextItem::~ContextItem() = default;

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AILanguageModel::Context::Context(uint32_t max_tokens,
                                  ContextItem initial_prompts)
    : max_tokens_(max_tokens), initial_prompts_(std::move(initial_prompts)) {
  CHECK_GE(max_tokens_, initial_prompts_.tokens)
      << "the caller shouldn't create an AILanguageModel with the initial "
         "prompts containing more tokens than the limit.";
  current_tokens_ += initial_prompts.tokens;
}

AILanguageModel::Context::Context(const Context& context) = default;

AILanguageModel::Context::~Context() = default;

AILanguageModel::Context::SpaceReservationResult
AILanguageModel::Context::ReserveSpace(uint32_t num_tokens) {
  // If there is no enough space to hold the `initial_prompts_` as well as the
  // newly requested `num_tokens`,  return `kInsufficientSpace`.
  if (num_tokens + initial_prompts_.tokens > max_tokens_) {
    return AILanguageModel::Context::SpaceReservationResult::kInsufficientSpace;
  }

  if (current_tokens_ + num_tokens <= max_tokens_) {
    return AILanguageModel::Context::SpaceReservationResult::kSufficientSpace;
  }

  CHECK(!context_items_.empty());
  do {
    current_tokens_ -= context_items_.begin()->tokens;
    context_items_.pop_front();
  } while (current_tokens_ + num_tokens > max_tokens_);

  return AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable;
}

AILanguageModel::Context::SpaceReservationResult
AILanguageModel::Context::AddContextItem(ContextItem context_item) {
  auto result = ReserveSpace(context_item.tokens);
  if (result != SpaceReservationResult::kInsufficientSpace) {
    context_items_.emplace_back(context_item);
    current_tokens_ += context_item.tokens;
  }

  return result;
}

std::unique_ptr<google::protobuf::MessageLite>
AILanguageModel::Context::MaybeFormatRequest(PromptApiRequest request) {
  return std::make_unique<PromptApiRequest>(std::move(request));
}

std::unique_ptr<google::protobuf::MessageLite>
AILanguageModel::Context::MakeRequest() {
  PromptApiRequest request;
  request.mutable_initial_prompts()->MergeFrom(initial_prompts_.prompts);
  for (auto& context_item : context_items_) {
    request.mutable_prompt_history()->MergeFrom((context_item.prompts));
  }
  return MaybeFormatRequest(std::move(request));
}

bool AILanguageModel::Context::HasContextItem() {
  return current_tokens_;
}

AILanguageModel::AILanguageModel(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    base::WeakPtr<content::BrowserContext> browser_context,
    mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote,
    AIContextBoundObjectSet& context_bound_object_set,
    AIManager& ai_manager,
    AIUtils::LanguageCodes expected_input_languages,
    const std::optional<const Context>& context)
    : AIContextBoundObject(context_bound_object_set),
      session_(std::move(session)),
      browser_context_(browser_context),
      context_bound_object_set_(context_bound_object_set),
      ai_manager_(ai_manager),
      expected_input_languages_(std::move(expected_input_languages)),
      pending_remote_(std::move(pending_remote)),
      receiver_(this, pending_remote_.InitWithNewPipeAndPassReceiver()) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));

  auto metadata = ParseMetadata(session_->GetOnDeviceFeatureMetadata());
  is_on_device_session_streaming_chunk_by_chunk_ =
      metadata.is_streaming_chunk_by_chunk();

  if (context.has_value()) {
    // If the context is provided, it will be used in this session.
    context_ = std::make_unique<Context>(context.value());
    return;
  }

  // If the context is not provided, initialize a new context
  // with the default configuration.
  context_ = std::make_unique<Context>(
      session_->GetTokenLimits().max_context_tokens, Context::ContextItem());
}

AILanguageModel::~AILanguageModel() = default;

// static
PromptApiMetadata AILanguageModel::ParseMetadata(
    const optimization_guide::proto::Any& any) {
  PromptApiMetadata metadata;
  if (any.type_url() == "type.googleapis.com/" + metadata.GetTypeName()) {
    metadata.ParseFromString(any.value());
  }
  return metadata;
}

void AILanguageModel::SetInitialPrompts(
    const std::optional<std::string> system_prompt,
    std::vector<blink::mojom::AILanguageModelInitialPromptPtr> initial_prompts,
    CreateLanguageModelCallback callback) {
  PromptApiRequest request;
  if (system_prompt) {
    *request.add_initial_prompts() =
        MakePrompt(PromptApiRole::PROMPT_API_ROLE_SYSTEM, *system_prompt);
  }
  for (const auto& prompt : initial_prompts) {
    *request.add_initial_prompts() =
        MakePrompt(ConvertRole(prompt->role), prompt->content);
  }
  session_->GetContextSizeInTokens(
      *context_->MaybeFormatRequest(request),
      base::BindOnce(&AILanguageModel::InitializeContextWithInitialPrompts,
                     weak_ptr_factory_.GetWeakPtr(), request,
                     std::move(callback)));
}

void AILanguageModel::InitializeContextWithInitialPrompts(
    optimization_guide::proto::PromptApiRequest initial_request,
    CreateLanguageModelCallback callback,
    uint32_t size) {
  // If the on device model service fails to get the size, it will be 0.
  // TODO(crbug.com/351935691): make sure the error is explicitly returned and
  // handled accordingly.
  if (!size) {
    std::move(callback).Run(
        base::unexpected(blink::mojom::AIManagerCreateClientError::
                             kUnableToCalculateTokenSize),
        /*info=*/nullptr);
    return;
  }

  uint32_t max_token = context_->max_tokens();
  if (size > max_token) {
    // The session cannot be created if the system prompt contains more tokens
    // than the limit.
    std::move(callback).Run(
        base::unexpected(
            blink::mojom::AIManagerCreateClientError::kInitialPromptsTooLarge),
        /*info=*/nullptr);
    return;
  }

  auto initial_prompts = Context::ContextItem();
  initial_prompts.tokens = size;
  initial_prompts.prompts.Swap(initial_request.mutable_initial_prompts());
  context_ = std::make_unique<Context>(max_token, std::move(initial_prompts));
  std::move(callback).Run(TakePendingRemote(), GetLanguageModelInstanceInfo());
}

void AILanguageModel::ModelExecutionCallback(
    const PromptApiRequest& input,
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  if (!result.response.has_value()) {
    responder->OnError(
        AIUtils::ConvertModelExecutionError(result.response.error().error()));
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  std::string streaming_result = response->value();
  bool should_stream_full_response = base::FeatureList::IsEnabled(
      features::kAILanguageModelForceStreamingFullResponse);
  if (is_on_device_session_streaming_chunk_by_chunk_) {
    // We need this for the context adding.
    current_response_ += response->value();
    if (should_stream_full_response) {
      // Adapting the chunk-by-chunk mode to the current-response mode.
      streaming_result = current_response_;
    }
  } else {
    if (!should_stream_full_response) {
      // Adapting the current-response mode to the chunk-by-chunk mode.
      streaming_result = response->value().substr(current_response_.size());
    }
    current_response_ = response->value();
  }

  if (response->has_value()) {
    responder->OnStreaming(
        streaming_result,
        should_stream_full_response
            ? blink::mojom::ModelStreamingResponderAction::kReplace
            : blink::mojom::ModelStreamingResponderAction::kAppend);
  }

  if (result.response->is_complete) {
    uint32_t token_count = result.response->input_token_count +
                           result.response->output_token_count;
    // If the on device model service fails to calculate the size, it will be 0.
    // TODO(crbug.com/351935691): make sure the error is explicitly returned
    // and handled accordingly.
    if (token_count) {
      auto item = Context::ContextItem();
      item.tokens = token_count;
      item.prompts.CopyFrom(input.current_prompts());
      item.prompts.Add(MakePrompt(PromptApiRole::PROMPT_API_ROLE_ASSISTANT,
                                  current_response_));
      if (context_->AddContextItem(std::move(item)) ==
          Context::SpaceReservationResult::kSpaceMadeAvailable) {
        responder->OnContextOverflow();
      }
    }
    responder->OnCompletion(blink::mojom::ModelExecutionContextInfo::New(
        context_->current_tokens()));
  }
}

void AILanguageModel::PromptGetInputSizeCompletion(
    mojo::RemoteSetElementId responder_id,
    PromptApiRequest request,
    uint32_t number_of_tokens) {
  if (!session_) {
    // If the session is destroyed before this callback is invoked, we should
    // not do anything further.
    return;
  }

  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  auto result = context_->ReserveSpace(number_of_tokens);
  if (result == Context::SpaceReservationResult::kInsufficientSpace) {
    responder->OnError(blink::mojom::ModelStreamingResponseStatus::
                           kErrorPromptRequestTooLarge);
    return;
  }

  if (result == Context::SpaceReservationResult::kSpaceMadeAvailable) {
    responder->OnContextOverflow();
  }

  if (context_->HasContextItem()) {
    session_->AddContext(*context_->MakeRequest());
  }

  session_->ExecuteModel(
      *context_->MaybeFormatRequest(request),
      base::BindRepeating(&AILanguageModel::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), request,
                          responder_id));
}

void AILanguageModel::Prompt(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  // Clear the response from the previous execution.
  current_response_ = "";
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  PromptApiRequest request;
  *request.add_current_prompts() =
      MakePrompt(PromptApiRole::PROMPT_API_ROLE_USER, input);

  session_->GetExecutionInputSizeInTokens(
      *context_->MaybeFormatRequest(request),
      base::BindOnce(&AILanguageModel::PromptGetInputSizeCompletion,
                     weak_ptr_factory_.GetWeakPtr(), responder_id, request));
}

AIUtils::LanguageCodes AILanguageModel::GetExpectedInputLanguagesCopy() {
  if (!expected_input_languages_.has_value()) {
    return std::nullopt;
  }
  std::vector<blink::mojom::AILanguageCodePtr> cloned_languages;
  for (auto& language : expected_input_languages_.value()) {
    cloned_languages.emplace_back(language->Clone());
  }
  return cloned_languages;
}

void AILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  if (!browser_context_) {
    // The `browser_context_` is already destroyed before the renderer owner
    // is gone.
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  const optimization_guide::SamplingParams sampling_param =
      session_->GetSamplingParams();

  ai_manager_->CreateLanguageModelForCloning(
      base::PassKey<AILanguageModel>(),
      blink::mojom::AILanguageModelSamplingParams::New(
          sampling_param.top_k, sampling_param.temperature),
      context_bound_object_set_.get(), GetExpectedInputLanguagesCopy(),
      *context_, std::move(client_remote));
}

void AILanguageModel::Destroy() {
  if (session_) {
    session_.reset();
  }

  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }

  responder_set_.Clear();
}

blink::mojom::AILanguageModelInstanceInfoPtr
AILanguageModel::GetLanguageModelInstanceInfo() {
  const optimization_guide::SamplingParams session_sampling_params =
      session_->GetSamplingParams();
  return blink::mojom::AILanguageModelInstanceInfo::New(
      context_->max_tokens(), context_->current_tokens(),
      blink::mojom::AILanguageModelSamplingParams::New(
          session_sampling_params.top_k, session_sampling_params.temperature),
      GetExpectedInputLanguagesCopy());
}

void AILanguageModel::CountPromptTokens(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::AILanguageModelCountPromptTokensClient>
        client) {
  PromptApiRequest request;
  *request.add_current_prompts() =
      MakePrompt(PromptApiRole::PROMPT_API_ROLE_USER, input);

  session_->GetExecutionInputSizeInTokens(
      *context_->MaybeFormatRequest(request),
      base::BindOnce(
          [](mojo::Remote<blink::mojom::AILanguageModelCountPromptTokensClient>
                 client_remote,
             uint32_t number_of_tokens) {
            client_remote->OnResult(number_of_tokens);
          },
          mojo::Remote<blink::mojom::AILanguageModelCountPromptTokensClient>(
              std::move(client))));
}

mojo::PendingRemote<blink::mojom::AILanguageModel>
AILanguageModel::TakePendingRemote() {
  return std::move(pending_remote_);
}
