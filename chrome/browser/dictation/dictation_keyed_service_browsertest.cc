// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"

namespace dictation {

namespace {

using ExtensionStreamState = extensions::api::dictation_private::StreamState;
using ExtensionTranscriptionType =
    extensions::api::dictation_private::TranscriptionType;

class DictationKeyedServiceBrowserTest : public PlatformBrowserTest {
 public:
  DictationKeyedServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
  }
  ~DictationKeyedServiceBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        kDictationTestExtensionId);
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  DictationKeyedService& dictation_service() {
    auto* service = DictationKeyedService::Get(profile());
    CHECK(service);
    return *service;
  }

  void SimulateSpeechRecognition(ListenerStreamProvider* provider,
                                 ExtensionTranscriptionType type,
                                 std::string_view text) {
    ExtensionSendTranscriptUpdate(profile(), provider->stream_id_for_testing(),
                                  type, text);
    WaitForTranscriptUpdate(provider);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       CreatedForRegularProfile) {
  EXPECT_NE(DictationKeyedService::Get(profile()), nullptr);
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       NotCreatedForOTRProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(DictationKeyedService::Get(otr_profile), nullptr);
}

class DictationKeyedServiceDisabledBrowserTest
    : public DictationKeyedServiceBrowserTest {
 public:
  DictationKeyedServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(kDictation);
  }
  ~DictationKeyedServiceDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceDisabledBrowserTest,
                       NotCreatedWhenDisabled) {
  EXPECT_EQ(DictationKeyedService::Get(profile()), nullptr);
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       ShouldShowContextMenuItem) {
  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());

  dictation_service().StartSession(*GetBrowserWindowInterface(), nullptr);

  EXPECT_FALSE(dictation_service().ShouldShowContextMenuItem());

  dictation_service().EndSession();

  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       ExecuteContextMenuCommand) {
  content::ContextMenuParams params;
  params.is_editable = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_DICTATION));
  ASSERT_TRUE(menu.IsItemEnabled(IDC_CONTENT_CONTEXT_DICTATION));

  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_DICTATION, 0);

  EXPECT_NE(dictation_service().session_controller(), nullptr);
}

// TODO(crbug.com/502587072): Add tests which have the test extension simulate
// stream failures, including on start and mid stream.

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       StartSessionAndReceiveTranscription) {
  LoadTestExtension(profile());
  SetMockTranscript(profile(), "Hello world");

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   std::make_unique<Target>());

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      dictation_service().session_controller()->attached_stream_provider());
  base::RunLoop wait_for_updates_loop;
  bool seen_partial = false;
  provider->SetOnUpdateForTesting(base::BindLambdaForTesting([&]() {
    if (!seen_partial) {
      if (provider->GetLatestTranscriptionForTesting() == "Hello") {
        EXPECT_FALSE(provider->IsTranscriptionFinalForTesting());
        EXPECT_EQ(provider->GetState(),
                  StreamProvider::StreamState::kTranscribing);
        seen_partial = true;
      }
    }
    if (provider->GetLatestTranscriptionForTesting() == "Hello world" &&
        provider->IsTranscriptionFinalForTesting()) {
      EXPECT_TRUE(seen_partial);
      wait_for_updates_loop.Quit();
    }
  }));
  wait_for_updates_loop.Run();

  provider->Stop();

  WaitForStreamState(provider, StreamProvider::StreamState::kComplete);

  dictation_service().EndSession();
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       EndActiveStreamEntersFinalizingState) {
  LoadTestExtensionInManualMode(profile());

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   std::make_unique<Target>());

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  WaitForStreamState(provider, StreamProvider::StreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  // Send a transcription update ("Hello") as a partial update and wait for it
  // to be received.
  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kPartial,
                            "Hello");

  ASSERT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello");
  ASSERT_FALSE(provider->IsTranscriptionFinalForTesting());

  // Simulate the stream being ended.
  controller->UiRequestEndActiveStream();

  // Ensure the controller enters kFinalizing state.
  EXPECT_EQ(controller->GetState(), SessionState::kFinalizing);

  // The finalizing provider should still be able to send a final transcription
  // update.
  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kFinal,
                            "Hello World");
  EXPECT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello World");
  EXPECT_TRUE(provider->IsTranscriptionFinalForTesting());

  // TODO(b/508729855) Ensure a final transcript update when finalizing gets
  // committed to the target.
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       StartNewStreamWhileFinalizing) {
  LoadTestExtensionInManualMode(profile());

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   std::make_unique<Target>());

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider1 = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider1, nullptr);

  // Wait for the first stream to transition to transcribing.
  ExtensionSendStreamStateUpdate(profile(), provider1->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  WaitForStreamState(provider1, StreamProvider::StreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  SimulateSpeechRecognition(provider1, ExtensionTranscriptionType::kPartial,
                            "Hello");

  // Put the first stream into finalization.
  controller->UiRequestEndActiveStream();

  // The first stream is now in the finalizing set, and the controller is in
  // kFinalizing.
  ASSERT_EQ(controller->GetState(), SessionState::kFinalizing);
  ASSERT_EQ(controller->attached_stream_provider(), nullptr);

  // Start a second stream while the first is finalizing. The controller should
  // immediately enter kStreamInitializing.
  controller->StartDictationStream(std::make_unique<Target>());
  EXPECT_EQ(controller->GetState(), SessionState::kStreamInitializing);

  // Wait for the stream to enter transcribing state.
  ListenerStreamProvider* provider2 = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ExtensionSendStreamStateUpdate(profile(), provider2->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  WaitForStreamState(provider2, StreamProvider::StreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  // Recognition from the new provider arrives.
  SimulateSpeechRecognition(provider2, ExtensionTranscriptionType::kPartial,
                            "World");
  EXPECT_EQ(provider2->GetLatestTranscriptionForTesting(), "World");
}

}  // namespace

}  // namespace dictation
