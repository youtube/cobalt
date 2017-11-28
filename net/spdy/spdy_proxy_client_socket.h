// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
#define NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_

#include <string>
#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/proxy_client_socket.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"


class GURL;

namespace net {

class AddressList;
class HttpStream;
class IOBuffer;
class SpdyStream;

class NET_EXPORT_PRIVATE SpdyProxyClientSocket : public ProxyClientSocket,
                                                 public SpdyStream::Delegate {
 public:
  // Create a socket on top of the |spdy_stream| by sending a SYN_STREAM
  // CONNECT frame for |endpoint|.  After the SYN_REPLY is received,
  // any data read/written to the socket will be transferred in data
  // frames.
  SpdyProxyClientSocket(SpdyStream* spdy_stream,
                        const std::string& user_agent,
                        const HostPortPair& endpoint,
                        const GURL& url,
                        const HostPortPair& proxy_server,
                        HttpAuthCache* auth_cache,
                        HttpAuthHandlerFactory* auth_handler_factory);


  // On destruction Disconnect() is called.
  virtual ~SpdyProxyClientSocket();

  // ProxyClientSocket methods:
  virtual const HttpResponseInfo* GetConnectResponseInfo() const override;
  virtual HttpStream* CreateConnectResponseStream() override;
  virtual const scoped_refptr<HttpAuthController>& GetAuthController() const
      override;
  virtual int RestartWithAuth(const CompletionCallback& callback) override;
  virtual bool IsUsingSpdy() const override;
  virtual NextProto GetProtocolNegotiated() const override;

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) override;
  virtual void Disconnect() override;
  virtual bool IsConnected() const override;
  virtual bool IsConnectedAndIdle() const override;
  virtual const BoundNetLog& NetLog() const override;
  virtual void SetSubresourceSpeculation() override;
  virtual void SetOmniboxSpeculation() override;
  virtual bool WasEverUsed() const override;
  virtual bool UsingTCPFastOpen() const override;
  virtual int64 NumBytesRead() const override;
  virtual base::TimeDelta GetConnectTimeMicros() const override;
  virtual bool WasNpnNegotiated() const override;
  virtual NextProto GetNegotiatedProtocol() const override;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) override;

  // Socket implementation.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) override;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) override;
  virtual bool SetReceiveBufferSize(int32 size) override;
  virtual bool SetSendBufferSize(int32 size) override;
  virtual int GetPeerAddress(IPEndPoint* address) const override;
  virtual int GetLocalAddress(IPEndPoint* address) const override;

  // SpdyStream::Delegate implementation.
  virtual bool OnSendHeadersComplete(int status) override;
  virtual int OnSendBody() override;
  virtual int OnSendBodyComplete(int status, bool* eof) override;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) override;
  virtual void OnHeadersSent() override;
  virtual int OnDataReceived(const char* data, int length) override;
  virtual void OnDataSent(int length) override;
  virtual void OnClose(int status) override;

 private:
  enum State {
    STATE_DISCONNECTED,
    STATE_GENERATE_AUTH_TOKEN,
    STATE_GENERATE_AUTH_TOKEN_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_REPLY_COMPLETE,
    STATE_OPEN,
    STATE_CLOSED
  };

  void LogBlockedTunnelResponse() const;

  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoGenerateAuthToken();
  int DoGenerateAuthTokenComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadReplyComplete(int result);

  // Populates |user_buffer_| with as much read data as possible
  // and returns the number of bytes read.
  int PopulateUserReadBuffer();

  State next_state_;

  // Pointer to the SPDY Stream that this sits on top of.
  scoped_refptr<SpdyStream> spdy_stream_;

  // Stores the callback to the layer above, called on completing Read() or
  // Connect().
  CompletionCallback read_callback_;
  // Stores the callback to the layer above, called on completing Write().
  CompletionCallback write_callback_;

  // CONNECT request and response.
  HttpRequestInfo request_;
  HttpResponseInfo response_;

  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  const HostPortPair endpoint_;
  scoped_refptr<HttpAuthController> auth_;

  // We buffer the response body as it arrives asynchronously from the stream.
  std::list<scoped_refptr<DrainableIOBuffer> > read_buffer_;

  // User provided buffer for the Read() response.
  scoped_refptr<DrainableIOBuffer> user_buffer_;

  // User specified number of bytes to be written.
  int write_buffer_len_;
  // Number of bytes written which have not been confirmed
  int write_bytes_outstanding_;

  // True if the transport socket has ever sent data.
  bool was_ever_used_;

  scoped_ptr<SpdyHttpStream> response_stream_;

  base::WeakPtrFactory<SpdyProxyClientSocket> weak_factory_;

  const BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(SpdyProxyClientSocket);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
