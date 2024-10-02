// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_navigation_item_storage.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/public/web_client.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Keys used to serialize navigation properties.
NSString* const kNavigationItemStorageURLKey = @"urlString";
NSString* const kNavigationItemStorageVirtualURLKey = @"virtualUrlString";
NSString* const kNavigationItemStorageReferrerURLKey = @"referrerUrlString";
NSString* const kNavigationItemStorageReferrerURLDeprecatedKey = @"referrer";
NSString* const kNavigationItemStorageReferrerPolicyKey = @"referrerPolicy";
NSString* const kNavigationItemStorageTimestampKey = @"timestamp";
NSString* const kNavigationItemStorageTitleKey = @"title";
NSString* const kNavigationItemStorageHTTPRequestHeadersKey = @"httpHeaders";
NSString* const kNavigationItemStorageUserAgentTypeKey = @"userAgentType";

const char kNavigationItemSerializedSizeHistogram[] =
    "Session.WebStates.NavigationItem.SerializedSize";
const char kNavigationItemSerializedVirtualURLSizeHistogram[] =
    "Session.WebStates.NavigationItem.SerializedVirtualURLSize";
const char kNavigationItemSerializedURLSizeHistogram[] =
    "Session.WebStates.NavigationItem.SerializedURLSize";
const char kNavigationItemSerializedReferrerURLSizeHistogram[] =
    "Session.WebStates.NavigationItem.SerializedReferrerURLSize";
const char kNavigationItemSerializedTitleSizeHistogram[] =
    "Session.WebStates.NavigationItem.SerializedTitleSize";
const char kNavigationItemSerializedRequestHeadersSizeHistogram[] =
    "Session.WebStates.NavigationItem.SerializedRequestHeadersSize";

}  // namespace web

@implementation CRWNavigationItemStorage {
  GURL _URL;
  GURL _virtualURL;
  std::u16string _title;
}

#pragma mark - NSObject

- (NSString*)description {
  NSMutableString* description =
      [NSMutableString stringWithString:[super description]];
  [description appendFormat:@"URL : %s, ", _URL.spec().c_str()];
  [description appendFormat:@"virtualURL : %s, ", _virtualURL.spec().c_str()];
  [description appendFormat:@"referrer : %s, ", _referrer.url.spec().c_str()];
  [description appendFormat:@"timestamp : %f, ", _timestamp.ToCFAbsoluteTime()];
  [description appendFormat:@"title : %@, ", base::SysUTF16ToNSString(_title)];
  [description
      appendFormat:@"userAgentType : %s, ",
                   web::GetUserAgentTypeDescription(_userAgentType).c_str()];
  [description appendFormat:@"HTTPRequestHeaders : %@", _HTTPRequestHeaders];
  return description;
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    // Desktop chrome only persists virtualUrl_ and uses it to feed the url
    // when creating a NavigationEntry. Chrome on iOS is also storing _url.
    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageVirtualURLKey]) {
      _virtualURL = GURL(web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageVirtualURLKey));
    }

    if ([aDecoder containsValueForKey:web::kNavigationItemStorageURLKey]) {
      _URL = GURL(web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageURLKey));
    }

    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageReferrerURLKey]) {
      const std::string referrerString(web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageReferrerURLKey));
      web::ReferrerPolicy referrerPolicy =
          static_cast<web::ReferrerPolicy>([aDecoder
              decodeIntForKey:web::kNavigationItemStorageReferrerPolicyKey]);
      _referrer = web::Referrer(GURL(referrerString), referrerPolicy);
    } else {
      // Backward compatibility.
      NSURL* referrerURL =
          [aDecoder decodeObjectForKey:
                        web::kNavigationItemStorageReferrerURLDeprecatedKey];
      _referrer = web::Referrer(net::GURLWithNSURL(referrerURL),
                                web::ReferrerPolicyDefault);
    }

    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageTimestampKey]) {
      int64_t us =
          [aDecoder decodeInt64ForKey:web::kNavigationItemStorageTimestampKey];
      _timestamp = base::Time::FromInternalValue(us);
    }

    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageUserAgentTypeKey]) {
      std::string userAgentDescription = web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageUserAgentTypeKey);
      _userAgentType =
          web::GetUserAgentTypeWithDescription(userAgentDescription);
    } else if (web::GetWebClient()->IsAppSpecificURL(_virtualURL)) {
      // Legacy CRWNavigationItemStorages didn't have the concept of a NONE
      // user agent for app-specific URLs, so check decoded virtual URL before
      // attempting to decode the deprecated key.
      _userAgentType = web::UserAgentType::NONE;
    }

    NSString* title =
        [aDecoder decodeObjectForKey:web::kNavigationItemStorageTitleKey];
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.  This is what desktop chrome does.
    _title = base::SysNSStringToUTF16(title);
    _HTTPRequestHeaders = [aDecoder
        decodeObjectForKey:web::kNavigationItemStorageHTTPRequestHeadersKey];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  // Desktop Chrome doesn't persist `url_` or `originalUrl_`, only
  // `virtualUrl_`. Chrome on iOS is persisting `url_`.
  int serializedSizeInBytes = 0;
  int serializedVirtualURLSizeInBytes = 0;
  if (_virtualURL != _URL) {
    // In most cases _virtualURL is the same as URL. Not storing virtual URL
    // will save memory during unarchiving.
    web::nscoder_util::EncodeString(
        aCoder, web::kNavigationItemStorageVirtualURLKey, _virtualURL.spec());
    serializedVirtualURLSizeInBytes = _virtualURL.spec().size();
    serializedSizeInBytes += serializedVirtualURLSizeInBytes;
  }
  base::UmaHistogramMemoryKB(
      web::kNavigationItemSerializedVirtualURLSizeHistogram,
      serializedVirtualURLSizeInBytes / 1024);

  web::nscoder_util::EncodeString(aCoder, web::kNavigationItemStorageURLKey,
                                  _URL.spec());
  int serializedURLSizeInBytes = _URL.spec().size();
  serializedSizeInBytes += serializedURLSizeInBytes;
  base::UmaHistogramMemoryKB(web::kNavigationItemSerializedURLSizeHistogram,
                             serializedURLSizeInBytes / 1024);

  web::nscoder_util::EncodeString(
      aCoder, web::kNavigationItemStorageReferrerURLKey, _referrer.url.spec());
  int serializedReferrerURLSizeInBytes = _referrer.url.spec().size();
  serializedSizeInBytes += serializedReferrerURLSizeInBytes;
  base::UmaHistogramMemoryKB(
      web::kNavigationItemSerializedReferrerURLSizeHistogram,
      serializedReferrerURLSizeInBytes / 1024);

  [aCoder encodeInt:_referrer.policy
             forKey:web::kNavigationItemStorageReferrerPolicyKey];
  [aCoder encodeInt64:_timestamp.ToInternalValue()
               forKey:web::kNavigationItemStorageTimestampKey];
  // Size of int is negligible, do not log or count towards session size.

  NSString* title = base::SysUTF16ToNSString(_title);
  [aCoder encodeObject:title forKey:web::kNavigationItemStorageTitleKey];
  int serializedTitleSizeInBytes =
      [[NSKeyedArchiver archivedDataWithRootObject:title
                             requiringSecureCoding:NO
                                             error:nullptr] length];
  serializedSizeInBytes += serializedTitleSizeInBytes;
  base::UmaHistogramMemoryKB(web::kNavigationItemSerializedTitleSizeHistogram,
                             serializedTitleSizeInBytes / 1024);

  std::string userAgent = web::GetUserAgentTypeDescription(_userAgentType);
  web::nscoder_util::EncodeString(
      aCoder, web::kNavigationItemStorageUserAgentTypeKey, userAgent);
  serializedSizeInBytes += userAgent.size();
  // No need to log the user agent type size, because it's a set of constants.

  [aCoder encodeObject:_HTTPRequestHeaders
                forKey:web::kNavigationItemStorageHTTPRequestHeadersKey];
  int serializedRequestHeadersSizeInBytes =
      [[NSKeyedArchiver archivedDataWithRootObject:_HTTPRequestHeaders
                             requiringSecureCoding:NO
                                             error:nullptr] length];
  serializedSizeInBytes += serializedRequestHeadersSizeInBytes;
  base::UmaHistogramMemoryKB(
      web::kNavigationItemSerializedRequestHeadersSizeHistogram,
      serializedRequestHeadersSizeInBytes / 1024);

  base::UmaHistogramMemoryKB(web::kNavigationItemSerializedSizeHistogram,
                             serializedSizeInBytes / 1024);
}

#pragma mark - Properties

- (const GURL&)URL {
  return _URL;
}

- (void)setURL:(const GURL&)URL {
  _URL = URL;
}

- (const GURL&)virtualURL {
  // virtualURL is not stored (see -encodeWithCoder:) if it's the same as URL.
  // This logic repeats NavigationItemImpl::GetURL to store virtualURL only when
  // different from URL.
  return _virtualURL.is_empty() ? _URL : _virtualURL;
}

- (void)setVirtualURL:(const GURL&)virtualURL {
  _virtualURL = virtualURL;
}

- (const std::u16string&)title {
  return _title;
}

- (void)setTitle:(const std::u16string&)title {
  _title = title;
}

@end
