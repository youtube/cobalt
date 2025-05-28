// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_REMOTE_SUGGESTIONS_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_REMOTE_SUGGESTIONS_SERVICE_OBSERVER_BRIDGE_H_

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/remote_suggestions_service.h"

@protocol RemoteSuggestionsServiceObserver
- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    createdRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                         request:(const network::ResourceRequest*)request;

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    startedRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                     requestBody:(NSString*)requestBody
                       URLLoader:(network::SimpleURLLoader*)URLLoader;

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    completedRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                      responseCode:(NSInteger)code
                      responseBody:(NSString*)responseBody;
@end

class RemoteSuggestionsServiceObserverBridge
    : public RemoteSuggestionsService::Observer {
 public:
  RemoteSuggestionsServiceObserverBridge(
      id<RemoteSuggestionsServiceObserver> observer,
      RemoteSuggestionsService* remote_suggestions_service);

  RemoteSuggestionsServiceObserverBridge(
      const RemoteSuggestionsServiceObserverBridge&) = delete;
  RemoteSuggestionsServiceObserverBridge& operator=(
      const RemoteSuggestionsServiceObserverBridge&) = delete;

  void OnRequestCreated(const base::UnguessableToken& request_id,
                        const network::ResourceRequest* request) override;

  void OnRequestStarted(const base::UnguessableToken& request_id,
                        network::SimpleURLLoader* loader,
                        const std::string& request_body) override;

  void OnRequestCompleted(
      const base::UnguessableToken& request_id,
      const int response_code,
      const std::unique_ptr<std::string>& response_body) override;

 private:
  __weak id<RemoteSuggestionsServiceObserver> observer_;
  raw_ptr<RemoteSuggestionsService> remote_suggestions_service_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_REMOTE_SUGGESTIONS_SERVICE_OBSERVER_BRIDGE_H_
