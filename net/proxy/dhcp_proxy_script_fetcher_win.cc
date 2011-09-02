// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/dhcp_proxy_script_fetcher_win.h"

#include "base/metrics/histogram.h"
#include "base/perftimer.h"
#include "base/threading/worker_pool.h"
#include "net/base/net_errors.h"
#include "net/proxy/dhcp_proxy_script_adapter_fetcher_win.h"

#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

namespace {

// How long to wait at maximum after we get results (a PAC file or
// knowledge that no PAC file is configured) from whichever network
// adapter finishes first.
const int kMaxWaitAfterFirstResultMs = 400;

const int kGetAdaptersAddressesErrors[] = {
  ERROR_ADDRESS_NOT_ASSOCIATED,
  ERROR_BUFFER_OVERFLOW,
  ERROR_INVALID_PARAMETER,
  ERROR_NOT_ENOUGH_MEMORY,
  ERROR_NO_DATA,
};

}  // namespace

namespace net {

DhcpProxyScriptFetcherWin::DhcpProxyScriptFetcherWin(
    URLRequestContext* url_request_context)
    : state_(STATE_START),
      ALLOW_THIS_IN_INITIALIZER_LIST(fetcher_callback_(
          this, &DhcpProxyScriptFetcherWin::OnFetcherDone)),
      num_pending_fetchers_(0),
      url_request_context_(url_request_context) {
  DCHECK(url_request_context_);
}

DhcpProxyScriptFetcherWin::~DhcpProxyScriptFetcherWin() {
  // Count as user-initiated if we are not yet in STATE_DONE.
  Cancel();

  // The WeakPtr we passed to the worker thread may be destroyed on the
  // worker thread.  This detaches any outstanding WeakPtr state from
  // the current thread.
  base::SupportsWeakPtr<DhcpProxyScriptFetcherWin>::DetachFromThread();
}

int DhcpProxyScriptFetcherWin::Fetch(string16* utf16_text,
                                     CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  if (state_ != STATE_START && state_ != STATE_DONE) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  fetch_start_time_ = base::TimeTicks::Now();

  state_ = STATE_WAIT_ADAPTERS;
  client_callback_ = callback;
  destination_string_ = utf16_text;

  worker_thread_ = ImplCreateWorkerThread(AsWeakPtr());
  worker_thread_->Start();

  return ERR_IO_PENDING;
}

void DhcpProxyScriptFetcherWin::Cancel() {
  DCHECK(CalledOnValidThread());

  if (state_ != STATE_DONE) {
    // We only count this stat if the cancel was explicitly initiated by
    // our client, and if we weren't already in STATE_DONE.
    UMA_HISTOGRAM_TIMES("Net.DhcpWpadCancelTime",
                        base::TimeTicks::Now() - fetch_start_time_);
  }

  CancelImpl();
}

void DhcpProxyScriptFetcherWin::CancelImpl() {
  DCHECK(CalledOnValidThread());

  if (state_ != STATE_DONE) {
    client_callback_ = NULL;
    wait_timer_.Stop();
    state_ = STATE_DONE;

    for (FetcherVector::iterator it = fetchers_.begin();
         it != fetchers_.end();
         ++it) {
      (*it)->Cancel();
    }

    fetchers_.reset();
  }
}

void DhcpProxyScriptFetcherWin::OnGetCandidateAdapterNamesDone(
    const std::set<std::string>& adapter_names) {
  DCHECK(CalledOnValidThread());

  // We may have been cancelled.
  if (state_ != STATE_WAIT_ADAPTERS)
    return;

  state_ = STATE_NO_RESULTS;

  if (adapter_names.empty()) {
    TransitionToDone();
    return;
  }

  for (std::set<std::string>::const_iterator it = adapter_names.begin();
       it != adapter_names.end();
       ++it) {
    DhcpProxyScriptAdapterFetcher* fetcher(ImplCreateAdapterFetcher());
    fetcher->Fetch(*it, &fetcher_callback_);
    fetchers_.push_back(fetcher);
  }
  num_pending_fetchers_ = fetchers_.size();
}

std::string DhcpProxyScriptFetcherWin::GetFetcherName() const {
  DCHECK(CalledOnValidThread());
  return "win";
}

const GURL& DhcpProxyScriptFetcherWin::GetPacURL() const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DONE);

  return pac_url_;
}

void DhcpProxyScriptFetcherWin::OnFetcherDone(int result) {
  DCHECK(state_ == STATE_NO_RESULTS || state_ == STATE_SOME_RESULTS);

  if (--num_pending_fetchers_ == 0) {
    TransitionToDone();
    return;
  }

  // If the only pending adapters are those less preferred than one
  // with a valid PAC script, we do not need to wait any longer.
  for (FetcherVector::iterator it = fetchers_.begin();
       it != fetchers_.end();
       ++it) {
    bool did_finish = (*it)->DidFinish();
    int result = (*it)->GetResult();
    if (did_finish && result == OK) {
      TransitionToDone();
      return;
    }
    if (!did_finish || result != ERR_PAC_NOT_IN_DHCP) {
      break;
    }
  }

  // Once we have a single result, we set a maximum on how long to wait
  // for the rest of the results.
  if (state_ == STATE_NO_RESULTS) {
    state_ = STATE_SOME_RESULTS;
    wait_timer_.Start(
        base::TimeDelta::FromMilliseconds(ImplGetMaxWaitMs()),
        this, &DhcpProxyScriptFetcherWin::OnWaitTimer);
  }
}

void DhcpProxyScriptFetcherWin::OnWaitTimer() {
  DCHECK_EQ(state_, STATE_SOME_RESULTS);

  // These are intended to help us understand whether our timeout may
  // be too aggressive or not aggressive enough.
  UMA_HISTOGRAM_COUNTS_100("Net.DhcpWpadNumAdaptersAtWaitTimer",
                           fetchers_.size());
  UMA_HISTOGRAM_COUNTS_100("Net.DhcpWpadNumPendingAdaptersAtWaitTimer",
                           num_pending_fetchers_);

  TransitionToDone();
}

void DhcpProxyScriptFetcherWin::TransitionToDone() {
  DCHECK(state_ == STATE_NO_RESULTS || state_ == STATE_SOME_RESULTS);

  int result = ERR_PAC_NOT_IN_DHCP;  // Default if no fetchers.
  if (!fetchers_.empty()) {
    // Scan twice for the result; once through the whole list for success,
    // then if no success, return result for most preferred network adapter,
    // preferring "real" network errors to the ERR_PAC_NOT_IN_DHCP error.
    // Default to ERR_ABORTED if no fetcher completed.
    result = ERR_ABORTED;
    for (FetcherVector::iterator it = fetchers_.begin();
         it != fetchers_.end();
         ++it) {
      if ((*it)->DidFinish() && (*it)->GetResult() == OK) {
        result = OK;
        *destination_string_ = (*it)->GetPacScript();
        pac_url_ = (*it)->GetPacURL();
        break;
      }
    }
    if (result != OK) {
      destination_string_->clear();
      for (FetcherVector::iterator it = fetchers_.begin();
           it != fetchers_.end();
           ++it) {
        if ((*it)->DidFinish()) {
          result = (*it)->GetResult();
          if (result != ERR_PAC_NOT_IN_DHCP) {
            break;
          }
        }
      }
    }
  }

  CompletionCallback* callback = client_callback_;
  CancelImpl();
  DCHECK_EQ(state_, STATE_DONE);
  DCHECK(fetchers_.empty());
  DCHECK(!client_callback_);  // Invariant of data.

  UMA_HISTOGRAM_TIMES("Net.DhcpWpadCompletionTime",
                      base::TimeTicks::Now() - fetch_start_time_);

  if (result != OK) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Net.DhcpWpadFetchError", std::abs(result), GetAllErrorCodesForUma());
  }

  // We may be deleted re-entrantly within this outcall.
  callback->Run(result);
}

int DhcpProxyScriptFetcherWin::num_pending_fetchers() const {
  return num_pending_fetchers_;
}

URLRequestContext* DhcpProxyScriptFetcherWin::url_request_context() const {
  return url_request_context_;
}

DhcpProxyScriptAdapterFetcher*
    DhcpProxyScriptFetcherWin::ImplCreateAdapterFetcher() {
  return new DhcpProxyScriptAdapterFetcher(url_request_context_);
}

DhcpProxyScriptFetcherWin::WorkerThread*
    DhcpProxyScriptFetcherWin::ImplCreateWorkerThread(
        const base::WeakPtr<DhcpProxyScriptFetcherWin>& owner) {
  return new WorkerThread(owner);
}

int DhcpProxyScriptFetcherWin::ImplGetMaxWaitMs() {
  return kMaxWaitAfterFirstResultMs;
}

bool DhcpProxyScriptFetcherWin::GetCandidateAdapterNames(
    std::set<std::string>* adapter_names) {
  DCHECK(adapter_names);
  adapter_names->clear();

  // The GetAdaptersAddresses MSDN page recommends using a size of 15000 to
  // avoid reallocation.
  ULONG adapters_size = 15000;
  scoped_ptr_malloc<IP_ADAPTER_ADDRESSES> adapters;
  ULONG error = ERROR_SUCCESS;
  int num_tries = 0;

  PerfTimer time_api_access;
  do {
    adapters.reset(
        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(malloc(adapters_size)));
    // Return only unicast addresses, and skip information we do not need.
    error = GetAdaptersAddresses(AF_UNSPEC,
                                 GAA_FLAG_SKIP_ANYCAST |
                                 GAA_FLAG_SKIP_MULTICAST |
                                 GAA_FLAG_SKIP_DNS_SERVER |
                                 GAA_FLAG_SKIP_FRIENDLY_NAME,
                                 NULL,
                                 adapters.get(),
                                 &adapters_size);
    ++num_tries;
  } while (error == ERROR_BUFFER_OVERFLOW && num_tries <= 3);

  // This is primarily to validate our belief that the GetAdaptersAddresses API
  // function is fast enough to call synchronously from the network thread.
  UMA_HISTOGRAM_TIMES("Net.DhcpWpadGetAdaptersAddressesTime",
                      time_api_access.Elapsed());

  if (error != ERROR_SUCCESS) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Net.DhcpWpadGetAdaptersAddressesError",
        error,
        base::CustomHistogram::ArrayToCustomRanges(
            kGetAdaptersAddressesErrors,
            arraysize(kGetAdaptersAddressesErrors)));
  }

  if (error == ERROR_NO_DATA) {
    // There are no adapters that we care about.
    return true;
  }

  if (error != ERROR_SUCCESS) {
    LOG(WARNING) << "Unexpected error retrieving WPAD configuration from DHCP.";
    return false;
  }

  IP_ADAPTER_ADDRESSES* adapter = NULL;
  for (adapter = adapters.get(); adapter; adapter = adapter->Next) {
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;
    if ((adapter->Flags & IP_ADAPTER_DHCP_ENABLED) == 0)
      continue;

    DCHECK(adapter->AdapterName);
    adapter_names->insert(adapter->AdapterName);
  }

  return true;
}

DhcpProxyScriptFetcherWin::WorkerThread::WorkerThread(
    const base::WeakPtr<DhcpProxyScriptFetcherWin>& owner) {
  Init(owner);
}

DhcpProxyScriptFetcherWin::WorkerThread::~WorkerThread() {
}

void DhcpProxyScriptFetcherWin::WorkerThread::Start() {
  bool succeeded = base::WorkerPool::PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DhcpProxyScriptFetcherWin::WorkerThread::ThreadFunc),
      true);
  DCHECK(succeeded);
}

void DhcpProxyScriptFetcherWin::WorkerThread::ThreadFunc() {
  ImplGetCandidateAdapterNames(&adapter_names_);

  bool succeeded = origin_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DhcpProxyScriptFetcherWin::WorkerThread::OnThreadDone));
  DCHECK(succeeded);
}

void DhcpProxyScriptFetcherWin::WorkerThread::OnThreadDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (owner_)
    owner_->OnGetCandidateAdapterNamesDone(adapter_names_);
}

DhcpProxyScriptFetcherWin::WorkerThread::WorkerThread() {
}

void DhcpProxyScriptFetcherWin::WorkerThread::Init(
    const base::WeakPtr<DhcpProxyScriptFetcherWin>& owner) {
  owner_ = owner;
  origin_loop_ = base::MessageLoopProxy::current();
}

bool DhcpProxyScriptFetcherWin::WorkerThread::ImplGetCandidateAdapterNames(
    std::set<std::string>* adapter_names) {
  return DhcpProxyScriptFetcherWin::GetCandidateAdapterNames(adapter_names);
}

}  // namespace net
