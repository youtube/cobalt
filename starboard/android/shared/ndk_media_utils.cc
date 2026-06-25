// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/android/shared/ndk_media_utils.h"

#include <media/NdkMediaError.h>

namespace starboard {

std::string ToString(media_status_t status) {
  switch (status) {
    case AMEDIA_OK:
      return "AMEDIA_OK";
    case AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE:
      return "AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE";
    case AMEDIACODEC_ERROR_RECLAIMED:
      return "AMEDIACODEC_ERROR_RECLAIMED";
    case AMEDIA_ERROR_UNKNOWN:
      return "AMEDIA_ERROR_UNKNOWN";
    case AMEDIA_ERROR_MALFORMED:
      return "AMEDIA_ERROR_MALFORMED";
    case AMEDIA_ERROR_UNSUPPORTED:
      return "AMEDIA_ERROR_UNSUPPORTED";
    case AMEDIA_ERROR_INVALID_OBJECT:
      return "AMEDIA_ERROR_INVALID_OBJECT";
    case AMEDIA_ERROR_INVALID_PARAMETER:
      return "AMEDIA_ERROR_INVALID_PARAMETER";
    case AMEDIA_ERROR_INVALID_OPERATION:
      return "AMEDIA_ERROR_INVALID_OPERATION";
    case AMEDIA_ERROR_END_OF_STREAM:
      return "AMEDIA_ERROR_END_OF_STREAM";
    case AMEDIA_ERROR_IO:
      return "AMEDIA_ERROR_IO";
    case AMEDIA_ERROR_WOULD_BLOCK:
      return "AMEDIA_ERROR_WOULD_BLOCK";
    case AMEDIA_DRM_ERROR_BASE:
      return "AMEDIA_DRM_ERROR_BASE";
    case AMEDIA_DRM_NOT_PROVISIONED:
      return "AMEDIA_DRM_NOT_PROVISIONED";
    case AMEDIA_DRM_RESOURCE_BUSY:
      return "AMEDIA_DRM_RESOURCE_BUSY";
    case AMEDIA_DRM_DEVICE_REVOKED:
      return "AMEDIA_DRM_DEVICE_REVOKED";
    case AMEDIA_DRM_SHORT_BUFFER:
      return "AMEDIA_DRM_SHORT_BUFFER";
    case AMEDIA_DRM_SESSION_NOT_OPENED:
      return "AMEDIA_DRM_SESSION_NOT_OPENED";
    case AMEDIA_DRM_TAMPER_DETECTED:
      return "AMEDIA_DRM_TAMPER_DETECTED";
    case AMEDIA_DRM_VERIFY_FAILED:
      return "AMEDIA_DRM_VERIFY_FAILED";
    case AMEDIA_DRM_NEED_KEY:
      return "AMEDIA_DRM_NEED_KEY";
    case AMEDIA_DRM_LICENSE_EXPIRED:
      return "AMEDIA_DRM_LICENSE_EXPIRED";
    case AMEDIA_IMGREADER_ERROR_BASE:
      return "AMEDIA_IMGREADER_ERROR_BASE";
    case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE:
      return "AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE";
    case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED:
      return "AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED";
    case AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE:
      return "AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE";
    case AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE:
      return "AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE";
    case AMEDIA_IMGREADER_IMAGE_NOT_LOCKED:
      return "AMEDIA_IMGREADER_IMAGE_NOT_LOCKED";
  }
  return "UNKNOWN_MEDIA_STATUS (" + std::to_string(static_cast<int>(status)) +
         ")";
}

std::string ToString(aaudio_stream_state_t state) {
  switch (state) {
    case AAUDIO_STREAM_STATE_UNINITIALIZED:
      return "UNINITIALIZED";
    case AAUDIO_STREAM_STATE_UNKNOWN:
      return "UNKNOWN";
    case AAUDIO_STREAM_STATE_OPEN:
      return "OPEN";
    case AAUDIO_STREAM_STATE_STARTING:
      return "STARTING";
    case AAUDIO_STREAM_STATE_STARTED:
      return "STARTED";
    case AAUDIO_STREAM_STATE_PAUSING:
      return "PAUSING";
    case AAUDIO_STREAM_STATE_PAUSED:
      return "PAUSED";
    case AAUDIO_STREAM_STATE_FLUSHING:
      return "FLUSHING";
    case AAUDIO_STREAM_STATE_FLUSHED:
      return "FLUSHED";
    case AAUDIO_STREAM_STATE_STOPPING:
      return "STOPPING";
    case AAUDIO_STREAM_STATE_STOPPED:
      return "STOPPED";
    case AAUDIO_STREAM_STATE_CLOSING:
      return "CLOSING";
    case AAUDIO_STREAM_STATE_CLOSED:
      return "CLOSED";
    case AAUDIO_STREAM_STATE_DISCONNECTED:
      return "DISCONNECTED";
  }
  return "UNKNOWN_STATE (" + std::to_string(static_cast<int>(state)) + ")";
}

}  // namespace starboard
