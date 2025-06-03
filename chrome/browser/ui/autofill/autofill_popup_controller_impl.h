// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/aliases.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Profile;

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace ui {
class AXPlatformNode;
}

namespace autofill {

class AutofillPopupDelegate;
class AutofillPopupView;

// Sub-popups and their parent popups are connected by providing children
// with links to their parents. This interface defines the API exposed by
// these links.
class ExpandablePopupParentControllerImpl {
 private:
  friend class AutofillPopupControllerImpl;

  // Creates a view for a sub-popup. On rare occasions opening the sub-popup
  // may fail (e.g. when there is no room to open the sub-popup or the popup
  // is in the middle of destroying and  has no widget already),
  // `nullptr` is returned in these cases.
  virtual base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillPopupController> sub_controller) = 0;
};

// This class is a controller for an AutofillPopupView. It implements
// AutofillPopupController to allow calls from AutofillPopupView. The
// other, public functions are available to its instantiator.
class AutofillPopupControllerImpl
    : public AutofillPopupController,
      public content::WebContentsObserver,
      public PictureInPictureWindowManager::Observer,
      public ExpandablePopupParentControllerImpl {
 public:
  AutofillPopupControllerImpl(const AutofillPopupControllerImpl&) = delete;
  AutofillPopupControllerImpl& operator=(const AutofillPopupControllerImpl&) =
      delete;

  // Creates a new `AutofillPopupControllerImpl`, or reuses `previous` if the
  // construction arguments are the same. `previous` may be invalidated by this
  // call. The controller will listen for keyboard input routed to
  // `web_contents` while the popup is showing, unless `web_contents` is NULL.
  static base::WeakPtr<AutofillPopupControllerImpl> GetOrCreate(
      base::WeakPtr<AutofillPopupControllerImpl> previous,
      base::WeakPtr<AutofillPopupDelegate> delegate,
      content::WebContents* web_contents,
      gfx::NativeView container_view,
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction);

  // Shows the popup, or updates the existing popup with the given values.
  virtual void Show(std::vector<Suggestion> suggestions,
                    AutofillSuggestionTriggerSource trigger_source,
                    AutoselectFirstSuggestion autoselect_first_suggestion);

  // Updates the data list values currently shown with the popup.
  virtual void UpdateDataListValues(base::span<const SelectOption> options);

  // Informs the controller that the popup may not be hidden by stale data or
  // interactions with native Chrome UI. This state remains active until the
  // view is destroyed.
  void PinView();

  // Hides the popup and destroys the controller. This also invalidates
  // `delegate_`.
  void Hide(PopupHidingReason reason) override;

  // Invoked when the view was destroyed by by someone other than this class.
  void ViewDestroyed() override;

  // Handles a key press event and returns whether the event should be swallowed
  // (meaning that no other handler, in not particular the default handler, can
  // process it).
  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  // AutofillPopupController:
  std::vector<Suggestion> GetSuggestions() const override;
  bool ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const override;
  base::WeakPtr<AutofillPopupController> OpenSubPopup(
      const gfx::RectF& anchor_bounds,
      std::vector<Suggestion> suggestions,
      AutoselectFirstSuggestion autoselect_first_suggestion) override;
  std::optional<AutofillClient::PopupScreenLocation> GetPopupScreenLocation()
      const override;
  void HideSubPopup() override;

  // PictureInPictureWindowManager::Observer
  void OnEnterPictureInPicture() override;

  void KeepPopupOpenForTesting() { keep_popup_open_for_testing_ = true; }

  // Disables show thresholds. See the documentation of the member for details.
  void DisableThresholdForTesting(bool disable_threshold) {
    disable_threshold_for_testing_ = disable_threshold;
  }

  void SetViewForTesting(base::WeakPtr<AutofillPopupView> view) {
    view_ = std::move(view);
    time_view_shown_ = base::TimeTicks::Now();
  }

  int GetLineCountForTesting() const { return GetLineCount(); }

 protected:
  FRIEND_TEST_ALL_PREFIXES(AutofillPopupControllerUnitTest,
                           ProperlyResetController);

  AutofillPopupControllerImpl(
      base::WeakPtr<AutofillPopupDelegate> delegate,
      content::WebContents* web_contents,
      gfx::NativeView container_view,
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      base::RepeatingCallback<void(
          gfx::NativeWindow,
          Profile*,
          password_manager::metrics_util::PasswordMigrationWarningTriggers)>
          show_pwd_migration_warning_callback,
      absl::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>>
          parent);
  ~AutofillPopupControllerImpl() override;

  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  void SetElementBounds(const gfx::RectF& bounds);
  base::i18n::TextDirection GetElementTextDirection() const override;

  // AutofillPopupController:
  void OnSuggestionsChanged() override;
  void SelectSuggestion(absl::optional<size_t> index) override;
  void AcceptSuggestion(int index, base::TimeTicks event_time) override;
  void PerformButtonActionForSuggestion(int index) override;
  bool RemoveSuggestion(int list_index) override;
  int GetLineCount() const override;
  const Suggestion& GetSuggestionAt(int row) const override;
  std::u16string GetSuggestionMainTextAt(int row) const override;
  std::u16string GetSuggestionMinorTextAt(int row) const override;
  std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override;
  bool GetRemovalConfirmationText(int list_index,
                                  std::u16string* title,
                                  std::u16string* body) override;
  PopupType GetPopupType() const override;
  AutofillSuggestionTriggerSource GetAutofillSuggestionTriggerSource()
      const override;

  // Returns true if the popup still has non-options entries to show the user.
  bool HasSuggestions() const;

  // Set the Autofill entry values. Exposed to allow tests to set these values
  // without showing the popup.
  void SetSuggestions(std::vector<Suggestion> suggestions);

  base::WeakPtr<AutofillPopupControllerImpl> GetWeakPtr();

  // Raise an accessibility event to indicate the controls relation of the
  // form control of the popup and popup itself has changed based on the popup's
  // show or hide action.
  void FireControlsChangedEvent(bool is_show);

  // Gets the root AXPlatformNode for our WebContents, which can be used
  // to find the AXPlatformNode specifically for the autofill text field.
  virtual ui::AXPlatformNode* GetRootAXPlatformNodeForWebContents();

  // Hides `view_` unless it is null and then deletes `this`.
  virtual void HideViewAndDie();

 private:
  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Clear the internal state of the controller. This is needed to ensure that
  // when the popup is reused it doesn't leak values between uses.
  void ClearState();

  // Returns true iff the focused frame has a pointer lock, which may be used to
  // trick the user into accepting some suggestion (crbug.com/1239496). In such
  // a case, we should hide the popup.
  bool IsMouseLocked() const;

  // ExpandablePopupParentControllerImpl:
  base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillPopupController> controller) override;

  // Returns `true` if this popup has no parent, and `false` for sub-popups.
  bool IsRootPopup() const;

  PopupControllerCommon controller_common_;
  base::WeakPtr<AutofillPopupView> view_;
  base::WeakPtr<AutofillPopupDelegate> delegate_;

  struct {
    content::GlobalRenderFrameHostId rfh;
    content::RenderWidgetHost::KeyPressEventCallback handler;
  } key_press_observer_;

  // The time the view was shown the last time. It is used to safeguard against
  // accepting suggestions too quickly after a the popup view was shown (see the
  // `show_threshold` parameter of `AcceptSuggestion`).
  base::TimeTicks time_view_shown_;

  // An override to suppress minimum show thresholds. It should only be set
  // during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  bool disable_threshold_for_testing_ = false;

  // If set to true, the popup will never be hidden because of stale data or if
  // the user interacts with native UI.
  bool is_view_pinned_ = false;

  // The current Autofill query values.
  std::vector<Suggestion> suggestions_;

  // The trigger source of the `suggestions_`.
  AutofillSuggestionTriggerSource trigger_source_;

  // If set to true, the popup will stay open regardless of external changes on
  // the machine that would normally cause the popup to be hidden.
  bool keep_popup_open_for_testing_ = false;

  // Observer needed to check autofill popup overlap with picture-in-picture
  // window. It is guaranteed that there can only be one
  // PictureInPictureWindowManager per Chrome instance, therefore, it is also
  // guaranteed that PictureInPictureWindowManager would outlive its observers.
  base::ScopedObservation<PictureInPictureWindowManager,
                          PictureInPictureWindowManager::Observer>
      picture_in_picture_window_observation_{this};

  // Callback invoked to try to show the password migration warning on Android.
  // Used to facilitate testing.
  // TODO(crbug.com/1454469): Remove when the warning isn't needed anymore.
  base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>
      show_pwd_migration_warning_callback_;

  // Whether the popup should ignore mouse observed outside check.
  bool should_ignore_mouse_observed_outside_item_bounds_check_ = false;

  // Parent's popup controller. The root popup doesn't have a parent, but in
  // sub-popups it must be present.
  const absl::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>>
      parent_controller_;

  // The open sub-popup controller if any, `nullptr` otherwise.
  base::WeakPtr<AutofillPopupControllerImpl> sub_popup_controller_;

  // AutofillPopupControllerImpl deletes itself. To simplify memory management,
  // we delete the object asynchronously.
  base::WeakPtrFactory<AutofillPopupControllerImpl>
      self_deletion_weak_ptr_factory_{this};

  base::WeakPtrFactory<AutofillPopupControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
