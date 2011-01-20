// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_stream_request.h"
#include "net/http/stream_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"

namespace net {

class HttpNetworkSession;
struct HttpRequestInfo;
class HttpStreamRequest;

class HttpStreamFactory : public StreamFactory,
                          public HttpStreamRequest::PreconnectDelegate {
 public:
  HttpStreamFactory();
  virtual ~HttpStreamFactory();

  // Static settings

  // Turns spdy on or off.
  static void set_spdy_enabled(bool value) {
    spdy_enabled_ = value;
    if (value == false)
      set_next_protos("");
  }
  static bool spdy_enabled() { return spdy_enabled_; }

  // Controls whether or not we use the Alternate-Protocol header.
  static void set_use_alternate_protocols(bool value) {
    use_alternate_protocols_ = value;
  }
  static bool use_alternate_protocols() { return use_alternate_protocols_; }

  // Controls whether or not we use ssl when in spdy mode.
  static void set_force_spdy_over_ssl(bool value) {
    force_spdy_over_ssl_ = value;
  }
  static bool force_spdy_over_ssl() {
    return force_spdy_over_ssl_;
  }

  // Controls whether or not we use spdy without npn.
  static void set_force_spdy_always(bool value) {
    force_spdy_always_ = value;
  }
  static bool force_spdy_always() { return force_spdy_always_; }

  // Add a URL to exclude from forced SPDY.
  static void add_forced_spdy_exclusion(const std::string& value) {
    HostPortPair pair = HostPortPair::FromURL(GURL(value));
    if (!forced_spdy_exclusions_)
        forced_spdy_exclusions_ = new std::list<HostPortPair>();
    forced_spdy_exclusions_->push_back(pair);
  }
  static std::list<HostPortPair>* forced_spdy_exclusions() {
    return forced_spdy_exclusions_;
  }

  // Sets the next protocol negotiation value used during the SSL handshake.
  static void set_next_protos(const std::string& value) {
    delete next_protos_;
    next_protos_ = new std::string(value);
  }
  static const std::string* next_protos() { return next_protos_; }

  // Sets the HttpStreamFactory into a mode where it can ignore certificate
  // errors.  This is for testing.
  static void set_ignore_certificate_errors(bool value) {
    ignore_certificate_errors_ = value;
  }
  static bool ignore_certificate_errors() {
    return ignore_certificate_errors_;
  }

  static void SetHostMappingRules(const std::string& rules);

  // StreamFactory Interface
  virtual StreamRequest* RequestStream(const HttpRequestInfo* info,
                                       SSLConfig* ssl_config,
                                       ProxyInfo* proxy_info,
                                       HttpNetworkSession* session,
                                       StreamRequest::Delegate* delegate,
                                       const BoundNetLog& net_log);
  virtual int PreconnectStreams(int num_streams,
                                const HttpRequestInfo* info,
                                SSLConfig* ssl_config,
                                ProxyInfo* proxy_info,
                                HttpNetworkSession* session,
                                const BoundNetLog& net_log,
                                CompletionCallback* callback);
  virtual void AddTLSIntolerantServer(const GURL& url);
  virtual bool IsTLSIntolerantServer(const GURL& url);
  virtual void ProcessAlternateProtocol(
      HttpAlternateProtocols* alternate_protocols,
      const std::string& alternate_protocol_str,
      const HostPortPair& http_host_port_pair);
  virtual GURL ApplyHostMappingRules(const GURL& url, HostPortPair* endpoint);

  // HttpStreamRequest::PreconnectDelegate API
  virtual void OnPreconnectsComplete(HttpStreamRequest* request, int result);

 private:
  typedef std::map<HttpStreamRequest*, CompletionCallback*> RequestCallbackMap;
  RequestCallbackMap request_callback_map_;
  std::set<std::string> tls_intolerant_servers_;

  static const HostMappingRules* host_mapping_rules_;
  static const std::string* next_protos_;
  static bool spdy_enabled_;
  static bool use_alternate_protocols_;
  static bool force_spdy_over_ssl_;
  static bool force_spdy_always_;
  static std::list<HostPortPair>* forced_spdy_exclusions_;
  static bool ignore_certificate_errors_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactory);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_H_

