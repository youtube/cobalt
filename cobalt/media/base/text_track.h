// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_TEXT_TRACK_H_
#define COBALT_MEDIA_BASE_TEXT_TRACK_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"

namespace cobalt {
namespace media {

class TextTrackConfig;

class TextTrack {
 public:
  virtual ~TextTrack() {}
  virtual void addWebVTTCue(const base::TimeDelta& start,
                            const base::TimeDelta& end, const std::string& id,
                            const std::string& content,
                            const std::string& settings) = 0;
};

typedef base::Callback<void(std::unique_ptr<TextTrack>)> AddTextTrackDoneCB;

typedef base::Callback<void(const TextTrackConfig& config,
                            const AddTextTrackDoneCB& done_cb)>
    AddTextTrackCB;

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_TEXT_TRACK_H_
