/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCCertificate.h"

#import "RTCDtlsFingerprint.h"
#import "base/RTCLogging.h"

#include "rtc_base/rtc_certificate.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"

@implementation RTC_OBJC_TYPE (RTCCertificate)

@synthesize private_key = _private_key;
@synthesize certificate = _certificate;

- (id)copyWithZone:(NSZone *)zone {
  id copy = [[[self class] alloc]
      initWithPrivateKey:[self.private_key copyWithZone:zone]
             certificate:[self.certificate copyWithZone:zone]];
  return copy;
}

- (instancetype)initWithPrivateKey:(NSString *)private_key
                       certificate:(NSString *)certificate {
  self = [super init];
  if (self) {
    _private_key = [private_key copy];
    _certificate = [certificate copy];
  }
  return self;
}

- (NSArray<RTC_OBJC_TYPE(RTCDtlsFingerprint) *> *)getFingerprints {
  // The fingerprints are not stored on the PEM representation, so the native
  // certificate is reconstructed from the PEM strings in order to compute them.
  // Native WebRTC tracks a single fingerprint per certificate (the one matching
  // the signature algorithm), so the returned array holds at most one entry.
  webrtc::RTCCertificatePEM pem(_private_key.UTF8String,
                                _certificate.UTF8String);
  webrtc::scoped_refptr<webrtc::RTCCertificate> certificate =
      webrtc::RTCCertificate::FromPEM(pem);
  if (!certificate) {
    RTCLogError(@"Failed to reconstruct certificate from PEM.");
    return @[];
  }
  std::unique_ptr<webrtc::SSLFingerprint> fingerprint =
      webrtc::SSLFingerprint::CreateFromCertificate(*certificate);
  if (!fingerprint) {
    RTCLogError(@"Failed to compute fingerprint for certificate.");
    return @[];
  }
  const std::string &algorithm = fingerprint->algorithm;
  std::string value = fingerprint->GetRfc4572Fingerprint();
  return @[ [[RTC_OBJC_TYPE(RTCDtlsFingerprint) alloc]
      initWithAlgorithm:[[NSString alloc] initWithBytes:algorithm.data()
                                                 length:algorithm.length()
                                               encoding:NSUTF8StringEncoding]
                  value:[[NSString alloc]
                            initWithBytes:value.data()
                                   length:value.length()
                                 encoding:NSUTF8StringEncoding]] ];
}

+ (nullable RTC_OBJC_TYPE(RTCCertificate) *)generateCertificateWithParams:
    (NSDictionary *)params {
  webrtc::KeyType keyType = webrtc::KT_ECDSA;
  NSString *keyTypeString = [params valueForKey:@"name"];
  if (keyTypeString && [keyTypeString isEqualToString:@"RSASSA-PKCS1-v1_5"]) {
    keyType = webrtc::KT_RSA;
  }

  NSNumber *expires = [params valueForKey:@"expires"];
  webrtc::scoped_refptr<webrtc::RTCCertificate> cc_certificate = nullptr;
  if (expires != nil) {
    uint64_t expirationTimestamp = [expires unsignedLongLongValue];
    cc_certificate = webrtc::RTCCertificateGenerator::GenerateCertificate(
        webrtc::KeyParams(keyType), expirationTimestamp);
  } else {
    cc_certificate = webrtc::RTCCertificateGenerator::GenerateCertificate(
        webrtc::KeyParams(keyType), std::nullopt);
  }
  if (!cc_certificate) {
    RTCLogError(@"Failed to generate certificate.");
    return nullptr;
  }
  // grab PEMs and create an NS RTCCerticicate
  webrtc::RTCCertificatePEM pem = cc_certificate->ToPEM();
  std::string pem_private_key = pem.private_key();
  std::string pem_certificate = pem.certificate();

  RTC_OBJC_TYPE(RTCCertificate) *cert = [[RTC_OBJC_TYPE(RTCCertificate) alloc]
      initWithPrivateKey:[[NSString alloc]
                             initWithBytes:pem_private_key.data()
                                    length:pem_private_key.length()
                                  encoding:NSUTF8StringEncoding]
             certificate:[[NSString alloc]
                             initWithBytes:pem_certificate.data()
                                    length:pem_certificate.length()
                                  encoding:NSUTF8StringEncoding]];
  return cert;
}

@end
