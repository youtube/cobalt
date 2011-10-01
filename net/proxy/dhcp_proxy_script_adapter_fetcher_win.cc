// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/dhcp_proxy_script_adapter_fetcher_win.h"

#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "net/proxy/dhcpcsvc_init_win.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/url_request/url_request_context.h"

#include <windows.h>
#include <winsock2.h>
#include <dhcpcsdk.h>
#pragma comment(lib, "dhcpcsvc.lib")

namespace {

// Maximum amount of time to wait for response from the Win32 DHCP API.
const int kTimeoutMs = 2000;

}  // namespace

namespace net {

DhcpProxyScriptAdapterFetcher::DhcpProxyScriptAdapterFetcher(
    URLRequestContext* url_request_context)
    : state_(STATE_START),
      result_(ERR_IO_PENDING),
      callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          script_fetcher_callback_(
              this, &DhcpProxyScriptAdapterFetcher::OnFetcherDone)),
      url_request_context_(url_request_context) {
}

DhcpProxyScriptAdapterFetcher::~DhcpProxyScriptAdapterFetcher() {
  Cancel();

  // The WeakPtr we passed to the worker thread may be destroyed on the
  // worker thread.  This detaches any outstanding WeakPtr state from
  // the current thread.
  base::SupportsWeakPtr<DhcpProxyScriptAdapterFetcher>::DetachFromThread();
}

void DhcpProxyScriptAdapterFetcher::Fetch(
    const std::string& adapter_name, OldCompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, STATE_START);
  result_ = ERR_IO_PENDING;
  pac_script_ = string16();
  state_ = STATE_WAIT_DHCP;
  callback_ = callback;

  wait_timer_.Start(FROM_HERE, ImplGetTimeout(),
                    this, &DhcpProxyScriptAdapterFetcher::OnTimeout);
  worker_thread_ = ImplCreateWorkerThread(AsWeakPtr());
  worker_thread_->Start(adapter_name);
}

void DhcpProxyScriptAdapterFetcher::Cancel() {
  DCHECK(CalledOnValidThread());
  callback_ = NULL;
  wait_timer_.Stop();
  script_fetcher_.reset();

  switch (state_) {
    case STATE_WAIT_DHCP:
      // Nothing to do here, we let the worker thread run to completion,
      // the task it posts back when it completes will check the state.
      break;
    case STATE_WAIT_URL:
      break;
    case STATE_START:
    case STATE_FINISH:
    case STATE_CANCEL:
      break;
  }

  if (state_ != STATE_FINISH) {
    result_ = ERR_ABORTED;
    state_ = STATE_CANCEL;
  }
}

bool DhcpProxyScriptAdapterFetcher::DidFinish() const {
  DCHECK(CalledOnValidThread());
  return state_ == STATE_FINISH;
}

int DhcpProxyScriptAdapterFetcher::GetResult() const {
  DCHECK(CalledOnValidThread());
  return result_;
}

string16 DhcpProxyScriptAdapterFetcher::GetPacScript() const {
  DCHECK(CalledOnValidThread());
  return pac_script_;
}

GURL DhcpProxyScriptAdapterFetcher::GetPacURL() const {
  DCHECK(CalledOnValidThread());
  return pac_url_;
}

DhcpProxyScriptAdapterFetcher::WorkerThread::WorkerThread(
    const base::WeakPtr<DhcpProxyScriptAdapterFetcher>& owner)
        : owner_(owner),
          origin_loop_(base::MessageLoopProxy::current()) {
}

DhcpProxyScriptAdapterFetcher::WorkerThread::~WorkerThread() {
}

void DhcpProxyScriptAdapterFetcher::WorkerThread::Start(
    const std::string& adapter_name) {
  bool succeeded = base::WorkerPool::PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DhcpProxyScriptAdapterFetcher::WorkerThread::ThreadFunc,
          adapter_name),
      true);
  DCHECK(succeeded);
}

void DhcpProxyScriptAdapterFetcher::WorkerThread::ThreadFunc(
    const std::string& adapter_name) {
  std::string url = ImplGetPacURLFromDhcp(adapter_name);

  bool succeeded = origin_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DhcpProxyScriptAdapterFetcher::WorkerThread::OnThreadDone,
          url));
  DCHECK(succeeded);
}

void DhcpProxyScriptAdapterFetcher::WorkerThread::OnThreadDone(
    const std::string& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (owner_)
    owner_->OnQueryDhcpDone(url);
}

std::string
    DhcpProxyScriptAdapterFetcher::WorkerThread::ImplGetPacURLFromDhcp(
        const std::string& adapter_name) {
  return DhcpProxyScriptAdapterFetcher::GetPacURLFromDhcp(adapter_name);
}

void DhcpProxyScriptAdapterFetcher::OnQueryDhcpDone(
    const std::string& url) {
  DCHECK(CalledOnValidThread());
  // Because we can't cancel the call to the Win32 API, we can expect
  // it to finish while we are in a few different states.  The expected
  // one is WAIT_DHCP, but it could be in CANCEL if Cancel() was called,
  // or FINISH if timeout occurred.
  DCHECK(state_ == STATE_WAIT_DHCP || state_ == STATE_CANCEL ||
         state_ == STATE_FINISH);
  if (state_ != STATE_WAIT_DHCP)
    return;

  wait_timer_.Stop();

  pac_url_ = GURL(url);
  if (pac_url_.is_empty() || !pac_url_.is_valid()) {
    result_ = ERR_PAC_NOT_IN_DHCP;
    TransitionToFinish();
  } else {
    state_ = STATE_WAIT_URL;
    script_fetcher_.reset(ImplCreateScriptFetcher());
    script_fetcher_->Fetch(pac_url_, &pac_script_, &script_fetcher_callback_);
  }
}

void DhcpProxyScriptAdapterFetcher::OnTimeout() {
  DCHECK_EQ(state_, STATE_WAIT_DHCP);
  result_ = ERR_TIMED_OUT;
  TransitionToFinish();
}

void DhcpProxyScriptAdapterFetcher::OnFetcherDone(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == STATE_WAIT_URL || state_ == STATE_CANCEL);
  if (state_ == STATE_CANCEL)
    return;

  // At this point, pac_script_ has already been written to.
  script_fetcher_.reset();
  result_ = result;
  TransitionToFinish();
}

void DhcpProxyScriptAdapterFetcher::TransitionToFinish() {
  DCHECK(state_ == STATE_WAIT_DHCP || state_ == STATE_WAIT_URL);
  state_ = STATE_FINISH;
  OldCompletionCallback* callback = callback_;
  callback_ = NULL;

  // Be careful not to touch any member state after this, as the client
  // may delete us during this callback.
  callback->Run(result_);
}

DhcpProxyScriptAdapterFetcher::State
    DhcpProxyScriptAdapterFetcher::state() const {
  return state_;
}

ProxyScriptFetcher* DhcpProxyScriptAdapterFetcher::ImplCreateScriptFetcher() {
  return new ProxyScriptFetcherImpl(url_request_context_);
}

DhcpProxyScriptAdapterFetcher::WorkerThread*
    DhcpProxyScriptAdapterFetcher::ImplCreateWorkerThread(
        const base::WeakPtr<DhcpProxyScriptAdapterFetcher>& owner) {
  return new WorkerThread(owner);
}

base::TimeDelta DhcpProxyScriptAdapterFetcher::ImplGetTimeout() const {
  return base::TimeDelta::FromMilliseconds(kTimeoutMs);
}

// static
std::string DhcpProxyScriptAdapterFetcher::GetPacURLFromDhcp(
    const std::string& adapter_name) {
  EnsureDhcpcsvcInit();

  std::wstring adapter_name_wide = base::SysMultiByteToWide(adapter_name,
                                                            CP_ACP);

  DHCPCAPI_PARAMS_ARRAY send_params = { 0, NULL };

  BYTE option_data[] = { 1, 252 };
  DHCPCAPI_PARAMS wpad_params = { 0 };
  wpad_params.OptionId = 252;
  wpad_params.IsVendor = FALSE;  // Surprising, but intentional.

  DHCPCAPI_PARAMS_ARRAY request_params = { 0 };
  request_params.nParams = 1;
  request_params.Params = &wpad_params;

  // The maximum message size is typically 4096 bytes on Windows per
  // http://support.microsoft.com/kb/321592
  DWORD result_buffer_size = 4096;
  scoped_ptr_malloc<BYTE> result_buffer;
  int retry_count = 0;
  DWORD res = NO_ERROR;
  do {
    result_buffer.reset(reinterpret_cast<BYTE*>(malloc(result_buffer_size)));

    // Note that while the DHCPCAPI_REQUEST_SYNCHRONOUS flag seems to indicate
    // there might be an asynchronous mode, there seems to be (at least in
    // terms of well-documented use of this API) only a synchronous mode, with
    // an optional "async notifications later if the option changes" mode.
    // Even IE9, which we hope to emulate as IE is the most widely deployed
    // previous implementation of the DHCP aspect of WPAD and the only one
    // on Windows (Konqueror is the other, on Linux), uses this API with the
    // synchronous flag.  There seem to be several Microsoft Knowledge Base
    // articles about calls to this function failing when other flags are used
    // (e.g. http://support.microsoft.com/kb/885270) so we won't take any
    // chances on non-standard, poorly documented usage.
    res = ::DhcpRequestParams(DHCPCAPI_REQUEST_SYNCHRONOUS,
                              NULL,
                              const_cast<LPWSTR>(adapter_name_wide.c_str()),
                              NULL,
                              send_params, request_params,
                              result_buffer.get(), &result_buffer_size,
                              NULL);
    ++retry_count;
  } while (res == ERROR_MORE_DATA && retry_count <= 3);

  if (res != NO_ERROR) {
    LOG(INFO) << "Error fetching PAC URL from DHCP: " << res;
    UMA_HISTOGRAM_COUNTS("Net.DhcpWpadUnhandledDhcpError", 1);
  } else if (wpad_params.nBytesData) {
    // The result should be ASCII, not wide character.
    DCHECK_EQ(strlen(reinterpret_cast<const char*>(wpad_params.Data)) + 1,
              wpad_params.nBytesData);
    // Return only up to the first null in case of embedded NULLs; if the
    // server is giving us back a buffer with embedded NULLs, something is
    // broken anyway.
    return std::string(reinterpret_cast<const char *>(wpad_params.Data));
  }

  return "";
}

}  // namespace net
