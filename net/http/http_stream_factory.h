// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_H_

#include <list>
#include <string>

#include "base/string16.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"

class GURL;

namespace net {

class BoundNetLog;
class HostMappingRules;
class HostPortPair;
class HttpAlternateProtocols;
class HttpAuthController;
class HttpNetworkSession;
class HttpResponseInfo;
class HttpStream;
class ProxyInfo;
class SSLCertRequestInfo;
class SSLInfo;
class X509Certificate;
struct HttpRequestInfo;
struct SSLConfig;

// The HttpStreamRequest is the client's handle to the worker object which
// handles the creation of an HttpStream.  While the HttpStream is being
// created, this object is the creator's handle for interacting with the
// HttpStream creation process.  The request is cancelled by deleting it, after
// which no callbacks will be invoked.
class HttpStreamRequest {
 public:
  // The HttpStreamRequest::Delegate is a set of callback methods for a
  // HttpStreamRequestJob.  Generally, only one of these methods will be
  // called as a result of a stream request.
  class Delegate {
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
  virtual int RestartTunnelWithProxyAuth(const string16& username,
                                         const string16& password) = 0;

  // Returns the LoadState for the request.
  virtual LoadState GetLoadState() const = 0;

  // Returns true if an AlternateProtocol for this request was available.
  virtual bool was_alternate_protocol_available() const = 0;

  // Returns true if TLS/NPN was negotiated for this stream.
  virtual bool was_npn_negotiated() const = 0;

  // Returns true if this stream is being fetched over SPDY.
  virtual bool using_spdy() const = 0;
};

// The HttpStreamFactory defines an interface for creating usable HttpStreams.
class HttpStreamFactory {
 public:
  virtual ~HttpStreamFactory();

  void ProcessAlternateProtocol(
      HttpAlternateProtocols* alternate_protocols,
      const std::string& alternate_protocol_str,
      const HostPortPair& http_host_port_pair);

  // Virtual interface methods.

  // Request a stream.
  // Will callback to the HttpStreamRequestDelegate upon completion.
  virtual HttpStreamRequest* RequestStream(
      const HttpRequestInfo& info,
      const SSLConfig& ssl_config,
      HttpStreamRequest::Delegate* delegate,
      const BoundNetLog& net_log) = 0;

  // Requests that enough connections for |num_streams| be opened.
  virtual void PreconnectStreams(int num_streams,
                                 const HttpRequestInfo& info,
                                 const SSLConfig& ssl_config,
                                 const BoundNetLog& net_log) = 0;

  virtual void AddTLSIntolerantServer(const GURL& url) = 0;
  virtual bool IsTLSIntolerantServer(const GURL& url) const = 0;

  // Static settings
  static GURL ApplyHostMappingRules(const GURL& url, HostPortPair* endpoint);

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
  static void add_forced_spdy_exclusion(const std::string& value);
  static std::list<HostPortPair>* forced_spdy_exclusions() {
    return forced_spdy_exclusions_;
  }

  // Sets the next protocol negotiation value used during the SSL handshake.
  static void set_next_protos(const std::string& value) {
    delete next_protos_;
    next_protos_ = new std::string(value);
  }
  static const std::string* next_protos() { return next_protos_; }

  // Sets the HttpStreamFactoryImpl into a mode where it can ignore certificate
  // errors.  This is for testing.
  static void set_ignore_certificate_errors(bool value) {
    ignore_certificate_errors_ = value;
  }
  static bool ignore_certificate_errors() {
    return ignore_certificate_errors_;
  }

  static void SetHostMappingRules(const std::string& rules);

 protected:
  HttpStreamFactory();

 private:
  static const HostMappingRules& host_mapping_rules();

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
