// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios_bridge.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"
#import "net/http/http_request_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate::
    GaiaAuthFetcherIOSBridgeDelegate() {}

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate::
    ~GaiaAuthFetcherIOSBridgeDelegate() {}

#pragma mark - GaiaAuthFetcherIOSBridge

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridge(
    GaiaAuthFetcherIOSBridgeDelegate* delegate)
    : delegate_(delegate) {}

GaiaAuthFetcherIOSBridge::~GaiaAuthFetcherIOSBridge() {}
