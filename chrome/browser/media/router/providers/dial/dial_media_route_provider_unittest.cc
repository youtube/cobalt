// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/browser/route_message_util.h"
#include "components/media_router/common/route_request_result.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_router::mojom::RouteMessagePtr;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::SaveArg;

namespace media_router {

namespace {
static constexpr int kFrameTreeNodeId = 1;
}

class TestDialMediaSinkServiceImpl : public DialMediaSinkServiceImpl {
 public:
  TestDialMediaSinkServiceImpl()
      : DialMediaSinkServiceImpl(base::DoNothing(),
                                 /*task_runner=*/nullptr) {}

  ~TestDialMediaSinkServiceImpl() override = default;

  MOCK_METHOD0(Start, void());

  MockDialAppDiscoveryService* app_discovery_service() override {
    return &app_discovery_service_;
  }

  base::CallbackListSubscription StartMonitoringAvailableSinksForApp(
      const std::string& app_name,
      const SinkQueryByAppCallback& callback) override {
    DoStartMonitoringAvailableSinksForApp(app_name);
    auto& cb_list = sink_query_cbs_[app_name];
    if (!cb_list)
      cb_list = std::make_unique<SinkQueryByAppCallbackList>();
    return cb_list->Add(callback);
  }
  MOCK_METHOD1(DoStartMonitoringAvailableSinksForApp,
               void(const std::string& app_name));

  void SetAvailableSinks(const std::string& app_name,
                         const std::vector<MediaSinkInternal>& sinks) {
    available_sinks_[app_name] = sinks;
    for (const auto& sink : sinks)
      AddOrUpdateSink(sink);
  }

  void NotifyAvailableSinks(const std::string& app_name) {
    auto& cb_list = sink_query_cbs_[app_name];
    if (cb_list)
      cb_list->Notify(app_name);
  }

  std::vector<MediaSinkInternal> GetAvailableSinks(
      const std::string& app_name) const override {
    auto it = available_sinks_.find(app_name);
    return it != available_sinks_.end() ? it->second
                                        : std::vector<MediaSinkInternal>();
  }

 private:
  MockDialAppDiscoveryService app_discovery_service_;
  base::flat_map<std::string, std::unique_ptr<SinkQueryByAppCallbackList>>
      sink_query_cbs_;
  base::flat_map<std::string, std::vector<MediaSinkInternal>> available_sinks_;
};

class DialMediaRouteProviderTest : public ::testing::Test {
 public:
  DialMediaRouteProviderTest() = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::MediaRouter> router_remote;
    router_receiver_ = std::make_unique<mojo::Receiver<mojom::MediaRouter>>(
        &mock_router_, router_remote.InitWithNewPipeAndPassReceiver());

    provider_ = std::make_unique<DialMediaRouteProvider>(
        provider_remote_.BindNewPipeAndPassReceiver(), std::move(router_remote),
        &mock_sink_service_, "hash-token",
        base::SequencedTaskRunner::GetCurrentDefault());

    auto activity_manager = std::make_unique<TestDialActivityManager>(
        mock_sink_service_.app_discovery_service(), &loader_factory_);
    activity_manager_ = activity_manager.get();
    provider_->SetActivityManagerForTest(std::move(activity_manager));

    base::RunLoop().RunUntilIdle();

    // Observe media routes in order for DialMediaRouteProvider to send back
    // route updates.
    provider_->StartObservingMediaRoutes();
  }

  void TearDown() override { provider_.reset(); }

  void ExpectRouteResult(mojom::RouteRequestResultCode expected_result_code,
                         const absl::optional<MediaRoute>& media_route,
                         mojom::RoutePresentationConnectionPtr,
                         const absl::optional<std::string>& error_text,
                         mojom::RouteRequestResultCode result_code) {
    EXPECT_EQ(expected_result_code, result_code);
    if (result_code == mojom::RouteRequestResultCode::OK) {
      ASSERT_TRUE(media_route);
      route_ = std::make_unique<MediaRoute>(*media_route);
    } else {
      EXPECT_FALSE(media_route);
    }
  }

  void ExpectDialInternalMessageType(const RouteMessagePtr& message,
                                     DialInternalMessageType expected_type) {
    ASSERT_TRUE(message->message);
    auto internal_message = ParseDialInternalMessage(*message->message);
    ASSERT_TRUE(internal_message);
    EXPECT_EQ(expected_type, internal_message->type);
  }

  void CreateRoute(const std::string& presentation_id = "presentationId") {
    const MediaSink::Id& sink_id = sink_.sink().id();
    std::vector<MediaSinkInternal> sinks = {sink_};
    mock_sink_service_.SetAvailableSinks("YouTube", sinks);

    MediaSource::Id source_id = "cast-dial:YouTube?clientId=12345";

    // DialMediaRouteProvider doesn't send route list update following
    // CreateRoute, but MR will add the route returned in the response.
    EXPECT_CALL(mock_router_, OnRoutesUpdated(_, _)).Times(0);
    provider_->CreateRoute(
        source_id, sink_id, presentation_id, origin_, kFrameTreeNodeId,
        base::TimeDelta(),
        /* off_the_record */ false,
        base::BindOnce(&DialMediaRouteProviderTest::ExpectRouteResult,
                       base::Unretained(this),
                       mojom::RouteRequestResultCode::OK));
    base::RunLoop().RunUntilIdle();
  }

  void TestCreateRoute() {
    const std::string presentation_id = "presentationId";
    CreateRoute(presentation_id);
    ASSERT_TRUE(route_);
    EXPECT_EQ(presentation_id, route_->presentation_id());
    EXPECT_FALSE(route_->is_off_the_record());

    const MediaRoute::Id& route_id = route_->media_route_id();
    std::vector<RouteMessagePtr> received_messages;
    EXPECT_CALL(mock_router_, OnRouteMessagesReceived(route_id, _))
        .WillOnce([&](const auto& route_id, auto messages) {
          for (auto& message : messages)
            received_messages.emplace_back(std::move(message));
        });
    provider_->StartListeningForRouteMessages(route_->media_route_id());
    base::RunLoop().RunUntilIdle();

    // RECEIVER_ACTION and NEW_SESSION messages are sent from MRP to page when
    // |provider_->CreateRoute()| succeeds.
    ASSERT_EQ(2u, received_messages.size());
    ExpectDialInternalMessageType(received_messages[0],
                                  DialInternalMessageType::kReceiverAction);
    ExpectDialInternalMessageType(received_messages[1],
                                  DialInternalMessageType::kNewSession);
  }

  void TestJoinRoute(
      mojom::RouteRequestResultCode expected_result,
      absl::optional<std::string> source_to_join = absl::nullopt,
      absl::optional<std::string> presentation_to_join = absl::nullopt,
      absl::optional<url::Origin> client_origin = absl::nullopt,
      absl::optional<bool> client_incognito = absl::nullopt) {
    CreateRoute();
    ASSERT_TRUE(route_);

    const std::string& source =
        source_to_join ? *source_to_join : route_->media_source().id();
    const std::string& presentation = presentation_to_join
                                          ? *presentation_to_join
                                          : route_->presentation_id();
    const url::Origin& origin = client_origin ? *client_origin : origin_;
    const bool incognito =
        client_incognito ? *client_incognito : route_->is_off_the_record();

    provider_->JoinRoute(
        source, presentation, origin, kFrameTreeNodeId, base::TimeDelta(),
        incognito,
        base::BindOnce(&DialMediaRouteProviderTest::ExpectRouteResult,
                       base::Unretained(this), expected_result));
  }

  // Note: |TestCreateRoute()| must be called first.
  void TestSendClientConnectMessage() {
    const MediaRoute::Id& route_id = route_->media_route_id();
    const char kClientConnectMessage[] = R"(
        {
          "type":"client_connect",
          "message":"",
          "sequenceNumber":-1,
          "timeoutMillis":0,
          "clientId":"12345"
        })";
    EXPECT_CALL(*mock_sink_service_.app_discovery_service(),
                DoFetchDialAppInfo(_, _));
    provider_->SendRouteMessage(route_id, kClientConnectMessage);
    base::RunLoop().RunUntilIdle();
    auto app_info_cb =
        mock_sink_service_.app_discovery_service()->PassCallback();
    ASSERT_FALSE(app_info_cb.is_null());

    // The page sends CLIENT_CONNECT message to the MRP. In response, the MRP
    // sends back CUSTOM_DIAL_LAUNCH message.
    std::vector<RouteMessagePtr> received_messages;
    EXPECT_CALL(mock_router_, OnRouteMessagesReceived(route_id, _))
        .WillOnce([&](const auto& route_id, auto messages) {
          for (auto& message : messages)
            received_messages.emplace_back(std::move(message));
        });
    std::move(app_info_cb)
        .Run(sink_.sink().id(), "YouTube",
             DialAppInfoResult(
                 CreateParsedDialAppInfoPtr("YouTube", DialAppState::kStopped),
                 DialAppInfoResultCode::kOk));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(1u, received_messages.size());
    ExpectDialInternalMessageType(received_messages[0],
                                  DialInternalMessageType::kCustomDialLaunch);
    auto internal_message =
        ParseDialInternalMessage(*received_messages[0]->message);
    ASSERT_TRUE(internal_message);
    EXPECT_GE(internal_message->sequence_number, 0);
    custom_dial_launch_seq_number_ = internal_message->sequence_number;
  }

  // Note: |TestSendClientConnectMessage()| must be called first.
  void TestSendCustomDialLaunchMessage() {
    const MediaRoute::Id& route_id = route_->media_route_id();
    const char kCustomDialLaunchMessage[] = R"(
        {
          "type":"custom_dial_launch",
          "message": {
            "doLaunch":true,
            "launchParameter":"pairingCode=foo"
          },
          "sequenceNumber":%d,
          "timeoutMillis":3000,
          "clientId":"152127444812943594"
        })";

    // The page sends back a CUSTOM_DIAL_LAUNCH response with the same
    // sequence number. DialMediaRouteProvider handles the message by launching
    // the app if |doLaunch| is |true|.
    auto* activity = activity_manager_->GetActivity(route_id);
    ASSERT_TRUE(activity);
    app_launch_url_ = activity->launch_info.app_launch_url;
    activity_manager_->SetExpectedRequest(app_launch_url_, "POST",
                                          "pairingCode=foo");
    provider_->SendRouteMessage(
        route_id, base::StringPrintf(kCustomDialLaunchMessage,
                                     custom_dial_launch_seq_number_));
    base::RunLoop().RunUntilIdle();

    // Simulate a successful launch response.
    app_instance_url_ = GURL(app_launch_url_.spec() + "/run");
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    response_head->headers->AddHeader("LOCATION", app_instance_url_.spec());
    loader_factory_.AddResponse(app_launch_url_, std::move(response_head), "",
                                network::URLLoaderCompletionStatus());
    std::vector<MediaRoute> routes;
    EXPECT_CALL(mock_router_, OnRoutesUpdated(_, Not(IsEmpty())))
        .WillOnce(SaveArg<1>(&routes));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, routes.size());
    EXPECT_EQ(routes[0], *route_);
  }

  // Note: |TestSendCustomDialLaunchMessage()| must be called first.
  void TestTerminateRoute() {
    const MediaRoute::Id& route_id = route_->media_route_id();
    ASSERT_TRUE(app_instance_url_.is_valid());
    activity_manager_->SetExpectedRequest(app_instance_url_, "DELETE",
                                          absl::nullopt);
    loader_factory_.AddResponse(app_instance_url_,
                                network::mojom::URLResponseHead::New(), "",
                                network::URLLoaderCompletionStatus());
    EXPECT_CALL(*this, OnTerminateRoute(_, mojom::RouteRequestResultCode::OK));
    provider_->TerminateRoute(
        route_id, base::BindOnce(&DialMediaRouteProviderTest::OnTerminateRoute,
                                 base::Unretained(this)));

    ExpectTerminateRouteCommon();
  }

  // Note: |CreateRoute()| must be called first.
  void TestTerminateRouteNoStopApp() {
    const MediaRoute::Id& route_id = route_->media_route_id();
    EXPECT_CALL(*activity_manager_, OnFetcherCreated()).Times(0);
    EXPECT_CALL(*this, OnTerminateRoute(_, mojom::RouteRequestResultCode::OK));
    provider_->TerminateRoute(
        route_id, base::BindOnce(&DialMediaRouteProviderTest::OnTerminateRoute,
                                 base::Unretained(this)));

    ExpectTerminateRouteCommon();
  }

  // Note: |TestSendClientConnectMessage()| must be called first.
  void TestTerminateRouteViaStopSessionMessage() {
    const MediaRoute::Id& route_id = route_->media_route_id();
    const char kStopSessionMessage[] = R"(
        {
          "type":"v2_message",
          "message": {
            "type":"STOP"
          },
          "sequenceNumber":-1,
          "timeoutMillis":0,
          "clientId":"15178573373126446"
        })";

    ASSERT_TRUE(app_instance_url_.is_valid());
    activity_manager_->SetExpectedRequest(app_instance_url_, "DELETE",
                                          absl::nullopt);
    loader_factory_.AddResponse(app_instance_url_,
                                network::mojom::URLResponseHead::New(), "",
                                network::URLLoaderCompletionStatus());

    provider_->SendRouteMessage(route_id, kStopSessionMessage);
    ExpectTerminateRouteCommon();
  }

  // Sets up expectations following a successful route termination.
  void ExpectTerminateRouteCommon() {
    const MediaRoute::Id& route_id = route_->media_route_id();

    std::vector<RouteMessagePtr> received_messages;
    EXPECT_CALL(mock_router_, OnRouteMessagesReceived(route_id, _))
        .WillOnce([&](const auto& route_id, auto messages) {
          for (auto& message : messages)
            received_messages.emplace_back(std::move(message));
        });
    EXPECT_CALL(
        mock_router_,
        OnPresentationConnectionStateChanged(
            route_id, blink::mojom::PresentationConnectionState::TERMINATED));
    EXPECT_CALL(mock_router_, OnRoutesUpdated(_, IsEmpty()));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, received_messages.size());
    ExpectDialInternalMessageType(received_messages[0],
                                  DialInternalMessageType::kReceiverAction);
  }

  // Note: |TestSendCustomDialLaunchMessage()| must be called first.
  void TestTerminateRouteFails() {
    const MediaRoute::Id& route_id = route_->media_route_id();
    ASSERT_TRUE(app_instance_url_.is_valid());
    activity_manager_->SetExpectedRequest(app_instance_url_, "DELETE",
                                          absl::nullopt);
    loader_factory_.AddResponse(
        app_instance_url_, network::mojom::URLResponseHead::New(), "",
        network::URLLoaderCompletionStatus(net::HTTP_SERVICE_UNAVAILABLE));
    loader_factory_.AddResponse(
        app_launch_url_, network::mojom::URLResponseHead::New(), "",
        network::URLLoaderCompletionStatus(net::HTTP_SERVICE_UNAVAILABLE));
    EXPECT_CALL(*this, OnTerminateRoute(
                           _, testing::Ne(mojom::RouteRequestResultCode::OK)));
    EXPECT_CALL(mock_router_, OnRouteMessagesReceived(_, _));
    EXPECT_CALL(mock_router_, OnRoutesUpdated(_, _)).Times(1);
    EXPECT_CALL(*mock_sink_service_.app_discovery_service(),
                DoFetchDialAppInfo(_, _));
    provider_->TerminateRoute(
        route_id, base::BindOnce(&DialMediaRouteProviderTest::OnTerminateRoute,
                                 base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    // The DialActivityManager requests to confirm the state of the app, so we
    // tell it that the app is still running.
    mock_sink_service_.app_discovery_service()->PassCallback().Run(
        sink_.sink().id(), "YouTube",
        DialAppInfoResult(
            CreateParsedDialAppInfoPtr("YouTube", DialAppState::kRunning),
            DialAppInfoResultCode::kOk));
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD2(OnTerminateRoute,
               void(const absl::optional<std::string>&,
                    mojom::RouteRequestResultCode));

 protected:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  network::TestURLLoaderFactory loader_factory_;

  mojo::Remote<mojom::MediaRouteProvider> provider_remote_;
  NiceMock<MockMojoMediaRouter> mock_router_;
  std::unique_ptr<mojo::Receiver<mojom::MediaRouter>> router_receiver_;

  TestDialMediaSinkServiceImpl mock_sink_service_;
  raw_ptr<TestDialActivityManager> activity_manager_ = nullptr;
  std::unique_ptr<DialMediaRouteProvider> provider_;

  MediaSinkInternal sink_{CreateDialSink(1)};
  std::unique_ptr<MediaRoute> route_;
  int custom_dial_launch_seq_number_{-1};
  GURL app_launch_url_;
  GURL app_instance_url_;
  url::Origin origin_{url::Origin::Create(GURL{"https://www.youtube.com"})};
};

TEST_F(DialMediaRouteProviderTest, AddRemoveSinkQuery) {
  std::vector<url::Origin> youtube_origins = {
      url::Origin::Create(GURL("https://music.youtube.com/")),
      url::Origin::Create(GURL("https://music-green-qa.youtube.com/")),
      url::Origin::Create(GURL("https://music-release-qa.youtube.com/")),
      url::Origin::Create(GURL("https://tv.youtube.com")),
      url::Origin::Create(GURL("https://tv-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://tv-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://www.youtube.com"))};
  std::string youtube_source("cast-dial:YouTube");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_,
              OnSinksReceived(mojom::MediaRouteProviderId::DIAL, youtube_source,
                              IsEmpty(), youtube_origins));
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  MediaSinkInternal sink = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks = {sink};
  mock_sink_service_.SetAvailableSinks("YouTube", sinks);

  EXPECT_CALL(mock_router_,
              OnSinksReceived(mojom::MediaRouteProviderId::DIAL, youtube_source,
                              sinks, youtube_origins));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, AddSinkQuerySameMediaSource) {
  std::string youtube_source("cast-dial:YouTube");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source, IsEmpty(), _));
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_sink_service_, DoStartMonitoringAvailableSinksForApp(_))
      .Times(0);
  EXPECT_CALL(mock_router_,
              OnSinksReceived(mojom::MediaRouteProviderId::DIAL, _, _, _))
      .Times(0);
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source);
  provider_->StopObservingMediaSinks(youtube_source);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest,
       TestAddSinkQuerySameAppDifferentMediaSources) {
  std::string youtube_source1("cast-dial:YouTube");
  std::string youtube_source2("cast-dial:YouTube?clientId=15178573373126446");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source1, IsEmpty(), _));
  provider_->StartObservingMediaSinks(youtube_source1);
  base::RunLoop().RunUntilIdle();

  MediaSinkInternal sink = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks = {sink};
  mock_sink_service_.SetAvailableSinks("YouTube", sinks);
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source1, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");

  EXPECT_CALL(mock_sink_service_, DoStartMonitoringAvailableSinksForApp(_))
      .Times(0);
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  provider_->StartObservingMediaSinks(youtube_source2);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source1, sinks, _));
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source1);
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source2);
  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, AddSinkQueryDifferentApps) {
  std::string youtube_source("cast-dial:YouTube");
  std::string netflix_source("cast-dial:Netflix");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source, IsEmpty(), _));
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("Netflix"));
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            netflix_source, IsEmpty(), _));
  provider_->StartObservingMediaSinks(netflix_source);
  base::RunLoop().RunUntilIdle();

  MediaSinkInternal sink = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks = {sink};
  mock_sink_service_.SetAvailableSinks("YouTube", sinks);
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            youtube_source, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  mock_sink_service_.SetAvailableSinks("Netflix", sinks);
  EXPECT_CALL(mock_router_, OnSinksReceived(mojom::MediaRouteProviderId::DIAL,
                                            netflix_source, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("Netflix");
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source);
  provider_->StopObservingMediaSinks(netflix_source);

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  mock_sink_service_.NotifyAvailableSinks("Netflix");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, ListenForRouteMessages) {
  std::vector<RouteMessagePtr> messages1;
  messages1.emplace_back(message_util::RouteMessageFromString("message1"));
  MediaRoute::Id route_id = "route_id";
  auto& message_sender = provider_->message_sender_;
  EXPECT_CALL(mock_router_, OnRouteMessagesReceived(_, _)).Times(0);
  message_sender->SendMessages(route_id, std::move(messages1));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnRouteMessagesReceived(route_id, _))
      .WillOnce([&](const auto& route_id, auto messages) {
        EXPECT_EQ(1UL, messages.size());
        EXPECT_EQ(message_util::RouteMessageFromString("message1"),
                  messages[0]);
      });

  provider_->StartListeningForRouteMessages(route_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnRouteMessagesReceived(route_id, _))
      .WillOnce([&](const auto& route_id, auto messages) {
        EXPECT_EQ(1UL, messages.size());
        EXPECT_EQ(message_util::RouteMessageFromString("message2"),
                  messages[0]);
      });

  std::vector<RouteMessagePtr> messages2;
  messages2.emplace_back(message_util::RouteMessageFromString("message2"));
  message_sender->SendMessages(route_id, std::move(messages2));
  base::RunLoop().RunUntilIdle();

  provider_->StopListeningForRouteMessages(route_id);
  EXPECT_CALL(mock_router_, OnRouteMessagesReceived(_, _)).Times(0);

  std::vector<RouteMessagePtr> messages3;
  messages3.emplace_back(message_util::RouteMessageFromString("message3"));
  message_sender->SendMessages(route_id, std::move(messages3));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, CreateRoute) {
  TestCreateRoute();
  TestSendClientConnectMessage();
  TestSendCustomDialLaunchMessage();
}

TEST_F(DialMediaRouteProviderTest, JoinRoute) {
  TestJoinRoute(mojom::RouteRequestResultCode::OK);
}

TEST_F(DialMediaRouteProviderTest, JoinRouteFailsForWrongMediaSource) {
  TestJoinRoute(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND,
                "wrong-media-source");
}

TEST_F(DialMediaRouteProviderTest, JoinRouteFailsForWrongPresentationId) {
  TestJoinRoute(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, absl::nullopt,
                "wrong-presentation-id");
}

TEST_F(DialMediaRouteProviderTest, JoinRouteFailsForWrongOrigin) {
  TestJoinRoute(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, absl::nullopt,
                absl::nullopt,
                url::Origin::Create(GURL("https://wrong-origin.com")));
}

TEST_F(DialMediaRouteProviderTest, JoinRouteFailsForIncognitoMismatch) {
  TestJoinRoute(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, absl::nullopt,
                absl::nullopt, absl::nullopt, true);
}

TEST_F(DialMediaRouteProviderTest, TerminateRoute) {
  TestCreateRoute();
  TestSendClientConnectMessage();
  TestSendCustomDialLaunchMessage();
  TestTerminateRoute();
}

TEST_F(DialMediaRouteProviderTest, TerminateRouteViaStopSessionMessage) {
  TestCreateRoute();
  TestSendClientConnectMessage();
  TestSendCustomDialLaunchMessage();
  TestTerminateRouteViaStopSessionMessage();
}

TEST_F(DialMediaRouteProviderTest, CreateRouteFailsCleansUpProperly) {
  // For some reason the SDK client does not complete the launch sequence.
  // |TerminateRoute()| should stop clean up the MediaRoute that was created.
  TestCreateRoute();
  TestSendClientConnectMessage();
  TestTerminateRouteNoStopApp();
}

TEST_F(DialMediaRouteProviderTest, TerminateRouteFailsThenSucceeds) {
  // TerminateRoute might fail due to transient network issues.
  TestCreateRoute();
  TestSendClientConnectMessage();
  TestSendCustomDialLaunchMessage();
  // Even if we fail to terminate an app, we remove it from the list of routes
  // tracked by DialActivityManager. This is done because manually stopping Cast
  // session from Dial device is not reflected on Chrome side.
  TestTerminateRouteFails();
}

TEST_F(DialMediaRouteProviderTest, GetDialAppinfoExtraData) {
  const int seq_number = 12345;
  const char kDialAppInfoRequestMessage[] = R"(
      {
        "type":"dial_app_info",
        "sequenceNumber":%d,
        "timeoutMillis":3000,
        "clientId":"152127444812943594"
      })";
  std::map<std::string, std::string> extra_data = {
      {"additionalKey1", "additional value 1"},
      {"additionalKey2", "additional value 2"}};
  TestCreateRoute();
  TestSendClientConnectMessage();
  TestSendCustomDialLaunchMessage();

  const MediaRoute::Id& route_id = route_->media_route_id();
  EXPECT_CALL(*mock_sink_service_.app_discovery_service(),
              DoFetchDialAppInfo(_, _));
  provider_->SendRouteMessage(
      route_id, base::StringPrintf(kDialAppInfoRequestMessage, seq_number));
  base::RunLoop().RunUntilIdle();
  auto app_info_cb = mock_sink_service_.app_discovery_service()->PassCallback();
  ASSERT_FALSE(app_info_cb.is_null());

  auto app_info = CreateParsedDialAppInfoPtr("YouTube", DialAppState::kRunning);
  app_info->extra_data = std::move(extra_data);
  std::move(app_info_cb)
      .Run(sink_.sink().id(), "YouTube",
           DialAppInfoResult(std::move(app_info), DialAppInfoResultCode::kOk));

  EXPECT_CALL(mock_router_, OnRouteMessagesReceived(route_id, _))
      .WillOnce([&](const auto& route_id, auto messages) {
        EXPECT_EQ(1UL, messages.size());
        auto message = base::test::ParseJsonDict(*messages[0]->message);

        EXPECT_TRUE(message.FindString("type"));
        EXPECT_TRUE(message.FindInt("sequenceNumber"));
        EXPECT_TRUE(
            message.FindStringByDottedPath("message.extraData.additionalKey1"));
        EXPECT_TRUE(
            message.FindStringByDottedPath("message.extraData.additionalKey2"));

        EXPECT_EQ("dial_app_info", *message.FindString("type"));
        EXPECT_EQ(seq_number, *message.FindInt("sequenceNumber"));
        EXPECT_EQ("additional value 1",
                  *message.FindStringByDottedPath(
                      "message.extraData.additionalKey1"));
        EXPECT_EQ("additional value 2",
                  *message.FindStringByDottedPath(
                      "message.extraData.additionalKey2"));
      });
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
