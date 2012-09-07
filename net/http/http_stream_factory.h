// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/http/http_server_properties.h"
#include "net/socket/ssl_client_socket.h"

class GURL;

namespace base {
class Value;
}

namespace net {

class AuthCredentials;
class BoundNetLog;
class HostMappingRules;
class HostPortPair;
class HttpAuthController;
class HttpResponseInfo;
class HttpServerProperties;
class HttpStream;
class ProxyInfo;
class SSLCertRequestInfo;
class SSLInfo;
struct HttpRequestInfo;
struct SSLConfig;

// The HttpStreamRequest is the client's handle to the worker object which
// handles the creation of an HttpStream.  While the HttpStream is being
// created, this object is the creator's handle for interacting with the
// HttpStream creation process.  The request is cancelled by deleting it, after
// which no callbacks will be invoked.
class NET_EXPORT_PRIVATE HttpStreamRequest {
 public:
  // The HttpStreamRequest::Delegate is a set of callback methods for a
  // HttpStreamRequestJob.  Generally, only one of these methods will be
  // called as a result of a stream request.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() {}

    // This is the success case.
    // |stream| is now owned by the delegate.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    // |used_proxy_info| indicates the actual ProxyInfo used for this stream,
    // since the HttpStreamRequest performs the proxy resolution.
    virtual void OnStreamReady(
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        HttpStream* stream) = 0;

    // This is the failure to create a stream case.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    virtual void OnStreamFailed(int status,
                                const SSLConfig& used_ssl_config) = 0;

    // Called when we have a certificate error for the request.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    virtual void OnCertificateError(int status,
                                    const SSLConfig& used_ssl_config,
                                    const SSLInfo& ssl_info) = 0;

    // This is the failure case where we need proxy authentication during
    // proxy tunnel establishment.  For the tunnel case, we were unable to
    // create the HttpStream, so the caller provides the auth and then resumes
    // the HttpStreamRequest.
    //
    // For the non-tunnel case, the caller will discover the authentication
    // failure when reading response headers. At that point, he will handle the
    // authentication failure and restart the HttpStreamRequest entirely.
    //
    // Ownership of |auth_controller| and |proxy_response| are owned
    // by the HttpStreamRequest. |proxy_response| is not guaranteed to be usable
    // after the lifetime of this callback.  The delegate may take a reference
    // to |auth_controller| if it is needed beyond the lifetime of this
    // callback.
    //
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    virtual void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                                  const SSLConfig& used_ssl_config,
                                  const ProxyInfo& used_proxy_info,
                                  HttpAuthController* auth_controller) = 0;

    // This is the failure for SSL Client Auth
    // Ownership of |cert_info| is retained by the HttpStreamRequest.  The
    // delegate may take a reference if it needs the cert_info beyond the
    // lifetime of this callback.
    virtual void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                                   SSLCertRequestInfo* cert_info) = 0;

    // This is the failure of the CONNECT request through an HTTPS proxy.
    // Headers can be read from |response_info|, while the body can be read
    // from |stream|.
    //
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    //
    // |used_proxy_info| indicates the actual ProxyInfo used for this stream,
    // since the HttpStreamRequest performs the proxy resolution.
    //
    // Ownership of |stream| is transferred to the delegate.
    virtual void OnHttpsProxyTunnelResponse(
        const HttpResponseInfo& response_info,
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        HttpStream* stream) = 0;
  };

  virtual ~HttpStreamRequest() {}

  // When a HttpStream creation process is stalled due to necessity
  // of Proxy authentication credentials, the delegate OnNeedsProxyAuth
  // will have been called.  It now becomes the delegate's responsibility
  // to collect the necessary credentials, and then call this method to
  // resume the HttpStream creation process.
  virtual int RestartTunnelWithProxyAuth(
      const AuthCredentials& credentials) = 0;

  // Returns the LoadState for the request.
  virtual LoadState GetLoadState() const = 0;

  // Returns true if TLS/NPN was negotiated for this stream.
  virtual bool was_npn_negotiated() const = 0;

  // Protocol negotiated with the server.
  virtual NextProto protocol_negotiated() const = 0;

  // Returns true if this stream is being fetched over SPDY.
  virtual bool using_spdy() const = 0;
};

// The HttpStreamFactory defines an interface for creating usable HttpStreams.
class NET_EXPORT HttpStreamFactory {
 public:
  virtual ~HttpStreamFactory();

  void ProcessAlternateProtocol(
      HttpServerProperties* http_server_properties,
      const std::string& alternate_protocol_str,
      const HostPortPair& http_host_port_pair);

  GURL ApplyHostMappingRules(const GURL& url, HostPortPair* endpoint);

  // Virtual interface methods.

  // Request a stream.
  // Will callback to the HttpStreamRequestDelegate upon completion.
  virtual HttpStreamRequest* RequestStream(
      const HttpRequestInfo& info,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HttpStreamRequest::Delegate* delegate,
      const BoundNetLog& net_log) = 0;

  // Requests that enough connections for |num_streams| be opened.
  virtual void PreconnectStreams(int num_streams,
                                 const HttpRequestInfo& info,
                                 const SSLConfig& server_ssl_config,
                                 const SSLConfig& proxy_ssl_config) = 0;

  // If pipelining is supported, creates a Value summary of the currently active
  // pipelines. Caller assumes ownership of the returned value. Otherwise,
  // returns an empty Value.
  virtual base::Value* PipelineInfoToValue() const = 0;

  virtual const HostMappingRules* GetHostMappingRules() const = 0;

  // Static settings

  // Reset all static settings to initialized values. Used to init test suite.
  static void ResetStaticSettingsToInit();

  // Turns spdy on or off.
  static void set_spdy_enabled(bool value) {
    spdy_enabled_ = value;
    if (!spdy_enabled_) {
      delete next_protos_;
      next_protos_ = NULL;
    }
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
  static void add_forced_spdy_exclusion(const std::string& value);
  // Check if a HostPortPair is excluded from using spdy.
  static bool HasSpdyExclusion(const HostPortPair& endpoint);

  // Sets http/1.1 as the only protocol supported via NPN.
  static void EnableNpnHttpOnly();

  // Sets http/1.1 and spdy/2 (the default spdy protocol) as the protocols
  // supported via NPN.
  static void EnableNpnSpdy();

  // Sets http/1.1, spdy/2, and spdy/3 as the protocols supported via NPN.
  static void EnableNpnSpdy3();

  // Sets the protocols supported by NPN (next protocol negotiation) during the
  // SSL handshake as well as by HTTP Alternate-Protocol.
  static void SetNextProtos(const std::vector<std::string>& value);
  static bool has_next_protos() { return next_protos_ != NULL; }
  static const std::vector<std::string>& next_protos() {
    return *next_protos_;
  }

 protected:
  HttpStreamFactory();

 private:
  static std::vector<std::string>* next_protos_;
  static bool enabled_protocols_[NUM_ALTERNATE_PROTOCOLS];
  static bool spdy_enabled_;
  static bool use_alternate_protocols_;
  static bool force_spdy_over_ssl_;
  static bool force_spdy_always_;
  static std::list<HostPortPair>* forced_spdy_exclusions_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactory);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_H_
