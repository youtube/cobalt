// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_scheme_handler.h"

#import <algorithm>
#import <map>

#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/public/web_state.h"
#import "ios/web/webui/url_fetcher_block_adapter.h"
#import "ios/web/webui/web_ui_constants.h"
#import "ios/web/webui/web_ui_ios_controller_factory_registry.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"
#import "url/scheme_host_port.h"

namespace {
// Returns the error code associated with `URL`.
NSInteger GetErrorCodeForUrl(const GURL& URL) {
  web::WebUIIOSControllerFactory* factory =
      web::WebUIIOSControllerFactoryRegistry::GetInstance();
  return factory ? factory->GetErrorCodeForWebUIURL(URL)
                 : NSURLErrorUnsupportedURL;
}
}  // namespace

@implementation CRWWebUISchemeHandler {
  scoped_refptr<network::SharedURLLoaderFactory> _URLLoaderFactory;

  // Set of live WebUI fetchers for retrieving data.
  std::map<id<WKURLSchemeTask>, std::unique_ptr<web::URLFetcherBlockAdapter>>
      _map;
}

- (instancetype)initWithURLLoaderFactory:
    (scoped_refptr<network::SharedURLLoaderFactory>)URLLoaderFactory {
  self = [super init];
  if (self) {
    _URLLoaderFactory = URLLoaderFactory;
  }
  return self;
}

- (void)webView:(WKWebView*)webView
    startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  GURL URL = net::GURLWithNSURL(urlSchemeTask.request.URL);
  // Check the mainDocumentURL as the URL might be one of the subresource, so
  // not a WebUI URL itself.
  GURL webUIURL = net::GURLWithNSURL(urlSchemeTask.request.mainDocumentURL);
  NSInteger errorCode = GetErrorCodeForUrl(webUIURL);
  if (errorCode != 0) {
    NSError* error = [NSError
        errorWithDomain:NSURLErrorDomain
                   code:errorCode
               userInfo:@{
                 NSURLErrorFailingURLErrorKey : urlSchemeTask.request.URL
               }];
    [urlSchemeTask didFailWithError:error];
    return;
  }

  // The "Access-Control-Allow-Origin" header is required below to allow
  // requests from any WebUI page to load chrome://resources URLs. However,
  // requests between different WebUI pages are blocked directly instead.

  // Allow the main-frame navigation request itself (its URL matches the
  // provisional webView.URL).
  // Subresource requests are gated on the WebState's last committed URL
  // because both `webView.URL` and `backForwardList.currentItem.URL` may
  // already point at the destination of an in-progress navigation. The main
  // document request itself is loaded before commit and is identified by
  // matching `webView.URL`.
  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  web::WebState* webState = web::GetWebStateForWebView(webView);
  GURL committedURL = webState ? webState->GetLastCommittedURL() : GURL();
  BOOL isMainDocumentRequest = URL.EqualsIgnoringRef(webViewURL);
  BOOL isSharedResourceRequest = URL.DomainIs(web::kWebUIResourcesHost) &&
                                 GetErrorCodeForUrl(committedURL) == 0;
  if (!webState || !webViewURL.is_valid() ||
      (!isMainDocumentRequest && !isSharedResourceRequest &&
       url::SchemeHostPort(URL) != url::SchemeHostPort(committedURL))) {
    NSError* error = [NSError
        errorWithDomain:NSURLErrorDomain
                   code:NSURLErrorNoPermissionsToReadFile
               userInfo:@{
                 NSURLErrorFailingURLErrorKey : urlSchemeTask.request.URL
               }];
    [urlSchemeTask didFailWithError:error];
    return;
  }

  __weak CRWWebUISchemeHandler* weakSelf = self;
  std::unique_ptr<web::URLFetcherBlockAdapter> adapter =
      std::make_unique<web::URLFetcherBlockAdapter>(
          URL, _URLLoaderFactory,
          ^(NSData* data, NSDictionary* headers,
            web::URLFetcherBlockAdapter* fetcher) {
            CRWWebUISchemeHandler* strongSelf = weakSelf;
            if (!strongSelf ||
                strongSelf.map->find(urlSchemeTask) == strongSelf.map->end()) {
              return;
            }
            // Content type must be set. Derive it from the file extension if it
            // was not already provided in the headers.
            if (!headers[@"Content-Type"]) {
              NSMutableDictionary* mutableHeaders =
                  [[NSMutableDictionary alloc] initWithDictionary:headers];

              NSString* mimeType = @"text/html";
              base::FilePath filePath =
                  base::FilePath(fetcher->getUrl().ExtractFileName());
              if (filePath.Extension() == ".js") {
                mimeType = @"text/javascript; charset=UTF-8";
              } else if (filePath.Extension() == ".css") {
                mimeType = @"text/css; charset=UTF-8";
              } else if (filePath.Extension() == ".svg") {
                mimeType = @"image/svg+xml";
              }
              mutableHeaders[@"Content-Type"] = mimeType;
              headers = mutableHeaders;
            }

            NSHTTPURLResponse* response =
                [[NSHTTPURLResponse alloc] initWithURL:urlSchemeTask.request.URL
                                            statusCode:200
                                           HTTPVersion:@"HTTP/1.1"
                                          headerFields:headers];
            [urlSchemeTask didReceiveResponse:response];
            [urlSchemeTask didReceiveData:data];
            [urlSchemeTask didFinish];
            [weakSelf removeFetcher:fetcher];
          });
  _map.insert(std::make_pair(urlSchemeTask, std::move(adapter)));
  _map.find(urlSchemeTask)->second->Start();
}

- (void)webView:(WKWebView*)webView
    stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  auto result = _map.find(urlSchemeTask);
  if (result != _map.end()) {
    _map.erase(result);
  }
}

#pragma mark - Private

// Returns a pointer to the `_map` ivar for strongSelf.
- (std::map<id<WKURLSchemeTask>, std::unique_ptr<web::URLFetcherBlockAdapter>>*)
    map {
  return &_map;
}

// Removes `fetcher` from map of active fetchers.
- (void)removeFetcher:(web::URLFetcherBlockAdapter*)fetcher {
  _map.erase(std::ranges::find(
      _map, fetcher,
      [](const std::pair<const id<WKURLSchemeTask>,
                         std::unique_ptr<web::URLFetcherBlockAdapter>>& entry) {
        return entry.second.get();
      }));
}

@end
