// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_H_

#include "chip_controller.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;

namespace content {
class WebContents;
}

class PermissionPromptBubble : public PermissionPromptDesktop,
                               public views::WidgetObserver {
 public:
  PermissionPromptBubble(Browser* browser,
                         content::WebContents* web_contents,
                         Delegate* delegate);
  ~PermissionPromptBubble() override;
  PermissionPromptBubble(const PermissionPromptBubble&) = delete;
  PermissionPromptBubble& operator=(const PermissionPromptBubble&) = delete;

  void ShowBubble();
  void CleanUpPromptBubble();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // permissions::PermissionPrompt:
  bool UpdateAnchor() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  views::Widget* GetPromptBubbleWidgetForTesting() override;

 private:
  // The popup bubble. Not owned by this class; it will delete itself when a
  // decision is made.
  raw_ptr<PermissionPromptBubbleView> prompt_bubble_ = nullptr;

  base::TimeTicks permission_requested_time_;

  bool parent_was_visible_when_activation_changed_;

  base::ScopedClosureRunner disallowed_custom_cursors_scope_;

  base::WeakPtrFactory<PermissionPromptBubble> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_H_
