// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
#define NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
#pragma once

#include <string>
#include <list>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"


class GURL;

namespace net {

class AddressList;
class ClientSocketHandle;
class HttpStream;
class IOBuffer;
class SpdySession;
class SpdyStream;

class SpdyProxyClientSocket : public ClientSocket, public SpdyStream::Delegate {
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

  const scoped_refptr<HttpAuthController>& auth_controller() {
    return auth_;
  }

  const HttpResponseInfo* GetConnectResponseInfo() const {
    return response_.headers ? &response_ : NULL;
  }

  // ClientSocket methods:

  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual const BoundNetLog& NetLog() const { return net_log_; }
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;
  virtual bool UsingTCPFastOpen() const;

  // Socket methods:

  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

  virtual int GetPeerAddress(AddressList* address) const;

  // SpdyStream::Delegate methods:

  // Called when SYN frame has been sent.
  // Returns true if no more data to be sent after SYN frame.
  virtual bool OnSendHeadersComplete(int status);

  // Called when stream is ready to send data.
  // Returns network error code. OK when it successfully sent data.
  virtual int OnSendBody();

  // Called when data has been sent. |status| indicates network error
  // or number of bytes has been sent.
  // Returns true if no more data to be sent.
  virtual bool OnSendBodyComplete(int status);

  // Called when SYN_STREAM or SYN_REPLY received. |status| indicates network
  // error. Returns network error code.
  virtual int OnResponseReceived(const spdy::SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status);

  // Called when data is received.
  virtual void OnDataReceived(const char* data, int length);

  // Called when data is sent.
  virtual void OnDataSent(int length);

  // Called when SpdyStream is closed.
  virtual void OnClose(int status);

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

  CompletionCallbackImpl<SpdyProxyClientSocket> io_callback_;
  State next_state_;

  // Pointer to the SPDY Stream that this sits on top of.
  scoped_refptr<SpdyStream> spdy_stream_;

  // Stores the callback to the layer above, called on completing Read() or
  // Connect().
  CompletionCallback* read_callback_;
  // Stores the callback to the layer above, called on completing Write().
  CompletionCallback* write_callback_;

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

  // True if read has ever returned zero for eof.
  bool eof_has_been_read_;
  // True if the transport socket has ever sent data.
  bool was_ever_used_;

  const BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(SpdyProxyClientSocket);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
