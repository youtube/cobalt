/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCodecParameters+Private.h"

#import "RTCMediaStreamTrack.h"
#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"
#include "rtc_base/checks.h"

const NSString *const kRTCRtxCodecName = @(webrtc::kRtxCodecName);
const NSString *const kRTCRedCodecName = @(webrtc::kRedCodecName);
const NSString *const kRTCUlpfecCodecName = @(webrtc::kUlpfecCodecName);
const NSString *const kRTCFlexfecCodecName = @(webrtc::kFlexfecCodecName);
const NSString *const kRTCOpusCodecName = @(webrtc::kOpusCodecName);
const NSString *const kRTCL16CodecName = @(webrtc::kL16CodecName);
const NSString *const kRTCG722CodecName = @(webrtc::kG722CodecName);
const NSString *const kRTCPcmuCodecName = @(webrtc::kPcmuCodecName);
const NSString *const kRTCPcmaCodecName = @(webrtc::kPcmaCodecName);
const NSString *const kRTCDtmfCodecName = @(webrtc::kDtmfCodecName);
const NSString *const kRTCComfortNoiseCodecName =
    @(webrtc::kComfortNoiseCodecName);
const NSString *const kRTCVp8CodecName = @(webrtc::kVp8CodecName);
const NSString *const kRTCVp9CodecName = @(webrtc::kVp9CodecName);
const NSString *const kRTCH264CodecName = @(webrtc::kH264CodecName);

@implementation RTC_OBJC_TYPE (RTCRtpCodecParameters)

@synthesize payloadType = _payloadType;
@synthesize name = _name;
@synthesize kind = _kind;
@synthesize clockRate = _clockRate;
@synthesize numChannels = _numChannels;
@synthesize parameters = _parameters;

- (instancetype)init {
  webrtc::RtpCodecParameters nativeParameters;
  return [self initWithNativeParameters:nativeParameters];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpCodecParameters &)nativeParameters {
  self = [super init];
  if (self) {
    _payloadType = nativeParameters.payload_type;
    _name = [NSString stringForStdString:nativeParameters.name];
    switch (nativeParameters.kind) {
      case webrtc::MediaType::AUDIO:
        _kind = kRTCMediaStreamTrackKindAudio;
        break;
      case webrtc::MediaType::VIDEO:
        _kind = kRTCMediaStreamTrackKindVideo;
        break;
      default:
        RTC_DCHECK_NOTREACHED();
        break;
    }
    if (nativeParameters.clock_rate) {
      _clockRate = [NSNumber numberWithInt:*nativeParameters.clock_rate];
    }
    if (nativeParameters.num_channels) {
      _numChannels = [NSNumber numberWithInt:*nativeParameters.num_channels];
    }
    NSMutableDictionary *parameters = [NSMutableDictionary dictionary];
    for (const auto &parameter : nativeParameters.parameters) {
      [parameters setObject:[NSString stringForStdString:parameter.second]
                     forKey:[NSString stringForStdString:parameter.first]];
    }
    _parameters = parameters;
  }
  return self;
}

- (webrtc::RtpCodecParameters)nativeParameters {
  webrtc::RtpCodecParameters parameters;
  parameters.payload_type = _payloadType;
  parameters.name = [NSString stdStringForString:_name];
  // NSString pointer comparison is safe here since "kind" is readonly and only
  // populated above.
  if (_kind == kRTCMediaStreamTrackKindAudio) {
    parameters.kind = webrtc::MediaType::AUDIO;
  } else if (_kind == kRTCMediaStreamTrackKindVideo) {
    parameters.kind = webrtc::MediaType::VIDEO;
  } else {
    RTC_DCHECK_NOTREACHED();
  }
  if (_clockRate != nil) {
    parameters.clock_rate = std::optional<int>(_clockRate.intValue);
  }
  if (_numChannels != nil) {
    parameters.num_channels = std::optional<int>(_numChannels.intValue);
  }
  for (NSString *paramKey in _parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:_parameters[paramKey]];
    parameters.parameters[key] = value;
  }
  return parameters;
}

@end
