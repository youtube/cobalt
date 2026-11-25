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

#include "test/gtest.h"

#include <vector>

#import "api/peerconnection/RTCCertificate.h"
#import "api/peerconnection/RTCConfiguration+Private.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCDtlsFingerprint.h"
#import "api/peerconnection/RTCIceServer.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "helpers/NSString+StdString.h"

@interface RTCCertificateTest : XCTestCase
@end

@implementation RTCCertificateTest

- (void)testCertificateIsUsedInConfig {
  RTC_OBJC_TYPE(RTCConfiguration) *originalConfig =
      [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];

  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTC_OBJC_TYPE(RTCIceServer) *server =
      [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithURLStrings:urlStrings];
  originalConfig.iceServers = @[ server ];

  // Generate a new certificate.
  RTC_OBJC_TYPE(RTCCertificate) *originalCertificate =
      [RTC_OBJC_TYPE(RTCCertificate) generateCertificateWithParams:@{
        @"expires" : @100000,
        @"name" : @"RSASSA-PKCS1-v1_5"
      }];

  // Store certificate in configuration.
  originalConfig.certificate = originalCertificate;

  RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc]
          initWithMandatoryConstraints:@{}
                   optionalConstraints:nil];
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *factory =
      [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];

  // Create PeerConnection with this certificate.
  RTC_OBJC_TYPE(RTCPeerConnection) *peerConnection =
      [factory peerConnectionWithConfiguration:originalConfig
                                   constraints:contraints
                                      delegate:nil];

  // Retrieve certificate from the configuration.
  RTC_OBJC_TYPE(RTCConfiguration) *retrievedConfig =
      peerConnection.configuration;

  // Extract PEM strings from original certificate.
  std::string originalPrivateKeyField =
      [[originalCertificate private_key] UTF8String];
  std::string originalCertificateField =
      [[originalCertificate certificate] UTF8String];

  // Extract PEM strings from certificate retrieved from configuration.
  RTC_OBJC_TYPE(RTCCertificate) *retrievedCertificate =
      retrievedConfig.certificate;
  std::string retrievedPrivateKeyField =
      [[retrievedCertificate private_key] UTF8String];
  std::string retrievedCertificateField =
      [[retrievedCertificate certificate] UTF8String];

  // Check that the original certificate and retrieved certificate match.
  EXPECT_EQ(originalPrivateKeyField, retrievedPrivateKeyField);
  EXPECT_EQ(retrievedCertificateField, retrievedCertificateField);

  // The fingerprints are derived from the certificate, so they survive the
  // round trip through the configuration.
  NSArray<RTC_OBJC_TYPE(RTCDtlsFingerprint) *> *originalFingerprints =
      [originalCertificate getFingerprints];
  NSArray<RTC_OBJC_TYPE(RTCDtlsFingerprint) *> *retrievedFingerprints =
      [retrievedCertificate getFingerprints];
  XCTAssertEqual(originalFingerprints.count, 1u);
  XCTAssertEqual(retrievedFingerprints.count, 1u);
  XCTAssertEqualObjects(originalFingerprints.firstObject.algorithm,
                        retrievedFingerprints.firstObject.algorithm);
  XCTAssertEqualObjects(originalFingerprints.firstObject.value,
                        retrievedFingerprints.firstObject.value);
}

- (void)testGeneratedCertificateHasFingerprint {
  RTC_OBJC_TYPE(RTCCertificate) *certificate = [RTC_OBJC_TYPE(RTCCertificate)
      generateCertificateWithParams:@{@"name" : @"RSASSA-PKCS1-v1_5"}];

  NSArray<RTC_OBJC_TYPE(RTCDtlsFingerprint) *> *fingerprints =
      [certificate getFingerprints];
  XCTAssertEqual(fingerprints.count, 1u);

  RTC_OBJC_TYPE(RTCDtlsFingerprint) *fingerprint = fingerprints.firstObject;
  // The algorithm is an IANA hash function textual name, e.g. "sha-256".
  NSPredicate *algorithmPredicate =
      [NSPredicate predicateWithFormat:@"SELF MATCHES %@", @"[a-z0-9\\-]+"];
  XCTAssertTrue([algorithmPredicate evaluateWithObject:fingerprint.algorithm],
                @"Unexpected algorithm format: %@",
                fingerprint.algorithm);

  // The value is colon-separated hexadecimal per RFC 4572, e.g. "AB:CD:...".
  NSPredicate *valuePredicate = [NSPredicate
      predicateWithFormat:@"SELF MATCHES %@", @"([0-9A-F]{2}:)+[0-9A-F]{2}"];
  XCTAssertTrue([valuePredicate evaluateWithObject:fingerprint.value],
                @"Unexpected fingerprint value format: %@",
                fingerprint.value);
}

- (void)testCertificateFromInvalidPemHasNoFingerprint {
  // A certificate built from PEM strings that are not valid certificates has
  // no computable fingerprint.
  RTC_OBJC_TYPE(RTCCertificate) *certificate =
      [[RTC_OBJC_TYPE(RTCCertificate) alloc] initWithPrivateKey:@"private"
                                                    certificate:@"certificate"];
  XCTAssertEqual([certificate getFingerprints].count, 0u);
}

@end
