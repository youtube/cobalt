// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class StartPresentationContext;
enum class MediaRouterDialogActivationLocation;

// An abstract base class for Media Router dialog controllers. Tied to a
// WebContents known as the |initiator|, and is lazily created when a Media
// Router dialog needs to be shown. The MediaRouterDialogController allows
// showing and closing a Media Router dialog modal to the initiator WebContents.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogController {
 public:
  MediaRouterDialogController(const MediaRouterDialogController&) = delete;
  MediaRouterDialogController& operator=(const MediaRouterDialogController&) =
      delete;

  virtual ~MediaRouterDialogController();

  using GetOrCreate = base::RepeatingCallback<MediaRouterDialogController*(
      content::WebContents*)>;

  // Sets the factory/getter for MediaRouterDialogController, which is platform
  // and embedder dependent. The callback should create or return an existing
  // instance as needed.
  static void SetGetOrCreate(const GetOrCreate& get_or_create);

  // Gets a reference to the MediaRouterDialogController associated with
  // |web_contents|, creating one if it does not exist. The returned pointer is
  // guaranteed to be non-null.
  static MediaRouterDialogController* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Shows the media router dialog modal to |initiator_|, with additional
  // context for a PresentationRequest coming from the page given by the input
  // parameters.
  // Returns true if the dialog is created as a result of this call.
  // If the dialog already exists, or dialog cannot be created, then false is
  // returned, and |error_cb| will be invoked.
  virtual bool ShowMediaRouterDialogForPresentation(
      std::unique_ptr<StartPresentationContext> context);

  // Shows the media router dialog modal to |initiator_|.
  // Creates the dialog if it did not exist prior to this call, returns true.
  // If the dialog already exists, brings it to the front, returns false.
  virtual bool ShowMediaRouterDialog(
      MediaRouterDialogActivationLocation activation_location);

  // Hides the media router dialog.
  // It is a no-op to call this function if there is currently no dialog.
  void HideMediaRouterDialog();

  // Indicates if the media router dialog already exists.
  virtual bool IsShowingMediaRouterDialog() const = 0;

 protected:
  // Use MediaRouterDialogController::GetOrCreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogController(content::WebContents* initiator);

  // Creates a media router dialog if necessary, then activates the WebContents
  // that initiated the dialog, e.g. focuses the tab.
  void FocusOnMediaRouterDialog(
      bool dialog_needs_creation,
      MediaRouterDialogActivationLocation activation_location);

  // Returns the WebContents that initiated showing the dialog.
  content::WebContents* initiator() const { return initiator_; }

  // Resets the state of the controller. Must be called from the overrides.
  virtual void Reset();
  // Creates a new media router dialog modal to |initiator_|.
  virtual void CreateMediaRouterDialog(
      MediaRouterDialogActivationLocation activation_location) = 0;
  // Closes the media router dialog if it exists.
  virtual void CloseMediaRouterDialog() = 0;

  // Data for dialogs created at the request of the Presentation API.
  // Created from arguments passed in via ShowMediaRouterDialogForPresentation.
  std::unique_ptr<StartPresentationContext> start_presentation_context_;

 private:
  class InitiatorWebContentsObserver;

  // An observer for the |initiator_| that closes the dialog when |initiator_|
  // is destroyed or navigated.
  std::unique_ptr<InitiatorWebContentsObserver> initiator_observer_;
  const raw_ptr<content::WebContents> initiator_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
