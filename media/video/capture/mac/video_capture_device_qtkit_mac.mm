// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

#import <QTKit/QTKit.h>

#include "base/logging.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

@implementation VideoCaptureDeviceQTKit

#pragma mark Class methods

+ (NSDictionary *)deviceNames {
  NSArray *captureDevices =
      [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
  NSMutableDictionary *deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];

  for (QTCaptureDevice* device in captureDevices) {
    NSString* qtDeviceName = [device localizedDisplayName];
    NSString* qtUniqueId = [device uniqueID];
    [deviceNames setObject:qtDeviceName forKey:qtUniqueId];
  }
  return deviceNames;
}

#pragma mark Public methods

- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac *)frameReceiver {
  self = [super init];
  if (self) {
    frameReceiver_ = frameReceiver;
  }
  return self;
}

- (void)dealloc {
  [captureSession_ release];
  [captureDeviceInput_ release];
  [super dealloc];
}

- (BOOL)setCaptureDevice:(NSString *)deviceId {
  if (deviceId) {
    // Set the capture device.
    if (captureDeviceInput_) {
      DLOG(ERROR) << "Video capture device already set.";
      return NO;
    }

    NSArray *captureDevices =
        [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
    NSArray *captureDevicesNames =
        [captureDevices valueForKey:@"uniqueID"];
    NSUInteger index = [captureDevicesNames indexOfObject:deviceId];
    if (index == NSNotFound) {
      DLOG(ERROR) << "Video capture device not found.";
      return NO;
    }
    QTCaptureDevice *device = [captureDevices objectAtIndex:index];
    NSError *error;
    if (![device open:&error]) {
      DLOG(ERROR) << "Could not open video capture device."
                  << [[error localizedDescription] UTF8String];
      return NO;
    }
    captureDeviceInput_ = [[QTCaptureDeviceInput alloc] initWithDevice:device];
    captureSession_ = [[QTCaptureSession alloc] init];

    QTCaptureDecompressedVideoOutput *captureDecompressedOutput =
        [[[QTCaptureDecompressedVideoOutput alloc] init] autorelease];
    [captureDecompressedOutput setDelegate:self];
    if (![captureSession_ addOutput:captureDecompressedOutput error:&error]) {
      DLOG(ERROR) << "Could not connect video capture output."
                  << [[error localizedDescription] UTF8String];
      return NO;
    }
    return YES;
  } else {
    // Remove the previously set capture device.
    if (!captureDeviceInput_) {
      DLOG(ERROR) << "No video capture device set.";
      return YES;
    }
    if ([[captureSession_ inputs] count] > 0) {
      // The device is still running.
      [self stopCapture];
    }
    [captureSession_ release];
    captureSession_ = nil;
    [captureDeviceInput_ release];
    captureDeviceInput_ = nil;
    return YES;
  }
}

- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate {
  if (!captureDeviceInput_) {
    DLOG(ERROR) << "No video capture device set.";
    return NO;
  }
  if ([[captureSession_ outputs] count] != 1) {
    DLOG(ERROR) << "Video capture capabilities already set.";
    return NO;
  }

  frameWidth_ = width;
  frameHeight_ = height;
  frameRate_ = frameRate;

  // Set up desired output properties.
  NSDictionary *captureDictionary =
      [NSDictionary dictionaryWithObjectsAndKeys:
          [NSNumber numberWithDouble:frameWidth_],
          (id)kCVPixelBufferWidthKey,
          [NSNumber numberWithDouble:frameHeight_],
          (id)kCVPixelBufferHeightKey,
          [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA],
          (id)kCVPixelBufferPixelFormatTypeKey,
          nil];
  [[[captureSession_ outputs] objectAtIndex:0]
      setPixelBufferAttributes:captureDictionary];
  return YES;
}

- (BOOL)startCapture {
  if ([[captureSession_ outputs] count] == 0) {
    // Capture properties not set.
    DLOG(ERROR) << "Video capture device not initialized.";
    return NO;
  }
  if ([[captureSession_ inputs] count] == 0) {
    NSError *error;
    if (![captureSession_ addInput:captureDeviceInput_ error:&error]) {
      DLOG(ERROR) << "Could not connect video capture device."
                  << [[error localizedDescription] UTF8String];
      return NO;
    }
    [captureSession_ startRunning];
  }
  return YES;
}

- (void)stopCapture {
  if ([[captureSession_ inputs] count] == 1) {
    [captureSession_ removeInput:captureDeviceInput_];
    [captureSession_ stopRunning];
  }
}

// |captureOutput| is called by the capture device to deliver a new frame.
- (void)captureOutput:(QTCaptureOutput *)captureOutput
  didOutputVideoFrame:(CVImageBufferRef)videoFrame
     withSampleBuffer:(QTSampleBuffer *)sampleBuffer
       fromConnection:(QTCaptureConnection *)connection {
  if(!frameReceiver_) {
    return;
  }

  // Lock the frame and calculate frame size.
  const int kLockFlags = 0;
  if (CVPixelBufferLockBaseAddress(videoFrame, kLockFlags)
      == kCVReturnSuccess) {
    void *baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
    int frameHeight = CVPixelBufferGetHeight(videoFrame);
    int frameSize = bytesPerRow * frameHeight;
    media::VideoCaptureCapability captureCapability;
    captureCapability.width = frameWidth_;
    captureCapability.height = frameHeight_;
    captureCapability.frame_rate = frameRate_;
    captureCapability.color = media::VideoCaptureCapability::kARGB;
    captureCapability.expected_capture_delay = 0;
    captureCapability.interlaced = false;

    // Deliver the captured video frame.
    frameReceiver_->ReceiveFrame(static_cast<UInt8*>(baseAddress), frameSize,
                                 captureCapability);

    CVPixelBufferUnlockBaseAddress(videoFrame, kLockFlags);
  }
}

@end
