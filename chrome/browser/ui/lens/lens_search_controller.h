// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

class LensOverlayController;
class GURL;

namespace lens {
class LensSessionMetricsLogger;
class LensOverlayEventHandler;
class LensOverlayGen204Controller;
class LensOverlaySidePanelCoordinator;
class LensPermissionBubbleController;
class LensSearchboxController;
class LensSearchContextualizationController;
}  // namespace lens

namespace variations {
class VariationsClient;
}  // namespace variations

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;
class ThemeService;

// Controller for all Lens Search features in Chrome. All external entry points
// should go through this controller.
// This migration is still in progress. Follow progress via crbug.com/404941800.
class LensSearchController {
 public:
  explicit LensSearchController(tabs::TabInterface* tab);
  virtual ~LensSearchController();

  // Initializes all the necessary dependencies for the LensSearchController.
  void Initialize(variations::VariationsClient* variations_client,
                  signin::IdentityManager* identity_manager,
                  PrefService* pref_service,
                  syncer::SyncService* sync_service,
                  ThemeService* theme_service);

  // A simple utility that gets the the LensSearchController TabFeature set by
  // the embedding tab of a lens WebUI hosted in `webui_web_contents`.
  // May return nullptr if no LensSearchController TabFeature is associated
  // with `webui_web_contents`.
  static LensSearchController* FromWebUIWebContents(
      content::WebContents* webui_web_contents);

  // A simple utility that gets the the LensSearchController TabFeature set by
  // the instances of WebContents associated with a tab.
  // May return nullptr if no LensSearchController TabFeature is associated
  // with `tab_web_contents`.
  static LensSearchController* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // This is an entry point for showing the overlay UI. This has no effect if
  // the overlay is not currently `kOff`.  This has no effect if the tab is not
  // in the foreground. If the overlay is successfully invoked, then the value
  // of `invocation_source` will be recorded in the relevant metrics. Virtual
  // for testing.
  virtual void OpenLensOverlay(
      lens::LensOverlayInvocationSource invocation_source);

  // Sets a region to search after the overlay loads, then calls ShowUI().
  // All units are in device pixels. region_bitmap contains the high definition
  // image bytes to use for the search instead of cropping the region from the
  // viewport. Virtual for testing.
  virtual void OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource invocation_source,
      lens::mojom::CenterRotatedBoxPtr region,
      const SkBitmap& region_bitmap);

  // Convenience method for calling OpenLensOverlayWithPendingRegion, that will
  // convert the bounds into a CenterRotated Box to pass to the overlay.
  void OpenLensOverlayWithPendingRegionFromBounds(
      lens::LensOverlayInvocationSource invocation_source,
      const gfx::Rect& tab_bounds,
      const gfx::Rect& view_bounds,
      const gfx::Rect& region_bounds,
      const SkBitmap& region_bitmap);

  // Starts the contextualization flow without the overlay being shown to the
  // user. Virtual for testing.
  virtual void StartContextualization(
      lens::LensOverlayInvocationSource invocation_source);

  // Issues a contextual search request for Lens to fulfill. Starts
  // contextualization flow if its not already in progress. If the Lens Overlay
  // is in the process of opening, the request will be queued until the overlay
  // is fully opened.
  // TODO(crbug.com/403629222): Revisit if it makes sense to pass the
  // destination URL instead of the query text directly.
  void IssueContextualSearchRequest(
      lens::LensOverlayInvocationSource invocation_source,
      const GURL& destination_url,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion);

  // Starts the closing process of the overlay. This is an asynchronous process
  // with the following sequence:
  //   (1) Close the side panel
  //   (2) Close the overlay.
  // Step (1) is asynchronous.
  virtual void CloseLensAsync(
      lens::LensOverlayDismissalSource dismissal_source);

  // Instantly closes all Lens components currently opened.This may not look
  // nice if the overlay is visible when this is called.
  virtual void CloseLensSync(lens::LensOverlayDismissalSource dismissal_source);

  // Launches the survey if the user has not already seen it.
  void MaybeLaunchSurvey();

  // Returns true if Lens is currently active on this tab.
  bool IsActive();

  // Returns true if Lens is currently off on this tab.
  bool IsOff();

  // Returns true if the overlay is in the process of closing. If true, Lens on
  // this tab will soon be off.
  bool IsClosing();

  // Returns whether the handshake with the Lens backend is complete.
  bool IsHandshakeComplete();

  // Returns the tab interface that owns this controller.
  tabs::TabInterface* GetTabInterface();

  // Returns the page URL of the tab if Lens has access to it.
  const GURL& GetPageURL() const;

  // Gets the page title.
  std::optional<std::string> GetPageTitle();

  // Returns the weak pointer to this class.
  base::WeakPtr<LensSearchController> GetWeakPtr();

  // Returns the LensOverlayController.
  LensOverlayController* lens_overlay_controller();
  const LensOverlayController* lens_overlay_controller() const;

  // Returns the LensOverlayQueryController.
  lens::LensOverlayQueryController* lens_overlay_query_controller();

  // Returns the LensOverlaySidePanelCoordinator.
  lens::LensOverlaySidePanelCoordinator* lens_overlay_side_panel_coordinator();

  // Returns the LensSearchboxController.
  lens::LensSearchboxController* lens_searchbox_controller();

  // Returns the event handler for this instance of the Lens Overlay.
  lens::LensOverlayEventHandler* lens_overlay_event_handler();

  optimization_guide::PageContextEligibility* page_context_eligibility();

  // Returns the LensSearchContextualizationController.
  lens::LensSearchContextualizationController*
  lens_search_contextualization_controller();

  // Returns the LensSessionMetricsLogger.
  lens::LensSessionMetricsLogger* lens_session_metrics_logger();

  // Testing function for setting the page context eligibility API for this
  // controller.
  void set_page_context_eligibility_for_testing(
      optimization_guide::PageContextEligibility* page_context_eligibility) {
    page_context_eligibility_ = page_context_eligibility;
  }

  lens::LensPermissionBubbleController*
  get_lens_permission_bubble_controller_for_testing() {
    return lens_permission_bubble_controller_.get();
  }

 protected:
  friend class LensOverlayController;
  friend class lens::LensOverlaySidePanelCoordinator;

  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensOverlayController> CreateLensOverlayController(
      tabs::TabInterface* tab,
      LensSearchController* lens_search_controller,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service);

  // Override these methods to stub out network requests for testing.
  virtual std::unique_ptr<lens::LensOverlayQueryController>
  CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      lens::UploadProgressCallback page_content_upload_progress_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller);

  // Override these methods to be able to track calls made to the side panel
  // coordinator.
  virtual std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
  CreateLensOverlaySidePanelCoordinator();

  // Override these methods to be able to track calls made to the searchbox
  // controller.
  virtual std::unique_ptr<lens::LensSearchboxController>
  CreateLensSearchboxController();

  // Override these methods to be able to track calls made to the
  // contextualization controller.
  virtual std::unique_ptr<lens::LensSearchContextualizationController>
  CreateLensSearchContextualizationController();

  // Called by the Lens overlay when it has finished opening and has moved to
  // the kOverlay state. This is how this class knows it can move into kActive
  // state.
  // TODO(crbug.com/404941800): Make this more generic to allow for multiple
  // features to initialize at the same time.
  void NotifyOverlayOpened();

  // Shared logic for cleanup that is called after all features have finished
  // cleaning up.
  void CloseLensPart2(lens::LensOverlayDismissalSource dismissal_source);

  // Called before the lens results panel begins hiding. This is called before
  // any side panel closing animations begin.
  void OnSidePanelWillHide(SidePanelEntryHideReason reason);

  // Called when the lens side panel has been hidden.
  void OnSidePanelHidden();

  // Override these methods to be able to track calls made to the page context
  // eligibility API.
  virtual void CreatePageContextEligibilityAPI();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. No feature is currently active or soon to be
    // active.
    kOff,

    // The controller is in the process of starting up. Soon to be kActive.
    kInitializing,

    // One or more Lens features are active on this tab.
    kActive,

    // The UI has been made inactive / backgrounded and is hidden. This differs
    // from kSuspended as the overlay and web view are not freed and could be
    // immediately reshown.
    kBackground,

    // The side panel is in the process of closing. Soon will move to kClosing.
    kClosingSidePanel,

    // The controller is in the process of closing all dependencies and cleaning
    // up. Will soon be kOff.
    kClosing,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

 private:
  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  // Passes the correct callbacks and dependencies to the protected
  // CreateLensQueryController method.
  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayInvocationSource invocation_source);

  // Creates all state necessary to start a Lens session. This method contains
  // shared state that is used no matter the entrypoint.
  void StartLensSession(lens::LensOverlayInvocationSource invocation_source);

  // Runs the eligibility checks necessary for Lens to open on this tab. If the
  // user has not granted permission to use Lens on this tab, the permission
  // request will be shown and callback will be called after the user accepts.
  // Returns true if the checks pass and its safe to open Lens, false otherwise.
  bool RunLensEligibilityChecks(
      lens::LensOverlayInvocationSource invocation_source,
      base::RepeatingClosure permission_granted_callback);

  // Callback used by the query controller to notify the search controller of
  // the response to the initial image upload request.
  void HandleStartQueryResponse(
      std::vector<lens::mojom::OverlayObjectPtr> objects,
      lens::mojom::TextPtr text,
      bool is_error);

  // Callback used by the query controller to notify the search controller of
  // the URL response to the interaction request, aka, the URL that should be
  // opened in the results frame.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Callback used by the query controller to notify the search controller of
  // the response of an interaction request. If this is a visual interaction
  // request, the response will contain the text container within that image.
  void HandleInteractionResponse(lens::mojom::TextPtr text);

  // Callback used by the query controller to notify the search controller of
  // the suggest inputs response. This is used to update the searchbox with
  // the most recent suggest inputs.
  void HandleSuggestInputsResponse(
      lens::proto::LensOverlaySuggestInputs suggest_inputs);

  // Callback used by the query controller to pass the thumbnail bytes of a
  // visual interaction request to the searchbox.
  void HandleThumbnailCreated(const std::string& thumbnail_bytes);

  // Callback used by the query controller to notify the search controller of
  // the progress of the page content upload.
  void HandlePageContentUploadProgress(uint64_t position, uint64_t total);

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab will enter the background.
  void TabWillEnterBackground(tabs::TabInterface* tab);

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Called when the tab will be removed from the window.
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

  // Whether the LensSearchController has been initialized. Meaning, all the
  // dependencies have been initialized and the controller is ready to use.
  bool initialized_ = false;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // Indicates whether a trigger for the HaTS survey has occurred in the current
  // session. Note that a trigger does not mean the survey will actually be
  // shown.
  bool hats_triggered_in_session_ = false;

  // If the side panel needed to be closed before dismissing Lens, this
  // stores the original dismissal_source so it is properly recorded when the
  // side panel is done closing and the callback is invoked.
  std::optional<lens::LensOverlayDismissalSource> last_dismissal_source_;

  // The query controller for the Lens Search feature on this tab. Lives for the
  // duration of a Lens feature being active on this tab.
  std::unique_ptr<lens::LensOverlayQueryController>
      lens_overlay_query_controller_;

  std::unique_ptr<lens::LensPermissionBubbleController>
      lens_permission_bubble_controller_;

  // The overlay controller for the Lens Search feature on this tab.
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;

  // The controller for sending gen204 pings. Owned by this class so it can
  // outlive the query controller, allowing gen204 requests to be sent upon
  // query end.
  std::unique_ptr<lens::LensOverlayGen204Controller> gen204_controller_;

  // The side panel coordinator for the Lens Search feature on this tab.
  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
      lens_overlay_side_panel_coordinator_;

  // The searchbox controller for the Lens Search feature on this tab.
  // TODO(crbug.com/413138792): Hook up this controller to handle searchbox
  // interactions, without a dependency on the overlay controller.
  std::unique_ptr<lens::LensSearchboxController> lens_searchbox_controller_;

  // The contextualization controller for the Lens Search feature on this tab.
  std::unique_ptr<lens::LensSearchContextualizationController>
      lens_contextualization_controller_;

  std::unique_ptr<lens::LensSessionMetricsLogger> lens_session_metrics_logger_;

  // Class for handling key events from the renderer that were not handled. This
  // is used by both the overlay and the WebUI to share common event handling
  // logic.
  std::unique_ptr<lens::LensOverlayEventHandler> lens_overlay_event_handler_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // The page context eligibility API if it has been fetched. Can be nullptr.
  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The pref service associated with the current profile. Owned by Profile,
  // and thus guaranteed to outlive this instance.
  raw_ptr<PrefService> pref_service_;

  // The sync service associated with the current profile.
  raw_ptr<syncer::SyncService> sync_service_;

  // The theme service associated with the current profile. Owned by Profile,
  // and thus guaranteed to outlive this instance.
  raw_ptr<ThemeService> theme_service_;

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;

  // Must be the last member.
  base::WeakPtrFactory<LensSearchController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_
