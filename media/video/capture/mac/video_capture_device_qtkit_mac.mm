// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

#import <QTKit/QTKit.h>

#include "base/logging.h"
#include "base/mac/crash_logging.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

@implementation VideoCaptureDeviceQTKit

#pragma mark Class methods

+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames {
  NSArray* captureDevices =
      [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];

  for (QTCaptureDevice* device in captureDevices) {
    [deviceNames setObject:[device localizedDisplayName]
                    forKey:[device uniqueID]];
  }
}

+ (NSDictionary*)deviceNames {
  NSMutableDictionary* deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];

  // TODO(shess): Post to the main thread to see if that helps
  // http://crbug.com/139164
  [self performSelectorOnMainThread:@selector(getDeviceNames:)
                         withObject:deviceNames
                      waitUntilDone:YES];
  return deviceNames;
}

#pragma mark Public methods

- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac *)frameReceiver {
  self = [super init];
  if (self) {
    frameReceiver_ = frameReceiver;
    lock_ = [[NSLock alloc] init];
  }
  return self;
}

- (void)dealloc {
  [captureSession_ release];
  [captureDeviceInput_ release];
  [super dealloc];
}

- (void)setFrameReceiver:(media::VideoCaptureDeviceMac *)frameReceiver {
  [lock_ lock];
  frameReceiver_ = frameReceiver;
  [lock_ unlock];
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

    // This key can be used to check if video capture code was related to a
    // particular crash.
    base::mac::SetCrashKeyValue(@"VideoCaptureDeviceQTKit", @"OpenedDevice");

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
    if ([[captureSession_ outputs] count] > 0) {
      // Only one output is set for |captureSession_|.
      id output = [[captureSession_ outputs] objectAtIndex:0];
      [output setDelegate:nil];

      // TODO(shess): QTKit achieves thread safety by posting messages
      // to the main thread.  As part of -addOutput:, it posts a
      // message to the main thread which in turn posts a notification
      // which will run in a future spin after the original method
      // returns.  -removeOutput: can post a main-thread message in
      // between while holding a lock which the notification handler
      // will need.  Posting either -addOutput: or -removeOutput: to
      // the main thread should fix it, remove is likely safer.
      // http://crbug.com/152757
      [captureSession_ performSelectorOnMainThread:@selector(removeOutput:)
                                        withObject:output
                                     waitUntilDone:YES];
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
  if (frameRate <= 0) {
    DLOG(ERROR) << "Wrong frame rate.";
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

  [[[captureSession_ outputs] objectAtIndex:0]
      setMinimumVideoFrameInterval:(NSTimeInterval)1/(float)frameRate];
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
  [lock_ lock];
  if(!frameReceiver_) {
    [lock_ unlock];
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

    // TODO(shess): bytesPerRow may not correspond to frameWidth_*4,
    // but VideoCaptureController::OnIncomingCapturedFrame() requires
    // it to do so.  Plumbing things through is intrusive, for now
    // just deliver an adjusted buffer.
    UInt8* addressToPass = static_cast<UInt8*>(baseAddress);
    size_t expectedBytesPerRow = frameWidth_ * 4;
    if (bytesPerRow > expectedBytesPerRow) {
      // TODO(shess): frameHeight and frameHeight_ are not the same,
      // try to do what the surrounding code seems to assume.
      // Ironically, captureCapability and frameSize are ignored
      // anyhow.
      adjustedFrame_.resize(expectedBytesPerRow * frameHeight);
      // std::vector is contiguous according to standard.
      UInt8* adjustedAddress = &adjustedFrame_[0];

      for (int y = 0; y < frameHeight; ++y) {
        memcpy(adjustedAddress + y * expectedBytesPerRow,
               addressToPass + y * bytesPerRow,
               expectedBytesPerRow);
      }

      addressToPass = adjustedAddress;
      frameSize = frameHeight * expectedBytesPerRow;
    }
    media::VideoCaptureCapability captureCapability;
    captureCapability.width = frameWidth_;
    captureCapability.height = frameHeight_;
    captureCapability.frame_rate = frameRate_;
    captureCapability.color = media::VideoCaptureCapability::kARGB;
    captureCapability.expected_capture_delay = 0;
    captureCapability.interlaced = false;

    // Deliver the captured video frame.
    frameReceiver_->ReceiveFrame(addressToPass, frameSize, captureCapability);

    CVPixelBufferUnlockBaseAddress(videoFrame, kLockFlags);
  }
  [lock_ unlock];
}

@end
