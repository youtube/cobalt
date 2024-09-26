// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is used by the RenderView to interact with a PhishingClassifier.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/scoped_observation.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace safe_browsing {
class ClientPhishingRequest;
class PhishingClassifier;
class Scorer;

enum class SBPhishingClassifierEvent {
  kPhishingDetectionRequested = 0,
  kPageTextCaptured = 1,
  // Phishing detection could not start because the page text was not loaded.
  kPageTextNotLoaded = 2,
  // Phishing detection could not start because the url was not specified to be
  // classified.
  kUrlShouldNotBeClassified = 3,
  // Phishing detection could not finish because the class was destructed.
  kDestructedBeforeClassificationDone = 4,
  kMaxValue = kDestructedBeforeClassificationDone,
};

class PhishingClassifierDelegate : public content::RenderFrameObserver,
                                   public mojom::PhishingDetector,
                                   public ScorerStorage::Observer {
 public:
  // The RenderFrame owns us.  This object takes ownership of the classifier.
  // Note that if classifier is null, a default instance of PhishingClassifier
  // will be used.
  static PhishingClassifierDelegate* Create(content::RenderFrame* render_frame,
                                            PhishingClassifier* classifier);

  PhishingClassifierDelegate(const PhishingClassifierDelegate&) = delete;
  PhishingClassifierDelegate& operator=(const PhishingClassifierDelegate&) =
      delete;

  ~PhishingClassifierDelegate() override;

  // Called by the RenderFrame once a page has finished loading.  Updates the
  // last-loaded URL and page text, then starts classification if all other
  // conditions are met (see MaybeStartClassification for details).
  // We ignore preliminary captures, since these happen before the page has
  // finished loading.
  void PageCaptured(std::u16string* page_text, bool preliminary_capture);

  // RenderFrameObserver implementation, public for testing.

  // Called by the RenderFrame when a page has started loading in the given
  // WebFrame.  Typically, this will cause any pending classification to be
  // cancelled.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  // Called by the RenderFrame when the same-document navigation has been
  // committed. We continue running the current classification.
  void DidFinishSameDocumentNavigation() override;

  bool is_ready();

 private:
  friend class PhishingClassifierDelegateTest;

  PhishingClassifierDelegate(content::RenderFrame* render_frame,
                             PhishingClassifier* classifier);

  enum CancelClassificationReason {
    NAVIGATE_AWAY,
    NAVIGATE_WITHIN_PAGE,
    PAGE_RECAPTURED,
    SHUTDOWN,
    NEW_PHISHING_SCORER,
    CANCEL_CLASSIFICATION_MAX  // Always add new values before this one.
  };

  void PhishingDetectorReceiver(
      mojo::PendingAssociatedReceiver<mojom::PhishingDetector> receiver);

  // Cancels any pending classification and frees the page text.
  void CancelPendingClassification(CancelClassificationReason reason);

  // Records in UMA of a specific event that happens in the phishing classifier.
  void RecordEvent(SBPhishingClassifierEvent event);

  void OnDestruct() override;

  // mojom::PhishingDetector
  // Called by the RenderFrame when it receives a StartPhishingDetection IPC
  // from the browser.  This signals that it is ok to begin classification
  // for the given toplevel URL.  If the URL has been fully loaded into the
  // RenderFrame and a Scorer has been set, this will begin classification,
  // otherwise classification will be deferred until these conditions are met.
  void StartPhishingDetection(const GURL& url,
                              StartPhishingDetectionCallback callback) override;

  // Called when classification for the current page finishes.
  void ClassificationDone(const ClientPhishingRequest& verdict);

  // Shared code to begin classification if all conditions are met.
  void MaybeStartClassification();

  // ScorerStorage::Observer implementation:
  void OnScorerChanged() override;

  // The PhishingClassifier to use for the RenderFrame.  This is created once
  // a scorer is made available via SetPhishingScorer().
  std::unique_ptr<PhishingClassifier> classifier_;

  // The last URL that the browser instructed us to classify,
  // with the ref stripped.
  GURL last_url_received_from_browser_;

  // The last top-level URL that has finished loading in the RenderFrame.
  // This corresponds to the text in classifier_page_text_.
  GURL last_finished_load_url_;

  // The transition type for the last load in the main frame.  We use this
  // to exclude back/forward loads from classification.  Note that this is
  // set in DidCommitProvisionalLoad(); the transition is reset after this
  // call in the RenderFrame, so we need to save off the value.
  ui::PageTransition last_main_frame_transition_;

  // The URL of the last load that we actually started classification on.
  // This is used to suppress phishing classification on subframe navigation
  // and back and forward navigations in history.
  GURL last_url_sent_to_classifier_;

  // The page text that will be analyzed by the phishing classifier.  This is
  // set by OnNavigate and cleared when the classifier finishes.  Note that if
  // there is no Scorer yet when OnNavigate is called, or the browser has not
  // instructed us to classify the page, the page text will be cached until
  // these conditions are met.
  std::u16string classifier_page_text_;

  // Tracks whether we have stored anything in classifier_page_text_ for the
  // most recent load.  We use this to distinguish empty text from cases where
  // PageCaptured has not been called.
  bool have_page_text_;

  // Set to true if the classifier is currently running.
  bool is_classifying_;

  // Set to true when StartPhishingDetection method is called. It is
  // set to false whenever phishing detection has finished.
  bool is_phishing_detection_running_ = false;

  // The callback from the most recent call to StartPhishingDetection.
  StartPhishingDetectionCallback callback_;

  mojo::AssociatedReceiver<mojom::PhishingDetector> phishing_detector_receiver_{
      this};

  base::ScopedObservation<ScorerStorage, ScorerStorage::Observer>
      model_change_observation_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_DELEGATE_H_
