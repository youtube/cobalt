// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui_bundled/popup/remote_suggestions_service_observer_bridge.h"

RemoteSuggestionsServiceObserverBridge::RemoteSuggestionsServiceObserverBridge(
    id<RemoteSuggestionsServiceObserver> observer,
    RemoteSuggestionsService* remote_suggestions_service)
    : observer_(observer),
      remote_suggestions_service_(remote_suggestions_service) {}

void RemoteSuggestionsServiceObserverBridge::OnRequestCreated(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest* request) {
  [observer_ remoteSuggestionsService:remote_suggestions_service_
         createdRequestWithIdentifier:request_id
                              request:request];
}

void RemoteSuggestionsServiceObserverBridge::OnRequestStarted(
    const base::UnguessableToken& request_id,
    network::SimpleURLLoader* loader,
    const std::string& request_body) {
  NSString* requestBody = base::SysUTF8ToNSString(request_body);
  [observer_ remoteSuggestionsService:remote_suggestions_service_
         startedRequestWithIdentifier:request_id
                          requestBody:requestBody
                            URLLoader:loader];
}

void RemoteSuggestionsServiceObserverBridge::OnRequestCompleted(
    const base::UnguessableToken& request_id,
    const int response_code,
    const std::unique_ptr<std::string>& response_body) {
  NSString* responseBody = nil;
  if (response_code == 200 && response_body) {
    responseBody = base::SysUTF8ToNSString(*response_body.get());
  }
  [observer_ remoteSuggestionsService:remote_suggestions_service_
       completedRequestWithIdentifier:request_id
                         responseCode:response_code
                         responseBody:responseBody];
}
