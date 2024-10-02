// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"

#include <memory>

#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/bindings/modules/v8/v8_document_picture_in_picture_options.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using ::testing::_;

namespace blink {

namespace {
#if !BUILDFLAG(IS_ANDROID)
KURL GetOpenerURL() {
  return KURL("https://example.com/");
}

// Should the PiP window get a copy of the style sheets from the opener?
enum class CopyStyleSheetOptions {
  kNo,
  kYes,
};

LocalDOMWindow* OpenDocumentPictureInPictureWindow(
    V8TestingScope& v8_scope,
    Document& document,
    CopyStyleSheetOptions copyStyleSheets,
    KURL opener_url = GetOpenerURL()) {
  auto& controller = PictureInPictureControllerImpl::From(document);
  EXPECT_EQ(nullptr, controller.pictureInPictureWindow());

  // Enable the DocumentPictureInPictureAPI flag.
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);

  // Get past the LocalDOMWindow::isSecureContext() check.
  document.domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      nullptr);
  document.domWindow()->GetSecurityContext().SetSecurityOrigin(
      SecurityOrigin::Create(opener_url));

  // Get past the BindingSecurity::ShouldAllowAccessTo() check.
  ScriptState* script_state = ToScriptStateForMainWorld(document.GetFrame());
  ScriptState::Scope entered_context_scope(script_state);

  // Create the DocumentPictureInPictureOptions.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "DocumentPictureInPicture", "requestWindow");

  v8::Local<v8::Object> v8_object = v8::Object::New(v8_scope.GetIsolate());
  v8_object
      ->Set(v8_scope.GetContext(), V8String(v8_scope.GetIsolate(), "width"),
            v8::Number::New(v8_scope.GetIsolate(), 640))
      .Check();
  v8_object
      ->Set(v8_scope.GetContext(), V8String(v8_scope.GetIsolate(), "height"),
            v8::Number::New(v8_scope.GetIsolate(), 320))
      .Check();
  if (copyStyleSheets == CopyStyleSheetOptions::kYes) {
    v8_object
        ->Set(v8_scope.GetContext(),
              V8String(v8_scope.GetIsolate(), "copyStyleSheets"),
              v8::Number::New(v8_scope.GetIsolate(), true))
        .Check();
  }
  DocumentPictureInPictureOptions* options =
      DocumentPictureInPictureOptions::Create(resolver->Promise().GetIsolate(),
                                              v8_object, exception_state);

  // Set a base URL for the opener window.
  document.SetBaseURLOverride(opener_url);
  EXPECT_EQ(opener_url.GetString(), document.BaseURL().GetString());

  controller.CreateDocumentPictureInPictureWindow(
      script_state, *document.domWindow(), options, resolver, exception_state);

  return controller.documentPictureInPictureWindow();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

viz::SurfaceId TestSurfaceId() {
  // Use a fake but valid viz::SurfaceId.
  return {viz::FrameSinkId(1, 1),
          viz::LocalSurfaceId(
              11, base::UnguessableToken::CreateForTesting(0x111111, 0))};
}

// The MockPictureInPictureSession implements a PictureInPicture session in the
// same process as the test and guarantees that the callbacks are called in
// order for the events to be fired.
class MockPictureInPictureSession
    : public mojom::blink::PictureInPictureSession {
 public:
  MockPictureInPictureSession(
      mojo::PendingReceiver<mojom::blink::PictureInPictureSession> receiver)
      : receiver_(this, std::move(receiver)) {
    ON_CALL(*this, Stop(_)).WillByDefault([](StopCallback callback) {
      std::move(callback).Run();
    });
  }
  ~MockPictureInPictureSession() override = default;

  MOCK_METHOD(void, Stop, (StopCallback));
  MOCK_METHOD(void,
              Update,
              (uint32_t,
               mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
               const viz::SurfaceId&,
               const gfx::Size&,
               bool));

 private:
  mojo::Receiver<mojom::blink::PictureInPictureSession> receiver_;
};

// The MockPictureInPictureService implements the PictureInPicture service in
// the same process as the test and guarantees that the callbacks are called in
// order for the events to be fired.
class MockPictureInPictureService
    : public mojom::blink::PictureInPictureService {
 public:
  MockPictureInPictureService() {
    // Setup default implementations.
    ON_CALL(*this, StartSession(_, _, _, _, _, _, _, _))
        .WillByDefault(testing::Invoke(
            this, &MockPictureInPictureService::StartSessionInternal));
  }

  MockPictureInPictureService(const MockPictureInPictureService&) = delete;
  MockPictureInPictureService& operator=(const MockPictureInPictureService&) =
      delete;

  ~MockPictureInPictureService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::PictureInPictureService>(
        std::move(handle)));

    session_ = std::make_unique<MockPictureInPictureSession>(
        session_remote_.InitWithNewPipeAndPassReceiver());
  }

  MOCK_METHOD(
      void,
      StartSession,
      (uint32_t,
       mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
       const viz::SurfaceId&,
       const gfx::Size&,
       bool,
       mojo::PendingRemote<mojom::blink::PictureInPictureSessionObserver>,
       const gfx::Rect&,
       StartSessionCallback));

  MockPictureInPictureSession& Session() { return *session_.get(); }

  void StartSessionInternal(
      uint32_t,
      mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
      const viz::SurfaceId&,
      const gfx::Size&,
      bool,
      mojo::PendingRemote<mojom::blink::PictureInPictureSessionObserver>,
      const gfx::Rect& source_bounds,
      StartSessionCallback callback) {
    source_bounds_ = source_bounds;
    std::move(callback).Run(std::move(session_remote_), gfx::Size());
  }

  const gfx::Rect& source_bounds() const { return source_bounds_; }

 private:
  mojo::Receiver<mojom::blink::PictureInPictureService> receiver_{this};
  std::unique_ptr<MockPictureInPictureSession> session_;
  mojo::PendingRemote<mojom::blink::PictureInPictureSession> session_remote_;
  gfx::Rect source_bounds_;
};

class PictureInPictureControllerFrameClient
    : public test::MediaStubLocalFrameClient {
 public:
  static PictureInPictureControllerFrameClient* Create(
      std::unique_ptr<WebMediaPlayer> player) {
    return MakeGarbageCollected<PictureInPictureControllerFrameClient>(
        std::move(player));
  }

  explicit PictureInPictureControllerFrameClient(
      std::unique_ptr<WebMediaPlayer> player)
      : test::MediaStubLocalFrameClient(std::move(player)) {}

  PictureInPictureControllerFrameClient(
      const PictureInPictureControllerFrameClient&) = delete;
  PictureInPictureControllerFrameClient& operator=(
      const PictureInPictureControllerFrameClient&) = delete;
};

class PictureInPictureControllerPlayer final : public EmptyWebMediaPlayer {
 public:
  PictureInPictureControllerPlayer() = default;

  PictureInPictureControllerPlayer(const PictureInPictureControllerPlayer&) =
      delete;
  PictureInPictureControllerPlayer& operator=(
      const PictureInPictureControllerPlayer&) = delete;

  ~PictureInPictureControllerPlayer() override = default;

  double Duration() const override {
    if (infinity_duration_)
      return std::numeric_limits<double>::infinity();
    return EmptyWebMediaPlayer::Duration();
  }
  ReadyState GetReadyState() const override { return kReadyStateHaveMetadata; }
  bool HasVideo() const override { return true; }
  void OnRequestPictureInPicture() override { surface_id_ = TestSurfaceId(); }
  absl::optional<viz::SurfaceId> GetSurfaceId() override { return surface_id_; }

  void set_infinity_duration(bool value) { infinity_duration_ = value; }

 private:
  bool infinity_duration_ = false;
  absl::optional<viz::SurfaceId> surface_id_;
};

class PictureInPictureTestWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  explicit PictureInPictureTestWebFrameClient(
      std::unique_ptr<WebMediaPlayer> web_media_player)
      : web_media_player_(std::move(web_media_player)) {}

  WebMediaPlayer* CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings* settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override {
    return web_media_player_.release();
  }

 private:
  std::unique_ptr<WebMediaPlayer> web_media_player_;
};

// PictureInPictureController tests that require a Widget.
// Video PiP tests typically do, while Document PiP tests typically do not.
// If you need to mock the ChromeClient, then this is not the right test harness
// for you. If you need to mock the client and have a Widget, then you'll
// probably need to modify `WebViewHelper`.
class PictureInPictureControllerTestWithWidget : public RenderingTest {
 public:
  void SetUp() override {
    client_ = std::make_unique<PictureInPictureTestWebFrameClient>(
        std::make_unique<PictureInPictureControllerPlayer>());

    helper_.Initialize(client_.get());

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_,
        WTF::BindRepeating(&MockPictureInPictureService::Bind,
                           WTF::Unretained(&mock_service_)));

    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetDocument().body()->AppendChild(video_);
    Video()->SetReadyState(HTMLMediaElement::ReadyState::kHaveMetadata);
    layer_ = cc::Layer::Create();
    Video()->SetCcLayerForTesting(layer_.get());

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.find("MediaSource") != std::string::npos) {
      MediaStreamComponentVector dummy_tracks;
      auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
          dummy_tracks, dummy_tracks);
      Video()->SetSrcObjectVariant(descriptor);
    } else {
      Video()->SetSrc("http://example.com/foo.mp4");
    }

    test::RunPendingTasks();
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_, {});
    RenderingTest::TearDown();
  }

  HTMLVideoElement* Video() const { return video_.Get(); }
  MockPictureInPictureService& Service() { return mock_service_; }

  LocalFrame& GetFrame() const { return *helper_.LocalMainFrame()->GetFrame(); }

  Document& GetDocument() const { return *GetFrame().GetDocument(); }

  WebFrameWidgetImpl* GetWidget() const {
    return static_cast<WebFrameWidgetImpl*>(
        GetDocument().GetFrame()->GetWidgetForLocalRoot());
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }

  void ResetMediaPlayerAndMediaSource() {
    DynamicTo<HTMLMediaElement>(Video())->ResetMediaPlayerAndMediaSource();
  }

 private:
  Persistent<HTMLVideoElement> video_;
  std::unique_ptr<frame_test_helpers::TestWebFrameClient> client_;
  testing::NiceMock<MockPictureInPictureService> mock_service_;
  scoped_refptr<cc::Layer> layer_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(PictureInPictureControllerTestWithWidget,
       EnterPictureInPictureFiresEvent) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  EXPECT_NE(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());
}

TEST_F(PictureInPictureControllerTestWithWidget,
       FrameThrottlingIsSetProperlyWithoutSetup) {
  // This test assumes that it throttling is allowed by default.
  ASSERT_TRUE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());

  // Entering PictureInPicture should disallow throttling.
  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);
  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);
  EXPECT_FALSE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());

  // Exiting PictureInPicture should re-enable it.
  PictureInPictureControllerImpl::From(GetDocument())
      .ExitPictureInPicture(Video(), nullptr /* resolver */);
  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kLeavepictureinpicture);
  EXPECT_TRUE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());
}

TEST_F(PictureInPictureControllerTestWithWidget,
       ExitPictureInPictureFiresEvent) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  EXPECT_CALL(Service().Session(), Stop(_));

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  PictureInPictureControllerImpl::From(GetDocument())
      .ExitPictureInPicture(Video(), nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kLeavepictureinpicture);

  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());
}

TEST_F(PictureInPictureControllerTestWithWidget, StartObserving) {
  EXPECT_FALSE(PictureInPictureControllerImpl::From(GetDocument())
                   .IsSessionObserverReceiverBoundForTesting());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  EXPECT_TRUE(PictureInPictureControllerImpl::From(GetDocument())
                  .IsSessionObserverReceiverBoundForTesting());
}

TEST_F(PictureInPictureControllerTestWithWidget, StopObserving) {
  EXPECT_FALSE(PictureInPictureControllerImpl::From(GetDocument())
                   .IsSessionObserverReceiverBoundForTesting());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  EXPECT_CALL(Service().Session(), Stop(_));

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  PictureInPictureControllerImpl::From(GetDocument())
      .ExitPictureInPicture(Video(), nullptr);
  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kLeavepictureinpicture);

  EXPECT_FALSE(PictureInPictureControllerImpl::From(GetDocument())
                   .IsSessionObserverReceiverBoundForTesting());
}

TEST_F(PictureInPictureControllerTestWithWidget,
       PlayPauseButton_InfiniteDuration) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  Video()->DurationChanged(std::numeric_limits<double>::infinity(), false);

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), false, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);
}

TEST_F(PictureInPictureControllerTestWithWidget, PlayPauseButton_MediaSource) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  // The test automatically setup the WebMediaPlayer with a MediaSource based on
  // the test name.

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), false, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);
}

TEST_F(PictureInPictureControllerTestWithWidget, PerformMediaPlayerAction) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();

  WebLocalFrameImpl* frame = helper.LocalMainFrame();
  Document* document = frame->GetFrame()->GetDocument();

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*document);
  document->body()->AppendChild(video);

  gfx::Point bounds = video->BoundsInWidget().CenterPoint();

  // Performs the specified media player action on the media element at the
  // given location.
  frame->GetFrame()->MediaPlayerActionAtViewportPoint(
      bounds, blink::mojom::MediaPlayerActionType::kPictureInPicture, true);
}

TEST_F(PictureInPictureControllerTestWithWidget,
       EnterPictureInPictureAfterResettingWMP) {
  V8TestingScope scope;

  EXPECT_NE(nullptr, Video()->GetWebMediaPlayer());

  // Reset web media player.
  ResetMediaPlayerAndMediaSource();
  EXPECT_EQ(nullptr, Video()->GetWebMediaPlayer());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  auto promise = resolver->Promise();
  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), resolver);

  // Verify rejected with DOMExceptionCode::kInvalidStateError.
  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
  DOMException* dom_exception = V8DOMException::ToImplWithTypeCheck(
      promise.GetIsolate(), promise.V8Promise()->Result());
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kInvalidStateError),
            dom_exception->code());
}

TEST_F(PictureInPictureControllerTestWithWidget,
       EnterPictureInPictureProvideSourceBoundsSetToBoundsInWidget) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  // We expect that the video element has some nontrivial rect, else this won't
  // really test anything.
  ASSERT_NE(Video()->BoundsInWidget(), gfx::Rect());
  EXPECT_EQ(Service().source_bounds(), Video()->BoundsInWidget());
}

TEST_F(PictureInPictureControllerTestWithWidget,
       EnterPictureInPictureProvideSourceBoundsSetToReplacedContentRect) {
  // Create one image with a size of 10x10px
  SkImageInfo raster_image_info =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
  ImageResourceContent* image_content = ImageResourceContent::CreateLoaded(
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot())
          .get());

  Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
  div->setAttribute(html_names::kStyleAttr,
                    "padding: 100px;"
                    "width: 150px;"
                    "height: 150px;"
                    "padding: 100px;"
                    "transform: scale(2)");
  GetDocument().body()->AppendChild(div);
  div->AppendChild(Video());
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Set poster image to video
  auto* layout_image = To<LayoutImage>(Video()->GetLayoutObject());
  const char kPosterUrl[] = "http://example.com/foo.jpg";
  url_test_helpers::RegisterMockedErrorURLLoad(
      url_test_helpers::ToKURL(kPosterUrl));
  Video()->setAttribute(html_names::kPosterAttr, kPosterUrl);
  Video()->setAttribute(html_names::kStyleAttr,
                        "object-fit: none;"
                        "height: 150px;"
                        "width: 150px;");
  layout_image->ImageResource()->SetImageResource(image_content);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  // Source bounds are expected to match the poster image size, not the bounds
  // of the video element.
  EXPECT_EQ(Video()->BoundsInWidget(), gfx::Rect(33, 33, 300, 300));
  EXPECT_EQ(Service().source_bounds(), gfx::Rect(173, 173, 20, 20));
}

TEST_F(PictureInPictureControllerTestWithWidget, VideoIsNotAllowedIfAutoPip) {
  EXPECT_EQ(PictureInPictureControllerImpl::Status::kEnabled,
            PictureInPictureControllerImpl::From(GetDocument())
                .IsElementAllowed(*Video(), /*report_failure=*/false));

  // Simulate auto-pip mode.
  Video()->SetPersistentState(true);

  EXPECT_EQ(PictureInPictureControllerImpl::Status::kAutoPipAndroid,
            PictureInPictureControllerImpl::From(GetDocument())
                .IsElementAllowed(*Video(), /*report_failure=*/false));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PictureInPictureControllerTestWithWidget,
       DocumentPiPDoesNotAllowVizThrottling) {
  EXPECT_TRUE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());

  V8TestingScope v8_scope;
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope entered_context_scope(script_state);
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  OpenDocumentPictureInPictureWindow(v8_scope, GetDocument(),
                                     CopyStyleSheetOptions::kNo);

  EXPECT_FALSE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());

  // TODO(1357125): Check that GetMayThrottle... returns true once the PiP
  // window is closed.
}

class PictureInPictureControllerChromeClient
    : public RenderingTestChromeClient {
 public:
  explicit PictureInPictureControllerChromeClient(
      DummyPageHolder* dummy_page_holder)
      : dummy_page_holder_(dummy_page_holder) {}

  Page* CreateWindowDelegate(LocalFrame*,
                             const FrameLoadRequest&,
                             const AtomicString&,
                             const WebWindowFeatures&,
                             network::mojom::blink::WebSandboxFlags,
                             const SessionStorageNamespaceId&,
                             bool& consumed_user_gesture) override {
    return &dummy_page_holder_->GetPage();
  }
  MOCK_METHOD(void, SetWindowRect, (const gfx::Rect&, LocalFrame&));

 private:
  DummyPageHolder* dummy_page_holder_;
};

// Tests for Picture in Picture with a mockable chrome client.  This makes it
// easy to mock things like `SetWindowRect` on the client.  However, it skips
// the setup in `WebViewHelper` that provides a Widget.  `WebViewHelper` makes
// it hard to mock the client, since it provides a real `ChromeClient`.
class PictureInPictureControllerTestWithChromeClient : public RenderingTest {
 public:
  void SetUp() override {
    chrome_client_ =
        MakeGarbageCollected<PictureInPictureControllerChromeClient>(
            &dummy_page_holder_);
    RenderingTest::SetUp();
  }

  Document& GetDocument() const { return *GetFrame().GetDocument(); }

  // Used by RenderingTest.
  RenderingTestChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  // Convenience function to set expectations on the mock.
  PictureInPictureControllerChromeClient& GetPipChromeClient() const {
    return *chrome_client_;
  }

  StyleSheet* FindStyleSheetInOpener() {
    // Remember that style sheet names are not preserved in the pip document,
    // because injection doesn't do that.
    StyleSheetList& list = GetDocument().StyleSheets();
    for (unsigned i = 0; i < list.length(); i++) {
      StyleSheet* sheet = list.item(i);
      if (sheet->title() && sheet->title() == style_sheet_title_) {
        return sheet;
      }
    }

    return nullptr;
  }

  String GetBodyBackgroundColor(V8TestingScope& v8_scope, Document* document) {
    const auto* styleMap = document->body()->ComputedStyleMap();
    CSSStyleValue* styleValue =
        styleMap->get(v8_scope.GetExecutionContext(), "background-color",
                      v8_scope.GetExceptionState());
    return styleValue->toString();
  }

  void InitializeDocumentPictureInPictureOpener(V8TestingScope& v8_scope) {
    // Get past the BindingSecurity::ShouldAllowAccessTo() check.
    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope entered_context_scope(script_state);

    // Add HTML to set the CSS to the opener, with a title so that we can find
    // it later.
    GetDocument().write("<head><style title='");
    GetDocument().write(style_sheet_title_);
    GetDocument().write(
        "'>"
        "body { background-color: blue; }"
        "</style></head>",
        /*entered_window=*/nullptr, v8_scope.GetExceptionState());
  }

 private:
  Persistent<PictureInPictureControllerChromeClient> chrome_client_;
  // This is used by our chrome client to create the PiP window.  We keep
  // ownership of it here so that it outlives the GC'd objects.  The client
  // cannot own it because it also has a GC root to the client; everything would
  // leak if we did so.
  DummyPageHolder dummy_page_holder_;

  const char* style_sheet_title_ = "our_style_sheet";
};

TEST_F(PictureInPictureControllerTestWithChromeClient,
       CreateDocumentPictureInPictureWindow) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .pictureInPictureWindow());
  V8TestingScope v8_scope;
  InitializeDocumentPictureInPictureOpener(v8_scope);
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* pictureInPictureWindow = OpenDocumentPictureInPictureWindow(
      v8_scope, GetDocument(), CopyStyleSheetOptions::kNo);
  ASSERT_NE(nullptr, pictureInPictureWindow);
  Document* document = pictureInPictureWindow->document();
  ASSERT_NE(nullptr, document);

  // The Picture in Picture window's base URL should match the opener.
  EXPECT_EQ(GetOpenerURL().GetString(), document->BaseURL().GetString());

  // By default, CSS should not be copied from the opener, so the background
  // color should be the default.
  EXPECT_EQ(GetBodyBackgroundColor(v8_scope, document), "rgba(0, 0, 0, 0)");

  // Verify that move* and resize* don't call through to the chrome client.
  EXPECT_CALL(GetPipChromeClient(), SetWindowRect(_, _)).Times(0);
  document->domWindow()->moveTo(10, 10);
  document->domWindow()->moveBy(10, 10);
  document->domWindow()->resizeTo(10, 10);
  document->domWindow()->resizeBy(10, 10);

  // Make sure that the `document` is not the same as the opener.
  EXPECT_NE(document, &GetDocument());

  // Make sure that the `window` attribute returns the window.
  {
    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope entered_context_scope(script_state);
    EXPECT_EQ(pictureInPictureWindow,
              DocumentPictureInPicture::From(*GetDocument().domWindow())
                  ->window(script_state));
  }
}

TEST_F(PictureInPictureControllerTestWithChromeClient,
       CopyStylesToDocumentPictureInPictureWindow) {
  V8TestingScope v8_scope;
  InitializeDocumentPictureInPictureOpener(v8_scope);
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* pictureInPictureWindow = OpenDocumentPictureInPictureWindow(
      v8_scope, GetDocument(), CopyStyleSheetOptions::kYes);
  Document* pictureInPictureDocument = pictureInPictureWindow->document();

  // CSS for a blue background should have been copied from the opener.
  EXPECT_EQ(GetBodyBackgroundColor(v8_scope, pictureInPictureDocument),
            "rgb(0, 0, 255)");
}

TEST_F(PictureInPictureControllerTestWithChromeClient,
       DoesNotCopyDisabledStyleSheetsToDocumentPictureInPictureWindow) {
  V8TestingScope v8_scope;
  InitializeDocumentPictureInPictureOpener(v8_scope);

  // Turn off our style sheet.
  StyleSheet* sheet = FindStyleSheetInOpener();
  ASSERT_NE(sheet, nullptr);
  sheet->setDisabled(true);

  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* pictureInPictureWindow = OpenDocumentPictureInPictureWindow(
      v8_scope, GetDocument(), CopyStyleSheetOptions::kYes);
  Document* pictureInPictureDocument = pictureInPictureWindow->document();
  EXPECT_EQ(GetBodyBackgroundColor(v8_scope, pictureInPictureDocument),
            "rgba(0, 0, 0, 0)");
}

TEST_F(PictureInPictureControllerTestWithChromeClient, RequiresUserGesture) {
  V8TestingScope v8_scope;
  InitializeDocumentPictureInPictureOpener(v8_scope);

  auto* pictureInPictureWindow = OpenDocumentPictureInPictureWindow(
      v8_scope, GetDocument(), CopyStyleSheetOptions::kYes);
  EXPECT_FALSE(pictureInPictureWindow);
}

TEST_F(PictureInPictureControllerTestWithChromeClient,
       OpenDocumentPiPTwiceSynchronouslyDoesNotCrash) {
  V8TestingScope v8_scope;
  InitializeDocumentPictureInPictureOpener(v8_scope);

  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* pictureInPictureWindow1 = OpenDocumentPictureInPictureWindow(
      v8_scope, GetDocument(), CopyStyleSheetOptions::kYes);
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* pictureInPictureWindow2 = OpenDocumentPictureInPictureWindow(
      v8_scope, GetDocument(), CopyStyleSheetOptions::kYes);

  // This should properly return two windows.
  EXPECT_NE(nullptr, pictureInPictureWindow1);
  EXPECT_NE(nullptr, pictureInPictureWindow2);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
