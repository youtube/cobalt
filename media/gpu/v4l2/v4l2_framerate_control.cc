// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_framerate_control.h"

#include "base/command_line.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"

namespace {
double GetUserFrameRate() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kHardwareVideoDecodeFrameRate))
    return 0.0;

  const std::string framerate_str(
      cmd_line->GetSwitchValueASCII(switches::kHardwareVideoDecodeFrameRate));

  constexpr double kMaxFramerate = 120.0;
  double framerate = 0;
  if (base::StringToDouble(framerate_str, &framerate) && framerate >= 0.0 &&
      framerate <= kMaxFramerate) {
    return framerate;
  }
  VLOG(1) << "Requested framerate is unreasonable: " << framerate
          << "fps, not overriding.";
  return 0.0;
}

bool FrameRateControlPresent(scoped_refptr<media::V4L2Device> device) {
  DCHECK(device);

  struct v4l2_streamparm parms;
  memset(&parms, 0, sizeof(parms));
  parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

  // Try to set the framerate to 30fps to see if the control is available.
  // VIDIOC_G_PARM can not be used as it does not return the current
  // framerate.
  parms.parm.output.timeperframe.numerator = 1;
  parms.parm.output.timeperframe.denominator = 30;

  const double user_framerate = GetUserFrameRate();
  if (user_framerate > 0.0) {
    parms.parm.output.timeperframe.numerator = 1000;
    parms.parm.output.timeperframe.denominator = 1000.0 * user_framerate;
    VLOG(1) << "Overriding playback framerate. Set at " << user_framerate
            << " fps.";
  }

  if (device->Ioctl(VIDIOC_S_PARM, &parms) != 0) {
    VLOG(1) << "Failed to issue VIDIOC_S_PARM command";
    return false;
  }

  return (user_framerate == 0) &&
         (parms.parm.output.capability & V4L2_CAP_TIMEPERFRAME);
}

}  // namespace
namespace media {

static constexpr int kMovingAverageWindowSize = 32;
static constexpr base::TimeDelta kFrameIntervalFor120fps =
    base::Milliseconds(8);
static constexpr base::TimeDelta kFrameIntervalFor24fps =
    base::Milliseconds(41);

V4L2FrameRateControl::V4L2FrameRateControl(
    scoped_refptr<V4L2Device> device,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : device_(device),
      framerate_control_present_(FrameRateControlPresent(device)),
      current_frame_duration_avg_ms_(0),
      last_frame_display_time_(base::TimeTicks::Now()),
      frame_duration_moving_average_(kMovingAverageWindowSize),
      task_runner_(task_runner),
      weak_this_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

V4L2FrameRateControl::~V4L2FrameRateControl() = default;

void V4L2FrameRateControl::UpdateFrameRate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if ((frame_duration_moving_average_.count() <
       frame_duration_moving_average_.depth())) {
    return;
  }

  const base::TimeDelta frame_duration_avg =
      frame_duration_moving_average_.Average();

  if (frame_duration_avg > kFrameIntervalFor120fps &&
      frame_duration_avg < kFrameIntervalFor24fps &&
      frame_duration_avg.InMilliseconds() != current_frame_duration_avg_ms_) {
    current_frame_duration_avg_ms_ = frame_duration_avg.InMilliseconds();

    struct v4l2_streamparm parms;
    memset(&parms, 0, sizeof(parms));
    parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parms.parm.output.timeperframe.numerator = current_frame_duration_avg_ms_;
    parms.parm.output.timeperframe.denominator = 1000L;

    const auto result = device_->Ioctl(VIDIOC_S_PARM, &parms);
    LOG_IF(ERROR, result != 0) << "Failed to issue VIDIOC_S_PARM command";

    VLOG(1) << "Average framerate: " << frame_duration_avg.ToHz();
  }
}

// static
void V4L2FrameRateControl::RecordFrameDurationThunk(
    base::WeakPtr<V4L2FrameRateControl> weak_this,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (task_runner->RunsTasksInCurrentSequence()) {
    if (weak_this) {
      weak_this->RecordFrameDuration();
    }
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2FrameRateControl::RecordFrameDuration, weak_this));
  }
}

void V4L2FrameRateControl::RecordFrameDuration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  constexpr base::TimeDelta kMaxFrameInterval = base::Milliseconds(500);
  const base::TimeTicks frame_display_time = base::TimeTicks::Now();
  const base::TimeDelta duration =
      frame_display_time - last_frame_display_time_;

  // Frames are expected to be returned at a consistent cadence because the
  // durations are recorded after the frame is displayed, but this isn't always
  // the case.  If the video is not visible, then the frames will be returned
  // as soon as they are decoded.  The window needs to be large enough to
  // handle this scenario.  The video may also be paused, in which case there
  // could be a large duration.  In this case, reset the filter.
  if (duration > kMaxFrameInterval)
    frame_duration_moving_average_.Reset();
  else
    frame_duration_moving_average_.AddSample(duration);

  last_frame_display_time_ = frame_display_time;

  UpdateFrameRate();
}

void V4L2FrameRateControl::AttachToVideoFrame(
    scoped_refptr<VideoFrame>& video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!framerate_control_present_)
    return;

  video_frame->AddDestructionObserver(
      base::BindOnce(&V4L2FrameRateControl::RecordFrameDurationThunk,
                     weak_this_factory_.GetWeakPtr(), task_runner_));
}

}  // namespace media
