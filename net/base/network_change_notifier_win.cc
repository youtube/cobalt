// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_win.h"

#include <iphlpapi.h>
#include <winsock2.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "net/base/winsock_init.h"
#include "net/dns/dns_config_watcher.h"

#pragma comment(lib, "iphlpapi.lib")

namespace net {

namespace {

// Time between NotifyAddrChange retries, on failure.
const int kWatchForAddressChangeRetryIntervalMs = 500;

}  // namespace

// Thread on which we can run DnsConfigWatcher, which requires AssertIOAllowed
// to open registry keys and to handle FilePathWatcher updates.
class NetworkChangeNotifierWin::DnsWatcherThread : public base::Thread {
 public:
  DnsWatcherThread() : base::Thread("NetworkChangeNotifier") {}

  virtual ~DnsWatcherThread() {
    Stop();
  }

  virtual void Init() OVERRIDE {
    watcher_.Init();
  }

  virtual void CleanUp() OVERRIDE {
    watcher_.CleanUp();
  }

 private:
  internal::DnsConfigWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(DnsWatcherThread);
};

NetworkChangeNotifierWin::NetworkChangeNotifierWin()
    : is_watching_(false),
      sequential_failures_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      dns_watcher_thread_(new DnsWatcherThread()) {
  memset(&addr_overlapped_, 0, sizeof addr_overlapped_);
  addr_overlapped_.hEvent = WSACreateEvent();
  dns_watcher_thread_->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

NetworkChangeNotifierWin::~NetworkChangeNotifierWin() {
  if (is_watching_) {
    CancelIPChangeNotify(&addr_overlapped_);
    addr_watcher_.StopWatching();
  }
  WSACloseEvent(addr_overlapped_.hEvent);
}

// Conceptually we would like to tell whether the user is "online" verus
// "offline".  This is challenging since the only thing we can test with
// certainty is whether a *particular* host is reachable.
//
// While we can't conclusively determine when a user is "online", we can at
// least reliably recognize some of the situtations when they are clearly
// "offline". For example, if the user's laptop is not plugged into an ethernet
// network and is not connected to any wireless networks, it must be offline.
//
// There are a number of different ways to implement this on Windows, each with
// their pros and cons. Here is a comparison of various techniques considered:
//
// (1) Use InternetGetConnectedState (wininet.dll). This function is really easy
// to use (literally a one-liner), and runs quickly. The drawback is it adds a
// dependency on the wininet DLL.
//
// (2) Enumerate all of the network interfaces using GetAdaptersAddresses
// (iphlpapi.dll), and assume we are "online" if there is at least one interface
// that is connected, and that interface is not a loopback or tunnel.
//
// Safari on Windows has a fairly simple implementation that does this:
// http://trac.webkit.org/browser/trunk/WebCore/platform/network/win/NetworkStateNotifierWin.cpp.
//
// Mozilla similarly uses this approach:
// http://mxr.mozilla.org/mozilla1.9.2/source/netwerk/system/win32/nsNotifyAddrListener.cpp
//
// The biggest drawback to this approach is it is quite complicated.
// WebKit's implementation for example doesn't seem to test for ICS gateways
// (internet connection sharing), whereas Mozilla's implementation has extra
// code to guess that.
//
// (3) The method used in this file comes from google talk, and is similar to
// method (2). The main difference is it enumerates the winsock namespace
// providers rather than the actual adapters.
//
// I ran some benchmarks comparing the performance of each on my Windows 7
// workstation. Here is what I found:
//   * Approach (1) was pretty much zero-cost after the initial call.
//   * Approach (2) took an average of 3.25 milliseconds to enumerate the
//     adapters.
//   * Approach (3) took an average of 0.8 ms to enumerate the providers.
//
// In terms of correctness, all three approaches were comparable for the simple
// experiments I ran... However none of them correctly returned "offline" when
// executing 'ipconfig /release'.
//
bool NetworkChangeNotifierWin::IsCurrentlyOffline() const {

  // TODO(eroman): We could cache this value, and only re-calculate it on
  //               network changes. For now we recompute it each time asked,
  //               since it is relatively fast (sub 1ms) and not called often.

  EnsureWinsockInit();

  // The following code was adapted from:
  // http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/net/notifier/base/win/async_network_alive_win32.cc?view=markup&pathrev=47343
  // The main difference is we only call WSALookupServiceNext once, whereas
  // the earlier code would traverse the entire list and pass LUP_FLUSHPREVIOUS
  // to skip past the large results.

  HANDLE ws_handle;
  WSAQUERYSET query_set = {0};
  query_set.dwSize = sizeof(WSAQUERYSET);
  query_set.dwNameSpace = NS_NLA;
  // Initiate a client query to iterate through the
  // currently connected networks.
  if (0 != WSALookupServiceBegin(&query_set, LUP_RETURN_ALL,
                                 &ws_handle)) {
    LOG(ERROR) << "WSALookupServiceBegin failed with: " << WSAGetLastError();
    return false;
  }

  bool found_connection = false;

  // Retrieve the first available network. In this function, we only
  // need to know whether or not there is network connection.
  // Allocate 256 bytes for name, it should be enough for most cases.
  // If the name is longer, it is OK as we will check the code returned and
  // set correct network status.
  char result_buffer[sizeof(WSAQUERYSET) + 256] = {0};
  DWORD length = sizeof(result_buffer);
  reinterpret_cast<WSAQUERYSET*>(&result_buffer[0])->dwSize =
      sizeof(WSAQUERYSET);
  int result = WSALookupServiceNext(
      ws_handle,
      LUP_RETURN_NAME,
      &length,
      reinterpret_cast<WSAQUERYSET*>(&result_buffer[0]));

  if (result == 0) {
    // Found a connection!
    found_connection = true;
  } else {
    DCHECK_EQ(SOCKET_ERROR, result);
    result = WSAGetLastError();

    // Error code WSAEFAULT means there is a network connection but the
    // result_buffer size is too small to contain the results. The
    // variable "length" returned from WSALookupServiceNext is the minimum
    // number of bytes required. We do not need to retrieve detail info,
    // it is enough knowing there was a connection.
    if (result == WSAEFAULT) {
      found_connection = true;
    } else if (result == WSA_E_NO_MORE || result == WSAENOMORE) {
      // There was nothing to iterate over!
    } else {
      LOG(WARNING) << "WSALookupServiceNext() failed with:" << result;
    }
  }

  result = WSALookupServiceEnd(ws_handle);
  LOG_IF(ERROR, result != 0)
      << "WSALookupServiceEnd() failed with: " << result;

  return !found_connection;
}

void NetworkChangeNotifierWin::OnObjectSignaled(HANDLE object) {
  DCHECK(CalledOnValidThread());
  DCHECK(is_watching_);
  is_watching_ = false;

  // Start watching for the next address change.
  WatchForAddressChange();

  NotifyObservers();
}

void NetworkChangeNotifierWin::NotifyObservers() {
  DCHECK(CalledOnValidThread());
  NotifyObserversOfIPAddressChange();

  // Calling IsOffline() at this very moment is likely to give
  // the wrong result, so we delay that until a little bit later.
  //
  // The one second delay chosen here was determined experimentally
  // by adamk on Windows 7.
  timer_.Stop();  // cancel any already waiting notification
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
               &NetworkChangeNotifierWin::NotifyParentOfOnlineStateChange);
}

void NetworkChangeNotifierWin::WatchForAddressChange() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_watching_);

  // NotifyAddrChange occasionally fails with ERROR_OPEN_FAILED for unknown
  // reasons.  More rarely, it's also been observed failing with
  // ERROR_NO_SYSTEM_RESOURCES.  When either of these happens, we retry later.
  if (!WatchForAddressChangeInternal()) {
    ++sequential_failures_;

    // TODO(mmenke):  If the UMA histograms indicate that this fixes
    // http://crbug.com/69198, remove this histogram and consider reducing the
    // retry interval.
    if (sequential_failures_ == 2000) {
      UMA_HISTOGRAM_COUNTS_10000("Net.NotifyAddrChangeFailures",
                                 sequential_failures_);
    }

    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NetworkChangeNotifierWin::WatchForAddressChange,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kWatchForAddressChangeRetryIntervalMs));
    return;
  }

  // Treat the transition from NotifyAddrChange failing to succeeding as a
  // network change event, since network changes were not being observed in
  // that interval.
  if (sequential_failures_ > 0)
    NotifyObservers();

  if (sequential_failures_ < 2000) {
    UMA_HISTOGRAM_COUNTS_10000("Net.NotifyAddrChangeFailures",
                               sequential_failures_);
  }

  is_watching_ = true;
  sequential_failures_ = 0;
}

bool NetworkChangeNotifierWin::WatchForAddressChangeInternal() {
  DCHECK(CalledOnValidThread());
  HANDLE handle = NULL;
  DWORD ret = NotifyAddrChange(&handle, &addr_overlapped_);
  if (ret != ERROR_IO_PENDING)
    return false;

  addr_watcher_.StartWatching(addr_overlapped_.hEvent, this);
  return true;
}

void NetworkChangeNotifierWin::NotifyParentOfOnlineStateChange() {
  NotifyObserversOfOnlineStateChange();
}

}  // namespace net
