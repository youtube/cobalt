// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "services/media_session/public/mojom/media_session.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom-blink.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_error.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NanSensitiveDoubleEq;
using ::testing::Return;

namespace blink {

namespace {

enum class TestURLScheme {
  kHttp,
  kHttps,
  kFtp,
  kFile,
  kData,
  kBlob,
};

AtomicString SrcSchemeToURL(TestURLScheme scheme) {
  switch (scheme) {
    case TestURLScheme::kHttp:
      return "http://example.com/foo.mp4";
    case TestURLScheme::kHttps:
      return "https://example.com/foo.mp4";
    case TestURLScheme::kFtp:
      return "ftp://example.com/foo.mp4";
    case TestURLScheme::kFile:
      return "file:///foo/bar.mp4";
    case TestURLScheme::kData:
      return "data:video/mp4;base64,XXXXXXX";
    case TestURLScheme::kBlob:
      return "blob:http://example.com/00000000-0000-0000-0000-000000000000";
    default:
      NOTREACHED();
  }
  return g_empty_atom;
}

class MockWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD0(OnTimeUpdate, void());
  MOCK_CONST_METHOD0(Seekable, WebTimeRanges());
  MOCK_METHOD0(OnFrozen, void());
  MOCK_CONST_METHOD0(HasAudio, bool());
  MOCK_CONST_METHOD0(HasVideo, bool());
  MOCK_CONST_METHOD0(Duration, double());
  MOCK_CONST_METHOD0(CurrentTime, double());
  MOCK_CONST_METHOD0(IsEnded, bool());
  MOCK_CONST_METHOD0(GetNetworkState, NetworkState());
  MOCK_CONST_METHOD0(WouldTaintOrigin, bool());
  MOCK_METHOD1(SetLatencyHint, void(double));
  MOCK_METHOD1(SetWasPlayedWithUserActivation, void(bool));
  MOCK_METHOD1(EnabledAudioTracksChanged, void(const WebVector<TrackId>&));
  MOCK_METHOD1(SelectedVideoTrackChanged, void(TrackId*));
  MOCK_METHOD4(
      Load,
      WebMediaPlayer::LoadTiming(LoadType load_type,
                                 const blink::WebMediaPlayerSource& source,
                                 CorsMode cors_mode,
                                 bool is_cache_disabled));
  MOCK_CONST_METHOD0(DidLazyLoad, bool());

  MOCK_METHOD0(GetSrcAfterRedirects, GURL());
};

class WebMediaStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  explicit WebMediaStubLocalFrameClient(std::unique_ptr<WebMediaPlayer> player)
      : player_(std::move(player)) {}

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    DCHECK(player_) << " Empty injected player - already used?";
    return std::move(player_);
  }

 private:
  std::unique_ptr<WebMediaPlayer> player_;
};

// Helper class that provides an implementation of the MediaPlayerObserver mojo
// interface to allow checking that messages sent over mojo are received with
// the right values in the other end.
class TestMediaPlayerObserver final
    : public media::mojom::blink::MediaPlayerObserver {
 public:
  struct OnMetadataChangedResult {
    bool has_audio;
    bool has_video;
    media::MediaContentType media_content_type;
  };

  // Needs to be called from tests after invoking a method from the MediaPlayer
  // mojo interface, so that we have enough time to process the message.
  void WaitUntilReceivedMessage() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // media::mojom::blink::MediaPlayerObserver implementation.
  void OnMediaPlaying() override {
    received_media_playing_ = true;
    run_loop_->Quit();
  }

  void OnMediaPaused(bool stream_ended) override {
    received_media_paused_stream_ended_ = stream_ended;
    run_loop_->Quit();
  }

  void OnMutedStatusChanged(bool muted) override {
    received_muted_status_type_ = muted;
    run_loop_->Quit();
  }

  void OnMediaMetadataChanged(bool has_audio,
                              bool has_video,
                              media::MediaContentType content_type) override {
    // struct OnMetadataChangedResult result{has_audio, has_video,
    // content_type};
    received_metadata_changed_result_ =
        OnMetadataChangedResult{has_audio, has_video, content_type};
    run_loop_->Quit();
  }

  void OnMediaPositionStateChanged(
      ::media_session::mojom::blink::MediaPositionPtr) override {}

  void OnMediaEffectivelyFullscreenChanged(
      blink::WebFullscreenVideoStatus status) override {}

  void OnMediaSizeChanged(const gfx::Size& size) override {
    received_media_size_ = size;
    run_loop_->Quit();
  }

  void OnPictureInPictureAvailabilityChanged(bool available) override {}

  void OnAudioOutputSinkChanged(const WTF::String& hashed_device_id) override {}

  void OnUseAudioServiceChanged(bool uses_audio_service) override {
    received_uses_audio_service_ = uses_audio_service;
    run_loop_->Quit();
  }

  void OnAudioOutputSinkChangingDisabled() override {}

  void OnRemotePlaybackMetadataChange(
      media_session::mojom::blink::RemotePlaybackMetadataPtr
          remote_playback_metadata) override {
    received_remote_playback_metadata_ = std::move(remote_playback_metadata);
    run_loop_->Quit();
  }

  // Getters used from HTMLMediaElementTest.
  bool received_media_playing() const { return received_media_playing_; }

  const absl::optional<bool>& received_media_paused_stream_ended() const {
    return received_media_paused_stream_ended_;
  }

  const absl::optional<bool>& received_muted_status() const {
    return received_muted_status_type_;
  }

  const absl::optional<OnMetadataChangedResult>&
  received_metadata_changed_result() const {
    return received_metadata_changed_result_;
  }

  gfx::Size received_media_size() const { return received_media_size_; }

  bool received_use_audio_service_changed(bool uses_audio_service) const {
    return received_uses_audio_service_.value() == uses_audio_service;
  }

  bool received_remote_playback_metadata(
      media_session::mojom::blink::RemotePlaybackMetadataPtr
          remote_playback_metadata) const {
    return received_remote_playback_metadata_ == remote_playback_metadata;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool received_media_playing_{false};
  absl::optional<bool> received_media_paused_stream_ended_;
  absl::optional<bool> received_muted_status_type_;
  absl::optional<OnMetadataChangedResult> received_metadata_changed_result_;
  gfx::Size received_media_size_{0, 0};
  absl::optional<bool> received_uses_audio_service_;
  media_session::mojom::blink::RemotePlaybackMetadataPtr
      received_remote_playback_metadata_;
};

class TestMediaPlayerHost final : public media::mojom::blink::MediaPlayerHost {
 public:
  void WaitForPlayer() { run_loop_.Run(); }

  // media::mojom::MediaPlayerHost
  void OnMediaPlayerAdded(
      mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>
      /*media_player*/,
      mojo::PendingAssociatedReceiver<media::mojom::blink::MediaPlayerObserver>
          media_player_observer,
      int32_t /*player_id*/) override {
    receiver_.Bind(std::move(media_player_observer));
    run_loop_.Quit();
  }

  TestMediaPlayerObserver& observer() { return observer_; }

 private:
  TestMediaPlayerObserver observer_;
  mojo::AssociatedReceiver<media::mojom::blink::MediaPlayerObserver> receiver_{
      &observer_};
  base::RunLoop run_loop_;
};

enum class MediaTestParam { kAudio, kVideo };

}  // namespace

class HTMLMediaElementTest : public testing::TestWithParam<MediaTestParam> {
 protected:
  void SetUp() override {
    // Sniff the media player pointer to facilitate mocking.
    auto mock_media_player = std::make_unique<MockWebMediaPlayer>();
    media_player_weak_ = mock_media_player->AsWeakPtr();
    media_player_ = mock_media_player.get();

    // Most tests do not care about this call, nor its return value. Those that
    // do will clear this expectation and set custom expectations/returns.
    EXPECT_CALL(*mock_media_player, Seekable())
        .WillRepeatedly(Return(WebTimeRanges()));
    EXPECT_CALL(*mock_media_player, HasAudio()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_media_player, HasVideo()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_media_player, Duration()).WillRepeatedly(Return(1.0));
    EXPECT_CALL(*mock_media_player, CurrentTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_media_player, Load(_, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(WebMediaPlayer::LoadTiming::kImmediate));
    EXPECT_CALL(*mock_media_player, DidLazyLoad).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_media_player, WouldTaintOrigin)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_media_player, GetNetworkState)
        .WillRepeatedly(Return(WebMediaPlayer::kNetworkStateIdle));
    EXPECT_CALL(*mock_media_player, SetLatencyHint(_)).Times(AnyNumber());

    dummy_page_holder_ = std::make_unique<DummyPageHolder>(
        gfx::Size(), nullptr,
        MakeGarbageCollected<WebMediaStubLocalFrameClient>(
            std::move(mock_media_player)));

    if (GetParam() == MediaTestParam::kAudio) {
      media_ = MakeGarbageCollected<HTMLAudioElement>(
          dummy_page_holder_->GetDocument());
    } else {
      media_ = MakeGarbageCollected<HTMLVideoElement>(
          dummy_page_holder_->GetDocument());
    }

    media_->SetMediaPlayerHostForTesting(
        media_player_host_receiver_.BindNewEndpointAndPassDedicatedRemote());
  }

  void WaitForPlayer() {
    Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
    Media()->Play();
    media_player_host_.WaitForPlayer();
  }

  HTMLMediaElement* Media() const { return media_.Get(); }
  void SetCurrentSrc(const AtomicString& src) {
    KURL url(src);
    Media()->current_src_.SetSource(
        url, HTMLMediaElement::SourceMetadata::SourceVisibility::kVisibleToApp);
  }

  MockWebMediaPlayer* MockMediaPlayer() { return media_player_; }

  bool WasAutoplayInitiated() { return Media()->WasAutoplayInitiated(); }

  bool CouldPlayIfEnoughData() { return Media()->CouldPlayIfEnoughData(); }

  bool PotentiallyPlaying() { return Media()->PotentiallyPlaying(); }

  bool ShouldDelayLoadEvent() { return Media()->should_delay_load_event_; }

  void SetReadyState(HTMLMediaElement::ReadyState state) {
    Media()->SetReadyState(state);
  }

  void SetNetworkState(WebMediaPlayer::NetworkState state) {
    Media()->SetNetworkState(state);
  }

  bool MediaIsPlaying() const { return Media()->playing_; }

  void ResetWebMediaPlayer() const { Media()->web_media_player_.reset(); }

  void MediaContextLifecycleStateChanged(mojom::FrameLifecycleState state) {
    Media()->ContextLifecycleStateChanged(state);
  }

  bool MediaShouldBeOpaque() const { return Media()->MediaShouldBeOpaque(); }

  void SetError(MediaError* err) { Media()->MediaEngineError(err); }

  void SimulateHighMediaEngagement() {
    Media()->GetDocument().GetPage()->AddAutoplayFlags(
        mojom::blink::kAutoplayFlagHighMediaEngagement);
  }

  bool HasLazyLoadObserver() const {
    return !!Media()->lazy_load_intersection_observer_;
  }

  bool ControlsVisible() const { return Media()->ShouldShowControls(); }

  bool MediaShouldShowAllControls() const {
    return Media()->ShouldShowAllControls();
  }

  ExecutionContext* GetExecutionContext() const {
    return dummy_page_holder_->GetFrame().DomWindow();
  }

  LocalDOMWindow* GetDomWindow() const {
    return dummy_page_holder_->GetFrame().DomWindow();
  }

 protected:
  // Helpers to call MediaPlayerObserver mojo methods and check their results.
  void NotifyMediaPlaying() {
    media_->DidPlayerStartPlaying();
    media_player_observer().WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMediaPlaying() {
    return media_player_observer().received_media_playing();
  }

  void NotifyMediaPaused(bool stream_ended) {
    media_->DidPlayerPaused(stream_ended);
    media_player_observer().WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMediaPaused(bool stream_ended) {
    return media_player_observer().received_media_paused_stream_ended() ==
           stream_ended;
  }

  void NotifyMutedStatusChange(bool muted) {
    media_->DidPlayerMutedStatusChange(muted);
    media_player_observer().WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMutedStatusChange(bool muted) {
    return media_player_observer().received_muted_status() == muted;
  }

  void NotifyMediaMetadataChanged(bool has_audio,
                                  bool has_video,
                                  media::AudioCodec audio_codec,
                                  media::VideoCodec video_codec,
                                  media::MediaContentType media_content_type,
                                  bool is_encrypted_media) {
    media_->DidMediaMetadataChange(has_audio, has_video, audio_codec,
                                   video_codec, media_content_type,
                                   is_encrypted_media);
    media_player_observer().WaitUntilReceivedMessage();
    // wait for OnRemotePlaybackMetadataChange() to be called.
    if (audio_codec != media::AudioCodec::kUnknown ||
        video_codec != media::VideoCodec::kUnknown) {
      media_player_observer().WaitUntilReceivedMessage();
    }
  }

  bool ReceivedMessageMediaMetadataChanged(
      bool has_audio,
      bool has_video,
      media::MediaContentType media_content_type) {
    const auto& result =
        media_player_observer().received_metadata_changed_result();
    return result->has_audio == has_audio && result->has_video == has_video &&
           result->media_content_type == media_content_type;
  }

  void NotifyMediaSizeChange(const gfx::Size& size) {
    media_->DidPlayerSizeChange(size);
    media_player_observer().WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMediaSizeChange(const gfx::Size& size) {
    return media_player_observer().received_media_size() == size;
  }

  void NotifyUseAudioServiceChanged(bool uses_audio_service) {
    media_->DidUseAudioServiceChange(uses_audio_service);
    media_player_observer().WaitUntilReceivedMessage();
  }

  bool ReceivedMessageUseAudioServiceChanged(bool uses_audio_service) {
    return media_player_observer().received_use_audio_service_changed(
        uses_audio_service);
  }

  void NotifyRemotePlaybackDisabled(bool is_remote_playback_disabled) {
    media_->OnRemotePlaybackDisabled(is_remote_playback_disabled);
    media_player_observer().WaitUntilReceivedMessage();
  }

  bool ReceivedRemotePlaybackMetadataChange(
      media_session::mojom::blink::RemotePlaybackMetadataPtr
          remote_playback_metadata) {
    return media_player_observer().received_remote_playback_metadata(
        std::move(remote_playback_metadata));
  }

  bool WasPlayerDestroyed() const { return !media_player_weak_; }

  // Create a dummy page holder with the given security origin.
  std::unique_ptr<DummyPageHolder> CreatePageWithSecurityOrigin(
      const char* origin,
      bool is_picture_in_picture) {
    // Make another document with the same security origin.

    auto dummy_page_holder = std::make_unique<DummyPageHolder>(
        gfx::Size(), nullptr,
        MakeGarbageCollected<WebMediaStubLocalFrameClient>(
            /*player=*/nullptr));
    Document& document = dummy_page_holder->GetDocument();
    document.domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
        SecurityOrigin::CreateFromString(origin));
    document.domWindow()->set_is_picture_in_picture_window_for_testing(
        is_picture_in_picture);

    return dummy_page_holder;
  }

  // Set the security origin of our window.
  void SetSecurityOrigin(const char* origin) {
    Media()
        ->GetDocument()
        .domWindow()
        ->GetSecurityContext()
        .SetSecurityOriginForTesting(SecurityOrigin::CreateFromString(origin));
  }

  // Move Media() from a document in `old_origin` to  one in `new_origin`, and
  // expect that `should_destroy` matches whether the player is destroyed.  If
  // the player should not be destroyed, then we also move it back to the
  // original document and verify that it works in both directions.
  void MoveElementAndTestPlayerDestruction(
      const char* old_origin,
      const char* new_origin,
      bool should_destroy,
      bool is_new_document_picture_in_picture,
      bool is_old_document_picture_in_picture,
      bool is_new_document_opener,
      bool is_old_document_opener) {
    // The two documents cannot both be opener.
    EXPECT_FALSE(is_new_document_opener && is_old_document_opener);

    SetSecurityOrigin(old_origin);
    WaitForPlayer();
    // Player should not be destroyed yet.
    EXPECT_FALSE(WasPlayerDestroyed());

    auto& original_document = Media()->GetDocument();
    if (is_old_document_picture_in_picture) {
      original_document.domWindow()
          ->set_is_picture_in_picture_window_for_testing(
              is_old_document_picture_in_picture);
    }

    // Make another document with the correct security origin.
    auto new_dummy_page_holder = CreatePageWithSecurityOrigin(
        new_origin, is_new_document_picture_in_picture);
    Document& new_document = new_dummy_page_holder->GetDocument();

    if (is_old_document_opener) {
      new_document.GetFrame()->SetOpener(original_document.GetFrame());
    } else if (is_new_document_opener) {
      original_document.GetFrame()->SetOpener(new_document.GetFrame());
    }

    // Move the element.
    new_document.adoptNode(Media(), ASSERT_NO_EXCEPTION);
    EXPECT_EQ(should_destroy, WasPlayerDestroyed());

    // If the player should be destroyed, then that's everything.
    if (should_destroy)
      return;

    // The move should always work in zero or two directions, so move it back
    // and make sure that the player is retained.
    original_document.adoptNode(Media(), ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(WasPlayerDestroyed());
  }

 private:
  TestMediaPlayerObserver& media_player_observer() {
    return media_player_host_.observer();
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<HTMLMediaElement> media_;

  // Owned by WebMediaStubLocalFrameClient.
  MockWebMediaPlayer* media_player_;
  base::WeakPtr<WebMediaPlayer> media_player_weak_;

  TestMediaPlayerHost media_player_host_;
  mojo::AssociatedReceiver<media::mojom::blink::MediaPlayerHost>
      media_player_host_receiver_{&media_player_host_};
};

INSTANTIATE_TEST_SUITE_P(Audio,
                         HTMLMediaElementTest,
                         testing::Values(MediaTestParam::kAudio));
INSTANTIATE_TEST_SUITE_P(Video,
                         HTMLMediaElementTest,
                         testing::Values(MediaTestParam::kVideo));

TEST_P(HTMLMediaElementTest, effectiveMediaVolume) {
  struct TestData {
    double volume;
    bool muted;
    double effective_volume;
  } test_data[] = {
      {0.0, false, 0.0}, {0.5, false, 0.5}, {1.0, false, 1.0},
      {0.0, true, 0.0},  {0.5, true, 0.0},  {1.0, true, 0.0},
  };

  for (const auto& data : test_data) {
    Media()->setVolume(data.volume);
    Media()->setMuted(data.muted);
    EXPECT_EQ(data.effective_volume, Media()->EffectiveMediaVolume());
  }
}

TEST_P(HTMLMediaElementTest, preloadType) {
  struct TestData {
    bool data_saver_enabled;
    bool is_cellular;
    TestURLScheme src_scheme;
    AtomicString preload_to_set;
    AtomicString preload_expected;
  } test_data[] = {
      // Tests for conditions in which preload type should be overriden to
      // "none".
      {false, false, TestURLScheme::kHttp, "auto", "auto"},
      {true, false, TestURLScheme::kHttps, "auto", "auto"},
      {true, false, TestURLScheme::kFtp, "metadata", "metadata"},
      {false, false, TestURLScheme::kHttps, "auto", "auto"},
      {false, false, TestURLScheme::kFile, "auto", "auto"},
      {false, false, TestURLScheme::kData, "metadata", "metadata"},
      {false, false, TestURLScheme::kBlob, "auto", "auto"},
      {false, false, TestURLScheme::kFile, "none", "none"},
      // Tests for conditions in which preload type should be overriden to
      // "metadata".
      {false, true, TestURLScheme::kHttp, "auto", "metadata"},
      {false, true, TestURLScheme::kHttp, "scheme", "metadata"},
      {false, true, TestURLScheme::kHttp, "none", "none"},
      // Tests that the preload is overriden to "metadata".
      {false, false, TestURLScheme::kHttp, "foo", "metadata"},
  };

  int index = 0;
  for (const auto& data : test_data) {
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(
        data.data_saver_enabled);
    if (data.is_cellular) {
      GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
          true, WebConnectionType::kWebConnectionTypeCellular3G,
          WebEffectiveConnectionType::kTypeUnknown, 1.0, 2.0);
    } else {
      GetNetworkStateNotifier().ClearOverride();
    }
    SetCurrentSrc(SrcSchemeToURL(data.src_scheme));
    Media()->setPreload(data.preload_to_set);

    EXPECT_EQ(data.preload_expected, Media()->preload())
        << "preload type differs at index" << index;
    ++index;
  }
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataRespondsToPlay) {
  EXPECT_FALSE(CouldPlayIfEnoughData());
  Media()->Play();
  EXPECT_TRUE(CouldPlayIfEnoughData());
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataRespondsToEnded) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();

  MockWebMediaPlayer* mock_wmpi =
      reinterpret_cast<MockWebMediaPlayer*>(Media()->GetWebMediaPlayer());
  ASSERT_NE(mock_wmpi, nullptr);
  EXPECT_CALL(*mock_wmpi, IsEnded()).WillRepeatedly(Return(false));
  EXPECT_TRUE(CouldPlayIfEnoughData());

  // Playback can only end once the ready state is above kHaveMetadata.
  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->ended());
  EXPECT_TRUE(CouldPlayIfEnoughData());

  // Now advance current time to duration and verify ended state.
  testing::Mock::VerifyAndClearExpectations(mock_wmpi);
  EXPECT_CALL(*mock_wmpi, CurrentTime())
      .WillRepeatedly(Return(Media()->duration()));
  EXPECT_CALL(*mock_wmpi, IsEnded()).WillRepeatedly(Return(true));
  EXPECT_FALSE(CouldPlayIfEnoughData());
  EXPECT_TRUE(Media()->ended());
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataRespondsToError) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();

  MockWebMediaPlayer* mock_wmpi =
      reinterpret_cast<MockWebMediaPlayer*>(Media()->GetWebMediaPlayer());
  EXPECT_NE(mock_wmpi, nullptr);
  EXPECT_TRUE(CouldPlayIfEnoughData());

  SetReadyState(HTMLMediaElement::kHaveMetadata);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->ended());
  EXPECT_TRUE(CouldPlayIfEnoughData());

  SetError(MakeGarbageCollected<MediaError>(MediaError::kMediaErrDecode, ""));
  EXPECT_FALSE(CouldPlayIfEnoughData());
}

TEST_P(HTMLMediaElementTest, SetLatencyHint) {
  const double kNan = std::numeric_limits<double>::quiet_NaN();

  // Initial value.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  EXPECT_CALL(*MockMediaPlayer(), SetLatencyHint(NanSensitiveDoubleEq(kNan)));

  test::RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Valid value.
  EXPECT_CALL(*MockMediaPlayer(), SetLatencyHint(NanSensitiveDoubleEq(1.0)));
  Media()->setLatencyHint(1.0);

  test::RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Invalid value.
  EXPECT_CALL(*MockMediaPlayer(), SetLatencyHint(NanSensitiveDoubleEq(kNan)));
  Media()->setLatencyHint(-1.0);
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataInfiniteStreamNeverEnds) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), Duration())
      .WillRepeatedly(Return(std::numeric_limits<double>::infinity()));
  EXPECT_CALL(*MockMediaPlayer(), CurrentTime())
      .WillRepeatedly(Return(std::numeric_limits<double>::infinity()));

  SetReadyState(HTMLMediaElement::kHaveMetadata);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->ended());
  EXPECT_TRUE(CouldPlayIfEnoughData());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_DocumentActivation_Low_Gesture) {
  // Setup is the following:
  // - Policy: DocumentUserActivation (aka. unified autoplay)
  // - MEI: low;
  // - Frame received user gesture.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest,
       AutoplayInitiated_DocumentActivation_High_Gesture) {
  // Setup is the following:
  // - Policy: DocumentUserActivation (aka. unified autoplay)
  // - MEI: high;
  // - Frame received user gesture.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  SimulateHighMediaEngagement();
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest,
       AutoplayInitiated_DocumentActivation_High_NoGesture) {
  // Setup is the following:
  // - Policy: DocumentUserActivation (aka. unified autoplay)
  // - MEI: high;
  // - Frame did not receive user gesture.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  SimulateHighMediaEngagement();

  Media()->Play();

  EXPECT_TRUE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_GestureRequired_Gesture) {
  // Setup is the following:
  // - Policy: user gesture is required.
  // - Frame received a user gesture.
  // - MEI doesn't matter as it's not used by the policy.
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_NoGestureRequired_Gesture) {
  // Setup is the following:
  // - Policy: no user gesture is required.
  // - Frame received a user gesture.
  // - MEI doesn't matter as it's not used by the policy.
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kNoUserGestureRequired);
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_NoGestureRequired_NoGesture) {
  // Setup is the following:
  // - Policy: no user gesture is required.
  // - Frame did not receive a user gesture.
  // - MEI doesn't matter as it's not used by the policy.
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kNoUserGestureRequired);

  Media()->Play();

  EXPECT_TRUE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest,
       DeferredMediaPlayerLoadDoesNotDelayWindowLoadEvent) {
  // Source isn't really important, we just need something to let load algorithm
  // run up to the point of calling WebMediaPlayer::Load().
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));

  // WebMediaPlayer will signal that it will defer loading to some later time.
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());
  EXPECT_CALL(*MockMediaPlayer(), Load(_, _, _, _))
      .WillOnce(Return(WebMediaPlayer::LoadTiming::kDeferred));

  // Window's 'load' event starts out "delayed".
  EXPECT_TRUE(ShouldDelayLoadEvent());
  Media()->load();
  test::RunPendingTasks();

  // No longer delayed because WMP loading is deferred.
  EXPECT_FALSE(ShouldDelayLoadEvent());
}

TEST_P(HTMLMediaElementTest, ImmediateMediaPlayerLoadDoesDelayWindowLoadEvent) {
  // Source isn't really important, we just need something to let load algorithm
  // run up to the point of calling WebMediaPlayer::Load().
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));

  // WebMediaPlayer will signal that it will do the load immediately.
  EXPECT_CALL(*MockMediaPlayer(), Load(_, _, _, _))
      .WillOnce(Return(WebMediaPlayer::LoadTiming::kImmediate));

  // Window's 'load' event starts out "delayed".
  EXPECT_TRUE(ShouldDelayLoadEvent());
  Media()->load();
  test::RunPendingTasks();

  // Still delayed because WMP loading is not deferred.
  EXPECT_TRUE(ShouldDelayLoadEvent());
}

TEST_P(HTMLMediaElementTest, DefaultTracksAreEnabled) {
  // Default tracks should start enabled, not be created then enabled.
  EXPECT_CALL(*MockMediaPlayer(), EnabledAudioTracksChanged(_)).Times(0);
  EXPECT_CALL(*MockMediaPlayer(), SelectedVideoTrackChanged(_)).Times(0);

  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  ASSERT_EQ(1u, Media()->audioTracks().length());
  ASSERT_EQ(1u, Media()->videoTracks().length());
  EXPECT_TRUE(Media()->audioTracks().AnonymousIndexedGetter(0)->enabled());
  EXPECT_TRUE(Media()->videoTracks().AnonymousIndexedGetter(0)->selected());
}

// Ensure a visibility observer is created for lazy loading.
TEST_P(HTMLMediaElementTest, VisibilityObserverCreatedForLazyLoad) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), DidLazyLoad()).WillRepeatedly(Return(true));

  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_EQ(HasLazyLoadObserver(), GetParam() == MediaTestParam::kVideo);
}

TEST_P(HTMLMediaElementTest, DomInteractive) {
  EXPECT_FALSE(Media()->GetDocument().GetTiming().DomInteractive().is_null());
}

TEST_P(HTMLMediaElementTest, ContextFrozen) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  // First, set frozen but with auto resume.
  EXPECT_CALL((*MockMediaPlayer()), OnFrozen());
  EXPECT_FALSE(Media()->paused());
  GetExecutionContext()->SetLifecycleState(
      mojom::FrameLifecycleState::kFrozenAutoResumeMedia);
  EXPECT_TRUE(Media()->paused());
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Now, if we set back to running the media should auto resume.
  GetExecutionContext()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  EXPECT_FALSE(Media()->paused());

  // Then set to frozen without auto resume.
  EXPECT_CALL((*MockMediaPlayer()), OnFrozen());
  GetExecutionContext()->SetLifecycleState(mojom::FrameLifecycleState::kFrozen);
  EXPECT_TRUE(Media()->paused());
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Now, the media should stay paused.
  GetExecutionContext()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  EXPECT_TRUE(Media()->paused());
}

TEST_P(HTMLMediaElementTest, GcMarkingNoAllocWebTimeRanges) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  auto* thread_state = ThreadState::Current();
  ThreadState::NoAllocationScope no_allocation_scope(thread_state);
  EXPECT_FALSE(thread_state->IsAllocationAllowed());
  // Use of TimeRanges is not allowed during GC marking (crbug.com/970150)
#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(MakeGarbageCollected<TimeRanges>(0, 0), "");
#endif  // DCHECK_IS_ON()
  // Instead of using TimeRanges, WebTimeRanges can be used without GC
  Vector<WebTimeRanges> ranges;
  ranges.emplace_back();
  ranges[0].emplace_back(0, 0);
}

// Reproduce crbug.com/970150
TEST_P(HTMLMediaElementTest, GcMarkingNoAllocHasActivity) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveFutureData);
  SetError(MakeGarbageCollected<MediaError>(MediaError::kMediaErrDecode, ""));

  EXPECT_FALSE(Media()->paused());

  auto* thread_state = ThreadState::Current();
  ThreadState::NoAllocationScope no_allocation_scope(thread_state);
  EXPECT_FALSE(thread_state->IsAllocationAllowed());
  Media()->HasPendingActivity();
}

TEST_P(HTMLMediaElementTest, CapturesRedirectedSrc) {
  // Verify that the element captures the redirected URL.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();
  test::RunPendingTasks();

  // Should start at the original.
  EXPECT_EQ(Media()->downloadURL(), Media()->currentSrc());

  KURL redirected_url("https://redirected.com");
  EXPECT_CALL(*MockMediaPlayer(), GetSrcAfterRedirects())
      .WillRepeatedly(Return(GURL(redirected_url)));
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  EXPECT_EQ(Media()->downloadURL(), redirected_url);
}

TEST_P(HTMLMediaElementTest, EmptyRedirectedSrcUsesOriginal) {
  // If the player returns an empty URL for the redirected src, then the element
  // should continue using currentSrc().
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();
  test::RunPendingTasks();
  EXPECT_EQ(Media()->downloadURL(), Media()->currentSrc());
  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_EQ(Media()->downloadURL(), Media()->currentSrc());
}

TEST_P(HTMLMediaElementTest, NoPendingActivityEvenIfBeforeMetadata) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  MockWebMediaPlayer* mock_wmpi =
      reinterpret_cast<MockWebMediaPlayer*>(Media()->GetWebMediaPlayer());
  EXPECT_CALL(*mock_wmpi, WouldTaintOrigin()).WillRepeatedly(Return(true));
  EXPECT_NE(mock_wmpi, nullptr);

  EXPECT_TRUE(MediaShouldBeOpaque());
  EXPECT_TRUE(Media()->HasPendingActivity());
  SetNetworkState(WebMediaPlayer::kNetworkStateIdle);
  EXPECT_FALSE(Media()->HasPendingActivity());
  EXPECT_TRUE(MediaShouldBeOpaque());
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_DurationChange) {
  // Prepare the player.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  // Change from no duration to 1s will trigger OnTimeUpdate().
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->DurationChanged(1, false);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Change from 1s to 2s will trigger OnTimeUpdate().
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->DurationChanged(2, false);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // No duration change -> no OnTimeUpdate().
  Media()->DurationChanged(2, false);
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_PlayPauseSetRate) {
  // Prepare the player.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->Play();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setPlaybackRate(0.5);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate()).Times(testing::AtLeast(1));
  Media()->pause();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setPlaybackRate(1.5);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->Play();
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_ReadyState) {
  // Prepare the player.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  // The ready state affects the progress of media time, so the player should
  // be kept informed.
  EXPECT_CALL(*MockMediaPlayer(), GetSrcAfterRedirects)
      .WillRepeatedly(Return(GURL()));
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  SetReadyState(HTMLMediaElement::kHaveCurrentData);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  SetReadyState(HTMLMediaElement::kHaveFutureData);
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_Seeking) {
  // Prepare the player and seekable ranges -- setCurrentTime()'s prerequisites.
  WebTimeRanges seekable;
  seekable.Add(0, 3);
  EXPECT_CALL(*MockMediaPlayer(), Seekable).WillRepeatedly(Return(seekable));
  EXPECT_CALL(*MockMediaPlayer(), GetSrcAfterRedirects)
      .WillRepeatedly(Return(GURL()));
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveCurrentData);

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setCurrentTime(1);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), Seekable).WillRepeatedly(Return(seekable));
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setCurrentTime(2);
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_InitiallyTrue) {
  // ShowPosterFlag should be true upon initialization
  EXPECT_TRUE(Media()->IsShowPosterFlagSet());

  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  EXPECT_TRUE(Media()->IsShowPosterFlagSet());

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  // ShowPosterFlag should still be true once video is ready to play
  EXPECT_TRUE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterPlay) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  Media()->Play();
  test::RunPendingTasks();

  // ShowPosterFlag should be false once video is playing
  ASSERT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterSeek) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  ASSERT_NE(Media()->duration(), 0.0);
  Media()->setCurrentTime(Media()->duration() / 2);
  test::RunPendingTasks();

  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterAutoPlay) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  Media()->SetBooleanAttribute(html_names::kAutoplayAttr, true);
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  ASSERT_TRUE(WasAutoplayInitiated());
  ASSERT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterPlayBeforeReady) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));

  // Initially we have nothing, we're not playing, trying to play, and the 'show
  // poster' flag is set
  EXPECT_EQ(Media()->getReadyState(), HTMLMediaElement::kHaveNothing);
  EXPECT_TRUE(Media()->paused());
  EXPECT_FALSE(PotentiallyPlaying());
  EXPECT_TRUE(Media()->IsShowPosterFlagSet());

  // Attempt to begin playback
  Media()->Play();
  test::RunPendingTasks();

  // We still have no data, but we're not paused, and the 'show poster' flag is
  // not set
  EXPECT_EQ(Media()->getReadyState(), HTMLMediaElement::kHaveNothing);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(PotentiallyPlaying());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());

  // Pretend we have data to begin playback
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  // We should have data, be playing, and the show poster flag should be unset
  EXPECT_EQ(Media()->getReadyState(), HTMLMediaElement::kHaveFutureData);
  EXPECT_FALSE(Media()->paused());
  EXPECT_TRUE(PotentiallyPlaying());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, SendMediaPlayingToObserver) {
  WaitForPlayer();

  NotifyMediaPlaying();
  EXPECT_TRUE(ReceivedMessageMediaPlaying());
}

TEST_P(HTMLMediaElementTest, SendMediaPausedToObserver) {
  WaitForPlayer();

  NotifyMediaPaused(true);
  EXPECT_TRUE(ReceivedMessageMediaPaused(true));

  NotifyMediaPaused(false);
  EXPECT_TRUE(ReceivedMessageMediaPaused(false));
}

TEST_P(HTMLMediaElementTest, SendMutedStatusChangeToObserver) {
  WaitForPlayer();

  NotifyMutedStatusChange(true);
  EXPECT_TRUE(ReceivedMessageMutedStatusChange(true));

  NotifyMutedStatusChange(false);
  EXPECT_TRUE(ReceivedMessageMutedStatusChange(false));
}

TEST_P(HTMLMediaElementTest, SendMediaMetadataChangedToObserver) {
  WaitForPlayer();

  bool has_audio = false;
  bool has_video = true;
  bool is_encrypted_media = false;
  media::AudioCodec audio_codec = media::AudioCodec::kUnknown;
  media::VideoCodec video_codec = media::VideoCodec::kUnknown;
  media::MediaContentType media_content_type =
      media::MediaContentType::Transient;

  NotifyMediaMetadataChanged(has_audio, has_video, audio_codec, video_codec,
                             media_content_type, is_encrypted_media);
  EXPECT_TRUE(ReceivedMessageMediaMetadataChanged(has_audio, has_video,
                                                  media_content_type));
  // Change values and test again.
  has_audio = true;
  has_video = false;
  media_content_type = media::MediaContentType::OneShot;
  NotifyMediaMetadataChanged(has_audio, has_video, audio_codec, video_codec,
                             media_content_type, is_encrypted_media);
  EXPECT_TRUE(ReceivedMessageMediaMetadataChanged(has_audio, has_video,
                                                  media_content_type));

  // Send codecs
  audio_codec = media::AudioCodec::kAAC;
  video_codec = media::VideoCodec::kH264;
  NotifyMediaMetadataChanged(has_audio, has_video, audio_codec, video_codec,
                             media_content_type, is_encrypted_media);
  EXPECT_TRUE(ReceivedRemotePlaybackMetadataChange(
      media_session::mojom::blink::RemotePlaybackMetadata::New(
          WTF::String(media::GetCodecName(video_codec)),
          WTF::String(media::GetCodecName(audio_codec)), false, false,
          WTF::String(), is_encrypted_media)));
}

TEST_P(HTMLMediaElementTest, SendMediaSizeChangeToObserver) {
  WaitForPlayer();

  const gfx::Size kTestMediaSizeChangedValue(16, 9);
  NotifyMediaSizeChange(kTestMediaSizeChangedValue);
  EXPECT_TRUE(ReceivedMessageMediaSizeChange(kTestMediaSizeChangedValue));
}

TEST_P(HTMLMediaElementTest, SendRemotePlaybackMetadataChangeToObserver) {
  WaitForPlayer();
  media::VideoCodec video_codec = media::VideoCodec::kH264;
  media::AudioCodec audio_codec = media::AudioCodec::kAAC;
  bool is_remote_playback_disabled = true;
  bool is_remote_playback_started = false;
  bool is_encrypted_media = false;
  NotifyMediaMetadataChanged(true, true, audio_codec, video_codec,
                             media::MediaContentType::Transient,
                             is_encrypted_media);
  NotifyRemotePlaybackDisabled(is_remote_playback_disabled);
  EXPECT_TRUE(ReceivedRemotePlaybackMetadataChange(
      media_session::mojom::blink::RemotePlaybackMetadata::New(
          WTF::String(media::GetCodecName(video_codec)),
          WTF::String(media::GetCodecName(audio_codec)),
          is_remote_playback_disabled, is_remote_playback_started,
          WTF::String(), is_encrypted_media)));
}

TEST_P(HTMLMediaElementTest, SendUseAudioServiceChangedToObserver) {
  WaitForPlayer();

  NotifyUseAudioServiceChanged(false);
  EXPECT_TRUE(ReceivedMessageUseAudioServiceChanged(false));

  NotifyUseAudioServiceChanged(true);
  EXPECT_TRUE(ReceivedMessageUseAudioServiceChanged(true));
}

TEST_P(HTMLMediaElementTest,
       ControlsVisibilityUserChoiceOverridesControlsAttr) {
  // Enable scripts to prevent controls being shown due to no scripts.
  Media()->GetDocument().GetSettings()->SetScriptEnabled(true);

  // Setting the controls attribute to true should show the controls.
  Media()->SetBooleanAttribute(html_names::kControlsAttr, true);
  EXPECT_TRUE(ControlsVisible());

  // Setting it to false should hide them.
  Media()->SetBooleanAttribute(html_names::kControlsAttr, false);
  EXPECT_FALSE(ControlsVisible());

  // If the user explicitly shows them, that should override the controls
  // attribute.
  Media()->SetUserWantsControlsVisible(true);
  EXPECT_TRUE(ControlsVisible());

  // Setting the controls attribute to false again should do nothing.
  Media()->SetBooleanAttribute(html_names::kControlsAttr, false);
  EXPECT_TRUE(ControlsVisible());

  // If the user explicitly hides the controls, that should also override any
  // controls attribute setting.
  Media()->SetUserWantsControlsVisible(false);
  EXPECT_FALSE(ControlsVisible());

  // So setting the controls attribute to true should not show the controls.
  Media()->SetBooleanAttribute(html_names::kControlsAttr, true);
  EXPECT_FALSE(ControlsVisible());
}

TEST_P(HTMLMediaElementTest,
       MediaShouldShowAllControlsDependsOnControlslistAttr) {
  // Enable scripts to prevent controls being shown due to no scripts.
  Media()->GetDocument().GetSettings()->SetScriptEnabled(true);

  // Setting the controls attribute to true should show the controls.
  Media()->SetBooleanAttribute(html_names::kControlsAttr, true);
  EXPECT_TRUE(MediaShouldShowAllControls());

  // Setting the controlsList attribute to a valid value should not show the
  // controls.
  Media()->setAttribute(blink::html_names::kControlslistAttr, "nofullscreen");
  EXPECT_FALSE(MediaShouldShowAllControls());

  // Removing the controlsList attribute should show the controls.
  Media()->removeAttribute(blink::html_names::kControlslistAttr);
  EXPECT_TRUE(MediaShouldShowAllControls());

  // Setting the controlsList attribute to an invalid value should still show
  // the controls.
  Media()->setAttribute(blink::html_names::kControlslistAttr, "foo");
  EXPECT_TRUE(MediaShouldShowAllControls());

  // Setting the controlsList attribute to another valid value should not show
  // the controls.
  Media()->setAttribute(blink::html_names::kControlslistAttr, "noplaybackrate");
  EXPECT_FALSE(MediaShouldShowAllControls());

  // If the user explicitly shows them, that should override the controlsList
  // attribute.
  Media()->SetUserWantsControlsVisible(true);
  EXPECT_TRUE(MediaShouldShowAllControls());
}

TEST_P(HTMLMediaElementTest,
       DestroyMediaPlayerWhenSwitchingSameOriginDocumentsIfReuseIsNotEnabled) {
  // Ensure that the WebMediaPlayer is destroyed when moving to a same-origin
  // document, if `kDocumentPictureInPictureAPI` is not enabled.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(false);
  MoveElementAndTestPlayerDestruction(
      "https://a.com", "https://a.com",
      /*should_destroy=*/true,
      /*is_new_document_picture_in_picture=*/false,
      /*is_old_document_picture_in_picture=*/false,
      /*is_new_document_opener=*/false,
      /*is_old_document_opener=*/false);
}

TEST_P(
    HTMLMediaElementTest,
    DestroyMediaPlayerWhenSwitchingDifferentOriginDocumentsIfReuseIsNotEnabled) {
  // Ensure that the WebMediaPlayer is destroyed when moving to a new origin
  // document, if `kDocumentPictureInPictureAPI` is not enabled.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(false);
  MoveElementAndTestPlayerDestruction(
      "https://a.com", "https://b.com",
      /*should_destroy=*/true,
      /*is_new_document_picture_in_picture=*/false,
      /*is_old_document_picture_in_picture=*/false,
      /*is_new_document_opener=*/false,
      /*is_old_document_opener=*/false);
}

TEST_P(
    HTMLMediaElementTest,
    DoNotDestroyMediaPlayerWhenSwitchingSameOriginDocumentsIfReuseIsEnabled) {
  // Ensure that the WebMediaPlayer is re-used when moving to a same-origin
  // document, if `kDocumentPictureInPictureAPI` is enabled.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  MoveElementAndTestPlayerDestruction(
      "https://a.com", "https://a.com",
      /*should_destroy=*/false,
      /*is_new_document_picture_in_picture=*/true,
      /*is_old_document_picture_in_picture=*/false,
      /*is_new_document_opener=*/false,
      /*is_old_document_opener=*/true);
}

TEST_P(
    HTMLMediaElementTest,
    DestroyMediaPlayerWhenSwitchingSameOriginDocumentsIfNewDocumentIsNotInPictureInPicture) {
  // Ensure that the WebMediaPlayer is destroyed when moving to a same-origin
  // document when the new frame is in picture in picture window, if
  // 'kDocumentPictureInPictureAPI' is enabled.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  MoveElementAndTestPlayerDestruction(
      "https//a.com", "https://a.com",
      /*should_destroy=*/true,
      /*is_new_document_picture_in_picture=*/false,
      /*is_old_document_picture_in_picture=*/false,
      /*is_new_document_opener=*/false,
      /*is_old_document_opener=*/false);
}

TEST_P(
    HTMLMediaElementTest,
    DoNotDestroyMediaPlayerWhenSwitchingSameOriginDocumentsIfOldDocumentIsInPictureInPicture) {
  // Ensure that the WebMediaPlayer is not destroyed when moving to a
  // same-origin document when the old document is in picture-in-picture window,
  // if 'kDocumentPictureInPictureAPI' is enabled.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  MoveElementAndTestPlayerDestruction(
      "https://a.com", "https://a.com",
      /*should_destroy=*/false,
      /*is_new_document_picture_in_picture=*/false,
      /*is_old_document_picture_in_picture=*/true,
      /*is_new_document_opener=*/true,
      /*is_old_document_opener=*/false);
}

TEST_P(
    HTMLMediaElementTest,
    DestroyMediaPlayerWhenSwitchingSameOriginDocumentsIfNotOpenerPipRelation) {
  // Ensure that the WebMediaPlayer is destroyed when moving to a
  // same-origin document when the new document is in picture-in-picture window
  // but not opened from old document.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  MoveElementAndTestPlayerDestruction(
      "https://a.com", "https://a.com",
      /*should_destroy=*/true,
      /*is_new_document_picture_in_picture=*/true,
      /*is_old_document_picture_in_picture=*/false,
      /*is_new_document_opener=*/false,
      /*is_old_document_opener=*/false);
}

TEST_P(
    HTMLMediaElementTest,
    DestroyMediaPlayerWhenSwitchingDifferentOriginDocumentsIfReuseIsEnabled) {
  // Ensure that the WebMediaPlayer is destroyed when moving to a new origin
  // document, if `kDocumentPictureInPictureAPI` is enabled. Re-use should only
  // occur if it's a same-origin document.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  MoveElementAndTestPlayerDestruction(
      "https://a.com", "https://b.com",
      /*should_destroy=*/true,
      /*is_new_document_picture_in_picture=*/true,
      /*is_old_document_picture_in_picture=*/false,
      /*is_new_document_opener=*/false,
      /*is_old_document_opener=*/true);
}

TEST_P(HTMLMediaElementTest,
       DestroyMediaPlayerWhenUnloadingOpenerIfReuseIsEnabled) {
  // Ensure that the WebMediaPlayer is re-used, that navigating the opener away
  // causes the player to be destroyed.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  const char* origin = "https://a.com";
  SetSecurityOrigin(origin);
  WaitForPlayer();
  auto new_dummy_page_holder =
      CreatePageWithSecurityOrigin(origin, /*is_picture_in_picture=*/true);
  new_dummy_page_holder->GetDocument().GetFrame()->SetOpener(
      Media()->GetDocument().GetFrame());
  new_dummy_page_holder->GetDocument().adoptNode(Media(), ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(WasPlayerDestroyed());
  GetDomWindow()->FrameDestroyed();
  EXPECT_TRUE(WasPlayerDestroyed());
}

TEST_P(HTMLMediaElementTest,
       CreateMediaPlayerAfterMovingElementUsesOpenerFrameIfReuseIsEnabled) {
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);
  // Move the element before creating the player.
  const char* origin = "https://a.com";
  SetSecurityOrigin(origin);
  auto new_dummy_page_holder =
      CreatePageWithSecurityOrigin(origin, /*is_picture_in_picture=*/true);
  Document& new_document = new_dummy_page_holder->GetDocument();
  LocalFrame* old_frame = Media()->GetDocument().GetFrame();
  new_document.GetFrame()->SetOpener(old_frame);
  EXPECT_EQ(old_frame, Media()->LocalFrameForPlayer());
  new_document.adoptNode(Media(), ASSERT_NO_EXCEPTION);
  // The element should still use the original frame.
  EXPECT_EQ(old_frame, Media()->LocalFrameForPlayer());
}

TEST_P(HTMLMediaElementTest,
       CreateMediaPlayerAfterMovingElementUsesNewFrameIfReuseIsNotEnabled) {
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(false);
  // Move the element before creating the player.
  const char* origin = "https://a.com";
  SetSecurityOrigin(origin);
  auto new_dummy_page_holder =
      CreatePageWithSecurityOrigin(origin, /*is_picture_in_picture=*/false);
  Document& new_document = new_dummy_page_holder->GetDocument();
  LocalFrame* old_frame = Media()->GetDocument().GetFrame();
  EXPECT_EQ(old_frame, Media()->LocalFrameForPlayer());
  new_document.adoptNode(Media(), ASSERT_NO_EXCEPTION);
  // The element should no longer use the original frame.
  EXPECT_NE(old_frame, Media()->LocalFrameForPlayer());
}

TEST_P(HTMLMediaElementTest, PlayedWithoutUserActivation) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), SetWasPlayedWithUserActivation(false));
  Media()->Play();
}

TEST_P(HTMLMediaElementTest, PlayedWithUserActivation) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  EXPECT_CALL(*MockMediaPlayer(), SetWasPlayedWithUserActivation(true));
  Media()->Play();
}

TEST_P(HTMLMediaElementTest, PlayedWithUserActivationBeforeLoad) {
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  EXPECT_CALL(*MockMediaPlayer(), SetWasPlayedWithUserActivation(_)).Times(0);
  Media()->Play();
}

TEST_P(HTMLMediaElementTest, CanFreezeWithoutMediaPlayerAttached) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), SetWasPlayedWithUserActivation(false));
  Media()->Play();

  ResetWebMediaPlayer();
  EXPECT_FALSE(Media()->GetWebMediaPlayer());
  EXPECT_TRUE(MediaIsPlaying());

  // Freeze with auto resume.
  MediaContextLifecycleStateChanged(
      mojom::FrameLifecycleState::kFrozenAutoResumeMedia);

  EXPECT_FALSE(MediaIsPlaying());
}

TEST_P(HTMLMediaElementTest, CanFreezeWithMediaPlayerAttached) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), SetWasPlayedWithUserActivation(false));
  EXPECT_CALL(*MockMediaPlayer(), OnFrozen());
  Media()->Play();

  EXPECT_TRUE(Media()->GetWebMediaPlayer());
  EXPECT_TRUE(MediaIsPlaying());

  // Freeze with auto resume.
  MediaContextLifecycleStateChanged(
      mojom::FrameLifecycleState::kFrozenAutoResumeMedia);

  EXPECT_FALSE(MediaIsPlaying());
}

}  // namespace blink
