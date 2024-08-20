// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_TIMING_INFO_H_
#define NET_BASE_LOAD_TIMING_INFO_H_

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "starboard/types.h"

namespace net {

// Structure containing timing information for a request.
// It addresses the needs of
// http://groups.google.com/group/http-archive-specification/web/har-1-1-spec,
// http://dev.w3.org/2006/webapi/WebTiming/, and
// http://www.w3.org/TR/resource-timing/.
//
// All events that do not apply to a request have null times.  For non-HTTP
// requests, all times other than the request_start times are null.
//
// Requests with connection errors generally only have request start times as
// well, since they never received an established socket.
//
// The general order for events is:
// request_start
// proxy_start
// proxy_end
// dns_start
// dns_end
// connect_start
// ssl_start
// ssl_end
// connect_end
// send_start
// send_end
// receive_headers_end
//
// Times represent when a request starts/stops blocking on an event(*), not the
// time the events actually occurred. In particular, in the case of preconnects
// and socket reuse, no time may be spent blocking on establishing a connection.
// In the case of SPDY, PAC scripts are only run once for each shared session,
// so no time may be spent blocking on them.
//
// (*) Note 1: push_start and push_end are the exception to this, as they
// represent the operation which is asynchronous to normal request flow and
// hence are provided as absolute values and not converted to "blocking" time.
//
// (*) Note 2: Internally to the network stack, times are those of actual event
// occurrence. URLRequest converts them to time which the network stack was
// blocked on each state, as per resource timing specs.
//
// DNS and SSL times are both times for the host, not the proxy, so DNS times
// when using proxies are null, and only requests to HTTPS hosts (Not proxies)
// have SSL times.  One exception to this is when a proxy server itself returns
// a redirect response.  In this case, the connect times treat the proxy as the
// host.  The send and receive times will all be null, however.
// See HttpNetworkTransaction::OnHttpsProxyTunnelResponse.
// TODO(mmenke):  Is this worth fixing?
//
struct NET_EXPORT LoadTimingInfo {
  // Contains the LoadTimingInfo events related to establishing a connection.
  // These are all set by ConnectJobs.
  struct NET_EXPORT_PRIVATE ConnectTiming {
    ConnectTiming();
    ~ConnectTiming();

    // The time spent looking up the host's DNS address.  Null for requests that
    // used proxies to look up the DNS address.  Also null for SOCKS4 proxies,
    // since the DNS address is only looked up after the connection is
    // established, which results in unexpected event ordering.
    // TODO(mmenke):  The SOCKS4 event ordering could be refactored to allow
    //                these times to be non-null.
    // Corresponds to |domainLookupStart| and |domainLookupEnd| in
    // ResourceTiming (http://www.w3.org/TR/resource-timing/) for Web-surfacing
    // requests.
    base::TimeTicks dns_start;
    base::TimeTicks dns_end;

    // The time spent establishing the connection. Connect time includes proxy
    // connect times (though not proxy_resolve or DNS lookup times), time spent
    // waiting in certain queues, TCP, and SSL time.
    // TODO(mmenke):  For proxies, this includes time spent blocking on higher
    //                level socket pools.  Fix this.
    // TODO(mmenke):  Retried connections to the same server should apparently
    //                be included in this time.  Consider supporting that.
    //                Since the network stack has multiple notions of a "retry",
    //                handled at different levels, this may not be worth
    //                worrying about - backup jobs, reused socket failure,
    //                multiple round authentication.
    // Corresponds to |connectStart| and |connectEnd| in ResourceTiming
    // (http://www.w3.org/TR/resource-timing/) for Web-surfacing requests.
    base::TimeTicks connect_start;
    base::TimeTicks connect_end;

    // The time when the SSL handshake started / completed. For non-HTTPS
    // requests these are null.  These times are only for the SSL connection to
    // the final destination server, not an SSL/SPDY proxy.
    // |ssl_start| corresponds to |secureConnectionStart| in ResourceTiming
    // (http://www.w3.org/TR/resource-timing/) for Web-surfacing requests.
    base::TimeTicks ssl_start;
    base::TimeTicks ssl_end;
  };

  LoadTimingInfo();
  LoadTimingInfo(const LoadTimingInfo& other);
  ~LoadTimingInfo();

  // True if the socket was reused.  When true, DNS, connect, and SSL times
  // will all be null.  When false, those times may be null, too, for non-HTTP
  // requests, or when they don't apply to a request.
  //
  // For requests that are sent again after an AUTH challenge, this will be true
  // if the original socket is reused, and false if a new socket is used.
  // Responding to a proxy AUTH challenge is never considered to be reusing a
  // socket, since a connection to the host wasn't established when the
  // challenge was received.
  bool socket_reused;

  // Unique socket ID, can be used to identify requests served by the same
  // socket.  For connections tunnelled over SPDY proxies, this is the ID of
  // the virtual connection (The SpdyProxyClientSocket), not the ID of the
  // actual socket.  HTTP requests handled by the SPDY proxy itself all use the
  // actual socket's ID.
  //
  // 0 when there is no socket associated with the request, or it's not an HTTP
  // request.
  uint32_t socket_log_id;

  // Start time as a base::Time, so times can be coverted into actual times.
  // Other times are recorded as TimeTicks so they are not affected by clock
  // changes.
  base::Time request_start_time;

  // Corresponds to |fetchStart| in ResourceTiming
  // (http://www.w3.org/TR/resource-timing/) for Web-surfacing requests.
  base::TimeTicks request_start;

  // The time spent determing which proxy to use.  Null when there is no PAC.
  base::TimeTicks proxy_resolve_start;
  base::TimeTicks proxy_resolve_end;

  ConnectTiming connect_timing;

  // The time that sending HTTP request started / ended.
  // |send_start| corresponds to |requestStart| in ResourceTiming
  // (http://www.w3.org/TR/resource-timing/) for Web-surfacing requests.
  base::TimeTicks send_start;
  base::TimeTicks send_end;

  // The time at which the end of the HTTP headers were received.
  // Corresponds to |responseStart| in ResourceTiming
  // (http://www.w3.org/TR/resource-timing/) for Web-surfacing requests.
  base::TimeTicks receive_headers_end;

  // In case the resource was proactively pushed by the server, these are
  // the times that push started and ended. Note that push_end will be null
  // if the request is still being transmitted, i.e. the underlying h2 stream
  // is not closed by the server.
  base::TimeTicks push_start;
  base::TimeTicks push_end;

#if defined(STARBOARD)
  uint64_t encoded_body_size;
  base::TimeTicks service_worker_start_time;
  std::string alpn_negotiated_protocol;
  std::string connection_info_string;
#endif  // defined(STARBOARD)
};

}  // namespace net

#endif  // NET_BASE_LOAD_TIMING_INFO_H_
