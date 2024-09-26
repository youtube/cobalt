// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_
#define REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/protocol/ice_config_request.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

namespace apis {
namespace v1 {
class GetIceConfigResponse;
}  // namespace v1
}  // namespace apis

class ProtobufHttpStatus;
class OAuthTokenGetter;

namespace protocol {

// IceConfigRequest that fetches IceConfig from the remoting NetworkTraversal
// service.
class RemotingIceConfigRequest final : public IceConfigRequest {
 public:
  RemotingIceConfigRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuthTokenGetter* oauth_token_getter);

  RemotingIceConfigRequest(const RemotingIceConfigRequest&) = delete;
  RemotingIceConfigRequest& operator=(const RemotingIceConfigRequest&) = delete;

  ~RemotingIceConfigRequest() override;

  // IceConfigRequest implementation.
  void Send(OnIceConfigCallback callback) override;

 private:
  friend class RemotingIceConfigRequestTest;

  void OnResponse(const ProtobufHttpStatus& status,
                  std::unique_ptr<apis::v1::GetIceConfigResponse> response);

  bool make_authenticated_requests_ = false;
  OnIceConfigCallback on_ice_config_callback_;
  ProtobufHttpClient http_client_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_
