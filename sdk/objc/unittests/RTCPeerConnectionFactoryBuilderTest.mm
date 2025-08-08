/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#ifdef __cplusplus
extern "C" {
#endif
#import <OCMock/OCMock.h>
#ifdef __cplusplus
}
#endif
#import "api/peerconnection/RTCPeerConnectionFactory+Native.h"
#import "api/peerconnection/RTCPeerConnectionFactoryBuilder+DefaultComponents.h"
#import "api/peerconnection/RTCPeerConnectionFactoryBuilder.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

#include "rtc_base/gunit.h"
#include "rtc_base/system/unused.h"

@interface RTCPeerConnectionFactoryBuilderTests : XCTestCase
@end

@implementation RTCPeerConnectionFactoryBuilderTests

- (void)testBuilder {
  id factoryMock = OCMStrictClassMock([RTC_OBJC_TYPE(RTCPeerConnectionFactory) class]);
  OCMExpect([factoryMock alloc]).andReturn(factoryMock);
  RTC_UNUSED([[[[factoryMock expect] andReturn:factoryMock] ignoringNonObjectArgs]
      initWithNativeAudioEncoderFactory:nullptr
              nativeAudioDecoderFactory:nullptr
              nativeVideoEncoderFactory:nullptr
              nativeVideoDecoderFactory:nullptr
                      audioDeviceModule:nullptr
                  audioProcessingModule:nullptr]);
  RTCPeerConnectionFactoryBuilder* builder = [[RTCPeerConnectionFactoryBuilder alloc] init];
  RTC_OBJC_TYPE(RTCPeerConnectionFactory)* peerConnectionFactory =
      [builder createPeerConnectionFactory];
  EXPECT_TRUE(peerConnectionFactory != nil);
  OCMVerifyAll(factoryMock);
}

- (void)testDefaultComponentsBuilder {
  id factoryMock = OCMStrictClassMock([RTC_OBJC_TYPE(RTCPeerConnectionFactory) class]);
  OCMExpect([factoryMock alloc]).andReturn(factoryMock);
  RTC_UNUSED([[[[factoryMock expect] andReturn:factoryMock] ignoringNonObjectArgs]
      initWithNativeAudioEncoderFactory:nullptr
              nativeAudioDecoderFactory:nullptr
              nativeVideoEncoderFactory:nullptr
              nativeVideoDecoderFactory:nullptr
                      audioDeviceModule:nullptr
                  audioProcessingModule:nullptr]);
  RTCPeerConnectionFactoryBuilder* builder = [RTCPeerConnectionFactoryBuilder defaultBuilder];
  RTC_OBJC_TYPE(RTCPeerConnectionFactory)* peerConnectionFactory =
      [builder createPeerConnectionFactory];
  EXPECT_TRUE(peerConnectionFactory != nil);
  OCMVerifyAll(factoryMock);
}
@end
