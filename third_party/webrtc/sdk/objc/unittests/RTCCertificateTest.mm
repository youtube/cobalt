/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <vector>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCConfiguration+Private.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCIceServer.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "helpers/NSString+StdString.h"

@interface RTCCertificateTest : XCTestCase
@end

@implementation RTCCertificateTest

- (void)testCertificateIsUsedInConfig {
  RTC_OBJC_TYPE(RTCConfiguration) *originalConfig = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];

  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTC_OBJC_TYPE(RTCIceServer) *server =
      [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithURLStrings:urlStrings];
  originalConfig.iceServers = @[ server ];

  // Generate a new certificate.
  RTC_OBJC_TYPE(RTCCertificate) *originalCertificate = [RTC_OBJC_TYPE(RTCCertificate)
      generateCertificateWithParams:@{@"expires" : @100000, @"name" : @"RSASSA-PKCS1-v1_5"}];

  // Store certificate in configuration.
  originalConfig.certificate = originalCertificate;

  RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                           optionalConstraints:nil];
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *factory =
      [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];

  // Create PeerConnection with this certificate.
  RTC_OBJC_TYPE(RTCPeerConnection) *peerConnection =
      [factory peerConnectionWithConfiguration:originalConfig constraints:contraints delegate:nil];

  // Retrieve certificate from the configuration.
  RTC_OBJC_TYPE(RTCConfiguration) *retrievedConfig = peerConnection.configuration;

  // Extract PEM strings from original certificate.
  std::string originalPrivateKeyField = [[originalCertificate private_key] UTF8String];
  std::string originalCertificateField = [[originalCertificate certificate] UTF8String];

  // Extract PEM strings from certificate retrieved from configuration.
  RTC_OBJC_TYPE(RTCCertificate) *retrievedCertificate = retrievedConfig.certificate;
  std::string retrievedPrivateKeyField = [[retrievedCertificate private_key] UTF8String];
  std::string retrievedCertificateField = [[retrievedCertificate certificate] UTF8String];

  // Check that the original certificate and retrieved certificate match.
  EXPECT_EQ(originalPrivateKeyField, retrievedPrivateKeyField);
  EXPECT_EQ(retrievedCertificateField, retrievedCertificateField);
}

@end
