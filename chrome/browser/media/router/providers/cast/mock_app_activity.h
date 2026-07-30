// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_APP_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_APP_ACTIVITY_H_

#include <string>

#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "components/media_router/common/mojom/debugger.mojom.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockAppActivity : public AppActivity {
 public:
  MockAppActivity(const MediaRoute& route, const std::string& app_id);
  ~MockAppActivity() override;

  MOCK_METHOD(cast_channel::Result,
              SendAppMessageToReceiver,
              (const CastInternalMessage& cast_message),
              (override));
  MOCK_METHOD(std::optional<int>,
              SendMediaRequestToReceiver,
              (const CastInternalMessage& cast_message),
              (override));
  MOCK_METHOD(void,
              SendSetVolumeRequestToReceiver,
              (const CastInternalMessage& cast_message,
               cast_channel::ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              StopSessionOnReceiver,
              (const std::string& client_id,
               cast_channel::ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              CloseConnectionOnReceiver,
              (const std::string& client_id,
               blink::mojom::PresentationConnectionCloseReason reason),
              (override));
  MOCK_METHOD(void,
              SendStopSessionMessageToClients,
              (const std::string& hash_token),
              (override));
  MOCK_METHOD(void,
              HandleLeaveSession,
              (const std::string& client_id),
              (override));
  MOCK_METHOD(mojom::RoutePresentationConnectionPtr,
              AddClient,
              (const CastMediaSource& source,
               const url::Origin& origin,
               content::FrameTreeNodeId tab_id),
              (override));
  MOCK_METHOD(void, RemoveClient, (const std::string& client_id), (override));
  MOCK_METHOD(void, OnSessionSet, (const CastSession& session), (override));
  MOCK_METHOD(void,
              OnSessionUpdated,
              (const CastSession& session, const std::string& hash_token),
              (override));
  MOCK_METHOD(void,
              SendMessageToClient,
              (const std::string& client_id,
               blink::mojom::PresentationConnectionMessagePtr message),
              (override));
  MOCK_METHOD(void,
              SendMediaStatusToClients,
              (const base::DictValue& media_status,
               std::optional<int> request_id),
              (override));
  MOCK_METHOD(void,
              ClosePresentationConnections,
              (blink::mojom::PresentationConnectionCloseReason close_reason),
              (override));
  MOCK_METHOD(void, TerminatePresentationConnections, (), (override));
  MOCK_METHOD(void,
              OnAppMessage,
              (const openscreen::cast::proto::CastMessage& message),
              (override));
  MOCK_METHOD(void,
              OnInternalMessage,
              (const cast_channel::InternalMessage& message),
              (override));
  MOCK_METHOD(void,
              BindMediaController,
              (mojo::PendingReceiver<mojom::MediaController> media_controller,
               mojo::PendingRemote<mojom::MediaStatusObserver> observer),
              (override));

 private:
  mojo::Remote<mojom::Logger> logger_;
  mojo::Remote<mojom::Debugger> debugger_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_APP_ACTIVITY_H_
