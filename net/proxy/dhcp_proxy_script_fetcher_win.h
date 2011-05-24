// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_DHCP_PROXY_SCRIPT_FETCHER_WIN_H_
#define NET_PROXY_DHCP_PROXY_SCRIPT_FETCHER_WIN_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"

namespace net {

class DhcpProxyScriptAdapterFetcher;
class URLRequestContext;

// Windows-specific implementation.
class NET_TEST DhcpProxyScriptFetcherWin
    : public DhcpProxyScriptFetcher,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Creates a DhcpProxyScriptFetcherWin that issues requests through
  // |url_request_context|. |url_request_context| must remain valid for
  // the lifetime of DhcpProxyScriptFetcherWin.
  explicit DhcpProxyScriptFetcherWin(URLRequestContext* url_request_context);
  virtual ~DhcpProxyScriptFetcherWin();

  // DhcpProxyScriptFetcher implementation.
  int Fetch(string16* utf16_text, CompletionCallback* callback) OVERRIDE;
  void Cancel() OVERRIDE;
  const GURL& GetPacURL() const OVERRIDE;
  std::string GetFetcherName() const OVERRIDE;

  // Sets |adapter_names| to contain the name of each network adapter on
  // this machine that has DHCP enabled and is not a loop-back adapter. Returns
  // false on error.
  static bool GetCandidateAdapterNames(std::set<std::string>* adapter_names);

 protected:
  // Event/state transition handlers
  void OnFetcherDone(int result);
  void OnWaitTimer();
  void TransitionToDone();

  // Virtual methods introduced to allow unit testing.
  virtual DhcpProxyScriptAdapterFetcher* ImplCreateAdapterFetcher();
  virtual bool ImplGetCandidateAdapterNames(
      std::set<std::string>* adapter_names);
  virtual int ImplGetMaxWaitMs();

  // This is the outer state machine for fetching PAC configuration from
  // DHCP.  It relies for sub-states on the state machine of the
  // DhcpProxyScriptAdapterFetcher class.
  //
  // The goal of the implementation is to the following work in parallel
  // for all network adapters that are using DHCP:
  // a) Try to get the PAC URL configured in DHCP;
  // b) If one is configured, try to fetch the PAC URL.
  // c) Once this is done for all adapters, or a timeout has passed after
  //    it has completed for the fastest adapter, return the PAC file
  //    available for the most preferred network adapter, if any.
  //
  // The state machine goes from START->NO_RESULTS when it creates
  // and starts an DhcpProxyScriptAdapterFetcher for each adapter.  It goes
  // from NO_RESULTS->SOME_RESULTS when it gets the first result; at this
  // point a wait timer is started.  It goes from SOME_RESULTS->DONE in
  // two cases: All results are known, or the wait timer expired.  A call
  // to Cancel() will also go straight to DONE from any state.  Any
  // way the DONE state is entered, we will at that point cancel any
  // outstanding work and return the best known PAC script or the empty
  // string.
  //
  // The state machine is reset for each Fetch(), a call to which is
  // only valid in states START and DONE, as only one Fetch() is
  // allowed to be outstanding at any given time.
  enum State {
    STATE_START,
    STATE_NO_RESULTS,
    STATE_SOME_RESULTS,
    STATE_DONE,
  };

  // Current state of this state machine.
  State state_;

  // Vector, in Windows' network adapter preference order, of
  // DhcpProxyScriptAdapterFetcher objects that are or were attempting
  // to fetch a PAC file based on DHCP configuration.
  typedef ScopedVector<DhcpProxyScriptAdapterFetcher> FetcherVector;
  FetcherVector fetchers_;

  // Callback invoked when any fetcher completes.
  CompletionCallbackImpl<DhcpProxyScriptFetcherWin> fetcher_callback_;

  // Number of fetchers we are waiting for.
  int num_pending_fetchers_;

  // Lets our client know we're done. Not valid in states START or DONE.
  CompletionCallback* client_callback_;

  // Pointer to string we will write results to. Not valid in states
  // START and DONE.
  string16* destination_string_;

  // PAC URL retrieved from DHCP, if any. Valid only in state STATE_DONE.
  GURL pac_url_;

  base::OneShotTimer<DhcpProxyScriptFetcherWin> wait_timer_;

  scoped_refptr<URLRequestContext> url_request_context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DhcpProxyScriptFetcherWin);
};

}  // namespace net

#endif  // NET_PROXY_DHCP_PROXY_SCRIPT_FETCHER_WIN_H_
