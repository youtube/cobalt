// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "chrome/browser/platform_util.h"
#import "components/policy/core/common/policy_logger.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_util.h"

namespace {

constexpr NSString* kHeader = @"header";
constexpr NSString* kSSOCookies = @"sso_cookies";
constexpr NSString* kPrtHeaders = @"prt_headers";
constexpr NSString* kDeviceHeaders = @"device_headers";

// Adds the header from `headers_response` with the `headers_key` into
// `auth_headers`. `headers_key` is either "prt_headers" or "device_headers".
// `headers_response` has the following format example.
// {
//   "prt_headers": [
//     {
//       "header": {
//         "x-ms-RefreshTokenCredential": "prt_header_value"
//       },
//       "home_account_id": "123-123-1234"
//     },
//     {
//       "header": {
//         "x-ms-RefreshTokenCredential1": "prt_header_value_1"
//       },
//       "home_account_id": "abc-abc-abcd"
//     },
//     {
//       "header": {
//         "x-ms-RefreshTokenCredential2": "prt_header_value_2"
//       },
//       "home_account_id": "123-abc-12ab"
//     }
//   ],
//   "device_headers": [
//     {
//       "header": {
//         "x-ms-DeviceCredential": "device_header_value"
//       },
//       "tenant_id": "qwe-qwe-qwer"
//     }
//   ]
// }
void AddMSAuthHeadersFromSSOCookiesResponse(
    net::HttpRequestHeaders& auth_headers,
    NSDictionary* sso_cookies_response,
    NSString* headers_key) {
  NSArray* headers = sso_cookies_response[headers_key];
  auto headers_key_str = base::SysNSStringToUTF8(headers_key);

  for (NSDictionary* header_data in headers) {
    NSDictionary* header_definition = header_data[kHeader];
    for (NSString* key in header_definition) {
      auto header_name = base::SysNSStringToUTF8(key);

      if (!net::HttpUtil::IsValidHeaderName(header_name)) {
        VLOG_POLICY(2, EXTENSIBLE_SSO)
            << "[ExtensibleEnterpriseSSO] Invalid header name "
            << headers_key_str << " : " << header_name;
        continue;
      }

      auto header_value = base::SysNSStringToUTF8(
          net::FixNSStringIncorrectlyDecodedAsLatin1(header_definition[key]));
      if (!net::HttpUtil::IsValidHeaderValue(header_value)) {
        VLOG_POLICY(2, EXTENSIBLE_SSO)
            << "[ExtensibleEnterpriseSSO] Invalid header value "
            << headers_key_str << " : " << header_value;
        continue;
      }

      VLOG_POLICY(2, EXTENSIBLE_SSO)
          << "[ExtensibleEnterpriseSSO] Header added : " << "{ " << header_name
          << ": " << header_value << "}";
      auth_headers.SetHeader(header_name, header_value);
    }
  }
}

}  // namespace

// Class that allows fetching authentication headers for a url if it is
// supported by any SSO extension on the device.
@implementation SSOServiceEntraAuthControllerDelegate {
  enterprise_auth::PlatformAuthProviderManager::GetDataCallback _callback;
  ASAuthorizationSingleSignOnProvider* _auth_provider;
  ASAuthorizationController* _controller;
}

- (instancetype)initWithAuthorizationSingleSignOnProvider:
    (ASAuthorizationSingleSignOnProvider*)auth_provider {
  CHECK(auth_provider);
  CHECK([auth_provider canPerformAuthorization]);
  if ((self = [super init])) {
    self->_auth_provider = auth_provider;
  }
  return self;
}

- (void)dealloc {
  // This is here for debugging purposes and will be removed once this code is
  // no longer experimental.
  if (_callback) {
    VLOG_POLICY(2, EXTENSIBLE_SSO)
        << "[ExtensibleEnterpriseSSO] Fetching headers aborted.";
    std::move(_callback).Run(net::HttpRequestHeaders());
  }
}

- (ASAuthorizationSingleSignOnRequest*)createRequest {
  // Create a request for `url`.
  ASAuthorizationSingleSignOnRequest* request = [_auth_provider createRequest];

  request.requestedOperation = @"get_sso_cookies";
  if (@available(macOS 12, *)) {
    request.userInterfaceEnabled = NO;
  }

  request.authorizationOptions = @[
    [NSURLQueryItem queryItemWithName:@"sso_url"
                                value:_auth_provider.url.absoluteString],
    // Response headers to fetch.
    // “0” -> All headers, "1" -> PRT headers only, "2" -> Device headers only.
    [NSURLQueryItem queryItemWithName:@"types_of_header" value:@"0"],
    [NSURLQueryItem queryItemWithName:@"msg_protocol_ver" value:@"4"],
  ];
  return request;
}

- (ASAuthorizationController*)createAuthorizationControllerWithRequest:
    (ASAuthorizationSingleSignOnRequest*)request {
  ASAuthorizationController* controller = [[ASAuthorizationController alloc]
      initWithAuthorizationRequests:[NSArray arrayWithObject:request]];
  controller.delegate = self;
  controller.presentationContextProvider = self;
  return controller;
}

// Gets authentication headers for `url` if the device can perform
// authentication for it.
// If the device can perform the authentication, `withCallback` is called
// with headers built from the response from the device, otherwise it is called
// with empty headers.
- (void)getAuthHeaders:(NSURL*)url
          withCallback:
              (enterprise_auth::PlatformAuthProviderManager::GetDataCallback)
                  callback {
  _callback = std::move(callback);

  ASAuthorizationSingleSignOnRequest* request = [self createRequest];
  _controller = [self createAuthorizationControllerWithRequest:request];
  [_controller performRequests];
}

- (net::HttpRequestHeaders)getHeadersFromHttpResponse:
    (NSHTTPURLResponse*)response {
  // An example response headers:
  // {
  //   "sso_cookies": "JSON formated object"
  //   "operation": "get_sso_cookies",
  //   "broker_version": "3.3.23",
  //   "preferred_auth_config": "preferredAuthNotConfigured",
  //   "success": "1",
  //   "wpj_status": "notJoined",
  //   "operation_response_type": "operation_get_sso_cookies_response",
  //   "request_received_timestamp": "1736954944.2245578766",
  //   "extraDeviceInfo": "{}",
  //   "sso_extension_mode": "full",
  //   "device_mode": "personal",
  //   "response_gen_timestamp": "1736954944.7778768539",
  //   "platform_sso_status": "platformSSONotEnabled"
  // }
  NSDictionary* all_headers = response.allHeaderFields;
  NSString* sso_cookies_json = all_headers[kSSOCookies];

  net::HttpRequestHeaders auth_headers;
  NSDictionary* sso_cookies = [NSJSONSerialization
      JSONObjectWithData:[sso_cookies_json
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  if (!sso_cookies) {
    VLOG_POLICY(2, EXTENSIBLE_SSO)
        << "[ExtensibleEnterpriseSSO] Failed to deserialize sso_cookies: "
        << sso_cookies_json;
    return auth_headers;
  }

  AddMSAuthHeadersFromSSOCookiesResponse(auth_headers, sso_cookies,
                                         kPrtHeaders);
  AddMSAuthHeadersFromSSOCookiesResponse(auth_headers, sso_cookies,
                                         kDeviceHeaders);
  return auth_headers;
}

#pragma mark - ASAuthorizationControllerDelegate

// Called when the authentication was successful and creates a
// HttpRequestHeaders from `authorization`.
- (void)authorizationController:(ASAuthorizationController*)controller
    didCompleteWithAuthorization:(ASAuthorization*)authorization {
  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[ExtensibleEnterpriseSSO] Fetching headers completed.";
  ASAuthorizationSingleSignOnCredential* credential = authorization.credential;
  std::move(_callback).Run(
      [self getHeadersFromHttpResponse:credential.authenticatedResponse]);
}

// Called when the authentication failed and creates a
// empty HttpRequestHeaders.
- (void)authorizationController:(ASAuthorizationController*)controller
           didCompleteWithError:(NSError*)error {
  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[ExtensibleEnterpriseSSO] Fetching headers failed";
  std::move(_callback).Run(net::HttpRequestHeaders());
}

#pragma mark - ASAuthorizationControllerPresentationContextProviding
- (ASPresentationAnchor)presentationAnchorForAuthorizationController:
    (ASAuthorizationController*)controller {
  // TODO (crbug/340868357): Pick the window where the url is being used.
  return platform_util::GetActiveWindow();
}

@end
