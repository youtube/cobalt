/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCCallbackLogger.h"

#import "helpers/NSString+StdString.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/log_sinks.h"
#include "rtc_base/logging.h"

namespace {

class CallbackLogSink final : public rtc::LogSink {
 public:
  CallbackLogSink(RTCCallbackLoggerMessageHandler callbackHandler)
      : callback_handler_(callbackHandler) {}

  void OnLogMessage(const std::string& message) override {
    OnLogMessage(absl::string_view(message));
  }

  void OnLogMessage(absl::string_view message) override {
    if (callback_handler_) {
      callback_handler_([NSString stringForAbslStringView:message]);
    }
  }

 private:
  RTCCallbackLoggerMessageHandler callback_handler_;
};

class CallbackWithSeverityLogSink final : public rtc::LogSink {
 public:
  CallbackWithSeverityLogSink(RTCCallbackLoggerMessageAndSeverityHandler callbackHandler)
      : callback_handler_(callbackHandler) {}

  void OnLogMessage(const std::string& message) override { RTC_DCHECK_NOTREACHED(); }

  void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity) override {
    OnLogMessage(absl::string_view(message), severity);
  }

  void OnLogMessage(absl::string_view message, rtc::LoggingSeverity severity) override {
    if (callback_handler_) {
      RTCLoggingSeverity loggingSeverity = NativeSeverityToObjcSeverity(severity);
      callback_handler_([NSString stringForAbslStringView:message], loggingSeverity);
    }
  }

 private:
  static RTCLoggingSeverity NativeSeverityToObjcSeverity(rtc::LoggingSeverity severity) {
    switch (severity) {
      case rtc::LS_VERBOSE:
        return RTCLoggingSeverityVerbose;
      case rtc::LS_INFO:
        return RTCLoggingSeverityInfo;
      case rtc::LS_WARNING:
        return RTCLoggingSeverityWarning;
      case rtc::LS_ERROR:
        return RTCLoggingSeverityError;
      case rtc::LS_NONE:
        return RTCLoggingSeverityNone;
    }
  }

  RTCCallbackLoggerMessageAndSeverityHandler callback_handler_;
};

}

@implementation RTC_OBJC_TYPE (RTCCallbackLogger) {
  BOOL _hasStarted;
  std::unique_ptr<rtc::LogSink> _logSink;
}

@synthesize severity = _severity;

- (instancetype)init {
  self = [super init];
  if (self != nil) {
    _severity = RTCLoggingSeverityInfo;
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)start:(nullable RTCCallbackLoggerMessageHandler)handler {
  if (_hasStarted) {
    return;
  }

  _logSink.reset(new CallbackLogSink(handler));

  rtc::LogMessage::AddLogToStream(_logSink.get(), [self rtcSeverity]);
  _hasStarted = YES;
}

- (void)startWithMessageAndSeverityHandler:
        (nullable RTCCallbackLoggerMessageAndSeverityHandler)handler {
  if (_hasStarted) {
    return;
  }

  _logSink.reset(new CallbackWithSeverityLogSink(handler));

  rtc::LogMessage::AddLogToStream(_logSink.get(), [self rtcSeverity]);
  _hasStarted = YES;
}

- (void)stop {
  if (!_hasStarted) {
    return;
  }
  RTC_DCHECK(_logSink);
  rtc::LogMessage::RemoveLogToStream(_logSink.get());
  _hasStarted = NO;
  _logSink.reset();
}

#pragma mark - Private

- (rtc::LoggingSeverity)rtcSeverity {
  switch (_severity) {
    case RTCLoggingSeverityVerbose:
      return rtc::LS_VERBOSE;
    case RTCLoggingSeverityInfo:
      return rtc::LS_INFO;
    case RTCLoggingSeverityWarning:
      return rtc::LS_WARNING;
    case RTCLoggingSeverityError:
      return rtc::LS_ERROR;
    case RTCLoggingSeverityNone:
      return rtc::LS_NONE;
  }
}

@end
