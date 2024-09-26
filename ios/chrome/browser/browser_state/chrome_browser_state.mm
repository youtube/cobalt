// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"

#import <memory>
#import <utility>

#import "base/check_op.h"
#import "base/files/file_path.h"
#import "base/task/sequenced_task_runner.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/variations/net/variations_http_headers.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/webui/url_data_manager_ios_backend.h"
#import "net/url_request/url_request_context_getter.h"
#import "net/url_request/url_request_interceptor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// All ChromeBrowserState will store a dummy base::SupportsUserData::Data
// object with this key. It can be used to check that a web::BrowserState
// is effectively a ChromeBrowserState when converting.
const char kBrowserStateIsChromeBrowserState[] = "IsChromeBrowserState";
}

ChromeBrowserState::ChromeBrowserState(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  DCHECK(io_task_runner_);
  SetUserData(kBrowserStateIsChromeBrowserState,
              std::make_unique<base::SupportsUserData::Data>());
}

ChromeBrowserState::~ChromeBrowserState() {}

// static
ChromeBrowserState* ChromeBrowserState::FromBrowserState(
    web::BrowserState* browser_state) {
  if (!browser_state)
    return nullptr;

  // Check that the BrowserState is a ChromeBrowserState. It should always
  // be true in production and during tests as the only BrowserState that
  // should be used in ios/chrome inherits from ChromeBrowserState.
  DCHECK(browser_state->GetUserData(kBrowserStateIsChromeBrowserState));
  return static_cast<ChromeBrowserState*>(browser_state);
}

// static
ChromeBrowserState* ChromeBrowserState::FromWebUIIOS(web::WebUIIOS* web_ui) {
  return FromBrowserState(web_ui->GetWebState()->GetBrowserState());
}

std::string ChromeBrowserState::GetDebugName() {
  // The debug name is based on the state path of the original browser state
  // to keep in sync with the meaning on other platforms.
  std::string name =
      GetOriginalChromeBrowserState()->GetStatePath().BaseName().MaybeAsASCII();
  if (name.empty()) {
    name = "UnknownBrowserState";
  }
  return name;
}

scoped_refptr<base::SequencedTaskRunner> ChromeBrowserState::GetIOTaskRunner() {
  return io_task_runner_;
}

PrefService* ChromeBrowserState::GetPrefs() {
  return GetSyncablePrefs();
}

net::URLRequestContextGetter* ChromeBrowserState::GetRequestContext() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!request_context_getter_) {
    ProtocolHandlerMap protocol_handlers;
    protocol_handlers[kChromeUIScheme] =
        web::URLDataManagerIOSBackend::CreateProtocolHandler(this);
    request_context_getter_ =
        base::WrapRefCounted(CreateRequestContext(&protocol_handlers));
  }
  return request_context_getter_.get();
}

void ChromeBrowserState::UpdateCorsExemptHeader(
    network::mojom::NetworkContextParams* params) {
  variations::UpdateCorsExemptHeaderForVariations(params);
}
