// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session.h"

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/stats_counters.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/signature_creator.h"
#include "net/base/asn1_util.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/server_bound_cert_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"

namespace net {

NetLogSpdySynParameter::NetLogSpdySynParameter(
    const linked_ptr<SpdyHeaderBlock>& headers,
    SpdyControlFlags flags,
    SpdyStreamId stream_id,
    SpdyStreamId associated_stream)
    : headers_(headers),
      flags_(flags),
      stream_id_(stream_id),
      associated_stream_(associated_stream) {
}

Value* NetLogSpdySynParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* headers_list = new ListValue();
  for (SpdyHeaderBlock::const_iterator it = headers_->begin();
      it != headers_->end(); ++it) {
    headers_list->Append(new StringValue(base::StringPrintf(
        "%s: %s", it->first.c_str(), it->second.c_str())));
  }
  dict->SetInteger("flags", flags_);
  dict->Set("headers", headers_list);
  dict->SetInteger("stream_id", stream_id_);
  if (associated_stream_)
    dict->SetInteger("associated_stream", associated_stream_);
  return dict;
}

NetLogSpdySynParameter::~NetLogSpdySynParameter() {}

NetLogSpdyCredentialParameter::NetLogSpdyCredentialParameter(
    size_t slot,
    const std::string& origin)
    : slot_(slot),
      origin_(origin) {
}

Value* NetLogSpdyCredentialParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("slot", slot_);
  dict->SetString("origin", origin_);
  return dict;
}

NetLogSpdyCredentialParameter::~NetLogSpdyCredentialParameter() {}

NetLogSpdySessionCloseParameter::NetLogSpdySessionCloseParameter(
    int status,
    const std::string& description)
    : status_(status),
      description_(description) {
}

Value* NetLogSpdySessionCloseParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("status", status_);
  dict->SetString("description", description_);
  return dict;
}

NetLogSpdySessionCloseParameter::~NetLogSpdySessionCloseParameter() {}

namespace {

const int kReadBufferSize = 8 * 1024;
const int kDefaultConnectionAtRiskOfLossSeconds = 10;
const int kTrailingPingDelayTimeSeconds = 1;
const int kHungIntervalSeconds = 10;

class NetLogSpdySessionParameter : public NetLog::EventParameters {
 public:
  NetLogSpdySessionParameter(const HostPortProxyPair& host_pair)
      : host_pair_(host_pair) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->Set("host", new StringValue(host_pair_.first.ToString()));
    dict->Set("proxy", new StringValue(host_pair_.second.ToPacString()));
    return dict;
  }

 private:
  virtual ~NetLogSpdySessionParameter() {}

  const HostPortProxyPair host_pair_;
  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySessionParameter);
};

class NetLogSpdySettingParameter : public NetLog::EventParameters {
 public:
  explicit NetLogSpdySettingParameter(SpdySettingsIds id,
                                      SpdySettingsFlags flags,
                                      uint32 value)
      : id_(id),
        flags_(flags),
        value_(value) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("id", id_);
    dict->SetInteger("flags", flags_);
    dict->SetInteger("value", value_);
    return dict;
  }

 private:
  virtual ~NetLogSpdySettingParameter() {}

  const SpdySettingsIds id_;
  const SpdySettingsFlags flags_;
  const uint32 value_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySettingParameter);
};

class NetLogSpdySettingsParameter : public NetLog::EventParameters {
 public:
  explicit NetLogSpdySettingsParameter(const SettingsMap& settings)
      : settings_(settings) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    ListValue* settings = new ListValue();
    for (SettingsMap::const_iterator it = settings_.begin();
         it != settings_.end(); ++it) {
      const SpdySettingsIds id = it->first;
      const SpdySettingsFlags flags = it->second.first;
      const uint32 value = it->second.second;
      settings->Append(new StringValue(
          base::StringPrintf("[id:%u flags:%u value:%u]", id, flags, value)));
    }
    dict->Set("settings", settings);
    return dict;
  }

 private:
  ~NetLogSpdySettingsParameter() {}
  const SettingsMap settings_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySettingsParameter);
};

class NetLogSpdyWindowUpdateParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyWindowUpdateParameter(SpdyStreamId stream_id, int32 delta)
      : stream_id_(stream_id), delta_(delta) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", static_cast<int>(stream_id_));
    dict->SetInteger("delta", delta_);
    return dict;
  }

 private:
  ~NetLogSpdyWindowUpdateParameter() {}
  const SpdyStreamId stream_id_;
  const int32 delta_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyWindowUpdateParameter);
};

class NetLogSpdyDataParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyDataParameter(SpdyStreamId stream_id,
                          int size,
                          SpdyDataFlags flags)
      : stream_id_(stream_id), size_(size), flags_(flags) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", static_cast<int>(stream_id_));
    dict->SetInteger("size", size_);
    dict->SetInteger("flags", static_cast<int>(flags_));
    return dict;
  }

 private:
  ~NetLogSpdyDataParameter() {}
  const SpdyStreamId stream_id_;
  const int size_;
  const SpdyDataFlags flags_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyDataParameter);
};

class NetLogSpdyRstParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyRstParameter(SpdyStreamId stream_id,
                         int status,
                         const std::string& description)
      : stream_id_(stream_id),
        status_(status),
        description_(description) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", static_cast<int>(stream_id_));
    dict->SetInteger("status", status_);
    dict->SetString("description", description_);
    return dict;
  }

 private:
  ~NetLogSpdyRstParameter() {}
  const SpdyStreamId stream_id_;
  const int status_;
  const std::string description_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyRstParameter);
};

class NetLogSpdyPingParameter : public NetLog::EventParameters {
 public:
  explicit NetLogSpdyPingParameter(uint32 unique_id, const std::string& type)
    : unique_id_(unique_id),
      type_(type) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("unique_id", unique_id_);
    dict->SetString("type", type_);
    return dict;
  }

 private:
  ~NetLogSpdyPingParameter() {}
  const uint32 unique_id_;
  const std::string type_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyPingParameter);
};

class NetLogSpdyGoAwayParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyGoAwayParameter(SpdyStreamId last_stream_id,
                            int active_streams,
                            int unclaimed_streams)
      : last_stream_id_(last_stream_id),
        active_streams_(active_streams),
        unclaimed_streams_(unclaimed_streams) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("last_accepted_stream_id",
                     static_cast<int>(last_stream_id_));
    dict->SetInteger("active_streams", active_streams_);
    dict->SetInteger("unclaimed_streams", unclaimed_streams_);
    return dict;
  }

 private:
  ~NetLogSpdyGoAwayParameter() {}
  const SpdyStreamId last_stream_id_;
  const int active_streams_;
  const int unclaimed_streams_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyGoAwayParameter);
};

NextProto g_default_protocol = kProtoUnknown;
size_t g_init_max_concurrent_streams = 10;
size_t g_max_concurrent_stream_limit = 256;
bool g_enable_ping_based_connection_checking = true;

}  // namespace

// static
void SpdySession::set_default_protocol(NextProto default_protocol) {
  g_default_protocol = default_protocol;
}

// static
void SpdySession::set_max_concurrent_streams(size_t value) {
  g_max_concurrent_stream_limit = value;
}

// static
void SpdySession::set_enable_ping_based_connection_checking(bool enable) {
  g_enable_ping_based_connection_checking = enable;
}

// static
void SpdySession::set_init_max_concurrent_streams(size_t value) {
  g_init_max_concurrent_streams =
      std::min(value, g_max_concurrent_stream_limit);
}

// static
void SpdySession::ResetStaticSettingsToInit() {
  // WARNING: These must match the initializers above.
  g_default_protocol = kProtoUnknown;
  g_init_max_concurrent_streams = 10;
  g_max_concurrent_stream_limit = 256;
  g_enable_ping_based_connection_checking = true;
}

SpdySession::SpdySession(const HostPortProxyPair& host_port_proxy_pair,
                         SpdySessionPool* spdy_session_pool,
                         HttpServerProperties* http_server_properties,
                         bool verify_domain_authentication,
                         const HostPortPair& trusted_spdy_proxy,
                         NetLog* net_log)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      host_port_proxy_pair_(host_port_proxy_pair),
      spdy_session_pool_(spdy_session_pool),
      http_server_properties_(http_server_properties),
      connection_(new ClientSocketHandle),
      read_buffer_(new IOBuffer(kReadBufferSize)),
      read_pending_(false),
      stream_hi_water_mark_(1),  // Always start at 1 for the first stream id.
      write_pending_(false),
      delayed_write_pending_(false),
      is_secure_(false),
      certificate_error_code_(OK),
      error_(OK),
      state_(IDLE),
      max_concurrent_streams_(g_init_max_concurrent_streams),
      streams_initiated_count_(0),
      streams_pushed_count_(0),
      streams_pushed_and_claimed_count_(0),
      streams_abandoned_count_(0),
      bytes_received_(0),
      sent_settings_(false),
      received_settings_(false),
      stalled_streams_(0),
      pings_in_flight_(0),
      next_ping_id_(1),
      received_data_time_(base::TimeTicks::Now()),
      trailing_ping_pending_(false),
      check_ping_status_pending_(false),
      need_to_send_ping_(false),
      flow_control_(false),
      initial_send_window_size_(kSpdyStreamInitialWindowSize),
      initial_recv_window_size_(kSpdyStreamInitialWindowSize),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SPDY_SESSION)),
      verify_domain_authentication_(verify_domain_authentication),
      credential_state_(SpdyCredentialState::kDefaultNumSlots),
      connection_at_risk_of_loss_time_(
          base::TimeDelta::FromSeconds(kDefaultConnectionAtRiskOfLossSeconds)),
      trailing_ping_delay_time_(
          base::TimeDelta::FromSeconds(kTrailingPingDelayTimeSeconds)),
      hung_interval_(
          base::TimeDelta::FromSeconds(kHungIntervalSeconds)),
      trusted_spdy_proxy_(trusted_spdy_proxy) {
  DCHECK(HttpStreamFactory::spdy_enabled());
  net_log_.BeginEvent(
      NetLog::TYPE_SPDY_SESSION,
      make_scoped_refptr(
          new NetLogSpdySessionParameter(host_port_proxy_pair_)));
  // TODO(mbelshe): consider randomization of the stream_hi_water_mark.
}

SpdySession::PendingCreateStream::~PendingCreateStream() {}

SpdySession::CallbackResultPair::~CallbackResultPair() {}

SpdySession::~SpdySession() {
  if (state_ != CLOSED) {
    state_ = CLOSED;

    // Cleanup all the streams.
    CloseAllStreams(net::ERR_ABORTED);
  }

  if (connection_->is_initialized()) {
    // With SPDY we can't recycle sockets.
    connection_->socket()->Disconnect();
  }

  // Streams should all be gone now.
  DCHECK_EQ(0u, num_active_streams());
  DCHECK_EQ(0u, num_unclaimed_pushed_streams());

  DCHECK(pending_callback_map_.empty());

  RecordHistograms();

  net_log_.EndEvent(NetLog::TYPE_SPDY_SESSION, NULL);
}

net::Error SpdySession::InitializeWithSocket(
    ClientSocketHandle* connection,
    bool is_secure,
    int certificate_error_code) {
  base::StatsCounter spdy_sessions("spdy.sessions");
  spdy_sessions.Increment();

  state_ = CONNECTED;
  connection_.reset(connection);
  is_secure_ = is_secure;
  certificate_error_code_ = certificate_error_code;

  NextProto protocol = g_default_protocol;
  NextProto protocol_negotiated = connection->socket()->GetNegotiatedProtocol();
  if (protocol_negotiated != kProtoUnknown) {
    protocol = protocol_negotiated;
  }

  SSLClientSocket* ssl_socket = GetSSLClientSocket();
  if (ssl_socket && ssl_socket->WasDomainBoundCertSent()) {
    // According to the SPDY spec, the credential associated with the TLS
    // connection is stored in slot[1].
    credential_state_.SetHasCredential(GURL("https://" +
                                            host_port_pair().ToString()));
  }

  DCHECK(protocol >= kProtoSPDY2);
  DCHECK(protocol <= kProtoSPDY3);
  int version = (protocol == kProtoSPDY3) ? 3 : 2;
  flow_control_ = (protocol >= kProtoSPDY3);

  buffered_spdy_framer_.reset(new BufferedSpdyFramer(version));
  buffered_spdy_framer_->set_visitor(this);
  SendSettings();

  // Write out any data that we might have to send, such as the settings frame.
  WriteSocketLater();
  net::Error error = ReadSocket();
  if (error == ERR_IO_PENDING)
    return OK;
  return error;
}

bool SpdySession::VerifyDomainAuthentication(const std::string& domain) {
  if (!verify_domain_authentication_)
    return true;

  if (state_ != CONNECTED)
    return false;

  SSLInfo ssl_info;
  bool was_npn_negotiated;
  NextProto protocol_negotiated = kProtoUnknown;
  if (!GetSSLInfo(&ssl_info, &was_npn_negotiated, &protocol_negotiated))
    return true;   // This is not a secure session, so all domains are okay.

  return !ssl_info.client_cert_sent && ssl_info.cert->VerifyNameMatch(domain);
}

int SpdySession::GetPushStream(
    const GURL& url,
    scoped_refptr<SpdyStream>* stream,
    const BoundNetLog& stream_net_log) {
  CHECK_NE(state_, CLOSED);

  *stream = NULL;

  // Don't allow access to secure push streams over an unauthenticated, but
  // encrypted SSL socket.
  if (is_secure_ && certificate_error_code_ != OK &&
      (url.SchemeIs("https") || url.SchemeIs("wss"))) {
    RecordProtocolErrorHistogram(
        PROTOCOL_ERROR_REQUST_FOR_SECURE_CONTENT_OVER_INSECURE_SESSION);
    CloseSessionOnError(
        static_cast<net::Error>(certificate_error_code_),
        true,
        "Tried to get SPDY stream for secure content over an unauthenticated "
        "session.");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  *stream = GetActivePushStream(url.spec());
  if (stream->get()) {
    DCHECK(streams_pushed_and_claimed_count_ < streams_pushed_count_);
    streams_pushed_and_claimed_count_++;
    return OK;
  }
  return 0;
}

int SpdySession::CreateStream(
    const GURL& url,
    RequestPriority priority,
    scoped_refptr<SpdyStream>* spdy_stream,
    const BoundNetLog& stream_net_log,
    const CompletionCallback& callback) {
  if (!max_concurrent_streams_ ||
      active_streams_.size() < max_concurrent_streams_) {
    return CreateStreamImpl(url, priority, spdy_stream, stream_net_log);
  }

  stalled_streams_++;
  net_log().AddEvent(NetLog::TYPE_SPDY_SESSION_STALLED_MAX_STREAMS, NULL);
  create_stream_queues_[priority].push(
      PendingCreateStream(url, priority, spdy_stream,
                          stream_net_log, callback));
  return ERR_IO_PENDING;
}

void SpdySession::ProcessPendingCreateStreams() {
  while (!max_concurrent_streams_ ||
         active_streams_.size() < max_concurrent_streams_) {
    bool no_pending_create_streams = true;
    for (int i = NUM_PRIORITIES - 1; i >= MINIMUM_PRIORITY; --i) {
      if (!create_stream_queues_[i].empty()) {
        PendingCreateStream pending_create = create_stream_queues_[i].front();
        create_stream_queues_[i].pop();
        no_pending_create_streams = false;
        int error = CreateStreamImpl(*pending_create.url,
                                     pending_create.priority,
                                     pending_create.spdy_stream,
                                     *pending_create.stream_net_log);
        scoped_refptr<SpdyStream>* stream = pending_create.spdy_stream;
        DCHECK(!ContainsKey(pending_callback_map_, stream));
        pending_callback_map_.insert(std::make_pair(stream,
            CallbackResultPair(pending_create.callback, error)));
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&SpdySession::InvokeUserStreamCreationCallback,
                       weak_factory_.GetWeakPtr(), stream));
        break;
      }
    }
    if (no_pending_create_streams)
      return;  // there were no streams in any queue
  }
}

void SpdySession::CancelPendingCreateStreams(
    const scoped_refptr<SpdyStream>* spdy_stream) {
  PendingCallbackMap::iterator it = pending_callback_map_.find(spdy_stream);
  if (it != pending_callback_map_.end()) {
    pending_callback_map_.erase(it);
    return;
  }

  for (int i = 0; i < NUM_PRIORITIES; ++i) {
    PendingCreateStreamQueue tmp;
    // Make a copy removing this trans
    while (!create_stream_queues_[i].empty()) {
      PendingCreateStream pending_create = create_stream_queues_[i].front();
      create_stream_queues_[i].pop();
      if (pending_create.spdy_stream != spdy_stream)
        tmp.push(pending_create);
    }
    // Now copy it back
    while (!tmp.empty()) {
      create_stream_queues_[i].push(tmp.front());
      tmp.pop();
    }
  }
}

int SpdySession::CreateStreamImpl(
    const GURL& url,
    RequestPriority priority,
    scoped_refptr<SpdyStream>* spdy_stream,
    const BoundNetLog& stream_net_log) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LT(priority, NUM_PRIORITIES);

  // Make sure that we don't try to send https/wss over an unauthenticated, but
  // encrypted SSL socket.
  if (is_secure_ && certificate_error_code_ != OK &&
      (url.SchemeIs("https") || url.SchemeIs("wss"))) {
    RecordProtocolErrorHistogram(
        PROTOCOL_ERROR_REQUST_FOR_SECURE_CONTENT_OVER_INSECURE_SESSION);
    CloseSessionOnError(
        static_cast<net::Error>(certificate_error_code_),
        true,
        "Tried to create SPDY stream for secure content over an "
        "unauthenticated session.");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  const std::string& path = url.PathForRequest();

  const SpdyStreamId stream_id = GetNewStreamId();

  *spdy_stream = new SpdyStream(this,
                                stream_id,
                                false,
                                stream_net_log);
  const scoped_refptr<SpdyStream>& stream = *spdy_stream;

  stream->set_priority(priority);
  stream->set_path(path);
  stream->set_send_window_size(initial_send_window_size_);
  stream->set_recv_window_size(initial_recv_window_size_);
  ActivateStream(stream);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyPriorityCount",
      static_cast<int>(priority), 0, 10, 11);

  // TODO(mbelshe): Optimize memory allocations

  DCHECK_EQ(active_streams_[stream_id].get(), stream.get());
  return OK;
}

bool SpdySession::NeedsCredentials() const {
  if (!is_secure_)
    return false;
  SSLClientSocket* ssl_socket = GetSSLClientSocket();
  if (ssl_socket->GetNegotiatedProtocol() < kProtoSPDY3)
    return false;
  return ssl_socket->WasDomainBoundCertSent();
}

void SpdySession::AddPooledAlias(const HostPortProxyPair& alias) {
  pooled_aliases_.insert(alias);
}

int SpdySession::GetProtocolVersion() const {
  DCHECK(buffered_spdy_framer_.get());
  return buffered_spdy_framer_->protocol_version();
}

int SpdySession::WriteSynStream(
    SpdyStreamId stream_id,
    RequestPriority priority,
    uint8 credential_slot,
    SpdyControlFlags flags,
    const linked_ptr<SpdyHeaderBlock>& headers) {
  // Find our stream
  if (!IsStreamActive(stream_id))
    return ERR_INVALID_SPDY_STREAM;
  const scoped_refptr<SpdyStream>& stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);

  SendPrefacePingIfNoneInFlight();

  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdySynStreamControlFrame> syn_frame(
      buffered_spdy_framer_->CreateSynStream(
          stream_id, 0,
          ConvertRequestPriorityToSpdyPriority(priority, GetProtocolVersion()),
          credential_slot, flags, false, headers.get()));
  QueueFrame(syn_frame.get(), priority, stream);

  base::StatsCounter spdy_requests("spdy.requests");
  spdy_requests.Increment();
  streams_initiated_count_++;

  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_SYN_STREAM,
        make_scoped_refptr(
            new NetLogSpdySynParameter(headers, flags, stream_id, 0)));
  }

  // Some servers don't like too many pings, so we limit our current sending to
  // no more than two pings for any syn frame or data frame sent.  To do this,
  // we avoid ever setting this to true unless we send a syn (which we have just
  // done) or data frame. This approach may change over time as servers change
  // their responses to pings.
  need_to_send_ping_ = true;

  return ERR_IO_PENDING;
}

int SpdySession::WriteCredentialFrame(const std::string& origin,
                                      SSLClientCertType type,
                                      const std::string& key,
                                      const std::string& cert,
                                      RequestPriority priority) {
  DCHECK(is_secure_);
  unsigned char secret[32];  // 32 bytes from the spec
  GetSSLClientSocket()->ExportKeyingMaterial("SPDY certificate proof",
                                             true, origin,
                                             secret, arraysize(secret));

  // Convert the key string into a vector<unit8>
  std::vector<uint8> key_data;
  for (size_t i = 0; i < key.length(); i++) {
    key_data.push_back(key[i]);
  }

  std::vector<uint8> proof;
  switch (type) {
    case CLIENT_CERT_ECDSA_SIGN: {
      base::StringPiece spki_piece;
      asn1::ExtractSPKIFromDERCert(cert, &spki_piece);
      std::vector<uint8> spki(spki_piece.data(),
                              spki_piece.data() + spki_piece.size());
      scoped_ptr<crypto::ECPrivateKey> private_key(
          crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
              ServerBoundCertService::kEPKIPassword, key_data, spki));
      scoped_ptr<crypto::ECSignatureCreator> creator(
          crypto::ECSignatureCreator::Create(private_key.get()));
      creator->Sign(secret, arraysize(secret), &proof);
      break;
    }
    default:
      NOTREACHED();
  }

  SpdyCredential credential;
  GURL origin_url(origin);
  credential.slot =
      credential_state_.SetHasCredential(origin_url);
  credential.certs.push_back(cert);
  credential.proof.assign(proof.begin(), proof.end());

  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdyCredentialControlFrame> credential_frame(
      buffered_spdy_framer_->CreateCredentialFrame(credential));
  QueueFrame(credential_frame.get(), priority, NULL);

  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_SEND_CREDENTIAL,
        make_scoped_refptr(
            new NetLogSpdyCredentialParameter(credential.slot,
                                              origin)));
  }
  return ERR_IO_PENDING;
}

int SpdySession::WriteStreamData(SpdyStreamId stream_id,
                                 net::IOBuffer* data, int len,
                                 SpdyDataFlags flags) {
  // Find our stream
  CHECK(IsStreamActive(stream_id));
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);

  if (len > kMaxSpdyFrameChunkSize) {
    len = kMaxSpdyFrameChunkSize;
    flags = static_cast<SpdyDataFlags>(flags & ~DATA_FLAG_FIN);
  }

  // Obey send window size of the stream if flow control is enabled.
  if (flow_control_) {
    if (stream->send_window_size() <= 0) {
      // Because we queue frames onto the session, it is possible that
      // a stream was not flow controlled at the time it attempted the
      // write, but when we go to fulfill the write, it is now flow
      // controlled.  This is why we need the session to mark the stream
      // as stalled - because only the session knows for sure when the
      // stall occurs.
      stream->set_stalled_by_flow_control(true);
      net_log().AddEvent(
          NetLog::TYPE_SPDY_SESSION_STALLED_ON_SEND_WINDOW,
          make_scoped_refptr(
              new NetLogIntegerParameter("stream_id", stream_id)));
      return ERR_IO_PENDING;
    }
    int new_len = std::min(len, stream->send_window_size());
    if (new_len < len) {
      len = new_len;
      flags = static_cast<SpdyDataFlags>(flags & ~DATA_FLAG_FIN);
    }
    stream->DecreaseSendWindowSize(len);
  }

  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_SEND_DATA,
        make_scoped_refptr(new NetLogSpdyDataParameter(stream_id, len, flags)));
  }

  // Send PrefacePing for DATA_FRAMEs with nonzero payload size.
  if (len > 0)
    SendPrefacePingIfNoneInFlight();

  // TODO(mbelshe): reduce memory copies here.
  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdyDataFrame> frame(
      buffered_spdy_framer_->CreateDataFrame(
          stream_id, data->data(), len, flags));
  QueueFrame(frame.get(), stream->priority(), stream);

  // Some servers don't like too many pings, so we limit our current sending to
  // no more than two pings for any syn frame or data frame sent.  To do this,
  // we avoid ever setting this to true unless we send a syn (which we have just
  // done) or data frame. This approach may change over time as servers change
  // their responses to pings.
  if (len > 0)
    need_to_send_ping_ = true;

  return ERR_IO_PENDING;
}

void SpdySession::CloseStream(SpdyStreamId stream_id, int status) {
  // TODO(mbelshe): We should send a RST_STREAM control frame here
  //                so that the server can cancel a large send.

  DeleteStream(stream_id, status);
}

void SpdySession::ResetStream(SpdyStreamId stream_id,
                              SpdyStatusCodes status,
                              const std::string& description) {
  net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_SEND_RST_STREAM,
      make_scoped_refptr(new NetLogSpdyRstParameter(stream_id, status,
                                                    description)));

  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdyRstStreamControlFrame> rst_frame(
      buffered_spdy_framer_->CreateRstStream(stream_id, status));

  // Default to lowest priority unless we know otherwise.
  RequestPriority priority = net::IDLE;
  if(IsStreamActive(stream_id)) {
    scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
    priority = stream->priority();
  }
  QueueFrame(rst_frame.get(), priority, NULL);
  RecordProtocolErrorHistogram(
      static_cast<SpdyProtocolErrorDetails>(status + STATUS_CODE_INVALID));
  DeleteStream(stream_id, ERR_SPDY_PROTOCOL_ERROR);
}

bool SpdySession::IsStreamActive(SpdyStreamId stream_id) const {
  return ContainsKey(active_streams_, stream_id);
}

LoadState SpdySession::GetLoadState() const {
  // NOTE: The application only queries the LoadState via the
  //       SpdyNetworkTransaction, and details are only needed when
  //       we're in the process of connecting.

  // If we're connecting, defer to the connection to give us the actual
  // LoadState.
  if (state_ == CONNECTING)
    return connection_->GetLoadState();

  // Just report that we're idle since the session could be doing
  // many things concurrently.
  return LOAD_STATE_IDLE;
}

void SpdySession::OnReadComplete(int bytes_read) {
  // Parse a frame.  For now this code requires that the frame fit into our
  // buffer (32KB).
  // TODO(mbelshe): support arbitrarily large frames!

  read_pending_ = false;

  if (bytes_read <= 0) {
    // Session is tearing down.
    net::Error error = static_cast<net::Error>(bytes_read);
    if (bytes_read == 0)
      error = ERR_CONNECTION_CLOSED;
    CloseSessionOnError(error, true, "bytes_read is <= 0.");
    return;
  }

  bytes_received_ += bytes_read;

  received_data_time_ = base::TimeTicks::Now();

  // The SpdyFramer will use callbacks onto |this| as it parses frames.
  // When errors occur, those callbacks can lead to teardown of all references
  // to |this|, so maintain a reference to self during this call for safe
  // cleanup.
  scoped_refptr<SpdySession> self(this);

  DCHECK(buffered_spdy_framer_.get());
  char *data = read_buffer_->data();
  while (bytes_read &&
         buffered_spdy_framer_->error_code() ==
             SpdyFramer::SPDY_NO_ERROR) {
    uint32 bytes_processed =
        buffered_spdy_framer_->ProcessInput(data, bytes_read);
    bytes_read -= bytes_processed;
    data += bytes_processed;
    if (buffered_spdy_framer_->state() == SpdyFramer::SPDY_DONE)
      buffered_spdy_framer_->Reset();
  }

  if (state_ != CLOSED)
    ReadSocket();
}

void SpdySession::OnWriteComplete(int result) {
  DCHECK(write_pending_);
  DCHECK(in_flight_write_.size());

  write_pending_ = false;

  scoped_refptr<SpdyStream> stream = in_flight_write_.stream();

  if (result >= 0) {
    // It should not be possible to have written more bytes than our
    // in_flight_write_.
    DCHECK_LE(result, in_flight_write_.buffer()->BytesRemaining());

    in_flight_write_.buffer()->DidConsume(result);

    // We only notify the stream when we've fully written the pending frame.
    if (!in_flight_write_.buffer()->BytesRemaining()) {
      if (stream) {
        // Report the number of bytes written to the caller, but exclude the
        // frame size overhead.  NOTE: if this frame was compressed the
        // reported bytes written is the compressed size, not the original
        // size.
        if (result > 0) {
          result = in_flight_write_.buffer()->size();
          DCHECK_GE(result, static_cast<int>(SpdyFrame::kHeaderSize));
          result -= static_cast<int>(SpdyFrame::kHeaderSize);
        }

        // It is possible that the stream was cancelled while we were writing
        // to the socket.
        if (!stream->cancelled())
          stream->OnWriteComplete(result);
      }

      // Cleanup the write which just completed.
      in_flight_write_.release();
    }

    // Write more data.  We're already in a continuation, so we can
    // go ahead and write it immediately (without going back to the
    // message loop).
    WriteSocketLater();
  } else {
    in_flight_write_.release();

    // The stream is now errored.  Close it down.
    CloseSessionOnError(
        static_cast<net::Error>(result), true, "The stream has errored.");
  }
}

net::Error SpdySession::ReadSocket() {
  if (read_pending_)
    return OK;

  if (state_ == CLOSED) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  CHECK(connection_.get());
  CHECK(connection_->socket());
  int bytes_read = connection_->socket()->Read(
      read_buffer_.get(),
      kReadBufferSize,
      base::Bind(&SpdySession::OnReadComplete, base::Unretained(this)));
  switch (bytes_read) {
    case 0:
      // Socket is closed!
      CloseSessionOnError(ERR_CONNECTION_CLOSED, true, "bytes_read is 0.");
      return ERR_CONNECTION_CLOSED;
    case net::ERR_IO_PENDING:
      // Waiting for data.  Nothing to do now.
      read_pending_ = true;
      return ERR_IO_PENDING;
    default:
      // Data was read, process it.
      // Schedule the work through the message loop to avoid recursive
      // callbacks.
      read_pending_ = true;
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&SpdySession::OnReadComplete,
                     weak_factory_.GetWeakPtr(), bytes_read));
      break;
  }
  return OK;
}

void SpdySession::WriteSocketLater() {
  if (delayed_write_pending_)
    return;

  if (state_ < CONNECTED)
    return;

  delayed_write_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SpdySession::WriteSocket, weak_factory_.GetWeakPtr()));
}

void SpdySession::WriteSocket() {
  // This function should only be called via WriteSocketLater.
  DCHECK(delayed_write_pending_);
  delayed_write_pending_ = false;

  // If the socket isn't connected yet, just wait; we'll get called
  // again when the socket connection completes.  If the socket is
  // closed, just return.
  if (state_ < CONNECTED || state_ == CLOSED)
    return;

  if (write_pending_)   // Another write is in progress still.
    return;

  // Loop sending frames until we've sent everything or until the write
  // returns error (or ERR_IO_PENDING).
  DCHECK(buffered_spdy_framer_.get());
  while (in_flight_write_.buffer() || !queue_.empty()) {
    if (!in_flight_write_.buffer()) {
      // Grab the next SpdyFrame to send.
      SpdyIOBuffer next_buffer = queue_.top();
      queue_.pop();

      // We've deferred compression until just before we write it to the socket,
      // which is now.  At this time, we don't compress our data frames.
      SpdyFrame uncompressed_frame(next_buffer.buffer()->data(), false);
      size_t size;
      if (buffered_spdy_framer_->IsCompressible(uncompressed_frame)) {
        DCHECK(uncompressed_frame.is_control_frame());
        scoped_ptr<SpdyFrame> compressed_frame(
            buffered_spdy_framer_->CompressControlFrame(
                reinterpret_cast<const SpdyControlFrame&>(uncompressed_frame)));
        if (!compressed_frame.get()) {
          RecordProtocolErrorHistogram(
              PROTOCOL_ERROR_SPDY_COMPRESSION_FAILURE);
          CloseSessionOnError(
              net::ERR_SPDY_PROTOCOL_ERROR, true, "SPDY Compression failure.");
          return;
        }

        size = compressed_frame->length() + SpdyFrame::kHeaderSize;

        DCHECK_GT(size, 0u);

        // TODO(mbelshe): We have too much copying of data here.
        IOBufferWithSize* buffer = new IOBufferWithSize(size);
        memcpy(buffer->data(), compressed_frame->data(), size);

        // Attempt to send the frame.
        in_flight_write_ = SpdyIOBuffer(buffer, size, HIGHEST,
                                        next_buffer.stream());
      } else {
        size = uncompressed_frame.length() + SpdyFrame::kHeaderSize;
        in_flight_write_ = next_buffer;
      }
    } else {
      DCHECK(in_flight_write_.buffer()->BytesRemaining());
    }

    write_pending_ = true;
    int rv = connection_->socket()->Write(
        in_flight_write_.buffer(),
        in_flight_write_.buffer()->BytesRemaining(),
        base::Bind(&SpdySession::OnWriteComplete, base::Unretained(this)));
    if (rv == net::ERR_IO_PENDING)
      break;

    // We sent the frame successfully.
    OnWriteComplete(rv);

    // TODO(mbelshe):  Test this error case.  Maybe we should mark the socket
    //                 as in an error state.
    if (rv < 0)
      break;
  }
}

void SpdySession::CloseAllStreams(net::Error status) {
  base::StatsCounter abandoned_streams("spdy.abandoned_streams");
  base::StatsCounter abandoned_push_streams(
      "spdy.abandoned_push_streams");

  if (!active_streams_.empty())
    abandoned_streams.Add(active_streams_.size());
  if (!unclaimed_pushed_streams_.empty()) {
    streams_abandoned_count_ += unclaimed_pushed_streams_.size();
    abandoned_push_streams.Add(unclaimed_pushed_streams_.size());
    unclaimed_pushed_streams_.clear();
  }

  for (int i = 0; i < NUM_PRIORITIES; ++i) {
    while (!create_stream_queues_[i].empty()) {
      PendingCreateStream pending_create = create_stream_queues_[i].front();
      create_stream_queues_[i].pop();
      pending_create.callback.Run(ERR_ABORTED);
    }
  }

  while (!active_streams_.empty()) {
    ActiveStreamMap::iterator it = active_streams_.begin();
    const scoped_refptr<SpdyStream>& stream = it->second;
    DCHECK(stream);
    std::string description = base::StringPrintf(
        "ABANDONED (stream_id=%d): ", stream->stream_id()) + stream->path();
    stream->LogStreamError(status, description);
    DeleteStream(stream->stream_id(), status);
  }

  // We also need to drain the queue.
  while (queue_.size())
    queue_.pop();
}

int SpdySession::GetNewStreamId() {
  int id = stream_hi_water_mark_;
  stream_hi_water_mark_ += 2;
  if (stream_hi_water_mark_ > 0x7fff)
    stream_hi_water_mark_ = 1;
  return id;
}

void SpdySession::QueueFrame(SpdyFrame* frame,
                             RequestPriority priority,
                             SpdyStream* stream) {
  int length = SpdyFrame::kHeaderSize + frame->length();
  IOBuffer* buffer = new IOBuffer(length);
  memcpy(buffer->data(), frame->data(), length);
  queue_.push(SpdyIOBuffer(buffer, length, priority, stream));

  WriteSocketLater();
}

void SpdySession::CloseSessionOnError(net::Error err,
                                      bool remove_from_pool,
                                      const std::string& description) {
  // Closing all streams can have a side-effect of dropping the last reference
  // to |this|.  Hold a reference through this function.
  scoped_refptr<SpdySession> self(this);

  DCHECK_LT(err, OK);
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_CLOSE,
      make_scoped_refptr(
          new NetLogSpdySessionCloseParameter(err, description)));

  // Don't close twice.  This can occur because we can have both
  // a read and a write outstanding, and each can complete with
  // an error.
  if (state_ != CLOSED) {
    state_ = CLOSED;
    error_ = err;
    if (remove_from_pool)
      RemoveFromPool();
    CloseAllStreams(err);
  }
}

Value* SpdySession::GetInfoAsValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetInteger("source_id", net_log_.source().id);

  dict->SetString("host_port_pair", host_port_proxy_pair_.first.ToString());
  if (!pooled_aliases_.empty()) {
    ListValue* alias_list = new ListValue();
    for (std::set<HostPortProxyPair>::const_iterator it =
             pooled_aliases_.begin();
         it != pooled_aliases_.end(); it++) {
      alias_list->Append(Value::CreateStringValue(it->first.ToString()));
    }
    dict->Set("aliases", alias_list);
  }
  dict->SetString("proxy", host_port_proxy_pair_.second.ToURI());

  dict->SetInteger("active_streams", active_streams_.size());

  dict->SetInteger("unclaimed_pushed_streams",
      unclaimed_pushed_streams_.size());

  dict->SetBoolean("is_secure", is_secure_);

  NextProto proto = kProtoUnknown;
  if (is_secure_) {
    proto = GetSSLClientSocket()->GetNegotiatedProtocol();
  }
  dict->SetString("protocol_negotiated",
                  SSLClientSocket::NextProtoToString(proto));

  dict->SetInteger("error", error_);
  dict->SetInteger("max_concurrent_streams", max_concurrent_streams_);

  dict->SetInteger("streams_initiated_count", streams_initiated_count_);
  dict->SetInteger("streams_pushed_count", streams_pushed_count_);
  dict->SetInteger("streams_pushed_and_claimed_count",
      streams_pushed_and_claimed_count_);
  dict->SetInteger("streams_abandoned_count", streams_abandoned_count_);
  DCHECK(buffered_spdy_framer_.get());
  dict->SetInteger("frames_received", buffered_spdy_framer_->frames_received());

  dict->SetBoolean("sent_settings", sent_settings_);
  dict->SetBoolean("received_settings", received_settings_);
  return dict;
}

bool SpdySession::IsReused() const {
  return buffered_spdy_framer_->frames_received() > 0;
}

int SpdySession::GetPeerAddress(AddressList* address) const {
  if (!connection_->socket())
    return ERR_SOCKET_NOT_CONNECTED;

  return connection_->socket()->GetPeerAddress(address);
}

int SpdySession::GetLocalAddress(IPEndPoint* address) const {
  if (!connection_->socket())
    return ERR_SOCKET_NOT_CONNECTED;

  return connection_->socket()->GetLocalAddress(address);
}

void SpdySession::ActivateStream(SpdyStream* stream) {
  const SpdyStreamId id = stream->stream_id();
  DCHECK(!IsStreamActive(id));

  active_streams_[id] = stream;
}

void SpdySession::DeleteStream(SpdyStreamId id, int status) {
  // For push streams, if they are being deleted normally, we leave
  // the stream in the unclaimed_pushed_streams_ list.  However, if
  // the stream is errored out, clean it up entirely.
  if (status != OK) {
    PushedStreamMap::iterator it;
    for (it = unclaimed_pushed_streams_.begin();
         it != unclaimed_pushed_streams_.end(); ++it) {
      scoped_refptr<SpdyStream> curr = it->second;
      if (id == curr->stream_id()) {
        unclaimed_pushed_streams_.erase(it);
        break;
      }
    }
  }

  // The stream might have been deleted.
  ActiveStreamMap::iterator it2 = active_streams_.find(id);
  if (it2 == active_streams_.end())
    return;

  // If this is an active stream, call the callback.
  const scoped_refptr<SpdyStream> stream(it2->second);
  active_streams_.erase(it2);
  if (stream)
    stream->OnClose(status);
  ProcessPendingCreateStreams();
}

void SpdySession::RemoveFromPool() {
  if (spdy_session_pool_) {
    SpdySessionPool* pool = spdy_session_pool_;
    spdy_session_pool_ = NULL;
    pool->Remove(make_scoped_refptr(this));
  }
}

scoped_refptr<SpdyStream> SpdySession::GetActivePushStream(
    const std::string& path) {
  base::StatsCounter used_push_streams("spdy.claimed_push_streams");

  PushedStreamMap::iterator it = unclaimed_pushed_streams_.find(path);
  if (it != unclaimed_pushed_streams_.end()) {
    net_log_.AddEvent(NetLog::TYPE_SPDY_STREAM_ADOPTED_PUSH_STREAM, NULL);
    scoped_refptr<SpdyStream> stream = it->second;
    unclaimed_pushed_streams_.erase(it);
    used_push_streams.Increment();
    return stream;
  }
  return NULL;
}

bool SpdySession::GetSSLInfo(SSLInfo* ssl_info,
                             bool* was_npn_negotiated,
                             NextProto* protocol_negotiated) {
  if (!is_secure_) {
    *protocol_negotiated = kProtoUnknown;
    return false;
  }
  SSLClientSocket* ssl_socket = GetSSLClientSocket();
  ssl_socket->GetSSLInfo(ssl_info);
  *was_npn_negotiated = ssl_socket->was_npn_negotiated();
  *protocol_negotiated = ssl_socket->GetNegotiatedProtocol();
  return true;
}

bool SpdySession::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  if (!is_secure_)
    return false;
  GetSSLClientSocket()->GetSSLCertRequestInfo(cert_request_info);
  return true;
}

ServerBoundCertService* SpdySession::GetServerBoundCertService() const {
  if (!is_secure_)
    return NULL;
  return GetSSLClientSocket()->GetServerBoundCertService();
}

SSLClientCertType SpdySession::GetDomainBoundCertType() const {
  if (!is_secure_)
    return CLIENT_CERT_INVALID_TYPE;
  return GetSSLClientSocket()->domain_bound_cert_type();
}

void SpdySession::OnError(SpdyFramer::SpdyError error_code) {
  RecordProtocolErrorHistogram(
      static_cast<SpdyProtocolErrorDetails>(error_code));
  std::string description = base::StringPrintf(
      "SPDY_ERROR error_code: %d.", error_code);
  CloseSessionOnError(net::ERR_SPDY_PROTOCOL_ERROR, true, description);
}

void SpdySession::OnStreamError(SpdyStreamId stream_id,
                                const std::string& description) {
  if (IsStreamActive(stream_id))
    ResetStream(stream_id, PROTOCOL_ERROR, description);
}

void SpdySession::OnStreamFrameData(SpdyStreamId stream_id,
                                    const char* data,
                                    size_t len) {
  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_RECV_DATA,
        make_scoped_refptr(new NetLogSpdyDataParameter(
            stream_id, len, SpdyDataFlags())));
  }

  if (!IsStreamActive(stream_id)) {
    // NOTE:  it may just be that the stream was cancelled.
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  stream->OnDataReceived(data, len);
}

void SpdySession::OnSetting(SpdySettingsIds id,
                            uint8 flags,
                            uint32 value) {
  HandleSetting(id, value);
  http_server_properties_->SetSpdySetting(
      host_port_pair(),
      id,
      static_cast<SpdySettingsFlags>(flags),
      value);
  received_settings_ = true;

  // Log the setting.
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_RECV_SETTING,
      make_scoped_refptr(new NetLogSpdySettingParameter(
          id, static_cast<SpdySettingsFlags>(flags), value)));
}

bool SpdySession::Respond(const SpdyHeaderBlock& headers,
                          const scoped_refptr<SpdyStream> stream) {
  int rv = OK;
  rv = stream->OnResponseReceived(headers);
  if (rv < 0) {
    DCHECK_NE(rv, ERR_IO_PENDING);
    const SpdyStreamId stream_id = stream->stream_id();
    DeleteStream(stream_id, rv);
    return false;
  }
  return true;
}

void SpdySession::OnSynStream(
    const SpdySynStreamControlFrame& frame,
    const linked_ptr<SpdyHeaderBlock>& headers) {
  SpdyStreamId stream_id = frame.stream_id();
  SpdyStreamId associated_stream_id = frame.associated_stream_id();

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_SPDY_SESSION_PUSHED_SYN_STREAM,
        make_scoped_refptr(new NetLogSpdySynParameter(
            headers, static_cast<SpdyControlFlags>(frame.flags()),
            stream_id, associated_stream_id)));
  }

  // Server-initiated streams should have even sequence numbers.
  if ((stream_id & 0x1) != 0) {
    LOG(WARNING) << "Received invalid OnSyn stream id " << stream_id;
    return;
  }

  if (IsStreamActive(stream_id)) {
    LOG(WARNING) << "Received OnSyn for active stream " << stream_id;
    return;
  }

  if (associated_stream_id == 0) {
    std::string description = base::StringPrintf(
        "Received invalid OnSyn associated stream id %d for stream %d",
        associated_stream_id, stream_id);
    ResetStream(stream_id, INVALID_STREAM, description);
    return;
  }

  streams_pushed_count_++;

  // TODO(mbelshe): DCHECK that this is a GET method?

  // Verify that the response had a URL for us.
  GURL gurl = GetUrlFromHeaderBlock(*headers, GetProtocolVersion(), true);
  if (!gurl.is_valid()) {
    ResetStream(stream_id, PROTOCOL_ERROR,
                "Pushed stream url was invalid: " + gurl.spec());
    return;
  }
  const std::string& url = gurl.spec();

  // Verify we have a valid stream association.
  if (!IsStreamActive(associated_stream_id)) {
    ResetStream(stream_id, INVALID_STREAM,
                base::StringPrintf(
                    "Received OnSyn with inactive associated stream %d",
                    associated_stream_id));
    return;
  }

  // Check that the SYN advertises the same origin as its associated stream.
  // Bypass this check if and only if this session is with a SPDY proxy that
  // is trusted explicitly via the --trusted-spdy-proxy switch.
  if (trusted_spdy_proxy_.Equals(host_port_pair())) {
    // Disallow pushing of HTTPS content.
    if (gurl.SchemeIs("https")) {
      ResetStream(stream_id, REFUSED_STREAM,
                  base::StringPrintf(
                      "Rejected push of Cross Origin HTTPS content %d",
                      associated_stream_id));
    }
  } else {
    scoped_refptr<SpdyStream> associated_stream =
        active_streams_[associated_stream_id];
    GURL associated_url(associated_stream->GetUrl());
    if (associated_url.GetOrigin() != gurl.GetOrigin()) {
      ResetStream(stream_id, REFUSED_STREAM,
                  base::StringPrintf(
                      "Rejected Cross Origin Push Stream %d",
                      associated_stream_id));
      return;
    }
  }

  // There should not be an existing pushed stream with the same path.
  PushedStreamMap::iterator it = unclaimed_pushed_streams_.find(url);
  if (it != unclaimed_pushed_streams_.end()) {
    ResetStream(stream_id, PROTOCOL_ERROR,
                "Received duplicate pushed stream with url: " + url);
    return;
  }

  scoped_refptr<SpdyStream> stream(
      new SpdyStream(this, stream_id, true, net_log_));

  stream->set_path(gurl.PathForRequest());
  stream->set_send_window_size(initial_send_window_size_);
  stream->set_recv_window_size(initial_recv_window_size_);

  unclaimed_pushed_streams_[url] = stream;

  ActivateStream(stream);
  stream->set_response_received();

  // Parse the headers.
  if (!Respond(*headers, stream))
    return;

  base::StatsCounter push_requests("spdy.pushed_streams");
  push_requests.Increment();
}

void SpdySession::OnSynReply(const SpdySynReplyControlFrame& frame,
                             const linked_ptr<SpdyHeaderBlock>& headers) {
  SpdyStreamId stream_id = frame.stream_id();

  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_SYN_REPLY,
        make_scoped_refptr(new NetLogSpdySynParameter(
            headers, static_cast<SpdyControlFlags>(frame.flags()),
            stream_id, 0)));
  }

  if (!IsStreamActive(stream_id)) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received SYN_REPLY for invalid stream " << stream_id;
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  if (stream->response_received()) {
    stream->LogStreamError(ERR_SYN_REPLY_NOT_RECEIVED,
                           "Received duplicate SYN_REPLY for stream.");
    RecordProtocolErrorHistogram(PROTOCOL_ERROR_SYN_REPLY_NOT_RECEIVED);
    CloseStream(stream->stream_id(), ERR_SPDY_PROTOCOL_ERROR);
    return;
  }
  stream->set_response_received();

  Respond(*headers, stream);
}

void SpdySession::OnHeaders(const SpdyHeadersControlFrame& frame,
                            const linked_ptr<SpdyHeaderBlock>& headers) {
  SpdyStreamId stream_id = frame.stream_id();

  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_HEADERS,
        make_scoped_refptr(new NetLogSpdySynParameter(
            headers, static_cast<SpdyControlFlags>(frame.flags()),
            stream_id, 0)));
  }

  if (!IsStreamActive(stream_id)) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received HEADERS for invalid stream " << stream_id;
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  int rv = stream->OnHeaders(*headers);
  if (rv < 0) {
    DCHECK_NE(rv, ERR_IO_PENDING);
    const SpdyStreamId stream_id = stream->stream_id();
    DeleteStream(stream_id, rv);
  }
}

void SpdySession::OnRstStream(const SpdyRstStreamControlFrame& frame) {
  SpdyStreamId stream_id = frame.stream_id();

  net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_RST_STREAM,
      make_scoped_refptr(
          new NetLogSpdyRstParameter(stream_id, frame.status(), "")));

  if (!IsStreamActive(stream_id)) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received RST for invalid stream" << stream_id;
    return;
  }
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  if (frame.status() == 0) {
    stream->OnDataReceived(NULL, 0);
  } else if (frame.status() == REFUSED_STREAM) {
    DeleteStream(stream_id, ERR_SPDY_SERVER_REFUSED_STREAM);
  } else {
    RecordProtocolErrorHistogram(
        PROTOCOL_ERROR_RST_STREAM_FOR_NON_ACTIVE_STREAM);
    stream->LogStreamError(ERR_SPDY_PROTOCOL_ERROR,
                           base::StringPrintf("SPDY stream closed: %d",
                                              frame.status()));
    // TODO(mbelshe): Map from Spdy-protocol errors to something sensical.
    //                For now, it doesn't matter much - it is a protocol error.
    DeleteStream(stream_id, ERR_SPDY_PROTOCOL_ERROR);
  }
}

void SpdySession::OnGoAway(const SpdyGoAwayControlFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_GOAWAY,
      make_scoped_refptr(
          new NetLogSpdyGoAwayParameter(frame.last_accepted_stream_id(),
                                        active_streams_.size(),
                                        unclaimed_pushed_streams_.size())));
  RemoveFromPool();
  CloseAllStreams(net::ERR_ABORTED);

  // TODO(willchan): Cancel any streams that are past the GoAway frame's
  // |last_accepted_stream_id|.

  // Don't bother killing any streams that are still reading.  They'll either
  // complete successfully or get an ERR_CONNECTION_CLOSED when the socket is
  // closed.
}

void SpdySession::OnPing(const SpdyPingControlFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_PING,
      make_scoped_refptr(
          new NetLogSpdyPingParameter(frame.unique_id(), "received")));

  // Send response to a PING from server.
  if (frame.unique_id() % 2 == 0) {
    WritePingFrame(frame.unique_id());
    return;
  }

  --pings_in_flight_;
  if (pings_in_flight_ < 0) {
    RecordProtocolErrorHistogram(PROTOCOL_ERROR_UNEXPECTED_PING);
    CloseSessionOnError(
        net::ERR_SPDY_PROTOCOL_ERROR, true, "pings_in_flight_ is < 0.");
    return;
  }

  if (pings_in_flight_ > 0)
    return;

  // We will record RTT in histogram when there are no more client sent
  // pings_in_flight_.
  RecordPingRTTHistogram(base::TimeTicks::Now() - last_ping_sent_time_);

  if (!need_to_send_ping_)
    return;

  PlanToSendTrailingPing();
}

void SpdySession::OnWindowUpdate(
    const SpdyWindowUpdateControlFrame& frame) {
  SpdyStreamId stream_id = frame.stream_id();
  int32 delta_window_size = static_cast<int32>(frame.delta_window_size());
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_RECEIVED_WINDOW_UPDATE,
      make_scoped_refptr(new NetLogSpdyWindowUpdateParameter(
          stream_id, delta_window_size)));

  if (!IsStreamActive(stream_id)) {
    LOG(WARNING) << "Received WINDOW_UPDATE for invalid stream " << stream_id;
    return;
  }

  if (delta_window_size < 1) {
    ResetStream(stream_id, FLOW_CONTROL_ERROR,
                base::StringPrintf(
                    "Received WINDOW_UPDATE with an invalid "
                    "delta_window_size %d", delta_window_size));
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  if (flow_control_)
    stream->IncreaseSendWindowSize(delta_window_size);
}

void SpdySession::SendWindowUpdate(SpdyStreamId stream_id,
                                   int32 delta_window_size) {
  CHECK(IsStreamActive(stream_id));
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_SENT_WINDOW_UPDATE,
      make_scoped_refptr(new NetLogSpdyWindowUpdateParameter(
          stream_id, delta_window_size)));

  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdyWindowUpdateControlFrame> window_update_frame(
      buffered_spdy_framer_->CreateWindowUpdate(stream_id, delta_window_size));
  QueueFrame(window_update_frame.get(), stream->priority(), NULL);
}

// Given a cwnd that we would have sent to the server, modify it based on the
// field trial policy.
uint32 ApplyCwndFieldTrialPolicy(int cwnd) {
  base::FieldTrial* trial = base::FieldTrialList::Find("SpdyCwnd");
  if (!trial) {
      LOG(WARNING) << "Could not find \"SpdyCwnd\" in FieldTrialList";
      return cwnd;
  }
  if (trial->group_name() == "cwnd10")
    return 10;
  else if (trial->group_name() == "cwnd16")
    return 16;
  else if (trial->group_name() == "cwndMin16")
    return std::max(cwnd, 16);
  else if (trial->group_name() == "cwndMin10")
    return std::max(cwnd, 10);
  else if (trial->group_name() == "cwndDynamic")
    return cwnd;
  NOTREACHED();
  return cwnd;
}

void SpdySession::SendSettings() {
  const SettingsMap& settings_map =
      http_server_properties_->GetSpdySettings(host_port_pair());
  if (settings_map.empty())
    return;

  // Record Histogram Data and Apply the SpdyCwnd FieldTrial if applicable.
  const SpdySettingsIds id = SETTINGS_CURRENT_CWND;
  SettingsMap::const_iterator it = settings_map.find(id);
  uint32 value = 0;
  if (it != settings_map.end())
    value = it->second.second;
  uint32 cwnd = ApplyCwndFieldTrialPolicy(value);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwndSent", cwnd, 1, 200, 100);
  if (cwnd != value) {
    http_server_properties_->SetSpdySetting(
        host_port_pair(), id, SETTINGS_FLAG_PLEASE_PERSIST, cwnd);
  }

  const SettingsMap& settings_map_new =
      http_server_properties_->GetSpdySettings(host_port_pair());
  for (SettingsMap::const_iterator i = settings_map_new.begin(),
           end = settings_map_new.end(); i != end; ++i) {
    const SpdySettingsIds new_id = i->first;
    const uint32 new_val = i->second.second;
    HandleSetting(new_id, new_val);
  }

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_SEND_SETTINGS,
      make_scoped_refptr(new NetLogSpdySettingsParameter(settings_map_new)));

  // Create the SETTINGS frame and send it.
  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdySettingsControlFrame> settings_frame(
      buffered_spdy_framer_->CreateSettings(settings_map_new));
  sent_settings_ = true;
  QueueFrame(settings_frame.get(), HIGHEST, NULL);
}

void SpdySession::HandleSetting(uint32 id, uint32 value) {
  switch (id) {
    case SETTINGS_MAX_CONCURRENT_STREAMS:
      max_concurrent_streams_ = std::min(static_cast<size_t>(value),
                                         g_max_concurrent_stream_limit);
      ProcessPendingCreateStreams();
      break;
    case SETTINGS_INITIAL_WINDOW_SIZE:
      if (static_cast<int32>(value) < 0) {
        net_log().AddEvent(
            NetLog::TYPE_SPDY_SESSION_NEGATIVE_INITIAL_WINDOW_SIZE,
            make_scoped_refptr(new NetLogIntegerParameter(
                "initial_window_size", value)));
      } else {
        // SETTINGS_INITIAL_WINDOW_SIZE updates initial_send_window_size_ only.
        int32 delta_window_size = value - initial_send_window_size_;
        initial_send_window_size_ = value;
        UpdateStreamsSendWindowSize(delta_window_size);
        net_log().AddEvent(
            NetLog::TYPE_SPDY_SESSION_UPDATE_STREAMS_SEND_WINDOW_SIZE,
            make_scoped_refptr(new NetLogIntegerParameter(
                "delta_window_size", delta_window_size)));
      }
      break;
  }
}

void SpdySession::UpdateStreamsSendWindowSize(int32 delta_window_size) {
  ActiveStreamMap::iterator it;
  for (it = active_streams_.begin(); it != active_streams_.end(); ++it) {
    const scoped_refptr<SpdyStream>& stream = it->second;
    DCHECK(stream);
    stream->AdjustSendWindowSize(delta_window_size);
  }
}

void SpdySession::SendPrefacePingIfNoneInFlight() {
  if (pings_in_flight_ || trailing_ping_pending_ ||
      !g_enable_ping_based_connection_checking)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  // If we haven't heard from server, then send a preface-PING.
  if ((now - received_data_time_) > connection_at_risk_of_loss_time_)
    SendPrefacePing();

  PlanToSendTrailingPing();
}

void SpdySession::SendPrefacePing() {
  WritePingFrame(next_ping_id_);
}

void SpdySession::PlanToSendTrailingPing() {
  if (trailing_ping_pending_)
    return;

  trailing_ping_pending_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SpdySession::SendTrailingPing, weak_factory_.GetWeakPtr()),
      trailing_ping_delay_time_);
}

void SpdySession::SendTrailingPing() {
  DCHECK(trailing_ping_pending_);
  trailing_ping_pending_ = false;
  WritePingFrame(next_ping_id_);
}

void SpdySession::WritePingFrame(uint32 unique_id) {
  DCHECK(buffered_spdy_framer_.get());
  scoped_ptr<SpdyPingControlFrame> ping_frame(
      buffered_spdy_framer_->CreatePingFrame(next_ping_id_));
  QueueFrame(ping_frame.get(), HIGHEST, NULL);

  if (net_log().IsLoggingAllEvents()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_PING,
        make_scoped_refptr(new NetLogSpdyPingParameter(next_ping_id_, "sent")));
  }
  if (unique_id % 2 != 0) {
    next_ping_id_ += 2;
    ++pings_in_flight_;
    need_to_send_ping_ = false;
    PlanToCheckPingStatus();
    last_ping_sent_time_ = base::TimeTicks::Now();
  }
}

void SpdySession::PlanToCheckPingStatus() {
  if (check_ping_status_pending_)
    return;

  check_ping_status_pending_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SpdySession::CheckPingStatus, weak_factory_.GetWeakPtr(),
                 base::TimeTicks::Now()), hung_interval_);
}

void SpdySession::CheckPingStatus(base::TimeTicks last_check_time) {
  // Check if we got a response back for all PINGs we had sent.
  if (pings_in_flight_ == 0) {
    check_ping_status_pending_ = false;
    return;
  }

  DCHECK(check_ping_status_pending_);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay = hung_interval_ - (now - received_data_time_);

  if (delay.InMilliseconds() < 0 || received_data_time_ < last_check_time) {
    CloseSessionOnError(net::ERR_SPDY_PING_FAILED, true, "Failed ping.");
    // Track all failed PING messages in a separate bucket.
    const base::TimeDelta kFailedPing =
        base::TimeDelta::FromInternalValue(INT_MAX);
    RecordPingRTTHistogram(kFailedPing);
    return;
  }

  // Check the status of connection after a delay.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SpdySession::CheckPingStatus, weak_factory_.GetWeakPtr(),
                 now),
      delay);
}

void SpdySession::RecordPingRTTHistogram(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Net.SpdyPing.RTT", duration);
}

void SpdySession::RecordProtocolErrorHistogram(
    SpdyProtocolErrorDetails details) {
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionErrorDetails", details,
                            NUM_SPDY_PROTOCOL_ERROR_DETAILS);
  if (EndsWith(host_port_pair().host(), "google.com", false)) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionErrorDetails_Google", details,
                              NUM_SPDY_PROTOCOL_ERROR_DETAILS);
  }
}

void SpdySession::RecordHistograms() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPerSession",
                              streams_initiated_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPushedPerSession",
                              streams_pushed_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPushedAndClaimedPerSession",
                              streams_pushed_and_claimed_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsAbandonedPerSession",
                              streams_abandoned_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySettingsSent",
                            sent_settings_ ? 1 : 0, 2);
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySettingsReceived",
                            received_settings_ ? 1 : 0, 2);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamStallsPerSession",
                              stalled_streams_,
                              0, 300, 50);
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionsWithStalls",
                            stalled_streams_ > 0 ? 1 : 0, 2);

  if (received_settings_) {
    // Enumerate the saved settings, and set histograms for it.
    const SettingsMap& settings_map =
        http_server_properties_->GetSpdySettings(host_port_pair());

    SettingsMap::const_iterator it;
    for (it = settings_map.begin(); it != settings_map.end(); ++it) {
      const SpdySettingsIds id = it->first;
      const uint32 val = it->second.second;
      switch (id) {
        case SETTINGS_CURRENT_CWND:
          // Record several different histograms to see if cwnd converges
          // for larger volumes of data being sent.
          UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwnd",
                                      val, 1, 200, 100);
          if (bytes_received_ > 10 * 1024) {
            UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwnd10K",
                                        val, 1, 200, 100);
            if (bytes_received_ > 25 * 1024) {
              UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwnd25K",
                                          val, 1, 200, 100);
              if (bytes_received_ > 50 * 1024) {
                UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwnd50K",
                                            val, 1, 200, 100);
                if (bytes_received_ > 100 * 1024) {
                  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwnd100K",
                                              val, 1, 200, 100);
                }
              }
            }
          }
          break;
        case SETTINGS_ROUND_TRIP_TIME:
          UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsRTT",
                                      val, 1, 1200, 100);
          break;
        case SETTINGS_DOWNLOAD_RETRANS_RATE:
          UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsRetransRate",
                                      val, 1, 100, 50);
          break;
        default:
          break;
      }
    }
  }
}

void SpdySession::InvokeUserStreamCreationCallback(
    scoped_refptr<SpdyStream>* stream) {
  PendingCallbackMap::iterator it = pending_callback_map_.find(stream);

  // Exit if the request has already been cancelled.
  if (it == pending_callback_map_.end())
    return;

  CompletionCallback callback = it->second.callback;
  int result = it->second.result;
  pending_callback_map_.erase(it);
  callback.Run(result);
}

SSLClientSocket* SpdySession::GetSSLClientSocket() const {
  if (!is_secure_)
    return NULL;
  SSLClientSocket* ssl_socket =
      reinterpret_cast<SSLClientSocket*>(connection_->socket());
  DCHECK(ssl_socket);
  return ssl_socket;
}

}  // namespace net
