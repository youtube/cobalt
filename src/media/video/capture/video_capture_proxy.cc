// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"

namespace {

// Called on VC thread: extracts the state out of the VideoCapture, and
// serialize it into a VideoCaptureState.
media::VideoCaptureHandlerProxy::VideoCaptureState GetState(
    media::VideoCapture* capture) {
  media::VideoCaptureHandlerProxy::VideoCaptureState state;
  state.started = capture->CaptureStarted();
  state.width = capture->CaptureWidth();
  state.height = capture->CaptureHeight();
  state.frame_rate = capture->CaptureFrameRate();
  return state;
}

}  // anonymous namespace

namespace media {

VideoCaptureHandlerProxy::VideoCaptureHandlerProxy(
    VideoCapture::EventHandler* proxied,
    scoped_refptr<base::MessageLoopProxy> main_message_loop)
    : proxied_(proxied),
      main_message_loop_(main_message_loop) {
}

VideoCaptureHandlerProxy::~VideoCaptureHandlerProxy() {
}

void VideoCaptureHandlerProxy::OnStarted(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
        &VideoCaptureHandlerProxy::OnStartedOnMainThread,
        base::Unretained(this),
        capture,
        GetState(capture)));
}

void VideoCaptureHandlerProxy::OnStopped(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
        &VideoCaptureHandlerProxy::OnStoppedOnMainThread,
        base::Unretained(this),
        capture,
        GetState(capture)));
}

void VideoCaptureHandlerProxy::OnPaused(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnPausedOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture)));
}

void VideoCaptureHandlerProxy::OnError(VideoCapture* capture, int error_code) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnErrorOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture),
      error_code));
}

void VideoCaptureHandlerProxy::OnRemoved(VideoCapture* capture) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnRemovedOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture)));
}

void VideoCaptureHandlerProxy::OnBufferReady(
    VideoCapture* capture,
    scoped_refptr<VideoCapture::VideoFrameBuffer> buffer) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnBufferReadyOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture),
      buffer));
}

void VideoCaptureHandlerProxy::OnDeviceInfoReceived(
    VideoCapture* capture,
    const VideoCaptureParams& device_info) {
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoCaptureHandlerProxy::OnDeviceInfoReceivedOnMainThread,
      base::Unretained(this),
      capture,
      GetState(capture),
      device_info));
}

void VideoCaptureHandlerProxy::OnStartedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnStarted(capture);
}

void VideoCaptureHandlerProxy::OnStoppedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnStopped(capture);
}

void VideoCaptureHandlerProxy::OnPausedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnPaused(capture);
}

void VideoCaptureHandlerProxy::OnErrorOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state,
    int error_code) {
  state_ = state;
  proxied_->OnError(capture, error_code);
}

void VideoCaptureHandlerProxy::OnRemovedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state) {
  state_ = state;
  proxied_->OnRemoved(capture);
}

void VideoCaptureHandlerProxy::OnBufferReadyOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state,
    scoped_refptr<VideoCapture::VideoFrameBuffer> buffer) {
  state_ = state;
  proxied_->OnBufferReady(capture, buffer);
}

void VideoCaptureHandlerProxy::OnDeviceInfoReceivedOnMainThread(
    VideoCapture* capture,
    const VideoCaptureState& state,
    const VideoCaptureParams& device_info) {
  state_ = state;
  proxied_->OnDeviceInfoReceived(capture, device_info);
}

}  // namespace media
