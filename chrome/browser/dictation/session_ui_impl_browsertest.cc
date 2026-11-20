// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "ui/base/interaction/element_tracker.h"

namespace dictation {

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStreamStartedEvent);

class DictationSessionUiImplBrowserTest : public InteractiveBrowserTest {
 public:
  DictationSessionUiImplBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
  }
  ~DictationSessionUiImplBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        kDictationTestExtensionId);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    LoadTestExtension(profile());
    SetMockTranscript(profile(), "test transcript");
  }

  void TearDownOnMainThread() override {
    dictation_service().EndSession();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  DictationKeyedService& dictation_service() {
    return *DictationKeyedService::Get(profile());
  }

  SessionUiImpl* session_ui() {
    if (!dictation_service().session_controller()) {
      return nullptr;
    }

    return static_cast<SessionUiImpl*>(
        dictation_service().session_controller()->ui_for_testing());
  }

  auto StartSession() {
    // clang-format off
    return Steps(
      Do([this]{
        dictation_service().StartSession(
            *browser(),
            std::make_unique<Target>());
        session_state_changed_callback_ =
            dictation_service()
                .session_controller()
                ->AddSessionStateChangedCallback(base::BindRepeating(
                    &DictationSessionUiImplBrowserTest::OnSessionStateChanged,
                    base::Unretained(this)));
      }),
      Check([this]{ return session_ui() != nullptr; })
    );
    // clang-format on
  }

  auto GetSessionState() {
    return [this]() {
      return dictation_service().session_controller()->GetState();
    };
  }

  auto HasAttachedStreamProvider() {
    return [this]() {
      return dictation_service()
                 .session_controller()
                 ->attached_stream_provider() != nullptr;
    };
  }

 private:
  void OnSessionStateChanged(SessionState new_state) {
    if (new_state == SessionState::kTranscribing) {
      BrowserElements::From(browser())->NotifyEvent(kBrowserViewElementId,
                                                    kStreamStartedEvent);
    }
  }

  base::CallbackListSubscription session_state_changed_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, StartSessionShowsUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       EndSessionTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    PressButton(DictationBubbleUi::kCloseButtonElementIdForTesting),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    Check([this]{ return session_ui() == nullptr; })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       DoneButtonEndsActiveStream) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForEvent(kBrowserViewElementId, kStreamStartedEvent),
    CheckResult(GetSessionState(), SessionState::kTranscribing),

    PressButton(DictationBubbleUi::kDoneButtonElementIdForTesting),

    // TODO(b/525943882): Currently the controller immediately disposes of the
    // stream but it should really be going into a finalization state. Update
    // once this is fixed.
    CheckResult(GetSessionState(), testing::Ne(SessionState::kTranscribing)),
    CheckResult(HasAttachedStreamProvider(), false)
  );
  // clang-format on
}

}  // namespace dictation
