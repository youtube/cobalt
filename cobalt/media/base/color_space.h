// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_COLOR_SPACE_H_
#define COBALT_MEDIA_BASE_COLOR_SPACE_H_

#include "base/logging.h"

// This is a modified version of gfx::ColorSpace with Skia dependency removed.
// It is tentatively put inside media to avoid introducing new code into
// ui/gfx.  It will be further simplified and merged into media in follow up
// refactors.
namespace gfx {

// Used to represent a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class ColorSpace {
 public:
  typedef float CustomPrimaryMatrix[12];

  enum PrimaryID {
    // The first 0-255 values should match the H264 specification (see Table E-3
    // Colour Primaries in https://www.itu.int/rec/T-REC-H.264/en).
    kPrimaryIdReserved0 = 0,
    kPrimaryIdBt709 = 1,
    kPrimaryIdUnspecified = 2,
    kPrimaryIdReserved = 3,
    kPrimaryIdBt470M = 4,
    kPrimaryIdBt470Bg = 5,
    kPrimaryIdSmpte170M = 6,
    kPrimaryIdSmpte240M = 7,
    kPrimaryIdFilm = 8,
    kPrimaryIdBt2020 = 9,
    kPrimaryIdSmpteSt4281 = 10,
    kPrimaryIdSmpteSt4312 = 11,
    kPrimaryIdSmpteSt4321 = 12,

    kPrimaryIdLastStandardValue = kPrimaryIdSmpteSt4321,

    // Chrome-specific values start at 1000.
    kPrimaryIdUnknown = 1000,
    kPrimaryIdXyzD50,
    kPrimaryIdCustom,
    kPrimaryIdLast = kPrimaryIdCustom
  };

  enum TransferID {
    // The first 0-255 values should match the H264 specification (see Table E-4
    // Transfer Characteristics in https://www.itu.int/rec/T-REC-H.264/en).
    kTransferIdReserved0 = 0,
    kTransferIdBt709 = 1,
    kTransferIdUnspecified = 2,
    kTransferIdReserved = 3,
    kTransferIdGamma22 = 4,
    kTransferIdGamma28 = 5,
    kTransferIdSmpte170M = 6,
    kTransferIdSmpte240M = 7,
    kTransferIdLinear = 8,
    kTransferIdLog = 9,
    kTransferIdLogSqrt = 10,
    kTransferIdIec6196624 = 11,
    kTransferIdBt1361Ecg = 12,
    kTransferIdIec6196621 = 13,
    kTransferId10BitBt2020 = 14,
    kTransferId12BitBt2020 = 15,
    kTransferIdSmpteSt2084 = 16,
    kTransferIdSmpteSt4281 = 17,
    kTransferIdAribStdB67 = 18,  // AKA hybrid-log gamma, HLG.

    kTransferIdLastStandardValue = kTransferIdSmpteSt4281,

    // Chrome-specific values start at 1000.
    kTransferIdUnknown = 1000,
    kTransferIdGamma24,

    // This is an ad-hoc transfer function that decodes SMPTE 2084 content
    // into a 0-1 range more or less suitable for viewing on a non-hdr
    // display.
    kTransferIdSmpteSt2084NonHdr,

    // TODO(hubbe): Need to store an approximation of the gamma function(s).
    kTransferIdCustom,
    kTransferIdLast = kTransferIdCustom,
  };

  enum MatrixID {
    // The first 0-255 values should match the H264 specification (see Table E-5
    // Matrix Coefficients in https://www.itu.int/rec/T-REC-H.264/en).
    kMatrixIdRgb = 0,
    kMatrixIdBt709 = 1,
    kMatrixIdUnspecified = 2,
    kMatrixIdReserved = 3,
    kMatrixIdFcc = 4,
    kMatrixIdBt470Bg = 5,
    kMatrixIdSmpte170M = 6,
    kMatrixIdSmpte240M = 7,
    kMatrixIdYCgCo = 8,
    kMatrixIdBt2020NonconstantLuminance = 9,
    kMatrixIdBt2020ConstantLuminance = 10,
    kMatrixIdYDzDx = 11,

    kMatrixIdLastStandardValue = kMatrixIdYDzDx,

    // Chrome-specific values start at 1000
    kMatrixIdUnknown = 1000,
    kMatrixIdLast = kMatrixIdUnknown,
  };

  // This corresponds to the WebM Range enum which is part of WebM color data
  // (see http://www.webmproject.org/docs/container/#Range).
  // H.264 only uses a bool, which corresponds to the LIMITED/FULL values.
  // Chrome-specific values start at 1000.
  enum RangeID {
    // Range is not explicitly specified / unknown.
    kRangeIdUnspecified = 0,

    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    kRangeIdLimited = 1,

    // Full RGB color range with RGB values from 0 to 255.
    kRangeIdFull = 2,

    // Range is defined by TransferID/MatrixID.
    kRangeIdDerived = 3,

    kRangeIdLast = kRangeIdDerived
  };

  ColorSpace();
  ColorSpace(PrimaryID primaries, TransferID transfer, MatrixID matrix,
             RangeID full_range);
  ColorSpace(const ColorSpace& other);
  ColorSpace(int primaries, int transfer, int matrix, RangeID full_range);
  ~ColorSpace();

  static PrimaryID PrimaryIDFromInt(int primary_id);
  static TransferID TransferIDFromInt(int transfer_id);
  static MatrixID MatrixIDFromInt(int matrix_id);

  static ColorSpace CreateXYZD50();

  // TODO: Remove these, and replace with more generic constructors.
  static ColorSpace CreateJpeg();
  static ColorSpace CreateREC601();
  static ColorSpace CreateREC709();

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;

  PrimaryID primaries() const { return primaries_; }
  TransferID transfer() const { return transfer_; }
  MatrixID matrix() const { return matrix_; }
  RangeID range() const { return range_; }

  const CustomPrimaryMatrix& custom_primary_matrix() const {
    DCHECK_EQ(primaries_, kPrimaryIdCustom);
    return custom_primary_matrix_;
  }

 private:
  PrimaryID primaries_;
  TransferID transfer_;
  MatrixID matrix_;
  RangeID range_;

  // Only used if primaries_ == kPrimaryIdCustom
  float custom_primary_matrix_[12];
};

}  // namespace gfx

#endif  // COBALT_MEDIA_BASE_COLOR_SPACE_H_
