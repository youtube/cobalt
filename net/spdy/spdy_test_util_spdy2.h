// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_H_
#define NET_SPDY_SPDY_TEST_UTIL_H_
#pragma once

#include "base/basictypes.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"

namespace net {

namespace test_spdy2 {

// Default upload data used by both, mock objects and framer when creating
// data frames.
const char kDefaultURL[] = "http://www.google.com";
const char kUploadData[] = "hello!";
const int kUploadDataSize = arraysize(kUploadData)-1;

// NOTE: In GCC, on a Mac, this can't be in an anonymous namespace!
// This struct holds information used to construct spdy control and data frames.
struct SpdyHeaderInfo {
  SpdyControlType kind;
  SpdyStreamId id;
  SpdyStreamId assoc_id;
  SpdyPriority priority;
  SpdyControlFlags control_flags;
  bool compressed;
  SpdyStatusCodes status;
  const char* data;
  uint32 data_length;
  SpdyDataFlags data_flags;
};

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const SpdyFrame& frame, int num_chunks);

// Chop a frame into an array of MockReads.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockReads.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const SpdyFrame& frame, int num_chunks);

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendHeadersToSpdyFrame(const char* const extra_headers[],
                              int extra_header_count,
                              SpdyHeaderBlock* headers);

// Writes |str| of the given |len| to the buffer pointed to by |buffer_handle|.
// Uses a template so buffer_handle can be a char* or an unsigned char*.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written into *|buffer_handle|
template<class T>
int AppendToBuffer(const char* str,
                   int len,
                   T** buffer_handle,
                   int* buffer_len_remaining) {
  DCHECK_GT(len, 0);
  DCHECK(NULL != buffer_handle) << "NULL buffer handle";
  DCHECK(NULL != *buffer_handle) << "NULL pointer";
  DCHECK(NULL != buffer_len_remaining)
      << "NULL buffer remainder length pointer";
  DCHECK_GE(*buffer_len_remaining, len) << "Insufficient buffer size";
  memcpy(*buffer_handle, str, len);
  *buffer_handle += len;
  *buffer_len_remaining -= len;
  return len;
}

// Writes |val| to a location of size |len|, in big-endian format.
// in the buffer pointed to by |buffer_handle|.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written
int AppendToBuffer(int val,
                   int len,
                   unsigned char** buffer_handle,
                   int* buffer_len_remaining);

// Construct a SPDY packet.
// |head| is the start of the packet, up to but not including
// the header value pairs.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |tail| is any (relatively constant) header-value pairs to add.
// |buffer| is the buffer we're filling in.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPacket(const SpdyHeaderInfo& header_info,
                                     const char* const extra_headers[],
                                     int extra_header_count,
                                     const char* const tail[],
                                     int tail_header_count);

// Construct a generic SpdyControlFrame.
SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                           int extra_header_count,
                                           bool compressed,
                                           int stream_id,
                                           RequestPriority request_priority,
                                           SpdyControlType type,
                                           SpdyControlFlags flags,
                                           const char* const* kHeaders,
                                           int kHeadersSize);
SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                           int extra_header_count,
                                           bool compressed,
                                           int stream_id,
                                           RequestPriority request_priority,
                                           SpdyControlType type,
                                           SpdyControlFlags flags,
                                           const char* const* kHeaders,
                                           int kHeadersSize,
                                           int associated_stream_id);

// Construct an expected SPDY reply string.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |buffer| is the buffer we're filling in.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyReplyString(const char* const extra_headers[],
                             int extra_header_count,
                             char* buffer,
                             int buffer_length);

// Construct an expected SPDY SETTINGS frame.
// |settings| are the settings to set.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdySettings(const SettingsMap& settings);

// Construct an expected SPDY CREDENTIAL frame.
// |credential| is the credential to send.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyCredential(
    const SpdyCredential& credential);

// Construct a SPDY PING frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyPing();

// Construct a SPDY GOAWAY frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyGoAway();

// Construct a SPDY WINDOW_UPDATE frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyWindowUpdate(SpdyStreamId,
                                           uint32 delta_window_size);

// Construct a SPDY RST_STREAM frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyRstStream(SpdyStreamId stream_id,
                                        SpdyStatusCodes status);

// Construct a single SPDY header entry, for validation.
// |extra_headers| are the extra header-value pairs.
// |buffer| is the buffer we're filling in.
// |index| is the index of the header we want.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyHeader(const char* const extra_headers[],
                        int extra_header_count,
                        char* buffer,
                        int buffer_length,
                        int index);

// Constructs a standard SPDY GET SYN packet, optionally compressed
// for the url |url|.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const url,
                                  bool compressed,
                                  int stream_id,
                                  RequestPriority request_priority);

// Constructs a standard SPDY GET SYN packet, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                                  int extra_header_count,
                                  bool compressed,
                                  int stream_id,
                                  RequestPriority request_priority);

// Constructs a standard SPDY GET SYN packet, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.  If |direct| is false, the
// the full url will be used instead of simply the path.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                                  int extra_header_count,
                                  bool compressed,
                                  int stream_id,
                                  RequestPriority request_priority,
                                  bool direct);

// Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
SpdyFrame* ConstructSpdyConnect(const char* const extra_headers[],
                                      int extra_header_count,
                                      int stream_id);

// Constructs a standard SPDY push SYN packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                                  int extra_header_count,
                                  int stream_id,
                                  int associated_stream_id);
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                                  int extra_header_count,
                                  int stream_id,
                                  int associated_stream_id,
                                  const char* url);
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                                  int extra_header_count,
                                  int stream_id,
                                  int associated_stream_id,
                                  const char* url,
                                  const char* status,
                                  const char* location);
SpdyFrame* ConstructSpdyPush(int stream_id,
                                  int associated_stream_id,
                                  const char* url);

SpdyFrame* ConstructSpdyPushHeaders(int stream_id,
                                          const char* const extra_headers[],
                                          int extra_header_count);

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                          int extra_header_count,
                                          int stream_id);

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGetSynReplyRedirect(int stream_id);

// Constructs a standard SPDY SYN_REPLY packet with an Internal Server
// Error status code.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdySynReplyError(int stream_id);

// Constructs a standard SPDY SYN_REPLY packet with the specified status code.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdySynReplyError(
    const char* const status,
    const char* const* const extra_headers,
    int extra_header_count,
    int stream_id);

// Constructs a standard SPDY POST SYN packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPost(int64 content_length,
                                   const char* const extra_headers[],
                                   int extra_header_count);

// Constructs a chunked transfer SPDY POST SYN packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructChunkedSpdyPost(const char* const extra_headers[],
                                          int extra_header_count);

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY POST.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                           int extra_header_count);

// Constructs a single SPDY data frame with the contents "hello!"
SpdyFrame* ConstructSpdyBodyFrame(int stream_id,
                                        bool fin);

// Constructs a single SPDY data frame with the given content.
SpdyFrame* ConstructSpdyBodyFrame(int stream_id, const char* data,
                                        uint32 len, bool fin);

// Wraps |frame| in the payload of a data frame in stream |stream_id|.
SpdyFrame* ConstructWrappedSpdyFrame(
    const scoped_ptr<SpdyFrame>& frame, int stream_id);

// Create an async MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(const SpdyFrame& req);

// Create an async MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq);

MockWrite CreateMockWrite(const SpdyFrame& req, int seq, IoMode mode);

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(const SpdyFrame& resp);

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq);

MockRead CreateMockRead(const SpdyFrame& resp, int seq, IoMode mode);

// Combines the given SpdyFrames into the given char array and returns
// the total length.
int CombineFrames(const SpdyFrame** frames, int num_frames,
                  char* buff, int buff_len);

// Helper to manage the lifetimes of the dependencies for a
// HttpNetworkTransaction.
class SpdySessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SpdySessionDependencies();

  // Custom proxy service dependency.
  explicit SpdySessionDependencies(ProxyService* proxy_service);

  ~SpdySessionDependencies();

  static HttpNetworkSession* SpdyCreateSession(
      SpdySessionDependencies* session_deps);
  static HttpNetworkSession* SpdyCreateSessionDeterministic(
      SpdySessionDependencies* session_deps);

  // NOTE: host_resolver must be ordered before http_auth_handler_factory.
  scoped_ptr<MockHostResolverBase> host_resolver;
  scoped_ptr<CertVerifier> cert_verifier;
  scoped_ptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  scoped_ptr<MockClientSocketFactory> socket_factory;
  scoped_ptr<DeterministicMockClientSocketFactory> deterministic_socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  HttpServerPropertiesImpl http_server_properties;
  std::string trusted_spdy_proxy;
};

class SpdyURLRequestContext : public URLRequestContext {
 public:
  SpdyURLRequestContext();

  MockClientSocketFactory& socket_factory() { return socket_factory_; }

 protected:
  virtual ~SpdyURLRequestContext();

 private:
  MockClientSocketFactory socket_factory_;
  net::URLRequestContextStorage storage_;
};

const SpdyHeaderInfo MakeSpdyHeader(SpdyControlType type);

class SpdySessionPoolPeer {
 public:
  explicit SpdySessionPoolPeer(SpdySessionPool* pool)
      : pool_(pool) {}

  void AddAlias(const IPEndPoint& address, const HostPortProxyPair& pair) {
    pool_->AddAlias(address, pair);
  }

  void RemoveAliases(const HostPortProxyPair& pair) {
    pool_->RemoveAliases(pair);
  }

  void RemoveSpdySession(const scoped_refptr<SpdySession>& session) {
    pool_->Remove(session);
  }

  void DisableDomainAuthenticationVerification() {
    pool_->verify_domain_authentication_ = false;
  }

 private:
  SpdySessionPool* const pool_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPoolPeer);
};

// Helper to manage the state of a number of SPDY global variables.
class SpdyTestStateHelper {
 public:
  SpdyTestStateHelper();
  ~SpdyTestStateHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyTestStateHelper);
};

}  // namespace test_spdy2

}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_H_
