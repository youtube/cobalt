// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder.h"

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/features_generated.h"

AISemanticEmbedder::AISemanticEmbedder(
    AIContextBoundObjectSet& context_bound_object_set,
    mojo::PendingReceiver<blink::mojom::AISemanticEmbedder> receiver)
    : AIContextBoundObject(context_bound_object_set),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AISemanticEmbedder::~AISemanticEmbedder() = default;

void AISemanticEmbedder::Embed(
    const std::vector<std::string>& inputs,
    blink::mojom::AISemanticEmbedderEmbedOptionsPtr options,
    EmbedCallback callback) {
  if (inputs.empty()) {
    receiver_.ReportBadMessage("Invalid empty inputs");
    return;
  }

  std::vector<std::string> formatted_inputs;
  formatted_inputs.reserve(inputs.size());

  for (const auto& input : inputs) {
    // We intentionally apply the task type prompt formatting here in the
    // Browser process, rather than the Utility process. This achieves a clean
    // separation of concerns: the Browser process handles WebIDL enum mapping
    // and string formatting, while the shared Utility process remains a "dumb"
    // executor that simply tokenizes the raw string it receives. This also
    // prevents us from having to plumb the 'task_type' across the shared Mojo
    // interfaces.
    if (options && options->task_type.has_value()) {
      switch (options->task_type.value()) {
        case blink::mojom::AISemanticEmbedderTaskType::kSemanticSimilarity:
          formatted_inputs.push_back("task: sentence similarity | query: " +
                                     input);
          break;
        case blink::mojom::AISemanticEmbedderTaskType::kClustering:
          formatted_inputs.push_back("task: clustering | query: " + input);
          break;
        case blink::mojom::AISemanticEmbedderTaskType::kClassification:
          formatted_inputs.push_back("task: classification | query: " + input);
          break;
        case blink::mojom::AISemanticEmbedderTaskType::kRetrievalQuery:
          formatted_inputs.push_back("task: search result | query: " + input);
          break;
        case blink::mojom::AISemanticEmbedderTaskType::kRetrievalDocument:
          formatted_inputs.push_back("title: none | text: " + input);
          break;
      }
    } else {
      formatted_inputs.push_back(input);
    }
  }

  auto* service_launcher = AISemanticEmbedderServiceLauncher::Get();
  if (!service_launcher->controller()->IsModelAvailable() ||
      !service_launcher->AllowedToLaunch()) {
    std::move(callback).Run(nullptr);
    return;
  }

  pending_jobs_.push_back(
      service_launcher->controller()->GetEmbedder()->ComputePassagesEmbeddings(
          passage_embeddings::PassagePriority::kUserInitiated,
          std::move(formatted_inputs),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindOnce(&AISemanticEmbedder::OnEmbedComplete,
                             weak_ptr_factory_.GetWeakPtr(),
                             mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                 std::move(callback), nullptr)))));
}

void AISemanticEmbedder::OnEmbedComplete(
    EmbedCallback callback,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  std::erase_if(pending_jobs_,
                [job_id](const passage_embeddings::Embedder::Job& job) {
                  return job.id() == job_id;
                });

  if (status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    AISemanticEmbedderServiceLauncher::Get()->RecordSuccessfulUse();
  }

  if (status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess &&
      embeddings.size() == passages.size()) {
    auto result = blink::mojom::SemanticEmbedderResult::New();
    result->embeddings.reserve(embeddings.size());
    for (auto& embedding : embeddings) {
      auto content_embedding = blink::mojom::ContentEmbedding::New();
      content_embedding->values = embedding.GetData();

      result->embeddings.push_back(std::move(content_embedding));
    }
    std::move(callback).Run(std::move(result));
    return;
  }

  std::move(callback).Run(nullptr);
}
