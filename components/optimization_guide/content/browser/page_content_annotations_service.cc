// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/optimization_guide/content/browser/page_content_annotations_validator.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/noisy_metrics_recorder.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/omnibox_proto/types.pb.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"
#endif

namespace optimization_guide {

namespace {

// Keep this in sync with the PageContentAnnotationsStorageType variant in
// ../optimization/histograms.xml.
std::string PageContentAnnotationsTypeToString(
    PageContentAnnotationsType annotation_type) {
  switch (annotation_type) {
    case PageContentAnnotationsType::kUnknown:
      return "Unknown";
    case PageContentAnnotationsType::kModelAnnotations:
      return "ModelAnnotations";
    case PageContentAnnotationsType::kRelatedSearches:
      return "RelatedSearches";
    case PageContentAnnotationsType::kSearchMetadata:
      return "SearchMetadata";
    case PageContentAnnotationsType::kRemoteMetdata:
      return "RemoteMetadata";
    case PageContentAnnotationsType::kSalientImageMetadata:
      return "SalientImageMetadata";
  }
}

void LogPageContentAnnotationsStorageStatus(
    PageContentAnnotationsStorageStatus status,
    PageContentAnnotationsType annotation_type) {
  DCHECK_NE(status, PageContentAnnotationsStorageStatus::kUnknown);
  DCHECK_NE(annotation_type, PageContentAnnotationsType::kUnknown);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      status);

  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus." +
          PageContentAnnotationsTypeToString(annotation_type),
      status);
}

void LogRelatedSearchesExtracted(bool success) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotationsService."
      "RelatedSearchesExtracted",
      success);
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Record the visibility score of the provided visit as a RAPPOR-style record to
// UKM.
void MaybeRecordVisibilityUKM(
    const HistoryVisit& visit,
    const absl::optional<history::VisitContentModelAnnotations>&
        content_annotations) {
  if (visit.visit_id) {
    // This is for a remote visit not tied to a navigation.
    return;
  }

  if (!content_annotations)
    return;

  if (content_annotations->visibility_score < 0)
    return;

  int64_t score =
      static_cast<int64_t>(100 * content_annotations->visibility_score);

  if (google_util::IsGoogleSearchUrl(visit.url)) {
    base::UmaHistogramPercentage(
        "OptimizationGuide.PageContentAnnotationsService."
        "VisibilityScoreOfGoogleSRP",
        score);
  }

  // We want 2^|num_bits| buckets, linearly spaced.
  uint32_t num_buckets =
      std::pow(2, optimization_guide::features::NumBitsForRAPPORMetrics());
  DCHECK_GT(num_buckets, 0u);
  float bucket_size = 100.0 / num_buckets;
  uint32_t bucketed_score = static_cast<uint32_t>(floor(score / bucket_size));
  DCHECK_LE(bucketed_score, num_buckets);
  uint32_t noisy_score = NoisyMetricsRecorder().GetNoisyMetric(
      optimization_guide::features::NoiseProbabilityForRAPPORMetrics(),
      bucketed_score, optimization_guide::features::NumBitsForRAPPORMetrics());
  ukm::SourceId ukm_source_id = ukm::ConvertToSourceId(
      visit.navigation_id, ukm::SourceIdType::NAVIGATION_ID);

  ukm::builders::PageContentAnnotations2(ukm_source_id)
      .SetVisibilityScore(static_cast<int64_t>(noisy_score))
      .Record(ukm::UkmRecorder::Get());
}
#endif /* BUILDFLAG(BUILD_WITH_TFLITE_LIB) */

}  // namespace

PageContentAnnotationsService::PageContentAnnotationsService(
    std::unique_ptr<AutocompleteProviderClient> autocomplete_provider_client,
    const std::string& application_locale,
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    history::HistoryService* history_service,
    TemplateURLService* template_url_service,
    ZeroSuggestCacheService* zero_suggest_cache_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& database_dir,
    OptimizationGuideLogger* optimization_guide_logger,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : autocomplete_provider_client_(std::move(autocomplete_provider_client)),
      min_page_category_score_to_persist_(
          features::GetMinimumPageCategoryScoreToPersist()),
      history_service_(history_service),
      template_url_service_(template_url_service),
      zero_suggest_cache_service_(zero_suggest_cache_service),
      last_annotated_history_visits_(
          features::MaxContentAnnotationRequestsCached()),
      annotated_text_cache_(features::MaxVisitAnnotationCacheSize()),
      optimization_guide_logger_(optimization_guide_logger) {
  DCHECK(optimization_guide_model_provider);
  DCHECK(history_service_);
  history_service_observation_.Observe(history_service_);
  if (zero_suggest_cache_service_) {
    zero_suggest_cache_service_observation_.Observe(
        zero_suggest_cache_service_);
  }
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
      optimization_guide_model_provider);
  annotator_ = model_manager_.get();

  if (features::ShouldExecutePageVisibilityModelOnPageContent(
          application_locale)) {
    model_manager_->RequestAndNotifyWhenModelAvailable(
        AnnotationType::kContentVisibility, base::DoNothing());
    annotation_types_to_execute_.push_back(AnnotationType::kContentVisibility);
  }
  if (features::ShouldExecutePageEntitiesModelOnPageContent(
          application_locale)) {
    model_manager_->RequestAndNotifyWhenModelAvailable(
        AnnotationType::kPageEntities, base::DoNothing());
    annotation_types_to_execute_.push_back(AnnotationType::kPageEntities);
  }
#endif

  validator_ =
      PageContentAnnotationsValidator::MaybeCreateAndStartTimer(annotator_);
}

PageContentAnnotationsService::~PageContentAnnotationsService() = default;

void PageContentAnnotationsService::Annotate(const HistoryVisit& visit) {
  if (last_annotated_history_visits_.Peek(visit) !=
      last_annotated_history_visits_.end()) {
    // We have already been requested to annotate this visit, so don't submit
    // for re-annotation.
    return;
  }
  last_annotated_history_visits_.Put(visit, true);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!visit.text_to_annotate)
    return;
  // Used for testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      "PageContentAnnotations.AnnotateVisit.AnnotationRequested", true);

  auto it = annotated_text_cache_.Peek(*visit.text_to_annotate);
  if (it != annotated_text_cache_.end()) {
    // We have annotations the text for this visit, so return that immediately
    // rather than re-executing the model.
    //
    // TODO(crbug.com/1291275): If the model was updated, the cached value could
    // be stale so we should invalidate the cache on model updates.
    OnPageContentAnnotated(visit, it->second);
    base::UmaHistogramBoolean(
        "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
        true);
    return;
  }
  if (switches::ShouldLogPageContentAnnotationsInput()) {
    LOG(ERROR) << "Adding annotation job: \n"
               << "URL: " << visit.url << "\n"
               << "Text: " << visit.text_to_annotate.value_or(std::string());
  }
  visits_to_annotate_.emplace_back(visit);

  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
      false);

  if (MaybeStartAnnotateVisitBatch())
    return;

  // Used for testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      "PageContentAnnotations.AnnotateVisit.AnnotationRequestQueued", true);

  if (visits_to_annotate_.size() > features::AnnotateVisitBatchSize()) {
    // The queue is full and an batch annotation is actively being done so
    // we will remove the "oldest" visit.
    visits_to_annotate_.erase(visits_to_annotate_.begin());
    // Used for testing.
    LOCAL_HISTOGRAM_BOOLEAN(
        "PageContentAnnotations.AnnotateVisit.QueueFullVisitDropped", true);
  }
#endif
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
bool PageContentAnnotationsService::MaybeStartAnnotateVisitBatch() {
  bool is_full_batch_available =
      visits_to_annotate_.size() >= features::AnnotateVisitBatchSize();
  bool batch_already_running = !current_visit_annotation_batch_.empty();

  if (is_full_batch_available && !batch_already_running) {
    // Used for testing.
    LOCAL_HISTOGRAM_BOOLEAN(
        "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", true);
    current_visit_annotation_batch_ = std::move(visits_to_annotate_);
    AnnotateVisitBatch();

    return true;
  }
  return false;
}

void PageContentAnnotationsService::AnnotateVisitBatch() {
  DCHECK(!current_visit_annotation_batch_.empty());

  std::vector<std::string> inputs;
  for (const HistoryVisit& visit : current_visit_annotation_batch_) {
    DCHECK(visit.text_to_annotate);
    inputs.push_back(*visit.text_to_annotate);
  }

  std::unique_ptr<
      std::vector<absl::optional<history::VisitContentModelAnnotations>>>
      merged_annotation_outputs = std::make_unique<
          std::vector<absl::optional<history::VisitContentModelAnnotations>>>();
  merged_annotation_outputs->reserve(inputs.size());
  for (size_t i = 0; i < inputs.size(); i++) {
    merged_annotation_outputs->push_back(absl::nullopt);
  }

  std::vector<absl::optional<history::VisitContentModelAnnotations>>*
      merged_annotation_outputs_ptr = merged_annotation_outputs.get();

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      annotation_types_to_execute_.size(),
      base::BindOnce(&PageContentAnnotationsService::OnBatchVisitsAnnotated,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(merged_annotation_outputs)));

  for (AnnotationType type : annotation_types_to_execute_) {
    annotator_->Annotate(
        base::BindOnce(
            &PageContentAnnotationsService::OnAnnotationBatchComplete,
            weak_ptr_factory_.GetWeakPtr(), type, merged_annotation_outputs_ptr,
            barrier_closure),
        inputs, type);
  }
}

void PageContentAnnotationsService::OnAnnotationBatchComplete(
    AnnotationType type,
    std::vector<absl::optional<history::VisitContentModelAnnotations>>*
        merge_to_output,
    base::OnceClosure signal_merge_complete_callback,
    const std::vector<BatchAnnotationResult>& batch_result) {
  DCHECK_EQ(merge_to_output->size(), batch_result.size());
  for (size_t i = 0; i < batch_result.size(); i++) {
    const BatchAnnotationResult result = batch_result[i];
    DCHECK_EQ(type, result.type());

    if (optimization_guide_logger_) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::PAGE_CONTENT_ANNOTATIONS,
          optimization_guide_logger_)
          << "PageContentAnnotationJob Result: " << result.ToString();
    }

    if (!result.HasOutputForType())
      continue;

    history::VisitContentModelAnnotations current_annotations;

    if (type == AnnotationType::kContentVisibility) {
      DCHECK(result.visibility_score());
      current_annotations.visibility_score = *result.visibility_score();
    } else if (type == AnnotationType::kPageEntities) {
      DCHECK(result.entities());
      for (const ScoredEntityMetadata& scored_md : *result.entities()) {
        DCHECK(scored_md.score >= 0.0 && scored_md.score <= 1.0);
        history::VisitContentModelAnnotations::Category category(
            scored_md.metadata.entity_id,
            static_cast<int>(100 * scored_md.score));
        history::VisitContentModelAnnotations::MergeCategoryIntoVector(
            category, &current_annotations.entities);
      }
    }

    history::VisitContentModelAnnotations previous_annotations =
        merge_to_output->at(i).value_or(
            history::VisitContentModelAnnotations());
    current_annotations.MergeFrom(previous_annotations);

    merge_to_output->at(i) = current_annotations;
  }

  // This needs to be ran last because |merge_to_output| may be deleted when
  // run.
  std::move(signal_merge_complete_callback).Run();
}

void PageContentAnnotationsService::OnBatchVisitsAnnotated(
    std::unique_ptr<
        std::vector<absl::optional<history::VisitContentModelAnnotations>>>
        merged_annotation_outputs) {
  DCHECK_EQ(merged_annotation_outputs->size(),
            current_visit_annotation_batch_.size());
  for (size_t i = 0; i < merged_annotation_outputs->size(); i++) {
    OnPageContentAnnotated(current_visit_annotation_batch_[i],
                           merged_annotation_outputs->at(i));
  }

  current_visit_annotation_batch_.clear();
  MaybeStartAnnotateVisitBatch();
}
#endif

void PageContentAnnotationsService::OverridePageContentAnnotatorForTesting(
    PageContentAnnotator* annotator) {
  annotator_ = annotator;
}

void PageContentAnnotationsService::BatchAnnotate(
    BatchAnnotationCallback callback,
    const std::vector<std::string>& inputs,
    AnnotationType annotation_type) {
  if (!annotator_) {
    std::move(callback).Run(CreateEmptyBatchAnnotationResults(inputs));
    return;
  }

  annotator_->Annotate(
      base::BindOnce(
          [](BatchAnnotationCallback original_callback,
             OptimizationGuideLogger* optimization_guide_logger,
             const std::vector<BatchAnnotationResult>& batch_result) {
            if (optimization_guide_logger) {
              for (const BatchAnnotationResult& result : batch_result) {
                OPTIMIZATION_GUIDE_LOGGER(
                    optimization_guide_common::mojom::LogSource::
                        PAGE_CONTENT_ANNOTATIONS,
                    optimization_guide_logger)
                    << "PageContentAnnotationJob Result: " << result.ToString();
              }
            }
            std::move(original_callback).Run(batch_result);
          },
          std::move(callback), optimization_guide_logger_),
      inputs, annotation_type);
}

absl::optional<ModelInfo> PageContentAnnotationsService::GetModelInfoForType(
    AnnotationType type) const {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  DCHECK(annotator_);
  return annotator_->GetModelInfoForType(type);
#else
  return absl::nullopt;
#endif
}

void PageContentAnnotationsService::RequestAndNotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  DCHECK(annotator_);
  annotator_->RequestAndNotifyWhenModelAvailable(type, std::move(callback));
#else
  std::move(callback).Run(false);
#endif
}

void PageContentAnnotationsService::ExtractRelatedSearches(
    const HistoryVisit& visit,
    content::WebContents* web_contents) {
  if (ShouldExtractRelatedSearchesFromZPSCache()) {
    return;
  }

  search_result_extractor_client_.RequestData(
      web_contents, {continuous_search::mojom::ResultType::kRelatedSearches},
      base::BindOnce(&PageContentAnnotationsService::OnRelatedSearchesExtracted,
                     weak_ptr_factory_.GetWeakPtr(), visit));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PageContentAnnotationsService::OnPageContentAnnotated(
    const HistoryVisit& visit,
    const absl::optional<history::VisitContentModelAnnotations>&
        content_annotations) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      content_annotations.has_value());
  if (!content_annotations)
    return;

  bool is_new_entry = false;
  if (annotated_text_cache_.Peek(*visit.text_to_annotate) ==
      annotated_text_cache_.end()) {
    is_new_entry = true;
    annotated_text_cache_.Put(*visit.text_to_annotate, *content_annotations);
  }

  // Only log entities for new entries for local visits.
  if (is_new_entry && !visit.visit_id) {
    for (const auto& entity : content_annotations->entities) {
      // Skip low weight entities.
      if (entity.weight < 50)
        continue;
      GetMetadataForEntityId(
          entity.id,
          base::BindOnce(
              &PageContentAnnotationsService::OnEntityMetadataRetrieved,
              weak_ptr_factory_.GetWeakPtr(), visit.url, entity.id,
              entity.weight));
    }
  }

  MaybeRecordVisibilityUKM(visit, content_annotations);
  NotifyPageContentAnnotatedObservers(
      AnnotationType::kContentVisibility, visit.url,
      PageContentAnnotationsResult::CreateContentVisibilityScoreResult(
          content_annotations->visibility_score));

  if (!features::ShouldWriteContentAnnotationsToHistoryService())
    return;

  if (visit.visit_id) {
    // If the visit ID is known, directly add the annotations for that visit
    // rather than querying history for the closest match.
    history_service_->AddContentModelAnnotationsForVisit(*content_annotations,
                                                         *visit.visit_id);
  } else {
    QueryURL(visit,
             base::BindOnce(
                 &history::HistoryService::AddContentModelAnnotationsForVisit,
                 history_service_->AsWeakPtr(), *content_annotations),
             PageContentAnnotationsType::kModelAnnotations);
  }
}
#endif

bool PageContentAnnotationsService::ShouldExtractRelatedSearchesFromZPSCache() {
  return base::FeatureList::IsEnabled(
             features::kExtractRelatedSearchesFromPrefetchedZPSResponse) &&
         autocomplete_provider_client_ && zero_suggest_cache_service_;
}

void PageContentAnnotationsService::OnZeroSuggestResponseUpdated(
    const std::string& page_url,
    const ZeroSuggestCacheService::CacheEntry& response) {
  if (!ShouldExtractRelatedSearchesFromZPSCache()) {
    return;
  }

  if (page_url.empty() || !google_util::IsGoogleSearchUrl(GURL(page_url))) {
    return;
  }

  history_service_->QueryURL(
      GURL(page_url), /*want_visits=*/true,
      base::BindOnce(&PageContentAnnotationsService::
                         ExtractRelatedSearchesFromZeroSuggestResponse,
                     weak_ptr_factory_.GetWeakPtr(), response),
      &history_service_task_tracker_);
}

void PageContentAnnotationsService::
    ExtractRelatedSearchesFromZeroSuggestResponse(
        const ZeroSuggestCacheService::CacheEntry& response,
        history::QueryURLResult url_result) {
  if (!url_result.success || url_result.visits.empty()) {
    LogPageContentAnnotationsStorageStatus(
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl,
        PageContentAnnotationsType::kRelatedSearches);
    return;
  }

  AutocompleteInput input(u"", metrics::OmniboxEventProto::JOURNEYS,
                          autocomplete_provider_client_->GetSchemeClassifier());
  auto suggest_results =
      response.GetSuggestResults(input, *autocomplete_provider_client_);

  std::vector<std::string> related_searches;
  for (const auto& result : suggest_results) {
    const auto subtypes = result.subtypes();
    // Suggestions with HIVEMIND subtype are considered "related searches".
    auto it = std::find(subtypes.begin(), subtypes.end(),
                        omnibox::SuggestSubtype::SUBTYPE_HIVEMIND);
    if (it != subtypes.end()) {
      related_searches.push_back(base::UTF16ToUTF8(
          base::CollapseWhitespace(result.suggestion(), true)));
    }
  }

  if (related_searches.empty()) {
    LogRelatedSearchesExtracted(false);
    return;
  }

  LogRelatedSearchesExtracted(true);

  auto visit_id = url_result.visits.front().visit_id;
  history_service_->AddRelatedSearchesForVisit(related_searches, visit_id);

  LogPageContentAnnotationsStorageStatus(
      PageContentAnnotationsStorageStatus::kSuccess,
      PageContentAnnotationsType::kRelatedSearches);
}

void PageContentAnnotationsService::OnRelatedSearchesExtracted(
    const HistoryVisit& visit,
    continuous_search::SearchResultExtractorClientStatus status,
    continuous_search::mojom::CategoryResultsPtr results) {
  const bool success =
      status == continuous_search::SearchResultExtractorClientStatus::kSuccess;
  LogRelatedSearchesExtracted(success);

  if (!success) {
    return;
  }

  std::vector<std::string> related_searches;
  for (const auto& group : results->groups) {
    if (group->type != continuous_search::mojom::ResultType::kRelatedSearches) {
      continue;
    }
    base::ranges::transform(
        group->results, std::back_inserter(related_searches),
        [](const continuous_search::mojom::SearchResultPtr& result) {
          return base::UTF16ToUTF8(
              base::CollapseWhitespace(result->title, true));
        });
    break;
  }

  if (related_searches.empty()) {
    return;
  }

  if (!features::ShouldWriteContentAnnotationsToHistoryService()) {
    return;
  }

  QueryURL(visit,
           base::BindOnce(&history::HistoryService::AddRelatedSearchesForVisit,
                          history_service_->AsWeakPtr(), related_searches),
           PageContentAnnotationsType::kRelatedSearches);
}

void PageContentAnnotationsService::QueryURL(
    const HistoryVisit& visit,
    PersistAnnotationsCallback callback,
    PageContentAnnotationsType annotation_type) {
  history_service_->QueryURL(
      visit.url, /*want_visits=*/true,
      base::BindOnce(&PageContentAnnotationsService::OnURLQueried,
                     weak_ptr_factory_.GetWeakPtr(), visit, std::move(callback),
                     annotation_type),
      &history_service_task_tracker_);
}

void PageContentAnnotationsService::OnURLQueried(
    const HistoryVisit& visit,
    PersistAnnotationsCallback callback,
    PageContentAnnotationsType annotation_type,
    history::QueryURLResult url_result) {
  if (!url_result.success || url_result.visits.empty()) {
    LogPageContentAnnotationsStorageStatus(
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl, annotation_type);
    return;
  }

  bool did_store_content_annotations = false;
  for (const auto& visit_for_url : base::Reversed(url_result.visits)) {
    if (visit.nav_entry_timestamp != visit_for_url.visit_time) {
      continue;
    }

    std::move(callback).Run(visit_for_url.visit_id);

    did_store_content_annotations = true;
    break;
  }
  LogPageContentAnnotationsStorageStatus(
      did_store_content_annotations ? kSuccess : kSpecificVisitForUrlNotFound,
      annotation_type);
}

void PageContentAnnotationsService::GetMetadataForEntityId(
    const std::string& entity_id,
    EntityMetadataRetrievedCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_->GetMetadataForEntityId(entity_id, std::move(callback));
#else
  std::move(callback).Run(absl::nullopt);
#endif
}

void PageContentAnnotationsService::GetMetadataForEntityIds(
    const base::flat_set<std::string>& entity_ids,
    BatchEntityMetadataRetrievedCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_->GetMetadataForEntityIds(entity_ids, std::move(callback));
#else
  std::move(callback).Run({});
#endif
}

void PageContentAnnotationsService::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& visit_row) {
  DCHECK_EQ(history_service, history_service_);

  // By default, annotate the title.
  HistoryVisit history_visit(visit_row.visit_id);
  history_visit.text_to_annotate = base::UTF16ToUTF8(url_row.title());

  if (template_url_service_ &&
      (optimization_guide::features::
           ShouldPersistSearchMetadataForNonGoogleSearches() ||
       google_util::IsGoogleSearchUrl(url_row.url()))) {
    auto search_metadata =
        template_url_service_->ExtractSearchMetadata(url_row.url());
    if (search_metadata) {
      history_service_->AddSearchMetadataForVisit(
          search_metadata->normalized_url, search_metadata->search_terms,
          visit_row.visit_id);

      // If there's search metadata, annotate search terms instead.
      history_visit.text_to_annotate =
          base::UTF16ToUTF8(search_metadata->search_terms);
    }
  }

  // Annotate remote visits with the text that was determined above.
  if (!visit_row.originator_cache_guid.empty()) {
    if (switches::ShouldLogPageContentAnnotationsInput()) {
      LOG(ERROR) << "Annotating remote visit " << visit_row.visit_id << ":\n"
                 << "URL: " << url_row.url() << "\n"
                 << "Text: " << *(history_visit.text_to_annotate);
    }
    Annotate(history_visit);
  }
}

void PageContentAnnotationsService::AddObserver(
    AnnotationType annotation_type,
    PageContentAnnotationsService::PageContentAnnotationsObserver* observer) {
  DCHECK_EQ(AnnotationType::kContentVisibility, annotation_type);
  page_content_annotations_observers_[annotation_type].AddObserver(observer);
}

void PageContentAnnotationsService::RemoveObserver(
    AnnotationType annotation_type,
    PageContentAnnotationsService::PageContentAnnotationsObserver* observer) {
  DCHECK_EQ(AnnotationType::kContentVisibility, annotation_type);
  page_content_annotations_observers_[annotation_type].RemoveObserver(observer);
}

void PageContentAnnotationsService::PersistRemotePageMetadata(
    const HistoryVisit& visit,
    const proto::PageEntitiesMetadata& page_entities_metadata) {
  // Persist entities and categories to VisitContentModelAnnotations if that
  // feature is enabled.
  history::VisitContentModelAnnotations model_annotations;
  for (const auto& entity : page_entities_metadata.entities()) {
    if (entity.entity_id().empty()) {
      continue;
    }
    if (entity.score() < 0 || entity.score() > 100) {
      continue;
    }

    model_annotations.entities.emplace_back(entity.entity_id(), entity.score());
  }

  std::vector<history::VisitContentModelAnnotations::Category> categories;
  for (const auto& category : page_entities_metadata.categories()) {
    int category_score = static_cast<int>(100 * category.score());
    if (category_score < min_page_category_score_to_persist_) {
      continue;
    }
    model_annotations.categories.emplace_back(category.category_id(),
                                              category_score);
  }

  if (!model_annotations.entities.empty() ||
      !model_annotations.categories.empty()) {
    // Only persist if we have something to persist.
    QueryURL(visit,
             base::BindOnce(
                 &history::HistoryService::AddContentModelAnnotationsForVisit,
                 history_service_->AsWeakPtr(), model_annotations),
             // Even though we are persisting remote page entities, we store
             // these as an override to the model annotations.
             PageContentAnnotationsType::kModelAnnotations);
  }

  // Persist any other metadata to VisitContentAnnotations, if enabled.
  if (!page_entities_metadata.alternative_title().empty()) {
    QueryURL(visit,
             base::BindOnce(&history::HistoryService::AddPageMetadataForVisit,
                            history_service_->AsWeakPtr(),
                            page_entities_metadata.alternative_title()),
             PageContentAnnotationsType::kRemoteMetdata);
  }
}

void PageContentAnnotationsService::PersistSalientImageMetadata(
    const HistoryVisit& visit,
    const proto::SalientImageMetadata& salient_image_metadata) {
  if (salient_image_metadata.thumbnails_size() <= 0) {
    return;
  }

  // Persist the detail if at least one thumbnail has a non-empty URL.
  for (const auto& thumbnail : salient_image_metadata.thumbnails()) {
    if (!thumbnail.image_url().empty()) {
      QueryURL(visit,
               base::BindOnce(
                   &history::HistoryService::SetHasUrlKeyedImageForVisit,
                   history_service_->AsWeakPtr(), /*has_url_keyed_image=*/true),
               PageContentAnnotationsType::kSalientImageMetadata);
      return;
    }
  }
}

void PageContentAnnotationsService::OnEntityMetadataRetrieved(
    const GURL& url,
    const std::string& entity_id,
    int weight,
    const absl::optional<EntityMetadata>& entity_metadata) {
  if (!entity_metadata.has_value())
    return;

  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();

  for (const auto& collection : entity_metadata->collections) {
    PageEntityCollection page_entity_collection =
        GetPageEntityCollectionForString(collection);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.PageContentAnnotations.EntityCollection_50",
        page_entity_collection);
  }

  if (optimization_guide_logger_) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::PAGE_CONTENT_ANNOTATIONS,
        optimization_guide_logger_)
        << "Entities: Url=" << url.ReplaceComponents(replacements)
        << " Weight=" << base::NumberToString(weight) << ". "
        << entity_metadata->ToHumanReadableString();
  }
}

void PageContentAnnotationsService::NotifyPageContentAnnotatedObservers(
    AnnotationType annotation_type,
    const GURL& url,
    const PageContentAnnotationsResult& page_content_annotations_result) {
  if (page_content_annotations_observers_.find(annotation_type) ==
      page_content_annotations_observers_.end()) {
    return;
  }
  for (auto& observer : page_content_annotations_observers_[annotation_type]) {
    observer.OnPageContentAnnotated(url, page_content_annotations_result);
  }
}

HistoryVisit::HistoryVisit() = default;

HistoryVisit::HistoryVisit(base::Time nav_entry_timestamp,
                           GURL url,
                           int64_t navigation_id) {
  this->nav_entry_timestamp = nav_entry_timestamp;
  this->url = url;
  this->navigation_id = navigation_id;
}

HistoryVisit::HistoryVisit(history::VisitID visit_id) {
  this->visit_id = visit_id;
}

HistoryVisit::~HistoryVisit() = default;
HistoryVisit::HistoryVisit(const HistoryVisit&) = default;

}  // namespace optimization_guide
