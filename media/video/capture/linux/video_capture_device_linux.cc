// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/video_capture_device_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/stringprintf.h"

// Workaround for some device. This query of all controls magically brings
// device back to normal from bad state.
// See http://crbug.com/94134.
static void ResetCameraByEnumeratingIoctlsHACK(int fd) {
  struct v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));

  for (query_ctrl.id = V4L2_CID_BASE;
       query_ctrl.id < V4L2_CID_LASTP1;
       query_ctrl.id++) {
    ioctl(fd, VIDIOC_QUERYCTRL, &query_ctrl);
  }
}

namespace media {

// Max number of video buffers VideoCaptureDeviceLinux can allocate.
enum { kMaxVideoBuffers = 2 };
// Timeout in microseconds v4l2_thread_ blocks waiting for a frame from the hw.
enum { kCaptureTimeoutUs = 200000 };
// Time to wait in milliseconds before v4l2_thread_ reschedules OnCaptureTask
// if an event is triggered (select) but no video frame is read.
enum { kCaptureSelectWaitMs = 10 };

// V4L2 color formats VideoCaptureDeviceLinux support.
static const int32 kV4l2Fmts[] = {
  V4L2_PIX_FMT_YUV420,
  V4L2_PIX_FMT_YUYV
};

static VideoCaptureDevice::Format V4l2ColorToVideoCaptureColorFormat(
    int32 v4l2_fourcc) {
  VideoCaptureDevice::Format result = VideoCaptureDevice::kColorUnknown;
  switch (v4l2_fourcc) {
    case V4L2_PIX_FMT_YUV420:
      result = VideoCaptureDevice::kI420;
      break;
    case V4L2_PIX_FMT_YUYV:
      result = VideoCaptureDevice::kYUY2;
      break;
  }
  DCHECK_NE(result, VideoCaptureDevice::kColorUnknown);
  return result;
}

void VideoCaptureDevice::GetDeviceNames(Names* device_names) {
  int fd = -1;

  // Empty the name list.
  device_names->clear();

  FilePath path("/dev/");
  file_util::FileEnumerator enumerator(
      path, false, file_util::FileEnumerator::FILES, "video*");

  while (!enumerator.Next().empty()) {
    file_util::FileEnumerator::FindInfo info;
    enumerator.GetFindInfo(&info);

    Name name;
    name.unique_id = path.value() + info.filename;
    if ((fd = open(name.unique_id.c_str() , O_RDONLY)) <= 0) {
      // Failed to open this device.
      continue;
    }
    // Test if this is a V4L2 device.
    v4l2_capability cap;
    if ((ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) &&
        (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      // This is a V4L2 video capture device
      name.device_name = StringPrintf("%s", cap.card);
      device_names->push_back(name);
    }
    close(fd);
  }
}

VideoCaptureDevice* VideoCaptureDevice::Create(const Name& device_name) {
  VideoCaptureDeviceLinux* self = new VideoCaptureDeviceLinux(device_name);
  if (!self)
    return NULL;
  // Test opening the device driver. This is to make sure it is available.
  // We will reopen it again in our worker thread when someone
  // allocates the camera.
  int fd = open(device_name.unique_id.c_str(), O_RDONLY);
  if (fd < 0) {
    DPLOG(ERROR) << "Cannot open device";
    delete self;
    return NULL;
  }
  close(fd);

  return self;
}

VideoCaptureDeviceLinux::VideoCaptureDeviceLinux(const Name& device_name)
    : state_(kIdle),
      observer_(NULL),
      device_name_(device_name),
      device_fd_(-1),
      v4l2_thread_("V4L2Thread"),
      buffer_pool_(NULL),
      buffer_pool_size_(0) {
}

VideoCaptureDeviceLinux::~VideoCaptureDeviceLinux() {
  state_ = kIdle;
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  DCHECK(!v4l2_thread_.IsRunning());

  v4l2_thread_.Stop();
  if (device_fd_ >= 0) {
    close(device_fd_);
  }
}

void VideoCaptureDeviceLinux::Allocate(int width,
                                       int height,
                                       int frame_rate,
                                       EventHandler* observer) {
  if (v4l2_thread_.IsRunning()) {
    return;  // Wrong state.
  }
  v4l2_thread_.Start();
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnAllocate, base::Unretained(this),
                 width, height, frame_rate, observer));
}

void VideoCaptureDeviceLinux::Start() {
  if (!v4l2_thread_.IsRunning()) {
    return;  // Wrong state.
  }
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnStart, base::Unretained(this)));
}

void VideoCaptureDeviceLinux::Stop() {
  if (!v4l2_thread_.IsRunning()) {
    return;  // Wrong state.
  }
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnStop, base::Unretained(this)));
}

void VideoCaptureDeviceLinux::DeAllocate() {
  if (!v4l2_thread_.IsRunning()) {
    return;  // Wrong state.
  }
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnDeAllocate,
                 base::Unretained(this)));
  v4l2_thread_.Stop();

  // Make sure no buffers are still allocated.
  // This can happen (theoretically) if an error occurs when trying to stop
  // the camera.
  DeAllocateVideoBuffers();
}

const VideoCaptureDevice::Name& VideoCaptureDeviceLinux::device_name() {
  return device_name_;
}

void VideoCaptureDeviceLinux::OnAllocate(int width,
                                         int height,
                                         int frame_rate,
                                         EventHandler* observer) {
  DCHECK_EQ(v4l2_thread_.message_loop(), MessageLoop::current());

  observer_ = observer;

  if ((device_fd_ = open(device_name_.unique_id.c_str(), O_RDONLY)) < 0) {
    SetErrorState("Failed to open V4L2 device driver.");
    return;
  }

  // Test if this is a V4L2 device.
  v4l2_capability cap;
  if (!((ioctl(device_fd_, VIDIOC_QUERYCAP, &cap) == 0) &&
      (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))) {
    // This is not a V4L2 video capture device.
    close(device_fd_);
    device_fd_ = -1;
    SetErrorState("This is not a V4L2 video capture device");
    return;
  }

  v4l2_format video_fmt;
  memset(&video_fmt, 0, sizeof(video_fmt));
  video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  video_fmt.fmt.pix.sizeimage = 0;
  video_fmt.fmt.pix.width = width;
  video_fmt.fmt.pix.height = height;

  // Some device failed in first VIDIOC_TRY_FMT with EBUSY or EIO.
  // But second VIDIOC_TRY_FMT succeeds.
  // See http://crbug.com/94134.
  bool format_match = false;
  for (unsigned int i = 0; i < arraysize(kV4l2Fmts) && !format_match; i++) {
    video_fmt.fmt.pix.pixelformat = kV4l2Fmts[i];
    for (int attempt = 0; attempt < 2 && !format_match; attempt++) {
      ResetCameraByEnumeratingIoctlsHACK(device_fd_);
      if (ioctl(device_fd_, VIDIOC_TRY_FMT, &video_fmt) < 0) {
        if (errno != EIO)
          break;
      } else {
        format_match = true;
      }
    }
  }

  if (!format_match) {
    SetErrorState("Failed to find supported camera format.");
    return;
  }
  // Set format and frame size now.
  if (ioctl(device_fd_, VIDIOC_S_FMT, &video_fmt) < 0) {
    SetErrorState("Failed to set camera format");
    return;
  }

  // Store our current width and height.
  Capability current_settings;
  current_settings.color = V4l2ColorToVideoCaptureColorFormat(
      video_fmt.fmt.pix.pixelformat);
  current_settings.width  = video_fmt.fmt.pix.width;
  current_settings.height = video_fmt.fmt.pix.height;
  current_settings.frame_rate = frame_rate;

  state_ = kAllocated;
  // Report the resulting frame size to the observer.
  observer_->OnFrameInfo(current_settings);
}

void VideoCaptureDeviceLinux::OnDeAllocate() {
  DCHECK_EQ(v4l2_thread_.message_loop(), MessageLoop::current());

  // If we are in error state or capturing
  // try to stop the camera.
  if (state_ == kCapturing) {
    OnStop();
  }
  if (state_ == kAllocated) {
    state_ = kIdle;
  }

  // We need to close and open the device if we want to change the settings
  // Otherwise VIDIOC_S_FMT will return error
  // Sad but true.
  close(device_fd_);
  device_fd_ = -1;
}

void VideoCaptureDeviceLinux::OnStart() {
  DCHECK_EQ(v4l2_thread_.message_loop(), MessageLoop::current());

  if (state_ != kAllocated) {
    return;
  }

  if (!AllocateVideoBuffers()) {
    // Error, We can not recover.
    SetErrorState("Allocate buffer failed");
    return;
  }

  // Start UVC camera.
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(device_fd_, VIDIOC_STREAMON, &type) == -1) {
    SetErrorState("VIDIOC_STREAMON failed");
    return;
  }

  state_ = kCapturing;
  // Post task to start fetching frames from v4l2.
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnCaptureTask,
                 base::Unretained(this)));
}

void VideoCaptureDeviceLinux::OnStop() {
  DCHECK_EQ(v4l2_thread_.message_loop(), MessageLoop::current());

  state_ = kAllocated;

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(device_fd_, VIDIOC_STREAMOFF, &type) < 0) {
    SetErrorState("VIDIOC_STREAMOFF failed");
    return;
  }
  // We don't dare to deallocate the buffers if we can't stop
  // the capture device.
  DeAllocateVideoBuffers();
}

void VideoCaptureDeviceLinux::OnCaptureTask() {
  DCHECK_EQ(v4l2_thread_.message_loop(), MessageLoop::current());

  if (state_ != kCapturing) {
    return;
  }

  fd_set r_set;
  FD_ZERO(&r_set);
  FD_SET(device_fd_, &r_set);
  timeval timeout;

  timeout.tv_sec = 0;
  timeout.tv_usec = kCaptureTimeoutUs;

  // First argument to select is the highest numbered file descriptor +1.
  // Refer to http://linux.die.net/man/2/select for more information.
  int result = select(device_fd_ + 1, &r_set, NULL, NULL, &timeout);
  // Check if select have failed.
  if (result < 0) {
    // EINTR is a signal. This is not really an error.
    if (errno != EINTR) {
      SetErrorState("Select failed");
      return;
    }
    v4l2_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&VideoCaptureDeviceLinux::OnCaptureTask,
                   base::Unretained(this)),
        kCaptureSelectWaitMs);
  }

  // Check if the driver have filled a buffer.
  if (FD_ISSET(device_fd_, &r_set)) {
    v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    // Dequeue a buffer.
    if (ioctl(device_fd_, VIDIOC_DQBUF, &buffer) == 0) {
      observer_->OnIncomingCapturedFrame(
          static_cast<uint8*> (buffer_pool_[buffer.index].start),
          buffer.bytesused, base::Time::Now());

      // Enqueue the buffer again.
      if (ioctl(device_fd_, VIDIOC_QBUF, &buffer) == -1) {
        SetErrorState(
            StringPrintf("Failed to enqueue capture buffer errno %d", errno));
      }
    }  // Else wait for next event.
  }

  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnCaptureTask,
                 base::Unretained(this)));
}

bool VideoCaptureDeviceLinux::AllocateVideoBuffers() {
  v4l2_requestbuffers r_buffer;
  memset(&r_buffer, 0, sizeof(r_buffer));

  r_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = kMaxVideoBuffers;

  if (ioctl(device_fd_, VIDIOC_REQBUFS, &r_buffer) < 0) {
    return false;
  }

  if (r_buffer.count > kMaxVideoBuffers) {
    r_buffer.count = kMaxVideoBuffers;
  }

  buffer_pool_size_ = r_buffer.count;

  // Map the buffers.
  buffer_pool_ = new Buffer[r_buffer.count];
  for (unsigned int i = 0; i < r_buffer.count; i++) {
    v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;

    if (ioctl(device_fd_, VIDIOC_QUERYBUF, &buffer) < 0) {
      return false;
    }

    buffer_pool_[i].start = mmap(NULL, buffer.length, PROT_READ,
                                 MAP_SHARED, device_fd_, buffer.m.offset);
    if (buffer_pool_[i].start == MAP_FAILED) {
      return false;
    }
    buffer_pool_[i].length = buffer.length;
    // Enqueue the buffer in the drivers incoming queue.
    if (ioctl(device_fd_, VIDIOC_QBUF, &buffer) < 0) {
      return false;
    }
  }
  return true;
}

void VideoCaptureDeviceLinux::DeAllocateVideoBuffers() {
  if (!buffer_pool_)
    return;

  // Unmaps buffers.
  for (int i = 0; i < buffer_pool_size_; i++) {
    munmap(buffer_pool_[i].start, buffer_pool_[i].length);
  }
  v4l2_requestbuffers r_buffer;
  memset(&r_buffer, 0, sizeof(r_buffer));
  r_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = 0;

  if (ioctl(device_fd_, VIDIOC_REQBUFS, &r_buffer) < 0) {
    SetErrorState("Failed to reset buf.");
  }

  delete [] buffer_pool_;
  buffer_pool_ = NULL;
  buffer_pool_size_ = 0;
}

void VideoCaptureDeviceLinux::SetErrorState(const std::string& reason) {
  DLOG(ERROR) << reason;
  state_ = kError;
  observer_->OnError();
}

}  // namespace media
