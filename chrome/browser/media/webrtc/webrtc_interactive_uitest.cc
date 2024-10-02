// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

static const char kKeygenAlgorithmRsa[] =
    "{ name: \"RSASSA-PKCS1-v1_5\", modulusLength: 2048, publicExponent: "
    "new Uint8Array([1, 0, 1]), hash: \"SHA-256\" }";
static const char kKeygenAlgorithmEcdsa[] =
    "{ name: \"ECDSA\", namedCurve: \"P-256\" }";

// Top-level integration test for WebRTC. It always uses fake devices; see
// WebRtcWebcamBrowserTest for a test that acquires any real webcam on the
// system.
class WebRtcBrowserTest : public WebRtcTestBase {
 public:
  WebRtcBrowserTest() : left_tab_(nullptr), right_tab_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Flag used by TestWebAudioMediaStream to force garbage collection.
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");
  }

  void RunsAudioVideoWebRTCCallInTwoTabs(
      const std::string& video_codec = WebRtcTestBase::kUseDefaultVideoCodec,
      bool prefer_hw_video_codec = false,
      const std::string& offer_cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen,
      const std::string& answer_cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen) {
    StartServerAndOpenTabs();

    SetupPeerconnectionWithLocalStream(left_tab_, offer_cert_keygen_alg);
    SetupPeerconnectionWithLocalStream(right_tab_, answer_cert_keygen_alg);

    if (!video_codec.empty()) {
      SetDefaultVideoCodec(left_tab_, video_codec, prefer_hw_video_codec);
      SetDefaultVideoCodec(right_tab_, video_codec, prefer_hw_video_codec);
    }
    NegotiateCall(left_tab_, right_tab_);

    DetectVideoAndHangUp();
  }

  void RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(
      const std::string& cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen) {
    StartServerAndOpenTabs();

    // Generate and clone a certificate, resulting in JavaScript variable
    // |gCertificateClone| being set to the resulting clone.
    DeleteDatabase(left_tab_);
    OpenDatabase(left_tab_);
    GenerateAndCloneCertificate(left_tab_, cert_keygen_alg);
    CloseDatabase(left_tab_);
    DeleteDatabase(left_tab_);

    SetupPeerconnectionWithCertificateAndLocalStream(left_tab_,
                                                     "gCertificateClone");
    SetupPeerconnectionWithLocalStream(right_tab_, cert_keygen_alg);

    NegotiateCall(left_tab_, right_tab_);
    VerifyLocalDescriptionContainsCertificate(left_tab_, "gCertificate");

    DetectVideoAndHangUp();
  }

  uint32_t GetPeerToPeerConnectionsCountChangeFromNetworkService() {
    uint32_t connection_count = 0u;
    if (content::IsInProcessNetworkService()) {
      base::RunLoop run_loop;
      content::GetNetworkTaskRunner()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([&connection_count, &run_loop] {
            connection_count =
                network::NetworkService::GetNetworkServiceForTesting()
                    ->network_quality_estimator()
                    ->GetPeerToPeerConnectionsCountChange();
            run_loop.Quit();
          }));
      run_loop.Run();
      return connection_count;
    }

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    // TODO(crbug.com/901026): Make sure the network process is started to avoid
    // a deadlock on Android.
    network_service_test.FlushForTesting();

    mojo::ScopedAllowSyncCallForTesting allow_sync_call;

    bool available = network_service_test->GetPeerToPeerConnectionsCountChange(
        &connection_count);
    EXPECT_TRUE(available);

    return connection_count;
  }

 protected:
  void StartServerAndOpenTabs() {
    ASSERT_TRUE(embedded_test_server()->Start());
    left_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  }

  void DetectVideoAndHangUp() {
    StartDetectingVideo(left_tab_, "remote-view");
    StartDetectingVideo(right_tab_, "remote-view");
#if !BUILDFLAG(IS_MAC)
    // Video is choppy on Mac OS X. http://crbug.com/443542.
    WaitForVideoToPlay(left_tab_);
    WaitForVideoToPlay(right_tab_);
#endif
    HangUp(left_tab_);
    HangUp(right_tab_);
  }

  raw_ptr<content::WebContents, DanglingUntriaged> left_tab_;
  raw_ptr<content::WebContents, DanglingUntriaged> right_tab_;
};

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsVP8) {
  RunsAudioVideoWebRTCCallInTwoTabs("VP8");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsVP9) {
  RunsAudioVideoWebRTCCallInTwoTabs("VP9");
}

#if BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsH264) {
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING)
        << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
           "Skipping WebRtcBrowserTest.RunsAudioVideoWebRTCCallInTwoTabsH264 "
           "(test \"OK\")";
    return;
  }

  RunsAudioVideoWebRTCCallInTwoTabs("H264", true /* prefer_hw_video_codec */);
}

#endif  // BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, TestWebAudioMediaStream) {
  // This tests against crash regressions for the WebAudio-MediaStream
  // integration.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/webrtc/webaudio_crash.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // A sleep is necessary to be able to detect the crash.
  test::SleepInJavascript(tab, 1000);

  ASSERT_FALSE(tab->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferRsaAnswerRsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    false /* prefer_hw_video_codec */,
                                    kKeygenAlgorithmRsa, kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferEcdsaAnswerEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(
      WebRtcTestBase::kUseDefaultVideoCodec, false /* prefer_hw_video_codec */,
      kKeygenAlgorithmEcdsa, kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(
    WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateRsa) {
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(kKeygenAlgorithmRsa);
}

// TODO(https://crbug.com/1291255): Flaky on Linux ASAN.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa \
  DISABLED_RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa
#else
#define MAYBE_RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa \
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcBrowserTest,
    MAYBE_RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferRsaAnswerEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    false /* prefer_hw_video_codec */,
                                    kKeygenAlgorithmRsa, kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferEcdsaAnswerRsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    false /* prefer_hw_video_codec */,
                                    kKeygenAlgorithmEcdsa, kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsGetStatsCallback) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);

  VerifyStatsGeneratedCallback(left_tab_);

  DetectVideoAndHangUp();
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       GetPeerToPeerConnectionsCountChangeFromNetworkService) {
  EXPECT_EQ(0u, GetPeerToPeerConnectionsCountChangeFromNetworkService());

  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);

  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);

  VerifyStatsGeneratedCallback(left_tab_);
  EXPECT_EQ(2u, GetPeerToPeerConnectionsCountChangeFromNetworkService());

  DetectVideoAndHangUp();
  EXPECT_EQ(0u, GetPeerToPeerConnectionsCountChangeFromNetworkService());
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromise) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  CreateDataChannel(left_tab_, "data");
  CreateDataChannel(right_tab_, "data");
  NegotiateCall(left_tab_, right_tab_);

  std::set<std::string> missing_expected_stats;
  for (const std::string& type : GetMandatoryStatsTypes(left_tab_)) {
    missing_expected_stats.insert(type);
  }
  for (const std::string& type : VerifyStatsGeneratedPromise(left_tab_)) {
    missing_expected_stats.erase(type);
  }
  EXPECT_THAT(missing_expected_stats, ::testing::IsEmpty());

  DetectVideoAndHangUp();
}

class WebRtcBrowserTestIdl : public WebRtcBrowserTest {
 public:
  WebRtcBrowserTestIdl() {
    scoped_features_.InitAndEnableFeature(
        blink::features::kWebRtcStatsReportIdl);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTestIdl,
                       RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromiseIDL) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  CreateDataChannel(left_tab_, "data");
  CreateDataChannel(right_tab_, "data");
  NegotiateCall(left_tab_, right_tab_);

  std::set<std::string> missing_expected_stats;
  for (const std::string& type : GetMandatoryStatsTypes(left_tab_)) {
    missing_expected_stats.insert(type);
  }
  for (const std::string& type : VerifyStatsGeneratedPromise(left_tab_)) {
    missing_expected_stats.erase(type);
  }
  EXPECT_THAT(missing_expected_stats, ::testing::IsEmpty());

  DetectVideoAndHangUp();
}

IN_PROC_BROWSER_TEST_F(
    WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsEmitsGatheringStateChange) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);

  std::string ice_gatheringstate =
      ExecuteJavascript("getLastGatheringState()", left_tab_);

  EXPECT_EQ("complete", ice_gatheringstate);
  DetectVideoAndHangUp();
}

IN_PROC_BROWSER_TEST_F(
    WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsEmitsGatheringStateChange_ConnectionCount) {
  EXPECT_EQ(0u, GetPeerToPeerConnectionsCountChangeFromNetworkService());
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);
  EXPECT_EQ(2u, GetPeerToPeerConnectionsCountChangeFromNetworkService());

  std::string ice_gatheringstate =
      ExecuteJavascript("getLastGatheringState()", left_tab_);

  EXPECT_EQ("complete", ice_gatheringstate);
  DetectVideoAndHangUp();
  EXPECT_EQ(0u, GetPeerToPeerConnectionsCountChangeFromNetworkService());
}
