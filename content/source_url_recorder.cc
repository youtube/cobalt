// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/content/source_url_recorder.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/platform/ukm.mojom.h"
#include "url/gurl.h"

namespace ukm {

namespace internal {

int64_t CreateUniqueTabId() {
  static int64_t unique_id_counter = 0;
  return ++unique_id_counter;
}

// SourceUrlRecorderWebContentsObserver is responsible for recording UKM source
// URLs, for all (any only) main frame navigations in a given WebContents.
// SourceUrlRecorderWebContentsObserver records both the final URL for a
// navigation, and, if the navigation was redirected, the initial URL as well.
class SourceUrlRecorderWebContentsObserver
    : public blink::mojom::UkmSourceIdFrameHost,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          SourceUrlRecorderWebContentsObserver> {
 public:
  // Creates a SourceUrlRecorderWebContentsObserver for the given
  // WebContents. If a SourceUrlRecorderWebContentsObserver is already
  // associated with the WebContents, this method is a no-op.
  static void CreateForWebContents(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  ukm::SourceId GetLastCommittedSourceId() const;

  // blink::mojom::UkmSourceIdFrameHost
  void SetDocumentSourceId(int64_t source_id) override;

 private:
  explicit SourceUrlRecorderWebContentsObserver(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      SourceUrlRecorderWebContentsObserver>;

  // Record any pending DocumentCreated events to UKM.
  void MaybeFlushPendingEvents();

  void MaybeRecordUrl(content::NavigationHandle* navigation_handle,
                      const GURL& initial_url);

  // Recieves document source IDs from the renderer.
  content::WebContentsFrameBindingSet<blink::mojom::UkmSourceIdFrameHost>
      bindings_;

  // Map from navigation ID to the initial URL for that navigation.
  base::flat_map<int64_t, GURL> pending_navigations_;

  // Holds pending DocumentCreated events.
  struct PendingEvent {
    PendingEvent() = delete;
    PendingEvent(int64_t source_id,
                 bool is_main_frame,
                 bool is_cross_origin_frame)
        : source_id(source_id),
          is_main_frame(is_main_frame),
          is_cross_origin_frame(is_cross_origin_frame) {}

    int64_t source_id;
    bool is_main_frame;
    bool is_cross_origin_frame;
  };
  std::vector<PendingEvent> pending_document_created_events_;

  SourceId last_committed_source_id_;

  // The source id before |last_committed_source_id_|.
  SourceId previous_committed_source_id_;

  // The source id of the last committed source in the tab that opened this tab.
  // Will be set to kInvalidSourceId after the first navigation in this tab is
  // finished.
  SourceId opener_source_id_;

  const int64_t tab_id_;

  DISALLOW_COPY_AND_ASSIGN(SourceUrlRecorderWebContentsObserver);
};

SourceUrlRecorderWebContentsObserver::SourceUrlRecorderWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      bindings_(web_contents, this),
      last_committed_source_id_(ukm::kInvalidSourceId),
      previous_committed_source_id_(ukm::kInvalidSourceId),
      opener_source_id_(ukm::kInvalidSourceId),
      tab_id_(CreateUniqueTabId()) {}

void SourceUrlRecorderWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // UKM only records URLs for main frame (web page) navigations, so ignore
  // non-main frame navs. Additionally, at least for the time being, we don't
  // track metrics for same-document navigations (e.g. changes in URL fragment,
  // or URL changes due to history.pushState) in UKM.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // UKM doesn't want to record URLs for downloads. However, at the point a
  // navigation is started, we don't yet know if the navigation will result in a
  // download. Thus, we record the URL at the time a navigation was initiated,
  // and only record it later, once we verify that the navigation didn't result
  // in a download.
  pending_navigations_.insert(std::make_pair(
      navigation_handle->GetNavigationId(), navigation_handle->GetURL()));

  // Clear any unassociated pending events.
  pending_document_created_events_.clear();
}

void SourceUrlRecorderWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  auto it = pending_navigations_.find(navigation_handle->GetNavigationId());
  if (it == pending_navigations_.end())
    return;

  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->IsSameDocument());

  if (navigation_handle->HasCommitted()) {
    previous_committed_source_id_ = last_committed_source_id_;
    last_committed_source_id_ = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  }

  GURL initial_url = std::move(it->second);
  pending_navigations_.erase(it);

  // UKM doesn't want to record URLs for navigations that result in downloads.
  if (navigation_handle->IsDownload())
    return;

  MaybeRecordUrl(navigation_handle, initial_url);

  MaybeFlushPendingEvents();

  // Reset the opener source id. Only the first source in a tab should have an
  // opener.
  opener_source_id_ = kInvalidSourceId;
}

void SourceUrlRecorderWebContentsObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  auto* new_recorder =
      SourceUrlRecorderWebContentsObserver::FromWebContents(new_contents);
  if (!new_recorder)
    return;
  new_recorder->opener_source_id_ = GetLastCommittedSourceId();
}

ukm::SourceId SourceUrlRecorderWebContentsObserver::GetLastCommittedSourceId()
    const {
  return last_committed_source_id_;
}

void SourceUrlRecorderWebContentsObserver::SetDocumentSourceId(
    int64_t source_id) {
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  content::RenderFrameHost* current_frame = bindings_.GetCurrentTargetFrame();
  bool is_main_frame = main_frame == current_frame;
  bool is_cross_origin_frame =
      is_main_frame ? false
                    : !main_frame->GetLastCommittedOrigin().IsSameOriginWith(
                          current_frame->GetLastCommittedOrigin());

  pending_document_created_events_.emplace_back(
      source_id, !bindings_.GetCurrentTargetFrame()->GetParent(),
      is_cross_origin_frame);
  MaybeFlushPendingEvents();
}

void SourceUrlRecorderWebContentsObserver::MaybeFlushPendingEvents() {
  if (!last_committed_source_id_)
    return;

  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (!ukm_recorder)
    return;

  while (!pending_document_created_events_.empty()) {
    auto record = pending_document_created_events_.back();

    ukm::builders::DocumentCreated(record.source_id)
        .SetNavigationSourceId(last_committed_source_id_)
        .SetIsMainFrame(record.is_main_frame)
        .SetIsCrossOriginFrame(record.is_cross_origin_frame)
        .Record(ukm_recorder);

    pending_document_created_events_.pop_back();
  }
}

void SourceUrlRecorderWebContentsObserver::MaybeRecordUrl(
    content::NavigationHandle* navigation_handle,
    const GURL& initial_url) {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->IsSameDocument());

  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (!ukm_recorder)
    return;

  UkmSource::NavigationData navigation_data;
  const GURL& final_url = navigation_handle->GetURL();
  // TODO(crbug.com/869123): This check isn't quite correct, as self redirecting
  // is possible. This may also be changed to include the entire redirect chain.
  if (final_url != initial_url)
    navigation_data.urls = {initial_url};
  navigation_data.urls.push_back(final_url);

  // Careful note: the current navigation may have failed.
  navigation_data.previous_source_id = navigation_handle->HasCommitted()
                                           ? previous_committed_source_id_
                                           : last_committed_source_id_;
  navigation_data.opener_source_id = opener_source_id_;
  navigation_data.tab_id = tab_id_;

  const ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder->RecordNavigation(source_id, navigation_data);
}

// static
void SourceUrlRecorderWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  if (!SourceUrlRecorderWebContentsObserver::FromWebContents(web_contents)) {
    web_contents->SetUserData(
        SourceUrlRecorderWebContentsObserver::UserDataKey(),
        base::WrapUnique(
            new SourceUrlRecorderWebContentsObserver(web_contents)));
  }
}

}  // namespace internal

void InitializeSourceUrlRecorderForWebContents(
    content::WebContents* web_contents) {
  internal::SourceUrlRecorderWebContentsObserver::CreateForWebContents(
      web_contents);
}

SourceId GetSourceIdForWebContentsDocument(
    const content::WebContents* web_contents) {
  const internal::SourceUrlRecorderWebContentsObserver* obs =
      internal::SourceUrlRecorderWebContentsObserver::FromWebContents(
          web_contents);
  return obs ? obs->GetLastCommittedSourceId() : kInvalidSourceId;
}

}  // namespace ukm
