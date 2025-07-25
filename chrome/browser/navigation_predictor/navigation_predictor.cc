// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// The maximum number of clicks to track in a single navigation.
size_t kMaxClicksTracked = 10;

bool IsPrerendering(content::RenderFrameHost& render_frame_host) {
  return render_frame_host.GetLifecycleState() ==
         content::RenderFrameHost::LifecycleState::kPrerendering;
}

int GetFontSizeFromPx(uint32_t font_size_px) {
  if (font_size_px < 10) {
    return 1;
  } else if (font_size_px < 18) {
    return 2;
  } else {
    return 3;
  }
}

bool IsBoldFont(uint32_t font_weight) {
  return font_weight > 500;
}

struct PathLengthDepthAndHash {
  int64_t path_length;
  int64_t path_depth;
  uint32_t hash;
};

PathLengthDepthAndHash GetUrlPathLengthDepthAndHash(const GURL& target_url) {
  base::StringPiece path = target_url.path_piece();
  int64_t path_length = path.length();
  path_length = ukm::GetLinearBucketMin(path_length, 10);
  // Truncate at 100 characters.
  path_length = std::min(path_length, static_cast<int64_t>(100));

  int64_t num_slashes = base::ranges::count(path, '/');
  // Truncate at 5.
  int64_t path_depth = std::min(num_slashes, static_cast<int64_t>(5));

  // 10-bucket hash of the URL's path.
  uint32_t hash = base::PersistentHash(path);
  hash = hash % 10;
  return {path_length, path_depth, hash};
}

base::TimeDelta MLModelExecutionTimerStartDelay() {
  static int timer_start_delay = base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kPreloadingHeuristicsMLModel, "timer_start_delay", 0);
  return base::Milliseconds(timer_start_delay);
}

base::TimeDelta MLModelExecutionTimerInterval() {
  static int timer_interval = base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kPreloadingHeuristicsMLModel, "timer_interval", 100);
  return base::Milliseconds(timer_interval);
}

bool IsTargetURLTheSameAsDocument(
    const blink::mojom::AnchorElementMetricsPtr& anchor) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  GURL document_url = anchor->source_url.ReplaceComponents(replacements);
  GURL target_url = anchor->target_url.ReplaceComponents(replacements);
  return target_url == document_url;
}

}  // namespace

NavigationPredictor::AnchorElementData::AnchorElementData(
    blink::mojom::AnchorElementMetricsPtr metrics,
    base::TimeTicks first_report_timestamp)
    : metrics(std::move(metrics)),
      first_report_timestamp(first_report_timestamp) {}
NavigationPredictor::AnchorElementData::~AnchorElementData() = default;

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<AnchorElementMetricsHost> receiver)
    : content::DocumentService<blink::mojom::AnchorElementMetricsHost>(
          render_frame_host,
          std::move(receiver)),
      clock_(base::DefaultTickClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // When using content::Page::IsPrimary, bfcache can cause returning a false in
  // the back/forward navigation. So, DCHECK only checks if current page is
  // prerendering until deciding how to handle bfcache navigations. See also
  // https://crbug.com/1239310.
  DCHECK(!IsPrerendering(render_frame_host));

  navigation_start_ = NowTicks();
  ukm_recorder_ = ukm::UkmRecorder::Get();
  ukm_source_id_ = render_frame_host.GetMainFrame()->GetPageUkmSourceId();
}

NavigationPredictor::~NavigationPredictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NavigationPredictor::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementMetricsHost> receiver) {
  CHECK(render_frame_host);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(*render_frame_host));

  // Only valid for the main frame.
  if (render_frame_host->GetParentOrOuterDocument())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  DCHECK(web_contents->GetBrowserContext());
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }

  // The object is bound to the lifetime of the |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new NavigationPredictor(*render_frame_host, std::move(receiver));
}

int NavigationPredictor::GetBucketMinForPageMetrics(int value) const {
  return ukm::GetExponentialBucketMin(value, 1.3);
}

int NavigationPredictor::GetLinearBucketForLinkLocation(int value) const {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 10);
}

int NavigationPredictor::GetLinearBucketForRatioArea(int value) const {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 5);
}

NavigationPredictorMetricsDocumentData&
NavigationPredictor::GetNavigationPredictorMetricsDocumentData() const {
  // Create the `NavigationPredictorMetricsDocumentData` object for this
  // document if it doesn't already exist.
  NavigationPredictorMetricsDocumentData* data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          &render_frame_host());
  DCHECK(data);
  return *data;
}

void NavigationPredictor::ReportNewAnchorElements(
    std::vector<blink::mojom::AnchorElementMetricsPtr> elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  // Create the AnchorsData object for this WebContents if it doesn't already
  // exist. Note that NavigationPredictor only runs on the main frame, but get
  // reports for links from all same-process iframes.
  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      GetNavigationPredictorMetricsDocumentData().GetAnchorsData();
  GURL document_url;
  std::vector<GURL> new_predictions;
  for (auto& element : elements) {
    AnchorId anchor_id(element->anchor_id);
    if (anchors_.find(anchor_id) != anchors_.end()) {
      continue;
    }

    data.number_of_anchors_++;
    if (element->contains_image) {
      data.number_of_anchors_contains_image_++;
    }
    if (element->is_url_incremented_by_one) {
      data.number_of_anchors_url_incremented_++;
    }
    if (element->is_in_iframe) {
      data.number_of_anchors_in_iframe_++;
    }
    if (element->is_same_host) {
      data.number_of_anchors_same_host_++;
    }
    data.viewport_height_ = element->viewport_size.height();
    data.viewport_width_ = element->viewport_size.width();
    data.total_clickable_space_ += element->ratio_area * 100;
    data.link_locations_.push_back(element->ratio_distance_top_to_visible_top);

    // Collect the target URL if it is new, without ref (# fragment).
    GURL::Replacements replacements;
    replacements.ClearRef();
    document_url = element->source_url.ReplaceComponents(replacements);
    GURL target_url = element->target_url.ReplaceComponents(replacements);
    if (target_url != document_url &&
        predicted_urls_.find(target_url) == predicted_urls_.end()) {
      predicted_urls_.insert(target_url);
      new_predictions.push_back(target_url);
    }

    anchors_.emplace(std::piecewise_construct, std::forward_as_tuple(anchor_id),
                     std::forward_as_tuple(std::move(element), NowTicks()));
    tracked_anchor_id_to_index_[anchor_id] = tracked_anchor_id_to_index_.size();
  }

  if (!new_predictions.empty()) {
    NavigationPredictorKeyedService* service =
        NavigationPredictorKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(
                render_frame_host().GetBrowserContext()));
    DCHECK(service);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(&render_frame_host());

    service->OnPredictionUpdated(
        web_contents, document_url,
        NavigationPredictorKeyedService::PredictionSource::
            kAnchorElementsParsedFromWebPage,
        new_predictions);
  }
}

void NavigationPredictor::OnPreloadingHeuristicsModelDone(
    GURL url,
    PreloadingModelKeyedService::Result result) {
  if (!result.has_value()) {
    return;
  }
  render_frame_host().OnPreloadingHeuristicsModelDone(url, result.value());
}

void NavigationPredictor::ProcessPointerEventUsingMLModel(
    blink::mojom::AnchorElementPointerEventForMLModelPtr pointer_event) {
  // Find anchor elements data.
  AnchorId anchor_id(pointer_event->anchor_id);
  auto it = anchors_.find(anchor_id);
  if (it == anchors_.end()) {
    return;
  }

  AnchorElementData& anchor = it->second;
  switch (pointer_event->user_interaction_event_type) {
    case blink::mojom::AnchorElementUserInteractionEventForMLModelType::
        kPointerOut: {
      anchor.pointer_over_timestamp.reset();
      ml_model_candidate_.reset();
      break;
    }
    case blink::mojom::AnchorElementUserInteractionEventForMLModelType::
        kPointerOver: {
      // Currently we only process mouse based events.
      if (!pointer_event->is_mouse) {
        return;
      }
      // Ignore anchors pointing to the same document.
      if (IsTargetURLTheSameAsDocument(anchor.metrics)) {
        return;
      }

      anchor.pointer_over_timestamp = NowTicks();
      anchor.pointer_hovering_over_count++;
      ml_model_candidate_ = anchor_id;
      if (!ml_model_execution_timer_.IsRunning()) {
        ml_model_execution_timer_.Start(
            FROM_HERE, MLModelExecutionTimerStartDelay(),
            base::BindOnce(&NavigationPredictor::OnMLModelExecutionTimerFired,
                           base::Unretained(this)));
      }
      break;
    }
    default:
      break;
  }
}

void NavigationPredictor::OnMLModelExecutionTimerFired() {
  // Check whether preloading is enabled or not.
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  if (prefetch::IsSomePreloadingEnabled(*profile->GetPrefs()) !=
      content::PreloadingEligibility::kEligible) {
    return;
  }

  // Execute the model.
  PreloadingModelKeyedService* model_service =
      PreloadingModelKeyedServiceFactory::GetForProfile(profile);
  if (!model_service) {
    return;
  }

  if (!ml_model_candidate_.has_value()) {
    return;
  }
  auto it = anchors_.find(ml_model_candidate_.value());
  if (it == anchors_.end()) {
    return;
  }

  AnchorElementData& anchor = it->second;

  PreloadingModelKeyedService::Inputs inputs;
  inputs.contains_image = anchor.metrics->contains_image;
  inputs.font_size = GetFontSizeFromPx(anchor.metrics->font_size_px);
  inputs.has_text_sibling = anchor.metrics->has_text_sibling;
  inputs.is_bold = IsBoldFont(anchor.metrics->font_weight);
  inputs.is_in_iframe = anchor.metrics->is_in_iframe;
  inputs.is_url_incremented_by_one = anchor.metrics->is_url_incremented_by_one;
  inputs.navigation_start_to_link_logged =
      anchor.first_report_timestamp - navigation_start_;
  auto path_info = GetUrlPathLengthDepthAndHash(anchor.metrics->target_url);
  inputs.path_length = path_info.path_length;
  inputs.path_depth = path_info.path_depth;
  // Convert the ratio area and ratio distance from [0,1] to [0,100].
  inputs.percent_clickable_area =
      static_cast<int>(anchor.metrics->ratio_area * 100);

  inputs.percent_vertical_distance =
      static_cast<int>(anchor.metrics->ratio_distance_root_top * 100);

  inputs.is_same_origin = anchor.metrics->is_same_host;
  auto to_timedelta = [this](absl::optional<base::TimeTicks> ts) {
    return ts.has_value() ? NowTicks() - ts.value() : base::TimeDelta();
  };
  inputs.entered_viewport_to_left_viewport =
      to_timedelta(anchor.entered_viewport_timestamp);
  inputs.hover_dwell_time = to_timedelta(anchor.pointer_over_timestamp);
  inputs.pointer_hovering_over_count = anchor.pointer_hovering_over_count;
  if (model_score_callback_) {
    std::move(model_score_callback_).Run(inputs);
  }
  model_service->Score(
      &scoring_model_task_tracker_, inputs,
      base::BindOnce(&NavigationPredictor::OnPreloadingHeuristicsModelDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     anchor.metrics->target_url));

  if (!ml_model_execution_timer_.IsRunning()) {
    ml_model_execution_timer_.Start(
        FROM_HERE, MLModelExecutionTimerInterval(),
        base::BindOnce(&NavigationPredictor::OnMLModelExecutionTimerFired,
                       base::Unretained(this)));
  }
}

void NavigationPredictor::SetModelScoreCallbackForTesting(
    ModelScoreCallbackForTesting callback) {
  model_score_callback_ = std::move(callback);
}

void NavigationPredictor::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock) {
  ml_model_execution_timer_.SetTaskRunner(task_runner);
  clock_ = clock;
  navigation_start_ = NowTicks();
}

void NavigationPredictor::ReportAnchorElementClick(
    blink::mojom::AnchorElementClickPtr click) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  navigation_start_to_click_ = click->navigation_start_to_click;

  clicked_count_++;
  if (clicked_count_ > kMaxClicksTracked)
    return;

  if (!ukm_recorder_) {
    return;
  }

  auto& navigation_predictor_metrics_data =
      GetNavigationPredictorMetricsDocumentData();
  // An anchor index of -1 indicates that we are not going to log details about
  // the anchor that was clicked.
  int anchor_index = -1;
  AnchorId anchor_id(click->anchor_id);
  auto index_it = tracked_anchor_id_to_index_.find(anchor_id);
  if (index_it != tracked_anchor_id_to_index_.end()) {
    anchor_index = index_it->second;

    // Record PreloadOnHover.HoverTakenMs and PreloadOnHover.PointerDownTakenMs
    // to UKM. We should make sure that we only process the `sampled` anchor
    // elements here, as `AnchorElementMetricsSender` reports all new anchor
    // elements to `NavigationPredictor`, but only reports user interactions
    // events for the  `sampled` anchors. Otherwise, we will end up creating
    // empty `UserInteractionsData` UKM records.
    auto& user_interactions =
        navigation_predictor_metrics_data.GetUserInteractionsData();
    auto user_interaction_it = user_interactions.find(index_it->second);
    if (user_interaction_it != user_interactions.end()) {
      auto& user_interaction = user_interactions[index_it->second];
      // navigation_start_to_click_ is set to click->navigation_start_to_click
      // and should always have a value.
      CHECK(navigation_start_to_click_.has_value());
      if (user_interaction.last_navigation_start_to_pointer_over.has_value() ||
          user_interaction.last_navigation_start_to_last_pointer_down
              .has_value()) {
        NavigationPredictorMetricsDocumentData::PreloadOnHoverData
            preload_on_hover;
        preload_on_hover.taken = true;
        if (user_interaction.last_navigation_start_to_pointer_over
                .has_value()) {
          // `hover_dwell_time` measures the time delta from the last mouse over
          // event to the last mouse click event.
          preload_on_hover.hover_dwell_time =
              navigation_start_to_click_.value() -
              user_interaction.last_navigation_start_to_pointer_over.value();
        }
        if (user_interaction.last_navigation_start_to_last_pointer_down
                .has_value()) {
          // `pointer_down_duration` measures the time delta from the last mouse
          // down event to the last mouse click event.
          preload_on_hover.pointer_down_duration =
              navigation_start_to_click_.value() -
              user_interaction.last_navigation_start_to_last_pointer_down
                  .value();
          user_interaction.last_navigation_start_to_last_pointer_down.reset();
        }
        navigation_predictor_metrics_data.AddPreloadOnHoverData(
            std::move(preload_on_hover));
      }
    }
  }

  NavigationPredictorMetricsDocumentData::PageLinkClickData page_link_click;
  page_link_click.anchor_element_index_ = anchor_index;
  auto it = anchors_.find(anchor_id);
  if (it != anchors_.end()) {
    page_link_click.href_unchanged_ =
        (it->second.metrics->target_url == click->target_url);
  }
  navigation_start_to_click_ = click->navigation_start_to_click;
  // navigation_start_to_click_ is set to click->navigation_start_to_click and
  // should always have a value.
  CHECK(navigation_start_to_click_.has_value());

  navigation_predictor_metrics_data.SetNavigationStartToClick(
      navigation_start_to_click_.value());

  page_link_click.navigation_start_to_link_clicked_ =
      navigation_start_to_click_.value();
  navigation_predictor_metrics_data.AddPageLinkClickData(
      std::move(page_link_click));
}

void NavigationPredictor::ReportAnchorElementsLeftViewport(
    std::vector<blink::mojom::AnchorElementLeftViewportPtr> elements) {
  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  for (const auto& element : elements) {
    auto index_it =
        tracked_anchor_id_to_index_.find(AnchorId(element->anchor_id));
    if (index_it == tracked_anchor_id_to_index_.end()) {
      continue;
    }
    auto& user_interaction = user_interactions[index_it->second];
    user_interaction.is_in_viewport = false;
    user_interaction.last_navigation_start_to_entered_viewport.reset();
    user_interaction.max_time_in_viewport = std::max(
        user_interaction.max_time_in_viewport.value_or(base::TimeDelta()),
        element->time_in_viewport);
  }
}

void NavigationPredictor::ReportAnchorElementPointerDataOnHoverTimerFired(
    blink::mojom::AnchorElementPointerDataOnHoverTimerFiredPtr msg) {
  if (!msg->pointer_data || !msg->pointer_data->is_mouse_pointer) {
    return;
  }

  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  auto index_it = tracked_anchor_id_to_index_.find(AnchorId(msg->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interaction = user_interactions[index_it->second];
  user_interaction.mouse_velocity = msg->pointer_data->mouse_velocity;
  user_interaction.mouse_acceleration = msg->pointer_data->mouse_acceleration;
}

void NavigationPredictor::ReportAnchorElementPointerOver(
    blink::mojom::AnchorElementPointerOverPtr pointer_over_event) {
  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  auto index_it =
      tracked_anchor_id_to_index_.find(AnchorId(pointer_over_event->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interaction = user_interactions[index_it->second];
  if (!user_interaction.is_hovered) {
    user_interaction.pointer_hovering_over_count++;
  }
  user_interaction.is_hovered = true;
  user_interaction.last_navigation_start_to_pointer_over =
      pointer_over_event->navigation_start_to_pointer_over;
}

void NavigationPredictor::ReportAnchorElementPointerOut(
    blink::mojom::AnchorElementPointerOutPtr hover_event) {
  auto& navigation_predictor_metrics_data =
      GetNavigationPredictorMetricsDocumentData();
  auto& user_interactions =
      navigation_predictor_metrics_data.GetUserInteractionsData();
  auto index_it =
      tracked_anchor_id_to_index_.find(AnchorId(hover_event->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interaction = user_interactions[index_it->second];
  // Record PreloadOnHover.HoverNotTakenMs and
  // PreloadOnHover.MouseDownNotTakenMs to UKM.
  NavigationPredictorMetricsDocumentData::PreloadOnHoverData preload_on_hover;
  preload_on_hover.taken = false;
  preload_on_hover.hover_dwell_time = hover_event->hover_dwell_time;
  if (user_interaction.last_navigation_start_to_last_pointer_down.has_value() &&
      user_interaction.last_navigation_start_to_pointer_over.has_value()) {
    preload_on_hover.pointer_down_duration =
        user_interaction.last_navigation_start_to_pointer_over.value() +
        hover_event->hover_dwell_time -
        user_interaction.last_navigation_start_to_last_pointer_down.value();
    user_interaction.last_navigation_start_to_last_pointer_down.reset();
  }
  navigation_predictor_metrics_data.AddPreloadOnHoverData(
      std::move(preload_on_hover));

  // Update user interactions.
  user_interaction.is_hovered = false;
  user_interaction.last_navigation_start_to_pointer_over.reset();
  user_interaction.max_hover_dwell_time = std::max(
      hover_event->hover_dwell_time,
      user_interaction.max_hover_dwell_time.value_or(base::TimeDelta()));
}

void NavigationPredictor::ReportAnchorElementPointerDown(
    blink::mojom::AnchorElementPointerDownPtr pointer_down_event) {
  auto index_it =
      tracked_anchor_id_to_index_.find(AnchorId(pointer_down_event->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  auto& user_interaction = user_interactions[index_it->second];
  user_interaction.last_navigation_start_to_last_pointer_down =
      pointer_down_event->navigation_start_to_pointer_down;
}

void NavigationPredictor::ReportAnchorElementsEnteredViewport(
    std::vector<blink::mojom::AnchorElementEnteredViewportPtr> elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  if (elements.empty()) {
    return;
  }
  auto& navigation_predictor_metrics_data =
      GetNavigationPredictorMetricsDocumentData();
  auto& user_interactions =
      navigation_predictor_metrics_data.GetUserInteractionsData();
  for (const auto& element : elements) {
    AnchorId anchor_id(element->anchor_id);
    auto index_it = tracked_anchor_id_to_index_.find(anchor_id);
    if (index_it == tracked_anchor_id_to_index_.end()) {
      // We're not tracking this element, no need to generate a
      // NavigationPredictorAnchorElementMetrics record.
      continue;
    }
    auto& user_interaction = user_interactions[index_it->second];
    if (!user_interaction.is_in_viewport) {
      user_interaction.entered_viewport_count++;
    }
    user_interaction.is_in_viewport = true;
    user_interaction.last_navigation_start_to_entered_viewport =
        element->navigation_start_to_entered_viewport;

    auto anchor_it = anchors_.find(anchor_id);
    if (anchor_it == anchors_.end()) {
      // We don't know about this anchor, likely because at its first paint,
      // AnchorElementMetricsSender didn't send it to NavigationPredictor.
      // Reasons could be that the link had non-HTTP scheme, the anchor had
      // zero width/height, etc.
      continue;
    }
    const auto& anchor = anchor_it->second.metrics;
    // Collect the target URL if it is new, without ref (# fragment).
    if (IsTargetURLTheSameAsDocument(anchor)) {
      // Ignore anchors pointing to the same document.
      continue;
    }

    if (!ukm_recorder_) {
      continue;
    }

    NavigationPredictorMetricsDocumentData::AnchorElementMetricsData metrics;

    metrics.is_in_iframe_ = anchor->is_in_iframe;
    metrics.is_url_incremented_by_one_ = anchor->is_url_incremented_by_one;
    metrics.contains_image_ = anchor->contains_image;
    metrics.is_same_origin_ = anchor->is_same_host;
    metrics.has_text_sibling_ = anchor->has_text_sibling;
    metrics.is_bold_ = IsBoldFont(anchor->font_weight);
    metrics.navigation_start_to_link_logged =
        element->navigation_start_to_entered_viewport;

    metrics.font_size_ = GetFontSizeFromPx(anchor->font_size_px);
    auto path_info = GetUrlPathLengthDepthAndHash(anchor->target_url);
    metrics.path_length_ = path_info.path_length;
    metrics.path_depth_ = path_info.path_depth;
    metrics.bucketed_path_hash_ = path_info.hash;

    // Convert the ratio area and ratio distance from [0,1] to [0,100].
    int percent_ratio_area = static_cast<int>(anchor->ratio_area * 100);
    metrics.percent_clickable_area_ =
        GetLinearBucketForRatioArea(percent_ratio_area);

    int percent_ratio_distance_root_top =
        static_cast<int>(anchor->ratio_distance_root_top * 100);
    metrics.percent_vertical_distance_ =
        GetLinearBucketForLinkLocation(percent_ratio_distance_root_top);

    navigation_predictor_metrics_data.AddAnchorElementMetricsData(
        index_it->second, std::move(metrics));
  }
}
