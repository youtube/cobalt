// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"

class TabListInterface;

namespace ui {
class WindowAndroid;
}

namespace content {
class WebContents;
}

namespace glic {

class GlicNudgeController;
enum class GlicNudgeActivity;

// C++ implementation of GlicNudgeDelegate for Android.
// Acts as JNI bridge to forward C++ nudge trigger/hide calls to Java's
// GlicNudgeDelegateBridge.
class GlicNudgeDelegateAndroid : public GlicNudgeDelegate {
 public:
  GlicNudgeDelegateAndroid(GlicNudgeController* controller,
                           TabListInterface* tab_list,
                           content::WebContents* web_contents);
  GlicNudgeDelegateAndroid(const GlicNudgeDelegateAndroid&) = delete;
  GlicNudgeDelegateAndroid& operator=(const GlicNudgeDelegateAndroid&) = delete;
  ~GlicNudgeDelegateAndroid() override;

  // GlicNudgeDelegate:
  void OnTriggerGlicNudgeUI(NudgeParams params) override;
  void OnHideGlicNudgeUI() override;
  bool GetIsShowingGlicNudge() override;

  void OnNudgeActivity(GlicNudgeActivity activity);

  ui::WindowAndroid* GetWindowAndroid();
  bool IsActiveTab();

 private:
  raw_ptr<GlicNudgeController> controller_ = nullptr;
  raw_ptr<TabListInterface> tab_list_ = nullptr;
  // Holding this raw pointer is safe because the WebContents is guaranteed to
  // outlive this delegate (via its ownership chain: WebContents owns
  // ContextualCueingHelper -> GlicNudgeControllerAndroid -> this).
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_ANDROID_H_
