// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_annotation_manager.h"

#include <optional>

#include "base/callback_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/state_transitions.h"
#include "base/strings/escape.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/prefs/pref_service.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace glic {

namespace {
void RunScrollToCallback(mojom::WebClientHandler::ScrollToCallback callback,
                         std::optional<mojom::ScrollToErrorReason> error) {
  if (error) {
    base::UmaHistogramEnumeration("Glic.ScrollTo.ErrorReason", *error);
  }
  std::move(callback).Run(error);
}

std::string AttachmentResultToString(blink::mojom::AttachmentResult result) {
  std::string string;
  switch (result) {
    case blink::mojom::AttachmentResult::kSuccess:
      string = "Success";
      break;
    case blink::mojom::AttachmentResult::kSelectorNotMatched:
      string = "SelectorNotMatched";
      break;
    case blink::mojom::AttachmentResult::kRangeInvalid:
      string = "RangeInvalid";
      break;
  }
  return string;
}
}  // namespace

GlicAnnotationManager::GlicAnnotationManager(GlicKeyedService* service)
    : service_(service) {}

GlicAnnotationManager::~GlicAnnotationManager() = default;

void GlicAnnotationManager::ScrollTo(
    mojom::ScrollToParamsPtr params,
    mojom::WebClientHandler::ScrollToCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  if (annotation_task_ && annotation_task_->IsRunning()) {
    annotation_task_->FailTaskOrDropAnnotation(
        mojom::ScrollToErrorReason::kNewerScrollToCall);
  }
  annotation_task_.reset();

  service_->metrics()->OnGlicScrollAttempt();

  mojom::WebClientHandler::ScrollToCallback wrapped_callback =
      base::BindOnce(&RunScrollToCallback, std::move(callback));
  mojom::ScrollToSelector* selector = params->selector.get();
  std::optional<shared_highlighting::TextFragment> text_fragment;
  std::optional<int> search_range_start_node_id = std::nullopt;
  std::optional<int> node_id = std::nullopt;

  if (selector->is_exact_text_selector()) {
    auto* exact_text_selector = selector->get_exact_text_selector().get();
    const std::string& exact_text = exact_text_selector->text;
    if (exact_text.empty()) {
      std::move(wrapped_callback)
          .Run(mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    if (exact_text_selector->search_range_start_node_id.has_value()) {
      if (!params->document_id) {
        mojo::ReportBadMessage(
            "When the range_start_node_id is set, the document_id should be "
            "set as well.");
        return;
      }
      search_range_start_node_id =
          exact_text_selector->search_range_start_node_id;
    }
    text_fragment = shared_highlighting::TextFragment(exact_text);
  } else if (selector->is_text_fragment_selector()) {
    auto* text_fragment_selector = selector->get_text_fragment_selector().get();
    const std::string& text_start = text_fragment_selector->text_start;
    if (text_start.empty()) {
      std::move(wrapped_callback)
          .Run(mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    const std::string& text_end = text_fragment_selector->text_end;
    if (text_end.empty()) {
      std::move(wrapped_callback)
          .Run(mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    if (text_fragment_selector->search_range_start_node_id.has_value()) {
      if (!params->document_id) {
        mojo::ReportBadMessage(
            "When the range_start_node_id is set, the document_id should be "
            "set as well.");
        return;
      }
      search_range_start_node_id =
          text_fragment_selector->search_range_start_node_id;
    }
    text_fragment = shared_highlighting::TextFragment(text_start, text_end,
                                                      /*prefix=*/std::string(),
                                                      /*suffix=*/std::string());
  } else if (selector->is_node_selector()) {
    if (!params->document_id) {
      mojo::ReportBadMessage(
          "When node_id is set, document_id should be set as well.");
      return;
    }
    node_id = selector->get_node_selector()->node_id;
  } else {
    mojo::ReportBadMessage(
        "The client should have verified that one of the selector types was "
        "specified.");
    return;
  }

  // "exact_text" and "text_fragment" selectors will set `text_fragment`, "node"
  // selector will set `node_id`.
  CHECK(text_fragment.has_value() || node_id.has_value());

  if (!service_->profile()->GetPrefs()->GetBoolean(
          prefs::kGlicTabContextEnabled)) {
    std::move(wrapped_callback)
        .Run(mojom::ScrollToErrorReason::kTabContextPermissionDisabled);
    return;
  }

  auto focused_tab_data = service_->GetFocusedTabData();
  content::Page* focused_primary_page = nullptr;
  if (focused_tab_data.focus()) {
    focused_primary_page = &focused_tab_data.focus()->GetPrimaryPage();
  }
  if (!focused_primary_page) {
    std::move(wrapped_callback).Run(mojom::ScrollToErrorReason::kNoFocusedTab);
    return;
  }

  // Note: `GlicWindowController::IsShowing()` will be false and
  // `focused_primary_page` will be non-null when `GlicWindowController` is
  // running the close animation.
  if (!service_->window_controller().IsShowing()) {
    std::move(wrapped_callback).Run(mojom::ScrollToErrorReason::kNoFocusedTab);
    return;
  }

  if (annotation_agent_container_.has_value() &&
      annotation_agent_container_->document.AsRenderFrameHostIfValid() !=
          &focused_primary_page->GetMainDocument()) {
    annotation_agent_container_ = std::nullopt;
  }

  if (!annotation_agent_container_.has_value()) {
    annotation_agent_container_.emplace();
    annotation_agent_container_->document =
        focused_primary_page->GetMainDocument().GetWeakDocumentPtr();
    focused_primary_page->GetMainDocument().GetRemoteInterfaces()->GetInterface(
        annotation_agent_container_->remote.BindNewPipeAndPassReceiver());
  }

  // The caller currently only enforces if the documentId is set when DOMNodeId
  // selector parameters are set. If this is configured to be true, we will
  // always check that the documentId is set, and fail otherwise.
  const bool fail_without_document_id =
      features::kGlicScrollToEnforceDocumentId.Get();
  if (fail_without_document_id && !params->document_id) {
    std::move(wrapped_callback).Run(mojom::ScrollToErrorReason::kNotSupported);
    return;
  }

  // Verifies that the document_id parameter (if set) refers to the primary
  // document in the currently focused tab.
  if (params->document_id) {
    // We only support scrolling the currently focused tab's main frame.
    content::RenderFrameHost& rfh = focused_primary_page->GetMainDocument();
    auto* document_identifier_user_data =
        optimization_guide::DocumentIdentifierUserData::GetForCurrentDocument(
            &rfh);
    if (!document_identifier_user_data ||
        document_identifier_user_data->serialized_token() !=
            params->document_id) {
      std::move(wrapped_callback)
          .Run(mojom::ScrollToErrorReason::kNoMatchingDocument);
      return;
    }
  }

  blink::mojom::SelectorPtr blink_mojom_selector;
  if (text_fragment) {
    blink_mojom_selector = blink::mojom::Selector::NewSerializedSelector(
        text_fragment->ToEscapedString(
            shared_highlighting::TextFragment::EscapedStringFormat::
                kWithoutTextDirective));
  } else {
    blink_mojom_selector = blink::mojom::Selector::NewNodeId(node_id.value());
  }

  mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> agent_host_receiver;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_remote;
  annotation_agent_container_->remote->CreateAgent(
      agent_host_receiver.InitWithNewPipeAndPassRemote(),
      agent_remote.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kGlic, std::move(blink_mojom_selector),
      search_range_start_node_id);
  annotation_task_ = std::make_unique<AnnotationTask>(
      this, std::move(agent_remote), std::move(agent_host_receiver),
      std::move(wrapped_callback), *focused_primary_page);
}

void GlicAnnotationManager::RemoveAnnotation(
    mojom::ScrollToErrorReason reason) {
  if (annotation_task_) {
    annotation_task_->FailTaskOrDropAnnotation(reason);
  }
}

GlicAnnotationManager::AnnotationTask::AnnotationTask(
    GlicAnnotationManager* annotation_manager,
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
        agent_host_pending_receiver,
    mojom::WebClientHandler::ScrollToCallback callback,
    content::Page& page)
    : annotation_manager_(*annotation_manager),
      annotation_agent_(std::move(agent_remote)),
      annotation_agent_host_receiver_(this,
                                      std::move(agent_host_pending_receiver)),
      scroll_to_callback_(std::move(callback)),
      page_(page.GetWeakPtr()),
      start_time_(base::TimeTicks::Now()) {
  GlicKeyedService* service = annotation_manager_->service_;
  CHECK(service);
  // Using base::Unretained is safe here because `this` owns the subscription.
  tab_change_subscription_ =
      service->AddFocusedTabChangedCallback(base::BindRepeating(
          &AnnotationTask::OnFocusedTabChanged, base::Unretained(this)));

  // Using base::Unretained is safe because `this` owns the receiver.
  annotation_agent_host_receiver_.set_disconnect_handler(base::BindOnce(
      &AnnotationTask::RemoteDisconnected, base::Unretained(this)));

  // Listens to the panel-closing notification.
  service->window_controller().AddStateObserver(this);

  pref_change_registrar_.Init(service->profile()->GetPrefs());
  // base::Unretained is safe because `this` owns `pref_change_registrar_`.
  pref_change_registrar_.Add(
      prefs::kGlicTabContextEnabled,
      base::BindRepeating(&AnnotationTask::OnTabContextPermissionChanged,
                          base::Unretained(this)));
}

GlicAnnotationManager::AnnotationTask::~AnnotationTask() {
  CHECK_EQ(IsRunning(), !scroll_to_callback_.is_null());
  if (IsRunning()) {
    std::move(scroll_to_callback_)
        .Run(mojom::ScrollToErrorReason::kNotSupported);
  }
  annotation_manager_->service_->window_controller().RemoveStateObserver(this);
}

bool GlicAnnotationManager::AnnotationTask::IsRunning() const {
  return state_ == State::kRunning;
}

void GlicAnnotationManager::AnnotationTask::FailTaskOrDropAnnotation(
    mojom::ScrollToErrorReason reason) {
  switch (state_) {
    case State::kRunning: {
      FailTask(reason);
      break;
    }
    case State::kActive: {
      DropAnnotation();
      break;
    }
    case State::kFailed:
    case State::kInactive: {
      break;
    }
  }
}

std::string GlicAnnotationManager::AnnotationTask::ToString(State state) {
  switch (state) {
    case State::kRunning:
      return "Running";
    case State::kFailed:
      return "Failed";
    case State::kActive:
      return "Active";
    case State::kInactive:
      return "Inactive";
  }
}

void GlicAnnotationManager::AnnotationTask::SetState(State new_state) {
  State old_state = state_;
  static const base::NoDestructor<base::StateTransitions<State>>
      allowed_transitions(base::StateTransitions<State>(
          {{State::kRunning, {State::kFailed, State::kActive}},
           {State::kFailed, {}},
           {State::kActive, {State::kInactive}},
           {State::kInactive, {}}}));
  CHECK_STATE_TRANSITION(allowed_transitions, old_state, new_state);
  state_ = new_state;

  switch (new_state) {
    case State::kActive:
    case State::kFailed:
      annotation_manager_->service_->metrics()->OnGlicScrollComplete(
          new_state == State::kActive);
      break;
    case State::kRunning:
    case State::kInactive:
      break;
  }
}

void GlicAnnotationManager::AnnotationTask::RemoteDisconnected() {
  switch (state_) {
    case State::kRunning:
      FailTask(mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated);
      return;
    case State::kActive:
      DropAnnotation();
      return;
    case State::kFailed:
    case State::kInactive:
      NOTREACHED();
  }
}

void GlicAnnotationManager::AnnotationTask::DropAnnotation() {
  SetState(State::kInactive);
  ResetConnections();
}

void GlicAnnotationManager::AnnotationTask::ResetConnections() {
  annotation_agent_.reset();
  annotation_agent_host_receiver_.reset();
  tab_change_subscription_ = base::CallbackListSubscription();
  content::WebContentsObserver::Observe(nullptr);
  annotation_manager_->service_->window_controller().RemoveStateObserver(this);
  pref_change_registrar_.Reset();
}

void GlicAnnotationManager::AnnotationTask::FailTask(
    mojom::ScrollToErrorReason error_reason) {
  SetState(State::kFailed);
  std::move(scroll_to_callback_).Run(error_reason);
  ResetConnections();
}

void GlicAnnotationManager::AnnotationTask::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {
  CHECK_EQ(state_, State::kRunning);

  // At this point, we're relying on `OnFocusedTabChanged()` to observe `page_`
  // being navigated from. But that notification can be delayed, so we could be
  // in a situation where page_ is nullptr, but we haven't received a
  // notification yet. We fail the task in that situation.
  if (!page_) {
    FailTask(mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated);
    return;
  }

  switch (attachment_result) {
    case blink::mojom::AttachmentResult::kSelectorNotMatched:
      FailTask(mojom::ScrollToErrorReason::kNoMatchFound);
      break;
    case blink::mojom::AttachmentResult::kRangeInvalid:
      FailTask(mojom::ScrollToErrorReason::kSearchRangeInvalid);
      break;
    case blink::mojom::AttachmentResult::kSuccess:
      SetState(State::kActive);
      annotation_agent_->ScrollIntoView(/*applies_focus=*/true);
      std::move(scroll_to_callback_).Run(std::nullopt);

      // After attachment, we only want to dismiss the active highlight when
      // the `page_`'s tab changes its primary page (i.e. navigates away from
      // `page_`), and not if the currently focused tab changes. We cannot rely
      // on FocusedTabManager to observe this, and observe
      // `WebContentsObserver::PrimaryPageChanged` from this point instead.
      tab_change_subscription_ = base::CallbackListSubscription();
      content::WebContentsObserver::Observe(
          content::WebContents::FromRenderFrameHost(&page_->GetMainDocument()));
      break;
  }
  base::UmaHistogramTimes(
      base::StringPrintf("Glic.ScrollTo.MatchDuration.%s",
                         AttachmentResultToString(attachment_result)),
      base::TimeTicks::Now() - start_time_);
}

// Note: We explicitly listen for `PrimaryPageChanged` to drop the highlight
// after the page gets put into the BackForwardCache.
void GlicAnnotationManager::AnnotationTask::PrimaryPageChanged(
    content::Page& page) {
  DropAnnotation();
}

// Remove the annotation when the panel is closed. When the web client is closed
// the `GlicAnnotationManager` is destroyed, removing all the annotation tasks
// as well.
void GlicAnnotationManager::AnnotationTask::PanelStateChanged(
    const mojom::PanelState& panel_state,
    Browser* attached_browser) {
  if (panel_state.kind != mojom::PanelState_Kind::kHidden) {
    return;
  }
  FailTaskOrDropAnnotation(
      mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated);
}

void GlicAnnotationManager::AnnotationTask::OnTabContextPermissionChanged(
    const std::string& pref_name) {
  CHECK_EQ(pref_name, prefs::kGlicTabContextEnabled);
  if (!annotation_manager_->service_->profile()->GetPrefs()->GetBoolean(
          prefs::kGlicTabContextEnabled)) {
    FailTaskOrDropAnnotation(
        mojom::ScrollToErrorReason::kTabContextPermissionDisabled);
  }
}

// Note: In addition to when the focused tab changes, this gets called when
// the currently focused tab navigates its primary page (i.e.
// PrimaryPageChanged). We also want to perform these steps in that scenario.
void GlicAnnotationManager::AnnotationTask::OnFocusedTabChanged(
    FocusedTabData focused_tab_data) {
  CHECK_EQ(state_, State::kRunning);
  // We've navigated away from the page this (in-progress) task was supposed to
  // run in, so we fail the task.
  if (!page_.get()) {
    FailTask(mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated);
    return;
  }

  content::Page* new_focused_primary_page = nullptr;
  if (focused_tab_data.focus()) {
    new_focused_primary_page = &focused_tab_data.focus()->GetPrimaryPage();
  }
  // If the focused tab hasn't changed and its primary page hasn't changed, we
  // don't need to do anything.
  if (page_.get() == new_focused_primary_page) {
    return;
  }
  FailTask(mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated);
}

GlicAnnotationManager::AnnotationAgentContainer::AnnotationAgentContainer() =
    default;

GlicAnnotationManager::AnnotationAgentContainer::~AnnotationAgentContainer() =
    default;

}  // namespace glic
