// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/formats/webm/webm_colour_parser.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "cobalt/media/formats/webm/webm_constants.h"

namespace cobalt {
namespace media {

// The definitions below are copied from the current libwebm top-of-tree.
// Chromium's third_party/libwebm doesn't have these yet. We can probably just
// include libwebm header directly in the future.
// ---- Begin copy/paste from libwebm/webm_parser/include/webm/dom_types.h ----

/**
 A parsed \WebMID{MatrixCoefficients} element.

 Matroska/WebM adopted these values from Table 4 of ISO/IEC 23001-8:2013/DCOR1.
 See that document for further details.
 */
enum MatrixCoefficient {
  /**
   The identity matrix.

   Typically used for GBR (often referred to as RGB); however, may also be used
   for YZX (often referred to as XYZ).
   */
  kMatrixCoefficientRgb = 0,

  /**
   Rec. ITU-R BT.709-5.
   */
  kMatrixCoefficientBt709 = 1,

  /**
   Image characteristics are unknown or are determined by the application.
   */
  kMatrixCoefficientUnspecified = 2,

  /**
   United States Federal Communications Commission Title 47 Code of Federal
   Regulations (2003) 73.682 (a) (20).
   */
  kMatrixCoefficientFcc = 4,

  /**
   Rec. ITU-R BT.470‑6 System B, G (historical).
   */
  kMatrixCoefficientBt470Bg = 5,

  /**
   Society of Motion Picture and Television Engineers 170M (2004).
   */
  kMatrixCoefficientSmpte170M = 6,

  /**
   Society of Motion Picture and Television Engineers 240M (1999).
   */
  kMatrixCoefficientSmpte240M = 7,

  /**
   YCgCo.
   */
  kMatrixCoefficientYCgCo = 8,

  /**
   Rec. ITU-R BT.2020 (non-constant luminance).
   */
  kMatrixCoefficientBt2020NonconstantLuminance = 9,

  /**
   Rec. ITU-R BT.2020 (constant luminance).
   */
  kMatrixCoefficientBt2020ConstantLuminance = 10,
};

/**
 A parsed \WebMID{Range} element.
 */
enum Range {
  /**
   Unspecified.
   */
  kRangeUnspecified = 0,

  /**
   Broadcast range.
   */
  kRangeBroadcast = 1,

  /**
   Full range (no clipping).
   */
  kRangeFull = 2,

  /**
   Defined by MatrixCoefficient/TransferCharacteristic.
   */
  kRangeDerived = 3,
};

/**
 A parsed \WebMID{TransferCharacteristic} element.

 Matroska/WebM adopted these values from Table 3 of ISO/IEC 23001-8:2013/DCOR1.
 See that document for further details.
 */
enum TransferCharacteristic {
  /**
   Rec. ITU-R BT.709-6.
   */
  kTransferCharacteristicBt709 = 1,

  /**
   Image characteristics are unknown or are determined by the application.
   */
  kTransferCharacteristicUnspecified = 2,

  /**
   Rec. ITU‑R BT.470‑6 System M (historical) with assumed display gamma 2.2.
   */
  kTransferCharacteristicGamma22curve = 4,

  /**
   Rec. ITU‑R BT.470-6 System B, G (historical) with assumed display gamma 2.8.
   */
  kTransferCharacteristicGamma28curve = 5,

  /**
   Society of Motion Picture and Television Engineers 170M (2004).
   */
  kTransferCharacteristicSmpte170M = 6,

  /**
   Society of Motion Picture and Television Engineers 240M (1999).
   */
  kTransferCharacteristicSmpte240M = 7,

  /**
   Linear transfer characteristics.
   */
  kTransferCharacteristicLinear = 8,

  /**
   Logarithmic transfer characteristic (100:1 range).
   */
  kTransferCharacteristicLog = 9,

  /**
   Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range).
   */
  kTransferCharacteristicLogSqrt = 10,

  /**
   IEC 61966-2-4.
   */
  kTransferCharacteristicIec6196624 = 11,

  /**
   Rec. ITU‑R BT.1361-0 extended colour gamut system (historical).
   */
  kTransferCharacteristicBt1361ExtendedColourGamut = 12,

  /**
   IEC 61966-2-1 sRGB or sYCC.
   */
  kTransferCharacteristicIec6196621 = 13,

  /**
   Rec. ITU-R BT.2020-2 (10-bit system).
   */
  kTransferCharacteristic10BitBt2020 = 14,

  /**
   Rec. ITU-R BT.2020-2 (12-bit system).
   */
  kTransferCharacteristic12BitBt2020 = 15,

  /**
   Society of Motion Picture and Television Engineers ST 2084.
   */
  kTransferCharacteristicSmpteSt2084 = 16,

  /**
   Society of Motion Picture and Television Engineers ST 428-1.
   */
  kTransferCharacteristicSmpteSt4281 = 17,

  /**
   Association of Radio Industries and Businesses (ARIB) STD-B67.
   */
  kTransferCharacteristicAribStdB67Hlg = 18,
};

/**
 A parsed \WebMID{Primaries} element.

 Matroska/WebM adopted these values from Table 2 of ISO/IEC 23001-8:2013/DCOR1.
 See that document for further details.
 */
enum Primary {
  /**
   Rec. ITU‑R BT.709-6.
   */
  kPrimaryBt709 = 1,

  /**
   Image characteristics are unknown or are determined by the application.
   */
  kPrimaryUnspecified = 2,

  /**
   Rec. ITU‑R BT.470‑6 System M (historical).
   */
  kPrimaryBt470M = 4,

  /**
   Rec. ITU‑R BT.470‑6 System B, G (historical).
   */
  kPrimaryBt470Bg = 5,

  /**
   Society of Motion Picture and Television Engineers 170M (2004).
   */
  kPrimarySmpte170M = 6,

  /**
   Society of Motion Picture and Television Engineers 240M (1999).
   */
  kPrimarySmpte240M = 7,

  /**
   Generic film.
   */
  kPrimaryFilm = 8,

  /**
   Rec. ITU-R BT.2020-2.
   */
  kPrimaryBt2020 = 9,

  /**
   Society of Motion Picture and Television Engineers ST 428-1.
   */
  kPrimarySmpteSt4281 = 10,

  /**
   JEDEC P22 phosphors/EBU Tech. 3213-E (1975).
   */
  kPrimaryJedecP22Phosphors = 22,
};

// ---- End copy/paste from libwebm/webm_parser/include/webm/dom_types.h ----

// Ensure that libwebm enum values match enums in gfx::ColorSpace.
#define STATIC_ASSERT_ENUM(a, b)                                              \
  COMPILE_ASSERT(static_cast<int>(a) == static_cast<int>(gfx::ColorSpace::b), \
                 mismatching_enums)

STATIC_ASSERT_ENUM(kMatrixCoefficientRgb, kMatrixIdRgb);
STATIC_ASSERT_ENUM(kMatrixCoefficientBt709, kMatrixIdBt709);
STATIC_ASSERT_ENUM(kMatrixCoefficientUnspecified, kMatrixIdUnspecified);
STATIC_ASSERT_ENUM(kMatrixCoefficientFcc, kMatrixIdFcc);
STATIC_ASSERT_ENUM(kMatrixCoefficientBt470Bg, kMatrixIdBt470Bg);
STATIC_ASSERT_ENUM(kMatrixCoefficientSmpte170M, kMatrixIdSmpte170M);
STATIC_ASSERT_ENUM(kMatrixCoefficientSmpte240M, kMatrixIdSmpte240M);
STATIC_ASSERT_ENUM(kMatrixCoefficientYCgCo, kMatrixIdYCgCo);
STATIC_ASSERT_ENUM(kMatrixCoefficientBt2020NonconstantLuminance,
                   kMatrixIdBt2020NonconstantLuminance);
STATIC_ASSERT_ENUM(kMatrixCoefficientBt2020ConstantLuminance,
                   kMatrixIdBt2020ConstantLuminance);

gfx::ColorSpace::MatrixID FromWebMMatrixCoefficient(MatrixCoefficient c) {
  return static_cast<gfx::ColorSpace::MatrixID>(c);
}

STATIC_ASSERT_ENUM(kRangeUnspecified, kRangeIdUnspecified);
STATIC_ASSERT_ENUM(kRangeBroadcast, kRangeIdLimited);
STATIC_ASSERT_ENUM(kRangeFull, kRangeIdFull);
STATIC_ASSERT_ENUM(kRangeDerived, kRangeIdDerived);

gfx::ColorSpace::RangeID FromWebMRange(Range range) {
  return static_cast<gfx::ColorSpace::RangeID>(range);
}

STATIC_ASSERT_ENUM(kTransferCharacteristicBt709, kTransferIdBt709);
STATIC_ASSERT_ENUM(kTransferCharacteristicUnspecified, kTransferIdUnspecified);
STATIC_ASSERT_ENUM(kTransferCharacteristicGamma22curve, kTransferIdGamma22);
STATIC_ASSERT_ENUM(kTransferCharacteristicGamma28curve, kTransferIdGamma28);
STATIC_ASSERT_ENUM(kTransferCharacteristicSmpte170M, kTransferIdSmpte170M);
STATIC_ASSERT_ENUM(kTransferCharacteristicSmpte240M, kTransferIdSmpte240M);
STATIC_ASSERT_ENUM(kTransferCharacteristicLinear, kTransferIdLinear);
STATIC_ASSERT_ENUM(kTransferCharacteristicLog, kTransferIdLog);
STATIC_ASSERT_ENUM(kTransferCharacteristicLogSqrt, kTransferIdLogSqrt);
STATIC_ASSERT_ENUM(kTransferCharacteristicIec6196624, kTransferIdIec6196624);
STATIC_ASSERT_ENUM(kTransferCharacteristicBt1361ExtendedColourGamut,
                   kTransferIdBt1361Ecg);
STATIC_ASSERT_ENUM(kTransferCharacteristicIec6196621, kTransferIdIec6196621);
STATIC_ASSERT_ENUM(kTransferCharacteristic10BitBt2020, kTransferId10BitBt2020);
STATIC_ASSERT_ENUM(kTransferCharacteristic12BitBt2020, kTransferId12BitBt2020);
STATIC_ASSERT_ENUM(kTransferCharacteristicSmpteSt2084, kTransferIdSmpteSt2084);
STATIC_ASSERT_ENUM(kTransferCharacteristicSmpteSt4281, kTransferIdSmpteSt4281);
STATIC_ASSERT_ENUM(kTransferCharacteristicAribStdB67Hlg, kTransferIdAribStdB67);

gfx::ColorSpace::TransferID FromWebMTransferCharacteristic(
    TransferCharacteristic tc) {
  return static_cast<gfx::ColorSpace::TransferID>(tc);
}

STATIC_ASSERT_ENUM(kPrimaryBt709, kPrimaryIdBt709);
STATIC_ASSERT_ENUM(kPrimaryUnspecified, kPrimaryIdUnspecified);
STATIC_ASSERT_ENUM(kPrimaryBt470M, kPrimaryIdBt470M);
STATIC_ASSERT_ENUM(kPrimaryBt470Bg, kPrimaryIdBt470Bg);
STATIC_ASSERT_ENUM(kPrimarySmpte170M, kPrimaryIdSmpte170M);
STATIC_ASSERT_ENUM(kPrimarySmpte240M, kPrimaryIdSmpte240M);
STATIC_ASSERT_ENUM(kPrimaryFilm, kPrimaryIdFilm);
STATIC_ASSERT_ENUM(kPrimaryBt2020, kPrimaryIdBt2020);
STATIC_ASSERT_ENUM(kPrimarySmpteSt4281, kPrimaryIdSmpteSt4281);

gfx::ColorSpace::PrimaryID FromWebMPrimary(Primary primary) {
  return static_cast<gfx::ColorSpace::PrimaryID>(primary);
}

WebMColorMetadata::WebMColorMetadata()
    : BitsPerChannel(0),
      ChromaSubsamplingHorz(0),
      ChromaSubsamplingVert(0),
      CbSubsamplingHorz(0),
      CbSubsamplingVert(0),
      ChromaSitingHorz(0),
      ChromaSitingVert(0) {}

bool WebMColorMetadata::operator==(const WebMColorMetadata& rhs) const {
  return (BitsPerChannel == rhs.BitsPerChannel &&
          ChromaSubsamplingHorz == rhs.ChromaSubsamplingHorz &&
          ChromaSubsamplingVert == rhs.ChromaSubsamplingVert &&
          CbSubsamplingHorz == rhs.CbSubsamplingHorz &&
          CbSubsamplingVert == rhs.CbSubsamplingVert &&
          ChromaSitingHorz == rhs.ChromaSitingHorz &&
          ChromaSitingVert == rhs.ChromaSitingVert &&
          color_space == rhs.color_space && hdr_metadata == rhs.hdr_metadata);
}

WebMMasteringMetadataParser::WebMMasteringMetadataParser() {}
WebMMasteringMetadataParser::~WebMMasteringMetadataParser() {}

bool WebMMasteringMetadataParser::OnFloat(int id, double val) {
  switch (id) {
    case kWebMIdPrimaryRChromaticityX:
      mastering_metadata_.primary_r_chromaticity_x = val;
      break;
    case kWebMIdPrimaryRChromaticityY:
      mastering_metadata_.primary_r_chromaticity_y = val;
      break;
    case kWebMIdPrimaryGChromaticityX:
      mastering_metadata_.primary_g_chromaticity_x = val;
      break;
    case kWebMIdPrimaryGChromaticityY:
      mastering_metadata_.primary_g_chromaticity_y = val;
      break;
    case kWebMIdPrimaryBChromaticityX:
      mastering_metadata_.primary_b_chromaticity_x = val;
      break;
    case kWebMIdPrimaryBChromaticityY:
      mastering_metadata_.primary_b_chromaticity_y = val;
      break;
    case kWebMIdWhitePointChromaticityX:
      mastering_metadata_.white_point_chromaticity_x = val;
      break;
    case kWebMIdWhitePointChromaticityY:
      mastering_metadata_.white_point_chromaticity_y = val;
      break;
    case kWebMIdLuminanceMax:
      mastering_metadata_.luminance_max = val;
      break;
    case kWebMIdLuminanceMin:
      mastering_metadata_.luminance_min = val;
      break;
    default:
      DVLOG(1) << "Unexpected id in MasteringMetadata: 0x" << std::hex << id;
      return false;
  }
  return true;
}

WebMColourParser::WebMColourParser() : mastering_metadata_parsed_(false) {
  Reset();
}

WebMColourParser::~WebMColourParser() {}

void WebMColourParser::Reset() {
  matrix_coefficients_ = -1;
  bits_per_channel_ = -1;
  chroma_subsampling_horz_ = -1;
  chroma_subsampling_vert_ = -1;
  cb_subsampling_horz_ = -1;
  cb_subsampling_vert_ = -1;
  chroma_siting_horz_ = -1;
  chroma_siting_vert_ = -1;
  range_ = -1;
  transfer_characteristics_ = -1;
  primaries_ = -1;
  max_cll_ = -1;
  max_fall_ = -1;
}

WebMParserClient* WebMColourParser::OnListStart(int id) {
  if (id == kWebMIdMasteringMetadata) {
    mastering_metadata_parsed_ = false;
    return &mastering_metadata_parser_;
  }

  return this;
}

bool WebMColourParser::OnListEnd(int id) {
  if (id == kWebMIdMasteringMetadata) mastering_metadata_parsed_ = true;
  return true;
}

bool WebMColourParser::OnUInt(int id, int64_t val) {
  int64_t* dst = NULL;

  switch (id) {
    case kWebMIdMatrixCoefficients:
      dst = &matrix_coefficients_;
      break;
    case kWebMIdBitsPerChannel:
      dst = &bits_per_channel_;
      break;
    case kWebMIdChromaSubsamplingHorz:
      dst = &chroma_subsampling_horz_;
      break;
    case kWebMIdChromaSubsamplingVert:
      dst = &chroma_subsampling_vert_;
      break;
    case kWebMIdCbSubsamplingHorz:
      dst = &cb_subsampling_horz_;
      break;
    case kWebMIdCbSubsamplingVert:
      dst = &cb_subsampling_vert_;
      break;
    case kWebMIdChromaSitingHorz:
      dst = &chroma_siting_horz_;
      break;
    case kWebMIdChromaSitingVert:
      dst = &chroma_siting_vert_;
      break;
    case kWebMIdRange:
      dst = &range_;
      break;
    case kWebMIdTransferCharacteristics:
      dst = &transfer_characteristics_;
      break;
    case kWebMIdPrimaries:
      dst = &primaries_;
      break;
    case kWebMIdMaxCLL:
      dst = &max_cll_;
      break;
    case kWebMIdMaxFALL:
      dst = &max_fall_;
      break;
    default:
      return true;
  }

  DCHECK(dst);
  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified ("
               << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

WebMColorMetadata WebMColourParser::GetWebMColorMetadata() const {
  WebMColorMetadata color_metadata;

  if (bits_per_channel_ != -1)
    color_metadata.BitsPerChannel = bits_per_channel_;

  if (chroma_subsampling_horz_ != -1)
    color_metadata.ChromaSubsamplingHorz = chroma_subsampling_horz_;
  if (chroma_subsampling_vert_ != -1)
    color_metadata.ChromaSubsamplingVert = chroma_subsampling_vert_;
  if (cb_subsampling_horz_ != -1)
    color_metadata.CbSubsamplingHorz = cb_subsampling_horz_;
  if (cb_subsampling_vert_ != -1)
    color_metadata.CbSubsamplingVert = cb_subsampling_vert_;
  if (chroma_siting_horz_ != -1)
    color_metadata.ChromaSitingHorz = chroma_siting_horz_;
  if (chroma_siting_vert_ != -1)
    color_metadata.ChromaSitingVert = chroma_siting_vert_;

  gfx::ColorSpace::MatrixID matrix_id = gfx::ColorSpace::kMatrixIdUnspecified;
  if (matrix_coefficients_ != -1)
    matrix_id = FromWebMMatrixCoefficient(
        static_cast<MatrixCoefficient>(matrix_coefficients_));

  gfx::ColorSpace::RangeID range_id = gfx::ColorSpace::kRangeIdUnspecified;
  if (range_ != -1) range_id = FromWebMRange(static_cast<Range>(range_));

  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::kTransferIdUnspecified;
  if (transfer_characteristics_ != -1)
    transfer_id = FromWebMTransferCharacteristic(
        static_cast<TransferCharacteristic>(transfer_characteristics_));

  gfx::ColorSpace::PrimaryID primary_id =
      gfx::ColorSpace::kPrimaryIdUnspecified;
  if (primaries_ != -1)
    primary_id = FromWebMPrimary(static_cast<Primary>(primaries_));

  color_metadata.color_space =
      gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);

  if (max_cll_ != -1) color_metadata.hdr_metadata.max_cll = max_cll_;

  if (max_fall_ != -1) color_metadata.hdr_metadata.max_fall = max_fall_;

  if (mastering_metadata_parsed_)
    color_metadata.hdr_metadata.mastering_metadata =
        mastering_metadata_parser_.GetMasteringMetadata();

  return color_metadata;
}

}  // namespace media
}  // namespace cobalt
