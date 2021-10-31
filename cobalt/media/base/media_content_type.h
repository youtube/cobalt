// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_CONTENT_TYPE_H_
#define COBALT_MEDIA_BASE_MEDIA_CONTENT_TYPE_H_

#include "base/time/time.h"
#include "cobalt/media/base/media_export.h"

namespace cobalt {
namespace media {

// The content type of a media player, which will be used by MediaSession to
// control its players.
enum class MediaContentType {
  // Type indicating that a player is persistent, which needs to take audio
  // focus to play.
  Persistent,
  // Type indicating that a player only plays a transient sound.
  Transient,
  // Type indicating that a player is a Pepper instance. MediaSession may duck
  // the player instead of pausing it.
  Pepper,
  // Type indicating that a player cannot be controlled. MediaSession wil ignore
  // this player.
  Uncontrollable
};

// Utility function for deciding the MediaContentType of a player based on its
// duration.
MEDIA_EXPORT MediaContentType
DurationToMediaContentType(base::TimeDelta duration);

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MEDIA_CONTENT_TYPE_H_
