/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCCameraVideoCapturer.h"
#import "base/RTCLogging.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#if TARGET_OS_IPHONE
#import "helpers/UIDevice+RTCDevice.h"
#endif

#import "helpers/AVCaptureSession+DevicePosition.h"
#import "helpers/RTCDispatcher+Private.h"
#include "rtc_base/system/gcd_helpers.h"

const int64_t kNanosecondsPerSecond = 1000000000;

@interface RTC_OBJC_TYPE (RTCCameraVideoCapturer)
()<AVCaptureVideoDataOutputSampleBufferDelegate> @property(nonatomic,
                                                           readonly) dispatch_queue_t frameQueue;
@property(nonatomic, strong) AVCaptureDevice *currentDevice;
@property(nonatomic, assign) BOOL hasRetriedOnFatalError;
@property(nonatomic, assign) BOOL isRunning;
// Will the session be running once all asynchronous operations have been completed?
@property(nonatomic, assign) BOOL willBeRunning;
@end

@implementation RTC_OBJC_TYPE (RTCCameraVideoCapturer) {
  AVCaptureVideoDataOutput *_videoDataOutput;
  AVCaptureSession *_captureSession;
  FourCharCode _preferredOutputPixelFormat;
  FourCharCode _outputPixelFormat;
  RTCVideoRotation _rotation;
#if TARGET_OS_IPHONE
  UIDeviceOrientation _orientation;
  BOOL _generatingOrientationNotifications;
#endif
}

@synthesize frameQueue = _frameQueue;
@synthesize captureSession = _captureSession;
@synthesize currentDevice = _currentDevice;
@synthesize hasRetriedOnFatalError = _hasRetriedOnFatalError;
@synthesize isRunning = _isRunning;
@synthesize willBeRunning = _willBeRunning;

- (instancetype)init {
  return [self initWithDelegate:nil captureSession:[[AVCaptureSession alloc] init]];
}

- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate {
  return [self initWithDelegate:delegate captureSession:[[AVCaptureSession alloc] init]];
}

// This initializer is used for testing.
- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate
                  captureSession:(AVCaptureSession *)captureSession {
  if (self = [super initWithDelegate:delegate]) {
    // Create the capture session and all relevant inputs and outputs. We need
    // to do this in init because the application may want the capture session
    // before we start the capturer for e.g. AVCapturePreviewLayer. All objects
    // created here are retained until dealloc and never recreated.
    if (![self setupCaptureSession:captureSession]) {
      return nil;
    }
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if TARGET_OS_IPHONE
    _orientation = UIDeviceOrientationPortrait;
    _rotation = RTCVideoRotation_90;
    [center addObserver:self
               selector:@selector(deviceOrientationDidChange:)
                   name:UIDeviceOrientationDidChangeNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(handleCaptureSessionInterruption:)
                   name:AVCaptureSessionWasInterruptedNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleCaptureSessionInterruptionEnded:)
                   name:AVCaptureSessionInterruptionEndedNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleApplicationDidBecomeActive:)
                   name:UIApplicationDidBecomeActiveNotification
                 object:[UIApplication sharedApplication]];
#endif
    [center addObserver:self
               selector:@selector(handleCaptureSessionRuntimeError:)
                   name:AVCaptureSessionRuntimeErrorNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleCaptureSessionDidStartRunning:)
                   name:AVCaptureSessionDidStartRunningNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleCaptureSessionDidStopRunning:)
                   name:AVCaptureSessionDidStopRunningNotification
                 object:_captureSession];
  }
  return self;
}

- (void)dealloc {
  NSAssert(!_willBeRunning,
           @"Session was still running in RTC_OBJC_TYPE(RTCCameraVideoCapturer) dealloc. Forgot to "
           @"call stopCapture?");
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

+ (NSArray<AVCaptureDevice *> *)captureDevices {
  AVCaptureDeviceDiscoverySession *session = [AVCaptureDeviceDiscoverySession
      discoverySessionWithDeviceTypes:@[ AVCaptureDeviceTypeBuiltInWideAngleCamera ]
                            mediaType:AVMediaTypeVideo
                             position:AVCaptureDevicePositionUnspecified];
  return session.devices;
}

+ (NSArray<AVCaptureDeviceFormat *> *)supportedFormatsForDevice:(AVCaptureDevice *)device {
  // Support opening the device in any format. We make sure it's converted to a format we
  // can handle, if needed, in the method `-setupVideoDataOutput`.
  return device.formats;
}

- (FourCharCode)preferredOutputPixelFormat {
  return _preferredOutputPixelFormat;
}

- (void)startCaptureWithDevice:(AVCaptureDevice *)device
                        format:(AVCaptureDeviceFormat *)format
                           fps:(NSInteger)fps {
  [self startCaptureWithDevice:device format:format fps:fps completionHandler:nil];
}

- (void)stopCapture {
  [self stopCaptureWithCompletionHandler:nil];
}

- (void)startCaptureWithDevice:(AVCaptureDevice *)device
                        format:(AVCaptureDeviceFormat *)format
                           fps:(NSInteger)fps
             completionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler {
  _willBeRunning = YES;
  [RTC_OBJC_TYPE(RTCDispatcher)
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                      RTCLogInfo("startCaptureWithDevice %@ @ %ld fps", format, (long)fps);

#if TARGET_OS_IPHONE
                      dispatch_async(dispatch_get_main_queue(), ^{
                        if (!self->_generatingOrientationNotifications) {
                          [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
                          self->_generatingOrientationNotifications = YES;
                        }
                      });
#endif

                      self.currentDevice = device;

                      NSError *error = nil;
                      if (![self.currentDevice lockForConfiguration:&error]) {
                        RTCLogError(@"Failed to lock device %@. Error: %@",
                                    self.currentDevice,
                                    error.userInfo);
                        if (completionHandler) {
                          completionHandler(error);
                        }
                        self.willBeRunning = NO;
                        return;
                      }
                      [self reconfigureCaptureSessionInput];
                      [self updateOrientation];
                      [self updateDeviceCaptureFormat:format fps:fps];
                      [self updateVideoDataOutputPixelFormat:format];
                      [self.captureSession startRunning];
                      [self.currentDevice unlockForConfiguration];
                      self.isRunning = YES;
                      if (completionHandler) {
                        completionHandler(nil);
                      }
                    }];
}

- (void)stopCaptureWithCompletionHandler:(nullable void (^)(void))completionHandler {
  _willBeRunning = NO;
  [RTC_OBJC_TYPE(RTCDispatcher)
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                      RTCLogInfo("Stop");
                      self.currentDevice = nil;
                      for (AVCaptureDeviceInput *oldInput in [self.captureSession.inputs copy]) {
                        [self.captureSession removeInput:oldInput];
                      }
                      [self.captureSession stopRunning];

#if TARGET_OS_IPHONE
                      dispatch_async(dispatch_get_main_queue(), ^{
                        if (self->_generatingOrientationNotifications) {
                          [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
                          self->_generatingOrientationNotifications = NO;
                        }
                      });
#endif
                      self.isRunning = NO;
                      if (completionHandler) {
                        completionHandler();
                      }
                    }];
}

#pragma mark iOS notifications

#if TARGET_OS_IPHONE
- (void)deviceOrientationDidChange:(NSNotification *)notification {
  [RTC_OBJC_TYPE(RTCDispatcher) dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                              block:^{
                                                [self updateOrientation];
                                              }];
}
#endif

#pragma mark AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
  NSParameterAssert(captureOutput == _videoDataOutput);

  if (CMSampleBufferGetNumSamples(sampleBuffer) != 1 || !CMSampleBufferIsValid(sampleBuffer) ||
      !CMSampleBufferDataIsReady(sampleBuffer)) {
    return;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (pixelBuffer == nil) {
    return;
  }

#if TARGET_OS_IPHONE
  // Default to portrait orientation on iPhone.
  BOOL usingFrontCamera = NO;
  // Check the image's EXIF for the camera the image came from as the image could have been
  // delayed as we set alwaysDiscardsLateVideoFrames to NO.
  AVCaptureDevicePosition cameraPosition =
      [AVCaptureSession devicePositionForSampleBuffer:sampleBuffer];
  if (cameraPosition != AVCaptureDevicePositionUnspecified) {
    usingFrontCamera = AVCaptureDevicePositionFront == cameraPosition;
  } else {
    AVCaptureDeviceInput *deviceInput =
        (AVCaptureDeviceInput *)((AVCaptureInputPort *)connection.inputPorts.firstObject).input;
    usingFrontCamera = AVCaptureDevicePositionFront == deviceInput.device.position;
  }
  switch (_orientation) {
    case UIDeviceOrientationPortrait:
      _rotation = RTCVideoRotation_90;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      _rotation = RTCVideoRotation_270;
      break;
    case UIDeviceOrientationLandscapeLeft:
      _rotation = usingFrontCamera ? RTCVideoRotation_180 : RTCVideoRotation_0;
      break;
    case UIDeviceOrientationLandscapeRight:
      _rotation = usingFrontCamera ? RTCVideoRotation_0 : RTCVideoRotation_180;
      break;
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
    case UIDeviceOrientationUnknown:
      // Ignore.
      break;
  }
#else
  // No rotation on Mac.
  _rotation = RTCVideoRotation_0;
#endif

  RTC_OBJC_TYPE(RTCCVPixelBuffer) *rtcPixelBuffer =
      [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc] initWithPixelBuffer:pixelBuffer];
  int64_t timeStampNs = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) *
      kNanosecondsPerSecond;
  RTC_OBJC_TYPE(RTCVideoFrame) *videoFrame =
      [[RTC_OBJC_TYPE(RTCVideoFrame) alloc] initWithBuffer:rtcPixelBuffer
                                                  rotation:_rotation
                                               timeStampNs:timeStampNs];
  [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
    didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection {
#if TARGET_OS_IPHONE
  CFStringRef droppedReason =
      CMGetAttachment(sampleBuffer, kCMSampleBufferAttachmentKey_DroppedFrameReason, nil);
#else
  // DroppedFrameReason unavailable on macOS.
  CFStringRef droppedReason = nil;
#endif
  RTCLogError(@"Dropped sample buffer. Reason: %@", (__bridge NSString *)droppedReason);
}

#pragma mark - AVCaptureSession notifications

- (void)handleCaptureSessionInterruption:(NSNotification *)notification {
  NSString *reasonString = nil;
#if TARGET_OS_IPHONE
  NSNumber *reason = notification.userInfo[AVCaptureSessionInterruptionReasonKey];
  if (reason) {
    switch (reason.intValue) {
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground:
        reasonString = @"VideoDeviceNotAvailableInBackground";
        break;
      case AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient:
        reasonString = @"AudioDeviceInUseByAnotherClient";
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient:
        reasonString = @"VideoDeviceInUseByAnotherClient";
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps:
        reasonString = @"VideoDeviceNotAvailableWithMultipleForegroundApps";
        break;
    }
  }
#endif
  RTCLog(@"Capture session interrupted: %@", reasonString);
}

- (void)handleCaptureSessionInterruptionEnded:(NSNotification *)notification {
  RTCLog(@"Capture session interruption ended.");
}

- (void)handleCaptureSessionRuntimeError:(NSNotification *)notification {
  NSError *error = [notification.userInfo objectForKey:AVCaptureSessionErrorKey];
  RTCLogError(@"Capture session runtime error: %@", error);

  [RTC_OBJC_TYPE(RTCDispatcher) dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                              block:^{
#if TARGET_OS_IPHONE
                                                if (error.code == AVErrorMediaServicesWereReset) {
                                                  [self handleNonFatalError];
                                                } else {
                                                  [self handleFatalError];
                                                }
#else
        [self handleFatalError];
#endif
                                              }];
}

- (void)handleCaptureSessionDidStartRunning:(NSNotification *)notification {
  RTCLog(@"Capture session started.");

  [RTC_OBJC_TYPE(RTCDispatcher) dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                              block:^{
                                                // If we successfully restarted after an unknown
                                                // error, allow future retries on fatal errors.
                                                self.hasRetriedOnFatalError = NO;
                                              }];
}

- (void)handleCaptureSessionDidStopRunning:(NSNotification *)notification {
  RTCLog(@"Capture session stopped.");
}

- (void)handleFatalError {
  [RTC_OBJC_TYPE(RTCDispatcher)
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                      if (!self.hasRetriedOnFatalError) {
                        RTCLogWarning(@"Attempting to recover from fatal capture error.");
                        [self handleNonFatalError];
                        self.hasRetriedOnFatalError = YES;
                      } else {
                        RTCLogError(@"Previous fatal error recovery failed.");
                      }
                    }];
}

- (void)handleNonFatalError {
  [RTC_OBJC_TYPE(RTCDispatcher) dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                                              block:^{
                                                RTCLog(@"Restarting capture session after error.");
                                                if (self.isRunning) {
                                                  [self.captureSession startRunning];
                                                }
                                              }];
}

#if TARGET_OS_IPHONE

#pragma mark - UIApplication notifications

- (void)handleApplicationDidBecomeActive:(NSNotification *)notification {
  [RTC_OBJC_TYPE(RTCDispatcher)
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                      if (self.isRunning && !self.captureSession.isRunning) {
                        RTCLog(@"Restarting capture session on active.");
                        [self.captureSession startRunning];
                      }
                    }];
}

#endif  // TARGET_OS_IPHONE

#pragma mark - Private

- (dispatch_queue_t)frameQueue {
  if (!_frameQueue) {
    _frameQueue = RTCDispatchQueueCreateWithTarget(
        "org.webrtc.cameravideocapturer.video",
        DISPATCH_QUEUE_SERIAL,
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
  }
  return _frameQueue;
}

- (BOOL)setupCaptureSession:(AVCaptureSession *)captureSession {
  NSAssert(_captureSession == nil, @"Setup capture session called twice.");
  _captureSession = captureSession;
#if defined(WEBRTC_IOS)
  _captureSession.sessionPreset = AVCaptureSessionPresetInputPriority;
  _captureSession.usesApplicationAudioSession = NO;
#endif
  [self setupVideoDataOutput];
  // Add the output.
  if (![_captureSession canAddOutput:_videoDataOutput]) {
    RTCLogError(@"Video data output unsupported.");
    return NO;
  }
  [_captureSession addOutput:_videoDataOutput];

  return YES;
}

- (void)setupVideoDataOutput {
  NSAssert(_videoDataOutput == nil, @"Setup video data output called twice.");
  AVCaptureVideoDataOutput *videoDataOutput = [[AVCaptureVideoDataOutput alloc] init];

  // `videoDataOutput.availableVideoCVPixelFormatTypes` returns the pixel formats supported by the
  // device with the most efficient output format first. Find the first format that we support.
  NSSet<NSNumber *> *supportedPixelFormats =
      [RTC_OBJC_TYPE(RTCCVPixelBuffer) supportedPixelFormats];
  NSMutableOrderedSet *availablePixelFormats =
      [NSMutableOrderedSet orderedSetWithArray:videoDataOutput.availableVideoCVPixelFormatTypes];
  [availablePixelFormats intersectSet:supportedPixelFormats];
  NSNumber *pixelFormat = availablePixelFormats.firstObject;
  NSAssert(pixelFormat, @"Output device has no supported formats.");

  _preferredOutputPixelFormat = [pixelFormat unsignedIntValue];
  _outputPixelFormat = _preferredOutputPixelFormat;
  videoDataOutput.videoSettings = @{(NSString *)kCVPixelBufferPixelFormatTypeKey : pixelFormat};
  videoDataOutput.alwaysDiscardsLateVideoFrames = NO;
  [videoDataOutput setSampleBufferDelegate:self queue:self.frameQueue];
  _videoDataOutput = videoDataOutput;
}

- (void)updateVideoDataOutputPixelFormat:(AVCaptureDeviceFormat *)format {
  FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(format.formatDescription);
  if (![[RTC_OBJC_TYPE(RTCCVPixelBuffer) supportedPixelFormats] containsObject:@(mediaSubType)]) {
    mediaSubType = _preferredOutputPixelFormat;
  }

  if (mediaSubType != _outputPixelFormat) {
    _outputPixelFormat = mediaSubType;
  }

  // Update videoSettings with dimensions, as some virtual cameras, e.g. Snap Camera, may not work
  // otherwise.
  CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
  _videoDataOutput.videoSettings = @{
    (id)kCVPixelBufferWidthKey : @(dimensions.width),
    (id)kCVPixelBufferHeightKey : @(dimensions.height),
    (id)kCVPixelBufferPixelFormatTypeKey : @(_outputPixelFormat),
  };
}

#pragma mark - Private, called inside capture queue

- (void)updateDeviceCaptureFormat:(AVCaptureDeviceFormat *)format fps:(NSInteger)fps {
  NSAssert([RTC_OBJC_TYPE(RTCDispatcher) isOnQueueForType:RTCDispatcherTypeCaptureSession],
           @"updateDeviceCaptureFormat must be called on the capture queue.");
  @try {
    _currentDevice.activeFormat = format;
    _currentDevice.activeVideoMinFrameDuration = CMTimeMake(1, fps);
  } @catch (NSException *exception) {
    RTCLogError(@"Failed to set active format!\n User info:%@", exception.userInfo);
    return;
  }
}

- (void)reconfigureCaptureSessionInput {
  NSAssert([RTC_OBJC_TYPE(RTCDispatcher) isOnQueueForType:RTCDispatcherTypeCaptureSession],
           @"reconfigureCaptureSessionInput must be called on the capture queue.");
  NSError *error = nil;
  AVCaptureDeviceInput *input =
      [AVCaptureDeviceInput deviceInputWithDevice:_currentDevice error:&error];
  if (!input) {
    RTCLogError(@"Failed to create front camera input: %@", error.localizedDescription);
    return;
  }
  [_captureSession beginConfiguration];
  for (AVCaptureDeviceInput *oldInput in [_captureSession.inputs copy]) {
    [_captureSession removeInput:oldInput];
  }
  if ([_captureSession canAddInput:input]) {
    [_captureSession addInput:input];
  } else {
    RTCLogError(@"Cannot add camera as an input to the session.");
  }
  [_captureSession commitConfiguration];
}

- (void)updateOrientation {
  NSAssert([RTC_OBJC_TYPE(RTCDispatcher) isOnQueueForType:RTCDispatcherTypeCaptureSession],
           @"updateOrientation must be called on the capture queue.");
#if TARGET_OS_IPHONE
  _orientation = [UIDevice currentDevice].orientation;
#endif
}

@end
