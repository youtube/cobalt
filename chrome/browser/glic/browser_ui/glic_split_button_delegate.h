// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_H_

#include "chrome/browser/glic/browser_ui/glic_actor_nudge_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"

namespace glic {

// Delegate interface for the UI container that houses GlicButton and
// GlicActorTaskIcon.
class GlicSplitButtonDelegate : public GlicNudgeDelegate,
                                public GlicActorNudgeDelegate,
                                public GlicButtonControllerDelegate {};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_H_
