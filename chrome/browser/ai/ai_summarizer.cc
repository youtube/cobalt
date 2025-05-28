// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_summarizer.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

optimization_guide::proto::SummarizerOutputType ToProtoType(
    blink::mojom::AISummarizerType type) {
  switch (type) {
    case blink::mojom::AISummarizerType::kTLDR:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_TL_DR;
    case blink::mojom::AISummarizerType::kKeyPoints:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_KEYPOINTS;
    case blink::mojom::AISummarizerType::kTeaser:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_TEASER;
    case blink::mojom::AISummarizerType::kHeadline:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_HEADLINES;
  }
}

optimization_guide::proto::SummarizerOutputFormat ToProtoFormat(
    blink::mojom::AISummarizerFormat format) {
  switch (format) {
    case blink::mojom::AISummarizerFormat::kPlainText:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_FORMAT_PLAIN_TEXT;
    case blink::mojom::AISummarizerFormat::kMarkDown:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_FORMAT_MARKDOWN;
  }
}

optimization_guide::proto::SummarizerOutputLength ToProtoLength(
    blink::mojom::AISummarizerLength length) {
  switch (length) {
    case blink::mojom::AISummarizerLength::kShort:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_LENGTH_SHORT;
    case blink::mojom::AISummarizerLength::kMedium:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_LENGTH_MEDIUM;
    case blink::mojom::AISummarizerLength::kLong:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_LENGTH_LONG;
  }
}

}  // namespace

AISummarizer::AISummarizer(
    AIContextBoundObjectSet& context_bound_object_set,
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        summarize_session,
    blink::mojom::AISummarizerCreateOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::AISummarizer> receiver)
    : AIContextBoundObject(context_bound_object_set),
      summarize_session_(std::move(summarize_session)),
      receiver_(this, std::move(receiver)),
      options_(std::move(options)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AISummarizer::~AISummarizer() {
  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }
}

// static
std::unique_ptr<optimization_guide::proto::SummarizeOptions>
AISummarizer::ToProtoOptions(
    const blink::mojom::AISummarizerCreateOptionsPtr& options) {
  auto proto_options =
      std::make_unique<optimization_guide::proto::SummarizeOptions>();
  proto_options->set_output_type(ToProtoType(options->type));
  proto_options->set_output_format(ToProtoFormat(options->format));
  proto_options->set_output_length(ToProtoLength(options->length));
  return proto_options;
}

// static
std::string AISummarizer::CombineContexts(const std::string& shared_context,
                                          const std::string& context) {
  std::string final_context = shared_context;
  if (!context.empty()) {
    if (!final_context.empty()) {
      final_context = final_context + " " + context;
    } else {
      final_context = context;
    }
  }
  if (!final_context.empty()) {
    final_context += "\n";
  }
  return final_context;
}

void AISummarizer::ModelExecutionCallback(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  if (!result.response.has_value()) {
    responder->OnError(
        AIUtils::ConvertModelExecutionError(result.response.error().error()));
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    responder->OnStreaming(
        response->value(),
        blink::mojom::ModelStreamingResponderAction::kReplace);
  }
  if (result.response->is_complete) {
    responder->OnCompletion(/*context_info=*/nullptr);
  }
}

void AISummarizer::Summarize(
    const std::string& input,
    const std::string& context,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!summarize_session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  optimization_guide::proto::SummarizeRequest request;
  request.set_article(input);
  request.set_allocated_options(ToProtoOptions(options_).release());
  request.set_context(
      CombineContexts(options_->shared_context.value_or(""), context));
  summarize_session_->ExecuteModel(
      request,
      base::BindRepeating(&AISummarizer::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id));
}
