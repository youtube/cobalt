// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/capture/video_capturer_source.h"
#include "base/functional/callback_helpers.h"

namespace media_m96 {

// TODO(mcasas): VideoCapturerSource is implemented in other .dll(s) (e.g.
// content) in Windows component build. The current compiler fails to generate
// object files for this destructor if it's defined in the header file and that
// breaks linking. Consider removing this file when the compiler+linker is able
// to generate symbols across linking units.
VideoCapturerSource::~VideoCapturerSource() = default;

media_m96::VideoCaptureFeedbackCB VideoCapturerSource::GetFeedbackCallback() const {
  return base::DoNothing();
}

}  // namespace media_m96
