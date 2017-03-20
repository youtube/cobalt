// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/sample_format.h"

#include "base/logging.h"

namespace media {

int SampleFormatToBytesPerChannel(SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return 0;
    case kSampleFormatU8:
      return 1;
    case kSampleFormatS16:
    case kSampleFormatPlanarS16:
      return 2;
    case kSampleFormatS24:
    case kSampleFormatS32:
    case kSampleFormatF32:
    case kSampleFormatPlanarF32:
    case kSampleFormatPlanarS32:
      return 4;
  }

  NOTREACHED() << "Invalid sample format provided: " << sample_format;
  return 0;
}

const char* SampleFormatToString(SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return "Unknown sample format";
    case kSampleFormatU8:
      return "Unsigned 8-bit with bias of 128";
    case kSampleFormatS16:
      return "Signed 16-bit";
    case kSampleFormatS24:
      return "Signed 24-bit";
    case kSampleFormatS32:
      return "Signed 32-bit";
    case kSampleFormatF32:
      return "Float 32-bit";
    case kSampleFormatPlanarS16:
      return "Signed 16-bit planar";
    case kSampleFormatPlanarF32:
      return "Float 32-bit planar";
    case kSampleFormatPlanarS32:
      return "Signed 32-bit planar";
  }
  NOTREACHED() << "Invalid sample format provided: " << sample_format;
  return "";
}

bool IsPlanar(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatPlanarS16:
    case kSampleFormatPlanarF32:
    case kSampleFormatPlanarS32:
      return true;
    case kUnknownSampleFormat:
    case kSampleFormatU8:
    case kSampleFormatS16:
    case kSampleFormatS24:
    case kSampleFormatS32:
    case kSampleFormatF32:
      return false;
  }

  NOTREACHED() << "Invalid sample format provided: " << sample_format;
  return false;
}

bool IsInterleaved(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatU8:
    case kSampleFormatS16:
    case kSampleFormatS24:
    case kSampleFormatS32:
    case kSampleFormatF32:
      return true;
    case kUnknownSampleFormat:
    case kSampleFormatPlanarS16:
    case kSampleFormatPlanarF32:
    case kSampleFormatPlanarS32:
      return false;
  }

  NOTREACHED() << "Invalid sample format provided: " << sample_format;
  return false;
}

}  // namespace media
