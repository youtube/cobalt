// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_H_
#define NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/proxy_client_socket.h"

class GURL;

namespace net {

class AddressList;
class ClientSocketHandle;
class GrowableIOBuffer;
class HttpAuthCache;
class HttpStream;
class HttpStreamParser;
class IOBuffer;

class HttpProxyClientSocket : public ProxyClientSocket {
 public:
  // Takes ownership of |transport_socket|, which should already be connected
  // by the time Connect() is called.  If tunnel is true then on Connect()
  // this socket will establish an Http tunnel.
  HttpProxyClientSocket(ClientSocketHandle* transport_socket,
                        const GURL& request_url,
                        const std::string& user_agent,
                        const HostPortPair& endpoint,
                        const HostPortPair& proxy_server,
                        HttpAuthCache* http_auth_cache,
                        HttpAuthHandlerFactory* http_auth_handler_factory,
                        bool tunnel,
                        bool using_spdy,
                        bool is_https_proxy);

  // On destruction Disconnect() is called.
  virtual ~HttpProxyClientSocket();

  bool using_spdy() {
    return using_spdy_;
  }

  // ProxyClientSocket methods:
  virtual const HttpResponseInfo* GetConnectResponseInfo() const OVERRIDE;
  virtual HttpStream* CreateConnectResponseStream() OVERRIDE;
  virtual int RestartWithAuth(OldCompletionCallback* callback) OVERRIDE;
  virtual const scoped_refptr<HttpAuthController>& auth_controller() OVERRIDE;

  // StreamSocket methods:
  virtual int Connect(OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // Socket methods:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;
  virtual int GetPeerAddress(AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;

 private:
  enum State {
    STATE_NONE,
    STATE_GENERATE_AUTH_TOKEN,
    STATE_GENERATE_AUTH_TOKEN_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_DRAIN_BODY,
    STATE_DRAIN_BODY_COMPLETE,
    STATE_DONE,
  };

  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small error
  // page just a few hundred bytes long.
  static const int kDrainBodyBufferSize = 1024;

  int PrepareForAuthRestart();
  int DidDrainBodyForAuthRestart(bool keep_alive);

  void LogBlockedTunnelResponse(int response_code) const;

  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoGenerateAuthToken();
  int DoGenerateAuthTokenComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoDrainBody();
  int DoDrainBodyComplete(int result);

  OldCompletionCallbackImpl<HttpProxyClientSocket> io_callback_;
  State next_state_;

  // Stores the callback to the layer above, called on completing Connect().
  OldCompletionCallback* user_callback_;

  HttpRequestInfo request_;
  HttpResponseInfo response_;

  scoped_refptr<GrowableIOBuffer> parser_buf_;
  scoped_ptr<HttpStreamParser> http_stream_parser_;
  scoped_refptr<IOBuffer> drain_buf_;

  // Stores the underlying socket.
  scoped_ptr<ClientSocketHandle> transport_;

  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  const HostPortPair endpoint_;
  scoped_refptr<HttpAuthController> auth_;
  const bool tunnel_;
  // If true, then the connection to the proxy is a SPDY connection.
  const bool using_spdy_;
  // If true, then SSL is used to communicate with this proxy
  const bool is_https_proxy_;

  std::string request_line_;
  HttpRequestHeaders request_headers_;

  const BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyClientSocket);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_H_
