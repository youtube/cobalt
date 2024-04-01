// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"

#include <va/va.h>
#include <va/va_enc_h264.h>

#include <utility>

#include "base/bits.h"
#include "base/cxx17_backports.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/h264_level_limits.h"

namespace media {
namespace {
// An IDR every 2048 frames (must be >= 16 per spec), no I frames and no B
// frames. We choose IDR period to equal MaxFrameNum so it must be a power of 2.
// Produce an IDR at least once per this many frames. Must be >= 16 (per spec).
constexpr uint32_t kIDRPeriod = 2048;
static_assert(kIDRPeriod >= 16u, "idr_period_frames must be >= 16");
// Produce an I frame at least once per this many frames.
constexpr uint32_t kIPeriod = 0;
// How often do we need to have either an I or a P frame in the stream.
// A period of 1 implies no B frames.
constexpr uint32_t kIPPeriod = 1;

// The qp range is 0-51 in H264. Select 26 because of the center value.
constexpr uint8_t kDefaultQP = 26;
// Note: Webrtc default values are 24 and 37 respectively, see
// h264_encoder_impl.cc.
// These values are selected to make our VEA tests pass.
constexpr uint8_t kMinQP = 24;
constexpr uint8_t kMaxQP = 42;

// Subjectively chosen bitrate window size for rate control, in ms.
constexpr uint32_t kCPBWindowSizeMs = 1500;

// Subjectively chosen.
// Generally use up to 2 reference frames.
constexpr size_t kMaxNumReferenceFrames = 2;
constexpr size_t kMaxRefIdxL0Size = kMaxNumReferenceFrames;

// HRD parameters (ch. E.2.2 in H264 spec).
constexpr int kBitRateScale = 0;  // bit_rate_scale for SPS HRD parameters.
constexpr int kCPBSizeScale = 0;  // cpb_size_scale for SPS HRD parameters.

// 4:2:0
constexpr int kChromaFormatIDC = 1;

constexpr uint8_t kMinSupportedH264TemporalLayers = 2;
constexpr uint8_t kMaxSupportedH264TemporalLayers = 3;

void FillVAEncRateControlParams(
    uint32_t bps,
    uint32_t window_size,
    uint32_t initial_qp,
    uint32_t min_qp,
    uint32_t max_qp,
    uint32_t framerate,
    uint32_t buffer_size,
    VAEncMiscParameterRateControl& rate_control_param,
    VAEncMiscParameterFrameRate& framerate_param,
    VAEncMiscParameterHRD& hrd_param) {
  memset(&rate_control_param, 0, sizeof(rate_control_param));
  rate_control_param.bits_per_second = bps;
  rate_control_param.window_size = window_size;
  rate_control_param.initial_qp = initial_qp;
  rate_control_param.min_qp = min_qp;
  rate_control_param.max_qp = max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  memset(&framerate_param, 0, sizeof(framerate_param));
  framerate_param.framerate = framerate;

  memset(&hrd_param, 0, sizeof(hrd_param));
  hrd_param.buffer_size = buffer_size;
  hrd_param.initial_buffer_fullness = buffer_size / 2;
}

// TODO(hiroh): Put this to media/gpu/gpu_video_encode_accelerator_helpers.h
VideoBitrateAllocation GetDefaultVideoBitrateAllocation(
    const VideoEncodeAccelerator::Config& config) {
  VideoBitrateAllocation bitrate_allocation;
  if (!config.HasTemporalLayer() && !config.HasSpatialLayer()) {
    bitrate_allocation.SetBitrate(0, 0, config.bitrate.target());
    return bitrate_allocation;
  }

  auto& spatial_layer = config.spatial_layers[0];
  const size_t num_temporal_layers = spatial_layer.num_of_temporal_layers;
  // TODO(hiroh): support one temporal layer when moving this function.
  DCHECK_GE(num_temporal_layers, 2u);
  constexpr double kTemporalLayersBitrateScaleFactors[][3] = {
      {0.60, 0.40, 0.00},  // For two temporal layers.
      {0.50, 0.20, 0.30},  // For three temporal layers.
  };

  const uint32_t bitrate_bps = spatial_layer.bitrate_bps;
  for (size_t tid = 0; tid < num_temporal_layers; ++tid) {
    const double factor =
        kTemporalLayersBitrateScaleFactors[num_temporal_layers - 2][tid];
    bitrate_allocation.SetBitrate(
        0, tid, base::checked_cast<int>(bitrate_bps * factor));
  }

  return bitrate_allocation;
}

static scoped_refptr<base::RefCountedBytes> MakeRefCountedBytes(void* ptr,
                                                                size_t size) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(ptr), size);
}

static void InitVAPictureH264(VAPictureH264* va_pic) {
  *va_pic = {};
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

// Updates |frame_num| as spec section 7.4.3 and sets it to |pic.frame_num|.
void UpdateAndSetFrameNum(H264Picture& pic, unsigned int& frame_num) {
  if (pic.idr)
    frame_num = 0;
  else if (pic.ref)
    frame_num++;
  DCHECK_LT(frame_num, kIDRPeriod);
  pic.frame_num = frame_num;
}

// Updates and fills variables in |pic|, |frame_num| and |ref_frame_idx| for
// temporal layer encoding. |frame_num| is the frame_num in H.264 spec for
// |pic|. |ref_frame_idx| is the index in |ref_pic_list0| of the frame
// referenced by |pic|.
void UpdatePictureForTemporalLayerEncoding(
    const size_t num_layers,
    H264Picture& pic,
    unsigned int& frame_num,
    absl::optional<size_t>& ref_frame_idx,
    const unsigned int num_encoded_frames,
    const base::circular_deque<scoped_refptr<H264Picture>>& ref_pic_list0) {
  DCHECK_GE(num_layers, kMinSupportedH264TemporalLayers);
  DCHECK_LE(num_layers, kMaxSupportedH264TemporalLayers);
  constexpr size_t kTemporalLayerCycle = 4;
  constexpr std::pair<H264Metadata, bool>
      kFrameMetadata[][kTemporalLayerCycle] = {
          {
              // For two temporal layers.
              {{.temporal_idx = 0, .layer_sync = false}, true},
              {{.temporal_idx = 1, .layer_sync = true}, false},
              {{.temporal_idx = 0, .layer_sync = false}, true},
              {{.temporal_idx = 1, .layer_sync = true}, false},
          },
          {
              // For three temporal layers.
              {{.temporal_idx = 0, .layer_sync = false}, true},
              {{.temporal_idx = 2, .layer_sync = true}, false},
              {{.temporal_idx = 1, .layer_sync = true}, true},
              {{.temporal_idx = 2, .layer_sync = false}, false},
          }};

  // Fill |pic.metadata_for_encoding| and |pic.ref|.
  H264Metadata metadata;
  std::tie(pic.metadata_for_encoding.emplace(), pic.ref) =
      kFrameMetadata[num_layers - 2][num_encoded_frames % kTemporalLayerCycle];

  UpdateAndSetFrameNum(pic, frame_num);

  if (pic.idr)
    return;

  // Fill reference frame related variables in |pic| and |ref_frame_idx|.
  DCHECK_EQ(pic.ref_pic_list_modification_flag_l0, 0);
  DCHECK_EQ(pic.abs_diff_pic_num_minus1, 0);
  DCHECK(!ref_pic_list0.empty());
  if (metadata.temporal_idx == 0)
    ref_frame_idx = base::checked_cast<size_t>(ref_pic_list0.size() - 1);
  else
    ref_frame_idx = 0;

  DCHECK_LT(*ref_frame_idx, ref_pic_list0.size());
  const H264Picture& ref_frame_pic = *ref_pic_list0[*ref_frame_idx];
  const int abs_diff_pic_num = pic.frame_num - ref_frame_pic.frame_num;
  if (*ref_frame_idx != 0 && abs_diff_pic_num > 0) {
    pic.ref_pic_list_modification_flag_l0 = 1;
    pic.abs_diff_pic_num_minus1 = abs_diff_pic_num - 1;
  }
}
}  // namespace

H264VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      max_num_ref_frames(kMaxNumReferenceFrames),
      max_ref_pic_list0_size(kMaxRefIdxL0Size) {}

H264VaapiVideoEncoderDelegate::H264VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb) {}

H264VaapiVideoEncoderDelegate::~H264VaapiVideoEncoderDelegate() {
  // H264VaapiVideoEncoderDelegate can be destroyed on any thread.
}

bool H264VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (config.output_profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_HIGH:
      break;

    default:
      NOTIMPLEMENTED() << "Unsupported profile "
                       << GetProfileName(config.output_profile);
      return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  if (config.HasSpatialLayer()) {
    DVLOGF(1) << "Spatial layer encoding is not supported";
    return false;
  }
  if (config.HasTemporalLayer()) {
    bool support_temporal_layer = false;
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)
    VAImplementation implementation = VaapiWrapper::GetImplementationType();
    // TODO(b/199487660): Enable H.264 temporal layer encoding on AMD once their
    // drivers support them.
    support_temporal_layer =
        base::FeatureList::IsEnabled(kVaapiH264TemporalLayerHWEncoding) &&
        (implementation == VAImplementation::kIntelI965 ||
         implementation == VAImplementation::kIntelIHD);
#endif
    if (!support_temporal_layer) {
      DVLOGF(1) << "Temporal layer encoding is not supported";
      return false;
    }
  }

  visible_size_ = config.input_visible_size;
  // For 4:2:0, the pixel sizes have to be even.
  if ((visible_size_.width() % 2 != 0) || (visible_size_.height() % 2 != 0)) {
    DVLOGF(1) << "The pixel sizes are not even: " << visible_size_.ToString();
    return false;
  }
  constexpr size_t kH264MacroblockSizeInPixels = 16;
  coded_size_ = gfx::Size(
      base::bits::AlignUp(visible_size_.width(), kH264MacroblockSizeInPixels),
      base::bits::AlignUp(visible_size_.height(), kH264MacroblockSizeInPixels));
  mb_width_ = coded_size_.width() / kH264MacroblockSizeInPixels;
  mb_height_ = coded_size_.height() / kH264MacroblockSizeInPixels;

  profile_ = config.output_profile;
  level_ = config.h264_output_level.value_or(H264SPS::kLevelIDC4p0);
  uint32_t initial_framerate = config.initial_framerate.value_or(
      VideoEncodeAccelerator::kDefaultFramerate);

  // Checks if |level_| is valid. If it is invalid, set |level_| to a minimum
  // level that comforts Table A-1 in H.264 spec with specified bitrate,
  // framerate and dimension.
  if (!CheckH264LevelLimits(profile_, level_, config.bitrate.target(),
                            initial_framerate, mb_width_ * mb_height_)) {
    absl::optional<uint8_t> valid_level =
        FindValidH264Level(profile_, config.bitrate.target(), initial_framerate,
                           mb_width_ * mb_height_);
    if (!valid_level) {
      VLOGF(1) << "Could not find a valid h264 level for"
               << " profile=" << profile_
               << " bitrate=" << config.bitrate.target()
               << " framerate=" << initial_framerate
               << " size=" << config.input_visible_size.ToString();
      return false;
    }
    level_ = *valid_level;
  }

  num_temporal_layers_ = 1;
  if (config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    num_temporal_layers_ = config.spatial_layers[0].num_of_temporal_layers;
    if (num_temporal_layers_ > kMaxSupportedH264TemporalLayers ||
        num_temporal_layers_ < kMinSupportedH264TemporalLayers) {
      DVLOGF(1) << "Unsupported number of temporal layers: "
                << base::strict_cast<size_t>(num_temporal_layers_);
      return false;
    }

    // |ave_config.max_num_ref_frames| represents the maximum number of
    // reference frames for both the reference picture list 0 (bottom 16 bits)
    // and the reference picture list 1 (top 16 bits) in H264 encoding.
    const size_t max_p_frame_slots = ave_config.max_num_ref_frames & 0xffff;
    if (max_p_frame_slots < num_temporal_layers_ - 1) {
      DVLOGF(1) << "P frame slots is too short: " << max_p_frame_slots;
      return false;
    }
  }

  curr_params_.max_ref_pic_list0_size =
      num_temporal_layers_ > 1u
          ? num_temporal_layers_ - 1
          : std::min(kMaxRefIdxL0Size, ave_config.max_num_ref_frames & 0xffff);
  curr_params_.max_num_ref_frames =
      std::min(kMaxNumReferenceFrames, curr_params_.max_ref_pic_list0_size);

  bool submit_packed_sps = false;
  bool submit_packed_pps = false;
  bool submit_packed_slice = false;
  if (!vaapi_wrapper_->GetSupportedPackedHeaders(
          config.output_profile, submit_packed_sps, submit_packed_pps,
          submit_packed_slice)) {
    DVLOGF(1) << "Failed getting supported packed headers";
    return false;
  }

  // Submit packed headers only if packed SPS, PPS and slice header all are
  // supported.
  submit_packed_headers_ =
      submit_packed_sps && submit_packed_pps && submit_packed_slice;
  if (submit_packed_headers_) {
    packed_sps_ = base::MakeRefCounted<H264BitstreamBuffer>();
    packed_pps_ = base::MakeRefCounted<H264BitstreamBuffer>();
  } else {
    DVLOGF(2) << "Packed headers are not submitted to a driver";
  }

  UpdateSPS();
  UpdatePPS();

  return UpdateRates(GetDefaultVideoBitrateAllocation(config),
                     initial_framerate);
}

gfx::Size H264VaapiVideoEncoderDelegate::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t H264VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return curr_params_.max_num_ref_frames;
}

std::vector<gfx::Size> H264VaapiVideoEncoderDelegate::GetSVCLayerResolutions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return {visible_size_};
}

BitstreamBufferMetadata H264VaapiVideoEncoderDelegate::GetMetadata(
    EncodeJob* encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto metadata =
      VaapiVideoEncoderDelegate::GetMetadata(encode_job, payload_size);
  auto picture = GetPicture(encode_job);
  DCHECK(picture);

  metadata.h264 = picture->metadata_for_encoding;

  return metadata;
}

bool H264VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<H264Picture> pic = GetPicture(encode_job);
  DCHECK(pic);

  if (encode_job->IsKeyframeRequested() || encoding_parameters_changed_)
    num_encoded_frames_ = 0;

  if (num_encoded_frames_ == 0) {
    pic->idr = true;
    // H264 spec mandates idr_pic_id to differ between two consecutive IDRs.
    idr_pic_id_ ^= 1;
    pic->idr_pic_id = idr_pic_id_;
    ref_pic_list0_.clear();

    encoding_parameters_changed_ = false;
    encode_job->ProduceKeyframe();
  }

  pic->type = pic->idr ? H264SliceHeader::kISlice : H264SliceHeader::kPSlice;

  absl::optional<size_t> ref_frame_index;
  if (num_temporal_layers_ > 1u) {
    UpdatePictureForTemporalLayerEncoding(num_temporal_layers_, *pic,
                                          frame_num_, ref_frame_index,
                                          num_encoded_frames_, ref_pic_list0_);
  } else {
    pic->ref = true;
    UpdateAndSetFrameNum(*pic, frame_num_);
  }

  pic->pic_order_cnt = num_encoded_frames_ * 2;
  pic->top_field_order_cnt = pic->pic_order_cnt;
  pic->pic_order_cnt_lsb = pic->pic_order_cnt;

  DVLOGF(4) << "Starting a new frame, type: " << pic->type
            << (encode_job->IsKeyframeRequested() ? " (keyframe)" : "")
            << " frame_num: " << pic->frame_num
            << " POC: " << pic->pic_order_cnt;

  // TODO(b/195407733): Use a software bitrate controller and specify QP.
  if (!SubmitFrameParameters(encode_job, curr_params_, current_sps_,
                             current_pps_, pic, ref_pic_list0_,
                             ref_frame_index)) {
    DVLOGF(1) << "Failed submitting frame parameters";
    return false;
  }

  if (pic->type == H264SliceHeader::kISlice && submit_packed_headers_) {
    // We always generate SPS and PPS with I(DR) frame. This will help for Seek
    // operation on the generated stream.
    if (!SubmitPackedHeaders(encode_job, packed_sps_, packed_pps_)) {
      DVLOGF(1) << "Failed submitting keyframe headers";
      return false;
    }
  }

  for (const auto& ref_pic : ref_pic_list0_)
    encode_job->AddReferencePicture(ref_pic);

  // Store the picture on the list of reference pictures and keep the list
  // below maximum size, dropping oldest references.
  if (pic->ref) {
    ref_pic_list0_.push_front(pic);
    const size_t max_num_ref_frames =
        base::checked_cast<size_t>(current_sps_.max_num_ref_frames);
    while (ref_pic_list0_.size() > max_num_ref_frames)
      ref_pic_list0_.pop_back();
  }

  num_encoded_frames_++;
  num_encoded_frames_ %= kIDRPeriod;
  return true;
}

bool H264VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint32_t bitrate = bitrate_allocation.GetSumBps();
  if (bitrate == 0 || framerate == 0)
    return false;

  if (curr_params_.bitrate_allocation == bitrate_allocation &&
      curr_params_.framerate == framerate) {
    return true;
  }
  VLOGF(2) << "New bitrate allocation: " << bitrate_allocation.ToString()
           << ", New framerate: " << framerate;

  curr_params_.bitrate_allocation = bitrate_allocation;
  curr_params_.framerate = framerate;

  base::CheckedNumeric<uint32_t> cpb_size_bits(bitrate);
  cpb_size_bits /= 1000;
  cpb_size_bits *= curr_params_.cpb_window_size_ms;
  if (!cpb_size_bits.AssignIfValid(&curr_params_.cpb_size_bits)) {
    VLOGF(1) << "Too large bitrate: " << bitrate_allocation.GetSumBps();
    return false;
  }

  bool previous_encoding_parameters_changed = encoding_parameters_changed_;

  UpdateSPS();

  // If SPS parameters are updated, it is required to send the SPS with IDR
  // frame. However, as a special case, we do not generate IDR frame if only
  // bitrate and framerate parameters are updated. This is safe because these
  // will not make a difference on decoder processing. The updated SPS will be
  // sent a next periodic or requested I(DR) frame. On the other hand, bitrate
  // and framerate parameter
  // changes must be affected for encoding. UpdateSPS()+SubmitFrameParameters()
  // shall apply them to an encoder properly.
  encoding_parameters_changed_ = previous_encoding_parameters_changed;
  return true;
}

void H264VaapiVideoEncoderDelegate::UpdateSPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memset(&current_sps_, 0, sizeof(H264SPS));

  // Spec A.2 and A.3.
  switch (profile_) {
    case H264PROFILE_BASELINE:
      // Due to https://crbug.com/345569, we don't distinguish between
      // constrained and non-constrained baseline profiles. Since many codecs
      // can't do non-constrained, and constrained is usually what we mean (and
      // it's a subset of non-constrained), default to it.
      current_sps_.profile_idc = H264SPS::kProfileIDCConstrainedBaseline;
      current_sps_.constraint_set0_flag = true;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_MAIN:
      current_sps_.profile_idc = H264SPS::kProfileIDCMain;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_HIGH:
      current_sps_.profile_idc = H264SPS::kProfileIDCHigh;
      break;
    default:
      NOTREACHED();
      return;
  }

  H264SPS::GetLevelConfigFromProfileLevel(profile_, level_,
                                          &current_sps_.level_idc,
                                          &current_sps_.constraint_set3_flag);

  current_sps_.seq_parameter_set_id = 0;
  current_sps_.chroma_format_idc = kChromaFormatIDC;

  current_sps_.log2_max_frame_num_minus4 =
      base::bits::Log2Ceiling(kIDRPeriod) - 4;
  current_sps_.pic_order_cnt_type = 0;
  current_sps_.log2_max_pic_order_cnt_lsb_minus4 =
      base::bits::Log2Ceiling(kIDRPeriod * 2) - 4;
  current_sps_.max_num_ref_frames = curr_params_.max_num_ref_frames;

  current_sps_.frame_mbs_only_flag = true;
  current_sps_.gaps_in_frame_num_value_allowed_flag = false;

  DCHECK_GT(mb_width_, 0u);
  DCHECK_GT(mb_height_, 0u);
  current_sps_.pic_width_in_mbs_minus1 = mb_width_ - 1;
  DCHECK(current_sps_.frame_mbs_only_flag);
  current_sps_.pic_height_in_map_units_minus1 = mb_height_ - 1;

  if (visible_size_ != coded_size_) {
    // Visible size differs from coded size, fill crop information.
    current_sps_.frame_cropping_flag = true;
    DCHECK(!current_sps_.separate_colour_plane_flag);
    // Spec table 6-1. Only 4:2:0 for now.
    DCHECK_EQ(current_sps_.chroma_format_idc, 1);
    // Spec 7.4.2.1.1. Crop is in crop units, which is 2 pixels for 4:2:0.
    const unsigned int crop_unit_x = 2;
    const unsigned int crop_unit_y = 2 * (2 - current_sps_.frame_mbs_only_flag);
    current_sps_.frame_crop_left_offset = 0;
    current_sps_.frame_crop_right_offset =
        (coded_size_.width() - visible_size_.width()) / crop_unit_x;
    current_sps_.frame_crop_top_offset = 0;
    current_sps_.frame_crop_bottom_offset =
        (coded_size_.height() - visible_size_.height()) / crop_unit_y;
  }

  current_sps_.vui_parameters_present_flag = true;
  current_sps_.timing_info_present_flag = true;
  current_sps_.num_units_in_tick = 1;
  current_sps_.time_scale =
      curr_params_.framerate * 2;  // See equation D-2 in spec.
  current_sps_.fixed_frame_rate_flag = true;

  current_sps_.nal_hrd_parameters_present_flag = true;
  // H.264 spec ch. E.2.2.
  current_sps_.cpb_cnt_minus1 = 0;
  current_sps_.bit_rate_scale = kBitRateScale;
  current_sps_.cpb_size_scale = kCPBSizeScale;
  current_sps_.bit_rate_value_minus1[0] =
      (curr_params_.bitrate_allocation.GetSumBps() >>
       (kBitRateScale + H264SPS::kBitRateScaleConstantTerm)) -
      1;
  current_sps_.cpb_size_value_minus1[0] =
      (curr_params_.cpb_size_bits >>
       (kCPBSizeScale + H264SPS::kCPBSizeScaleConstantTerm)) -
      1;
  current_sps_.cbr_flag[0] = true;
  current_sps_.initial_cpb_removal_delay_length_minus_1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.cpb_removal_delay_length_minus1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.dpb_output_delay_length_minus1 =
      H264SPS::kDefaultDPBOutputDelayLength - 1;
  current_sps_.time_offset_length = H264SPS::kDefaultTimeOffsetLength;
  current_sps_.low_delay_hrd_flag = false;

  if (submit_packed_headers_)
    GeneratePackedSPS();
  encoding_parameters_changed_ = true;
}

void H264VaapiVideoEncoderDelegate::UpdatePPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memset(&current_pps_, 0, sizeof(H264PPS));

  current_pps_.seq_parameter_set_id = current_sps_.seq_parameter_set_id;
  DCHECK_EQ(current_pps_.pic_parameter_set_id, 0);

  current_pps_.entropy_coding_mode_flag =
      current_sps_.profile_idc >= H264SPS::kProfileIDCMain;

  DCHECK_GT(curr_params_.max_ref_pic_list0_size, 0u);
  current_pps_.num_ref_idx_l0_default_active_minus1 =
      curr_params_.max_ref_pic_list0_size - 1;
  DCHECK_EQ(current_pps_.num_ref_idx_l1_default_active_minus1, 0);
  DCHECK_LE(curr_params_.initial_qp, 51u);
  current_pps_.pic_init_qp_minus26 =
      static_cast<int>(curr_params_.initial_qp) - 26;
  current_pps_.deblocking_filter_control_present_flag = true;
  current_pps_.transform_8x8_mode_flag =
      (current_sps_.profile_idc == H264SPS::kProfileIDCHigh);

  if (submit_packed_headers_)
    GeneratePackedPPS();
  encoding_parameters_changed_ = true;
}

void H264VaapiVideoEncoderDelegate::GeneratePackedSPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(submit_packed_headers_);
  DCHECK(packed_sps_);

  packed_sps_->Reset();

  packed_sps_->BeginNALU(H264NALU::kSPS, 3);

  packed_sps_->AppendBits(8, current_sps_.profile_idc);
  packed_sps_->AppendBool(current_sps_.constraint_set0_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set1_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set2_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set3_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set4_flag);
  packed_sps_->AppendBool(current_sps_.constraint_set5_flag);
  packed_sps_->AppendBits(2, 0);  // reserved_zero_2bits
  packed_sps_->AppendBits(8, current_sps_.level_idc);
  packed_sps_->AppendUE(current_sps_.seq_parameter_set_id);

  if (current_sps_.profile_idc == H264SPS::kProfileIDCHigh) {
    packed_sps_->AppendUE(current_sps_.chroma_format_idc);
    if (current_sps_.chroma_format_idc == 3)
      packed_sps_->AppendBool(current_sps_.separate_colour_plane_flag);
    packed_sps_->AppendUE(current_sps_.bit_depth_luma_minus8);
    packed_sps_->AppendUE(current_sps_.bit_depth_chroma_minus8);
    packed_sps_->AppendBool(current_sps_.qpprime_y_zero_transform_bypass_flag);
    packed_sps_->AppendBool(current_sps_.seq_scaling_matrix_present_flag);
    CHECK(!current_sps_.seq_scaling_matrix_present_flag);
  }

  packed_sps_->AppendUE(current_sps_.log2_max_frame_num_minus4);
  packed_sps_->AppendUE(current_sps_.pic_order_cnt_type);
  if (current_sps_.pic_order_cnt_type == 0)
    packed_sps_->AppendUE(current_sps_.log2_max_pic_order_cnt_lsb_minus4);
  else if (current_sps_.pic_order_cnt_type == 1)
    NOTREACHED();

  packed_sps_->AppendUE(current_sps_.max_num_ref_frames);
  packed_sps_->AppendBool(current_sps_.gaps_in_frame_num_value_allowed_flag);
  packed_sps_->AppendUE(current_sps_.pic_width_in_mbs_minus1);
  packed_sps_->AppendUE(current_sps_.pic_height_in_map_units_minus1);

  packed_sps_->AppendBool(current_sps_.frame_mbs_only_flag);
  if (!current_sps_.frame_mbs_only_flag)
    packed_sps_->AppendBool(current_sps_.mb_adaptive_frame_field_flag);

  packed_sps_->AppendBool(current_sps_.direct_8x8_inference_flag);

  packed_sps_->AppendBool(current_sps_.frame_cropping_flag);
  if (current_sps_.frame_cropping_flag) {
    packed_sps_->AppendUE(current_sps_.frame_crop_left_offset);
    packed_sps_->AppendUE(current_sps_.frame_crop_right_offset);
    packed_sps_->AppendUE(current_sps_.frame_crop_top_offset);
    packed_sps_->AppendUE(current_sps_.frame_crop_bottom_offset);
  }

  packed_sps_->AppendBool(current_sps_.vui_parameters_present_flag);
  if (current_sps_.vui_parameters_present_flag) {
    packed_sps_->AppendBool(false);  // aspect_ratio_info_present_flag
    packed_sps_->AppendBool(false);  // overscan_info_present_flag
    packed_sps_->AppendBool(false);  // video_signal_type_present_flag
    packed_sps_->AppendBool(false);  // chroma_loc_info_present_flag

    packed_sps_->AppendBool(current_sps_.timing_info_present_flag);
    if (current_sps_.timing_info_present_flag) {
      packed_sps_->AppendBits(32, current_sps_.num_units_in_tick);
      packed_sps_->AppendBits(32, current_sps_.time_scale);
      packed_sps_->AppendBool(current_sps_.fixed_frame_rate_flag);
    }

    packed_sps_->AppendBool(current_sps_.nal_hrd_parameters_present_flag);
    if (current_sps_.nal_hrd_parameters_present_flag) {
      packed_sps_->AppendUE(current_sps_.cpb_cnt_minus1);
      packed_sps_->AppendBits(4, current_sps_.bit_rate_scale);
      packed_sps_->AppendBits(4, current_sps_.cpb_size_scale);
      CHECK_LT(base::checked_cast<size_t>(current_sps_.cpb_cnt_minus1),
               base::size(current_sps_.bit_rate_value_minus1));
      for (int i = 0; i <= current_sps_.cpb_cnt_minus1; ++i) {
        packed_sps_->AppendUE(current_sps_.bit_rate_value_minus1[i]);
        packed_sps_->AppendUE(current_sps_.cpb_size_value_minus1[i]);
        packed_sps_->AppendBool(current_sps_.cbr_flag[i]);
      }
      packed_sps_->AppendBits(
          5, current_sps_.initial_cpb_removal_delay_length_minus_1);
      packed_sps_->AppendBits(5, current_sps_.cpb_removal_delay_length_minus1);
      packed_sps_->AppendBits(5, current_sps_.dpb_output_delay_length_minus1);
      packed_sps_->AppendBits(5, current_sps_.time_offset_length);
    }

    packed_sps_->AppendBool(false);  // vcl_hrd_parameters_flag
    if (current_sps_.nal_hrd_parameters_present_flag)
      packed_sps_->AppendBool(current_sps_.low_delay_hrd_flag);

    packed_sps_->AppendBool(false);  // pic_struct_present_flag
    packed_sps_->AppendBool(true);   // bitstream_restriction_flag

    packed_sps_->AppendBool(false);  // motion_vectors_over_pic_boundaries_flag
    packed_sps_->AppendUE(2);        // max_bytes_per_pic_denom
    packed_sps_->AppendUE(1);        // max_bits_per_mb_denom
    packed_sps_->AppendUE(16);       // log2_max_mv_length_horizontal
    packed_sps_->AppendUE(16);       // log2_max_mv_length_vertical

    // Explicitly set max_num_reorder_frames to 0 to allow the decoder to
    // output pictures early.
    packed_sps_->AppendUE(0);  // max_num_reorder_frames

    // The value of max_dec_frame_buffering shall be greater than or equal to
    // max_num_ref_frames.
    const unsigned int max_dec_frame_buffering =
        current_sps_.max_num_ref_frames;
    packed_sps_->AppendUE(max_dec_frame_buffering);
  }

  packed_sps_->FinishNALU();
}

void H264VaapiVideoEncoderDelegate::GeneratePackedPPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(submit_packed_headers_);
  DCHECK(packed_pps_);

  packed_pps_->Reset();

  packed_pps_->BeginNALU(H264NALU::kPPS, 3);

  packed_pps_->AppendUE(current_pps_.pic_parameter_set_id);
  packed_pps_->AppendUE(current_pps_.seq_parameter_set_id);
  packed_pps_->AppendBool(current_pps_.entropy_coding_mode_flag);
  packed_pps_->AppendBool(
      current_pps_.bottom_field_pic_order_in_frame_present_flag);
  CHECK_EQ(current_pps_.num_slice_groups_minus1, 0);
  packed_pps_->AppendUE(current_pps_.num_slice_groups_minus1);

  packed_pps_->AppendUE(current_pps_.num_ref_idx_l0_default_active_minus1);
  packed_pps_->AppendUE(current_pps_.num_ref_idx_l1_default_active_minus1);

  packed_pps_->AppendBool(current_pps_.weighted_pred_flag);
  packed_pps_->AppendBits(2, current_pps_.weighted_bipred_idc);

  packed_pps_->AppendSE(current_pps_.pic_init_qp_minus26);
  packed_pps_->AppendSE(current_pps_.pic_init_qs_minus26);
  packed_pps_->AppendSE(current_pps_.chroma_qp_index_offset);

  packed_pps_->AppendBool(current_pps_.deblocking_filter_control_present_flag);
  packed_pps_->AppendBool(current_pps_.constrained_intra_pred_flag);
  packed_pps_->AppendBool(current_pps_.redundant_pic_cnt_present_flag);

  packed_pps_->AppendBool(current_pps_.transform_8x8_mode_flag);
  packed_pps_->AppendBool(current_pps_.pic_scaling_matrix_present_flag);
  DCHECK(!current_pps_.pic_scaling_matrix_present_flag);
  packed_pps_->AppendSE(current_pps_.second_chroma_qp_index_offset);

  packed_pps_->FinishNALU();
}

scoped_refptr<H264BitstreamBuffer>
H264VaapiVideoEncoderDelegate::GeneratePackedSliceHeader(
    const VAEncPictureParameterBufferH264& pic_param,
    const VAEncSliceParameterBufferH264& slice_param,
    const H264Picture& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto packed_slice_header = base::MakeRefCounted<H264BitstreamBuffer>();
  const bool is_idr = !!pic_param.pic_fields.bits.idr_pic_flag;
  const bool is_ref = !!pic_param.pic_fields.bits.reference_pic_flag;
  // IDR:3, Non-IDR I slice:2, P slice:1, non ref frame: 0.
  size_t nal_ref_idc = 0;
  H264NALU::Type nalu_type = H264NALU::Type::kUnspecified;
  if (slice_param.slice_type == H264SliceHeader::kISlice) {
    nal_ref_idc = is_idr ? 3 : 2;
    nalu_type = is_idr ? H264NALU::kIDRSlice : H264NALU::kNonIDRSlice;
  } else {
    // B frames is not used, so this is P frame.
    nal_ref_idc = is_ref;
    nalu_type = H264NALU::kNonIDRSlice;
  }
  packed_slice_header->BeginNALU(nalu_type, nal_ref_idc);

  packed_slice_header->AppendUE(
      slice_param.macroblock_address);  // first_mb_in_slice
  packed_slice_header->AppendUE(slice_param.slice_type);
  packed_slice_header->AppendUE(slice_param.pic_parameter_set_id);
  packed_slice_header->AppendBits(current_sps_.log2_max_frame_num_minus4 + 4,
                                  pic_param.frame_num);  // frame_num

  DCHECK(current_sps_.frame_mbs_only_flag);
  if (is_idr)
    packed_slice_header->AppendUE(slice_param.idr_pic_id);

  DCHECK_EQ(current_sps_.pic_order_cnt_type, 0);
  packed_slice_header->AppendBits(
      current_sps_.log2_max_pic_order_cnt_lsb_minus4 + 4,
      pic_param.CurrPic.TopFieldOrderCnt);
  DCHECK(!current_pps_.bottom_field_pic_order_in_frame_present_flag);
  DCHECK(!current_pps_.redundant_pic_cnt_present_flag);

  if (slice_param.slice_type == H264SliceHeader::kPSlice) {
    packed_slice_header->AppendBits(
        1, slice_param.num_ref_idx_active_override_flag);
    if (slice_param.num_ref_idx_active_override_flag)
      packed_slice_header->AppendUE(slice_param.num_ref_idx_l0_active_minus1);
  }

  if (slice_param.slice_type != H264SliceHeader::kISlice) {
    packed_slice_header->AppendBits(1, pic.ref_pic_list_modification_flag_l0);
    // modification flag for P slice.
    if (pic.ref_pic_list_modification_flag_l0) {
      // modification_of_pic_num_idc
      packed_slice_header->AppendUE(0);
      // abs_diff_pic_num_minus1
      packed_slice_header->AppendUE(pic.abs_diff_pic_num_minus1);
      // modification_of_pic_num_idc
      packed_slice_header->AppendUE(3);
    }
  }
  DCHECK_NE(slice_param.slice_type, H264SliceHeader::kBSlice);
  DCHECK(!pic_param.pic_fields.bits.weighted_pred_flag ||
         !(slice_param.slice_type == H264SliceHeader::kPSlice));

  // dec_ref_pic_marking
  if (nal_ref_idc != 0) {
    if (is_idr) {
      packed_slice_header->AppendBool(false);  // no_output_of_prior_pics_flag
      packed_slice_header->AppendBool(false);  // long_term_reference_flag
    } else {
      packed_slice_header->AppendBool(
          false);  // adaptive_ref_pic_marking_mode_flag
    }
  }

  if (pic_param.pic_fields.bits.entropy_coding_mode_flag &&
      slice_param.slice_type != H264SliceHeader::kISlice) {
    packed_slice_header->AppendUE(slice_param.cabac_init_idc);
  }

  packed_slice_header->AppendSE(slice_param.slice_qp_delta);

  if (pic_param.pic_fields.bits.deblocking_filter_control_present_flag) {
    packed_slice_header->AppendUE(slice_param.disable_deblocking_filter_idc);

    if (slice_param.disable_deblocking_filter_idc != 1) {
      packed_slice_header->AppendSE(slice_param.slice_alpha_c0_offset_div2);
      packed_slice_header->AppendSE(slice_param.slice_beta_offset_div2);
    }
  }

  packed_slice_header->Flush();
  return packed_slice_header;
}

void H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer(
    scoped_refptr<H264BitstreamBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderDataBufferType,
                                    buffer->BytesInBuffer(), buffer->data())) {
    error_cb_.Run();
  }
}

bool H264VaapiVideoEncoderDelegate::SubmitFrameParameters(
    VaapiVideoEncoderDelegate::EncodeJob* job,
    const H264VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const H264SPS& sps,
    const H264PPS& pps,
    scoped_refptr<H264Picture> pic,
    const base::circular_deque<scoped_refptr<H264Picture>>& ref_pic_list0,
    const absl::optional<size_t>& ref_frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VAEncSequenceParameterBufferH264 seq_param = {};

#define SPS_TO_SP(a) seq_param.a = sps.a;
  SPS_TO_SP(seq_parameter_set_id);
  SPS_TO_SP(level_idc);

  seq_param.intra_period = kIPeriod;
  seq_param.intra_idr_period = kIDRPeriod;
  seq_param.ip_period = kIPPeriod;
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();

  SPS_TO_SP(max_num_ref_frames);
  absl::optional<gfx::Size> coded_size = sps.GetCodedSize();
  if (!coded_size) {
    DVLOGF(1) << "Invalid coded size";
    return false;
  }
  constexpr int kH264MacroblockSizeInPixels = 16;
  seq_param.picture_width_in_mbs =
      coded_size->width() / kH264MacroblockSizeInPixels;
  seq_param.picture_height_in_mbs =
      coded_size->height() / kH264MacroblockSizeInPixels;

#define SPS_TO_SP_FS(a) seq_param.seq_fields.bits.a = sps.a;
  SPS_TO_SP_FS(chroma_format_idc);
  SPS_TO_SP_FS(frame_mbs_only_flag);
  SPS_TO_SP_FS(log2_max_frame_num_minus4);
  SPS_TO_SP_FS(pic_order_cnt_type);
  SPS_TO_SP_FS(log2_max_pic_order_cnt_lsb_minus4);
#undef SPS_TO_SP_FS

  SPS_TO_SP(bit_depth_luma_minus8);
  SPS_TO_SP(bit_depth_chroma_minus8);

  SPS_TO_SP(frame_cropping_flag);
  if (sps.frame_cropping_flag) {
    SPS_TO_SP(frame_crop_left_offset);
    SPS_TO_SP(frame_crop_right_offset);
    SPS_TO_SP(frame_crop_top_offset);
    SPS_TO_SP(frame_crop_bottom_offset);
  }

  SPS_TO_SP(vui_parameters_present_flag);
#define SPS_TO_SP_VF(a) seq_param.vui_fields.bits.a = sps.a;
  SPS_TO_SP_VF(timing_info_present_flag);
#undef SPS_TO_SP_VF
  SPS_TO_SP(num_units_in_tick);
  SPS_TO_SP(time_scale);
#undef SPS_TO_SP

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  VAEncPictureParameterBufferH264 pic_param = {};

  auto va_surface_id = pic->AsVaapiH264Picture()->GetVASurfaceID();
  pic_param.CurrPic.picture_id = va_surface_id;
  pic_param.CurrPic.TopFieldOrderCnt = pic->top_field_order_cnt;
  pic_param.CurrPic.BottomFieldOrderCnt = pic->bottom_field_order_cnt;
  pic_param.CurrPic.flags = 0;

  pic_param.coded_buf = job->coded_buffer_id();
  pic_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  pic_param.seq_parameter_set_id = pps.seq_parameter_set_id;
  pic_param.frame_num = pic->frame_num;
  pic_param.pic_init_qp = pps.pic_init_qp_minus26 + 26;
  pic_param.num_ref_idx_l0_active_minus1 =
      pps.num_ref_idx_l0_default_active_minus1;

  pic_param.pic_fields.bits.idr_pic_flag = pic->idr;
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#define PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps.a;
  PPS_TO_PP_PF(entropy_coding_mode_flag);
  PPS_TO_PP_PF(transform_8x8_mode_flag);
  PPS_TO_PP_PF(deblocking_filter_control_present_flag);
#undef PPS_TO_PP_PF

  VAEncSliceParameterBufferH264 slice_param = {};

  slice_param.num_macroblocks =
      seq_param.picture_width_in_mbs * seq_param.picture_height_in_mbs;
  slice_param.macroblock_info = VA_INVALID_ID;
  slice_param.slice_type = pic->type;
  slice_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  slice_param.idr_pic_id = pic->idr_pic_id;
  slice_param.pic_order_cnt_lsb = pic->pic_order_cnt_lsb;
  slice_param.num_ref_idx_active_override_flag = true;
  if (slice_param.slice_type == H264SliceHeader::kPSlice) {
    slice_param.num_ref_idx_l0_active_minus1 =
        ref_frame_index.has_value() ? 0 : ref_pic_list0.size() - 1;
  } else {
    slice_param.num_ref_idx_l0_active_minus1 = 0;
  }

  for (VAPictureH264& picture : pic_param.ReferenceFrames)
    InitVAPictureH264(&picture);

  for (VAPictureH264& picture : slice_param.RefPicList0)
    InitVAPictureH264(&picture);

  for (VAPictureH264& picture : slice_param.RefPicList1)
    InitVAPictureH264(&picture);

  for (size_t i = 0, j = 0; i < ref_pic_list0.size(); ++i) {
    H264Picture& ref_pic = *ref_pic_list0[i];
    VAPictureH264 va_pic_h264;
    InitVAPictureH264(&va_pic_h264);
    va_pic_h264.picture_id = ref_pic.AsVaapiH264Picture()->GetVASurfaceID();
    va_pic_h264.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    va_pic_h264.frame_idx = ref_pic.frame_num;
    va_pic_h264.TopFieldOrderCnt = ref_pic.top_field_order_cnt;
    va_pic_h264.BottomFieldOrderCnt = ref_pic.bottom_field_order_cnt;
    // Initialize the current entry on slice and picture reference lists to
    // |ref_pic| and advance list pointers.
    pic_param.ReferenceFrames[i] = va_pic_h264;
    if (!ref_frame_index || *ref_frame_index == i)
      slice_param.RefPicList0[j++] = va_pic_h264;
  }

  VAEncMiscParameterRateControl rate_control_param;
  VAEncMiscParameterFrameRate framerate_param;
  VAEncMiscParameterHRD hrd_param;
  FillVAEncRateControlParams(
      encode_params.bitrate_allocation.GetSumBps(),
      encode_params.cpb_window_size_ms,
      base::strict_cast<uint32_t>(pic_param.pic_init_qp),
      base::strict_cast<uint32_t>(encode_params.min_qp),
      base::strict_cast<uint32_t>(encode_params.max_qp),
      encode_params.framerate,
      base::strict_cast<uint32_t>(encode_params.cpb_size_bits),
      rate_control_param, framerate_param, hrd_param);

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncSliceParameterBufferType,
                     MakeRefCountedBytes(&slice_param, sizeof(slice_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
      base::Unretained(this), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
      base::Unretained(this), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
                     base::Unretained(this), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  if (!submit_packed_headers_)
    return true;

  scoped_refptr<H264BitstreamBuffer> packed_slice_header =
      GeneratePackedSliceHeader(pic_param, slice_param, *pic);
  VAEncPackedHeaderParameterBuffer packed_slice_param_buffer;
  packed_slice_param_buffer.type = VAEncPackedHeaderSlice;
  packed_slice_param_buffer.bit_length = packed_slice_header->BitsInBuffer();
  packed_slice_param_buffer.has_emulation_bytes = 0;

  // Submit packed slice header.
  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitBuffer, base::Unretained(this),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&packed_slice_param_buffer,
                          sizeof(packed_slice_param_buffer))));
  job->AddSetupCallback(
      base::BindOnce(&H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer,
                     base::Unretained(this), packed_slice_header));

  return true;
}

scoped_refptr<H264Picture> H264VaapiVideoEncoderDelegate::GetPicture(
    EncodeJob* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::WrapRefCounted(
      reinterpret_cast<H264Picture*>(job->picture().get()));
}

bool H264VaapiVideoEncoderDelegate::SubmitPackedHeaders(
    EncodeJob* job,
    scoped_refptr<H264BitstreamBuffer> packed_sps,
    scoped_refptr<H264BitstreamBuffer> packed_pps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(submit_packed_headers_);
  DCHECK(packed_sps);
  DCHECK(packed_pps);

  // Submit SPS.
  VAEncPackedHeaderParameterBuffer par_buffer = {};
  par_buffer.type = VAEncPackedHeaderSequence;
  par_buffer.bit_length = packed_sps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitBuffer, base::Unretained(this),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer,
                     base::Unretained(this), packed_sps));

  // Submit PPS.
  par_buffer = {};
  par_buffer.type = VAEncPackedHeaderPicture;
  par_buffer.bit_length = packed_pps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitBuffer, base::Unretained(this),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&H264VaapiVideoEncoderDelegate::SubmitH264BitstreamBuffer,
                     base::Unretained(this), packed_pps));
  return true;
}

}  // namespace media
