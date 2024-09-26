// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
#define COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/web_modal_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"

namespace web_modal {

class WebContentsModalDialogManagerDelegate;

// Per-WebContents class to manage WebContents-modal dialogs.
class WEB_MODAL_EXPORT WebContentsModalDialogManager
    : public SingleWebContentsDialogManagerDelegate,
      public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsModalDialogManager> {
 public:
  WebContentsModalDialogManager(const WebContentsModalDialogManager&) = delete;
  WebContentsModalDialogManager& operator=(
      const WebContentsModalDialogManager&) = delete;

  ~WebContentsModalDialogManager() override;

  WebContentsModalDialogManagerDelegate* delegate() const { return delegate_; }
  void SetDelegate(WebContentsModalDialogManagerDelegate* d);

  // Allow clients to supply their own native dialog manager. Suitable for
  // bubble clients.
  void ShowDialogWithManager(
      gfx::NativeWindow dialog,
      std::unique_ptr<SingleWebContentsDialogManager> manager);

  // Returns true if any dialogs are active and not closed.
  bool IsDialogActive() const;

  // Focus the topmost modal dialog.  IsDialogActive() must be true when calling
  // this function.
  void FocusTopmostDialog() const;

  // SingleWebContentsDialogManagerDelegate:
  content::WebContents* GetWebContents() const override;
  void WillClose(gfx::NativeWindow dialog) override;

  // For testing.
  class TestApi {
   public:
    explicit TestApi(WebContentsModalDialogManager* manager)
        : manager_(manager) {}

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void CloseAllDialogs() { manager_->CloseAllDialogs(); }
    void WebContentsVisibilityChanged(content::Visibility visibility) {
      manager_->OnVisibilityChanged(visibility);
    }

   private:
    raw_ptr<WebContentsModalDialogManager> manager_;
  };

  // Closes all WebContentsModalDialogs.
  void CloseAllDialogs();

 private:
  explicit WebContentsModalDialogManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebContentsModalDialogManager>;

  struct DialogState {
    DialogState(gfx::NativeWindow dialog,
                std::unique_ptr<SingleWebContentsDialogManager> manager);
    DialogState(DialogState&& state);
    ~DialogState();

    gfx::NativeWindow dialog;
    std::unique_ptr<SingleWebContentsDialogManager> manager;
  };

  // Blocks/unblocks interaction with renderer process.
  void BlockWebContentsInteraction(bool blocked);

  bool IsWebContentsVisible() const;

  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetIgnoredUIEvent() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Delegate for notifying our owner about stuff. Not owned by us.
  raw_ptr<WebContentsModalDialogManagerDelegate, DanglingUntriaged> delegate_ =
      nullptr;

  // All active dialogs.
  base::circular_deque<DialogState> child_dialogs_;

  // Whether the WebContents' visibility is content::Visibility::HIDDEN.
  bool web_contents_is_hidden_;

  // True while closing the dialogs on WebContents close.
  bool closing_all_dialogs_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
