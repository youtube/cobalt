// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_OUTPUT_DEVICE_INFO_H_
#define COBALT_MEDIA_BASE_OUTPUT_DEVICE_INFO_H_

#include <string>

#include "base/callback.h"
#include "cobalt/media/base/audio_parameters.h"
#include "cobalt/media/base/media_export.h"

namespace media {

// Result of an audio output device switch operation
enum OutputDeviceStatus {
  OUTPUT_DEVICE_STATUS_OK = 0,
  OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
  OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
  OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT,
  OUTPUT_DEVICE_STATUS_ERROR_INTERNAL,
  OUTPUT_DEVICE_STATUS_LAST = OUTPUT_DEVICE_STATUS_ERROR_INTERNAL,
};

using OutputDeviceStatusCB = base::Callback<void(OutputDeviceStatus)>;

// Output device information returned by
// AudioRendererSink::GetOutputDeviceInfo()
class MEDIA_EXPORT OutputDeviceInfo {
 public:
  // Use this constructor to initialize with "no info available" values.
  OutputDeviceInfo();

  // Use this constructor to indicate a specific error status of the device.
  explicit OutputDeviceInfo(OutputDeviceStatus device_status);

  OutputDeviceInfo(const std::string& device_id,
                   OutputDeviceStatus device_status,
                   const AudioParameters& output_params);

  OutputDeviceInfo(const OutputDeviceInfo& other);

  OutputDeviceInfo& operator=(const OutputDeviceInfo&);

  ~OutputDeviceInfo();

  // Returns the device ID.
  const std::string& device_id() const { return device_id_; }

  // Returns the status of output device.
  OutputDeviceStatus device_status() const { return device_status_; }

  // Returns the device's audio output parameters.
  // The return value is undefined if the device status (as returned by
  // device_status()) is different from OUTPUT_DEVICE_STATUS_OK.
  const AudioParameters& output_params() const { return output_params_; };

  // Returns a human-readable string describing |*this|.  For debugging & test
  // output only.
  std::string AsHumanReadableString() const;

 private:
  std::string device_id_;
  OutputDeviceStatus device_status_;
  AudioParameters output_params_;
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_OUTPUT_DEVICE_INFO_H_
