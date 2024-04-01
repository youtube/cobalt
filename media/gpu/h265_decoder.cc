// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/limits.h"
#include "media/gpu/h265_decoder.h"

namespace media {

namespace {

struct POCAscCompare {
  bool operator()(const scoped_refptr<H265Picture>& a,
                  const scoped_refptr<H265Picture>& b) const {
    return a->pic_order_cnt_val_ < b->pic_order_cnt_val_;
  }
};

bool ParseBitDepth(const H265SPS& sps, uint8_t& bit_depth) {
  // Spec 7.4.3.2.1
  if (sps.bit_depth_y != sps.bit_depth_c) {
    DVLOG(1) << "Different bit depths among planes is not supported";
    return false;
  }
  bit_depth = base::checked_cast<uint8_t>(sps.bit_depth_y);
  return true;
}

bool IsValidBitDepth(uint8_t bit_depth, VideoCodecProfile profile) {
  // Spec A.3.
  switch (profile) {
    case HEVCPROFILE_MAIN:
      return bit_depth == 8u;
    case HEVCPROFILE_MAIN10:
      return bit_depth == 8u || bit_depth == 10u;
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return bit_depth == 8u;
    default:
      DVLOG(1) << "Invalid profile specified for H265";
      return false;
  }
}

bool IsYUV420Sequence(const H265SPS& sps) {
  // Spec 6.2
  return sps.chroma_format_idc == 1;
}
}  // namespace

H265Decoder::H265Accelerator::H265Accelerator() = default;

H265Decoder::H265Accelerator::~H265Accelerator() = default;

H265Decoder::H265Accelerator::Status H265Decoder::H265Accelerator::SetStream(
    base::span<const uint8_t> stream,
    const DecryptConfig* decrypt_config) {
  return H265Decoder::H265Accelerator::Status::kNotSupported;
}

H265Decoder::H265Decoder(std::unique_ptr<H265Accelerator> accelerator,
                         VideoCodecProfile profile,
                         const VideoColorSpace& container_color_space)
    : state_(kAfterReset),
      container_color_space_(container_color_space),
      profile_(profile),
      accelerator_(std::move(accelerator)) {
  DCHECK(accelerator_);
  Reset();
}

H265Decoder::~H265Decoder() = default;

#define SET_ERROR_AND_RETURN()         \
  do {                                 \
    DVLOG(1) << "Error during decode"; \
    state_ = kError;                   \
    return H265Decoder::kDecodeError;  \
  } while (0)

#define CHECK_ACCELERATOR_RESULT(func)                       \
  do {                                                       \
    H265Accelerator::Status result = (func);                 \
    switch (result) {                                        \
      case H265Accelerator::Status::kOk:                     \
        break;                                               \
      case H265Accelerator::Status::kTryAgain:               \
        DVLOG(1) << #func " needs to try again";             \
        return H265Decoder::kTryAgain;                       \
      case H265Accelerator::Status::kFail: /* fallthrough */ \
      case H265Accelerator::Status::kNotSupported:           \
        SET_ERROR_AND_RETURN();                              \
    }                                                        \
  } while (0)

void H265Decoder::SetStream(int32_t id, const DecoderBuffer& decoder_buffer) {
  const uint8_t* ptr = decoder_buffer.data();
  const size_t size = decoder_buffer.data_size();
  const DecryptConfig* decrypt_config = decoder_buffer.decrypt_config();

  DCHECK(ptr);
  DCHECK(size);
  DVLOG(4) << "New input stream id: " << id << " at: " << (void*)ptr
           << " size: " << size;
  stream_id_ = id;
  current_stream_ = ptr;
  current_stream_size_ = size;
  current_stream_has_been_changed_ = true;
  if (decrypt_config) {
    parser_.SetEncryptedStream(ptr, size, decrypt_config->subsamples());
    current_decrypt_config_ = decrypt_config->Clone();
  } else {
    parser_.SetStream(ptr, size);
    current_decrypt_config_ = nullptr;
  }
}

void H265Decoder::Reset() {
  curr_pic_ = nullptr;
  curr_nalu_ = nullptr;
  curr_slice_hdr_ = nullptr;
  last_slice_hdr_ = nullptr;
  curr_sps_id_ = -1;
  curr_pps_id_ = -1;

  prev_tid0_pic_ = nullptr;
  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  dpb_.Clear();
  parser_.Reset();
  accelerator_->Reset();

  state_ = kAfterReset;
}

H265Decoder::DecodeResult H265Decoder::Decode() {
  if (state_ == kError) {
    DVLOG(1) << "Decoder in error state";
    return kDecodeError;
  }

  if (current_stream_has_been_changed_) {
    // Calling H265Accelerator::SetStream() here instead of when the stream is
    // originally set in case the accelerator needs to return kTryAgain.
    H265Accelerator::Status result = accelerator_->SetStream(
        base::span<const uint8_t>(current_stream_, current_stream_size_),
        current_decrypt_config_.get());
    switch (result) {
      case H265Accelerator::Status::kOk:  // fallthrough
      case H265Accelerator::Status::kNotSupported:
        // kNotSupported means the accelerator can't handle this stream,
        // so everything will be done through the parser.
        break;
      case H265Accelerator::Status::kTryAgain:
        DVLOG(1) << "SetStream() needs to try again";
        return H265Decoder::kTryAgain;
      case H265Accelerator::Status::kFail:
        SET_ERROR_AND_RETURN();
    }

    // Reset the flag so that this is only called again next time SetStream()
    // is called.
    current_stream_has_been_changed_ = false;
  }

  while (true) {
    H265Parser::Result par_res;

    if (!curr_nalu_) {
      curr_nalu_ = std::make_unique<H265NALU>();
      par_res = parser_.AdvanceToNextNALU(curr_nalu_.get());
      if (par_res == H265Parser::kEOStream) {
        curr_nalu_.reset();
        // We receive one frame per buffer, so we can output the frame now.
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        return kRanOutOfStreamData;
      }
      if (par_res != H265Parser::kOk) {
        curr_nalu_.reset();
        SET_ERROR_AND_RETURN();
      }

      DVLOG(4) << "New NALU: " << static_cast<int>(curr_nalu_->nal_unit_type);
    }

    // 8.1.2 We only want nuh_layer_id of zero.
    if (curr_nalu_->nuh_layer_id) {
      DVLOG(4) << "Skipping NALU with nuh_layer_id="
               << curr_nalu_->nuh_layer_id;
      curr_nalu_.reset();
      continue;
    }

    switch (curr_nalu_->nal_unit_type) {
      case H265NALU::BLA_W_LP:  // fallthrough
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::TRAIL_N:
      case H265NALU::TRAIL_R:
      case H265NALU::TSA_N:
      case H265NALU::TSA_R:
      case H265NALU::STSA_N:
      case H265NALU::STSA_R:
      case H265NALU::RADL_N:
      case H265NALU::RADL_R:
      case H265NALU::RASL_N:
      case H265NALU::RASL_R:
      case H265NALU::CRA_NUT:
        if (!curr_slice_hdr_) {
          curr_slice_hdr_.reset(new H265SliceHeader());
          par_res = parser_.ParseSliceHeader(*curr_nalu_, curr_slice_hdr_.get(),
                                             last_slice_hdr_.get());
          if (par_res == H265Parser::kMissingParameterSet) {
            // We may still be able to recover if we skip until we find the
            // SPS/PPS.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }
          if (par_res != H265Parser::kOk)
            SET_ERROR_AND_RETURN();
          if (!curr_slice_hdr_->irap_pic && state_ == kAfterReset) {
            // We can't resume from a non-IRAP picture.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }

          state_ = kTryPreprocessCurrentSlice;
          if (curr_slice_hdr_->slice_pic_parameter_set_id != curr_pps_id_) {
            bool need_new_buffers = false;
            if (!ProcessPPS(curr_slice_hdr_->slice_pic_parameter_set_id,
                            &need_new_buffers)) {
              SET_ERROR_AND_RETURN();
            }

            if (need_new_buffers) {
              curr_pic_ = nullptr;
              return kConfigChange;
            }
          }
        }

        if (state_ == kTryPreprocessCurrentSlice) {
          CHECK_ACCELERATOR_RESULT(PreprocessCurrentSlice());
          state_ = kEnsurePicture;
        }

        if (state_ == kEnsurePicture) {
          if (curr_pic_) {
            // |curr_pic_| already exists, so skip to ProcessCurrentSlice().
            state_ = kTryCurrentSlice;
          } else {
            // New picture, try to start a new one or tell client we need more
            // surfaces.
            curr_pic_ = accelerator_->CreateH265Picture();
            if (!curr_pic_)
              return kRanOutOfSurfaces;
            if (current_decrypt_config_)
              curr_pic_->set_decrypt_config(current_decrypt_config_->Clone());

            curr_pic_->first_picture_ = first_picture_;
            first_picture_ = false;
            state_ = kTryNewFrame;
          }
        }

        if (state_ == kTryNewFrame) {
          CHECK_ACCELERATOR_RESULT(StartNewFrame(curr_slice_hdr_.get()));
          state_ = kTryCurrentSlice;
        }

        DCHECK_EQ(state_, kTryCurrentSlice);
        CHECK_ACCELERATOR_RESULT(ProcessCurrentSlice());
        state_ = kDecoding;
        last_slice_hdr_.swap(curr_slice_hdr_);
        curr_slice_hdr_.reset();
        break;
      case H265NALU::SPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int sps_id;
        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();

        break;
      case H265NALU::PPS_NUT:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        int pps_id;
        par_res = parser_.ParsePPS(*curr_nalu_, &pps_id);
        if (par_res != H265Parser::kOk)
          SET_ERROR_AND_RETURN();

        break;
      case H265NALU::EOS_NUT:
        first_picture_ = true;
        FALLTHROUGH;
      case H265NALU::EOB_NUT:  // fallthrough
      case H265NALU::AUD_NUT:
      case H265NALU::RSV_NVCL41:
      case H265NALU::RSV_NVCL42:
      case H265NALU::RSV_NVCL43:
      case H265NALU::RSV_NVCL44:
      case H265NALU::UNSPEC48:
      case H265NALU::UNSPEC49:
      case H265NALU::UNSPEC50:
      case H265NALU::UNSPEC51:
      case H265NALU::UNSPEC52:
      case H265NALU::UNSPEC53:
      case H265NALU::UNSPEC54:
      case H265NALU::UNSPEC55:
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        break;
      default:
        DVLOG(4) << "Skipping NALU type: " << curr_nalu_->nal_unit_type;
        break;
    }

    DVLOG(4) << "NALU done";
    curr_nalu_.reset();
  }
}

gfx::Size H265Decoder::GetPicSize() const {
  return pic_size_;
}

gfx::Rect H265Decoder::GetVisibleRect() const {
  return visible_rect_;
}

VideoCodecProfile H265Decoder::GetProfile() const {
  return profile_;
}

uint8_t H265Decoder::GetBitDepth() const {
  return bit_depth_;
}

size_t H265Decoder::GetRequiredNumOfPictures() const {
  constexpr size_t kPicsInPipeline = limits::kMaxVideoFrames + 1;
  return GetNumReferenceFrames() + kPicsInPipeline;
}

size_t H265Decoder::GetNumReferenceFrames() const {
  // Use the maximum number of pictures in the Decoded Picture Buffer.
  return dpb_.max_num_pics();
}

bool H265Decoder::ProcessPPS(int pps_id, bool* need_new_buffers) {
  DVLOG(4) << "Processing PPS id:" << pps_id;

  const H265PPS* pps = parser_.GetPPS(pps_id);
  // Slice header parsing already verified this should exist.
  DCHECK(pps);

  const H265SPS* sps = parser_.GetSPS(pps->pps_seq_parameter_set_id);
  // PPS parsing already verified this should exist.
  DCHECK(sps);

  if (need_new_buffers)
    *need_new_buffers = false;

  gfx::Size new_pic_size = sps->GetCodedSize();
  gfx::Rect new_visible_rect = sps->GetVisibleRect();
  if (visible_rect_ != new_visible_rect) {
    DVLOG(2) << "New visible rect: " << new_visible_rect.ToString();
    visible_rect_ = new_visible_rect;
  }
  if (!IsYUV420Sequence(*sps)) {
    DVLOG(1) << "Only YUV 4:2:0 is supported";
    return false;
  }

  // Equation 7-8
  max_pic_order_cnt_lsb_ =
      std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  VideoCodecProfile new_profile = H265Parser::ProfileIDCToVideoCodecProfile(
      sps->profile_tier_level.general_profile_idc);
  uint8_t new_bit_depth = 0;
  if (!ParseBitDepth(*sps, new_bit_depth))
    return false;
  if (!IsValidBitDepth(new_bit_depth, new_profile)) {
    DVLOG(1) << "Invalid bit depth=" << base::strict_cast<int>(new_bit_depth)
             << ", profile=" << GetProfileName(new_profile);
    return false;
  }
  if (pic_size_ != new_pic_size || dpb_.max_num_pics() != sps->max_dpb_size ||
      profile_ != new_profile || bit_depth_ != new_bit_depth) {
    if (!Flush())
      return false;
    DVLOG(1) << "Codec profile: " << GetProfileName(new_profile)
             << ", level(x30): " << sps->profile_tier_level.general_level_idc
             << ", DPB size: " << sps->max_dpb_size
             << ", Picture size: " << new_pic_size.ToString()
             << ", bit_depth: " << base::strict_cast<int>(new_bit_depth);
    profile_ = new_profile;
    bit_depth_ = new_bit_depth;
    pic_size_ = new_pic_size;
    dpb_.set_max_num_pics(sps->max_dpb_size);
    if (need_new_buffers)
      *need_new_buffers = true;
  }

  return true;
}

H265Decoder::H265Accelerator::Status H265Decoder::PreprocessCurrentSlice() {
  const H265SliceHeader* slice_hdr = curr_slice_hdr_.get();
  DCHECK(slice_hdr);

  if (slice_hdr->first_slice_segment_in_pic_flag) {
    // New picture, so first finish the previous one before processing it.
    H265Accelerator::Status result = FinishPrevFrameIfPresent();
    if (result != H265Accelerator::Status::kOk)
      return result;

    DCHECK(!curr_pic_);
  }

  return H265Accelerator::Status::kOk;
}

H265Decoder::H265Accelerator::Status H265Decoder::ProcessCurrentSlice() {
  DCHECK(curr_pic_);

  const H265SliceHeader* slice_hdr = curr_slice_hdr_.get();
  DCHECK(slice_hdr);

  const H265SPS* sps = parser_.GetSPS(curr_sps_id_);
  DCHECK(sps);

  const H265PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);
  return accelerator_->SubmitSlice(sps, pps, slice_hdr, ref_pic_list0_,
                                   ref_pic_list1_, curr_pic_.get(),
                                   slice_hdr->nalu_data, slice_hdr->nalu_size,
                                   parser_.GetCurrentSubsamples());
}

void H265Decoder::CalcPicOutputFlags(const H265SliceHeader* slice_hdr) {
  if (slice_hdr->irap_pic) {
    // 8.1.3
    curr_pic_->no_rasl_output_flag_ =
        (curr_nalu_->nal_unit_type >= H265NALU::BLA_W_LP &&
         curr_nalu_->nal_unit_type <= H265NALU::IDR_N_LP) ||
        curr_pic_->first_picture_;
  } else {
    curr_pic_->no_rasl_output_flag_ = false;
  }

  // C.5.2.2
  if (slice_hdr->irap_pic && curr_pic_->no_rasl_output_flag_ &&
      !curr_pic_->first_picture_) {
    curr_pic_->no_output_of_prior_pics_flag_ =
        (slice_hdr->nal_unit_type == H265NALU::CRA_NUT) ||
        slice_hdr->no_output_of_prior_pics_flag;
  } else {
    curr_pic_->no_output_of_prior_pics_flag_ = false;
  }

  if ((slice_hdr->nal_unit_type == H265NALU::RASL_N ||
       slice_hdr->nal_unit_type == H265NALU::RASL_R) &&
      curr_pic_->no_rasl_output_flag_) {
    curr_pic_->pic_output_flag_ = false;
  } else {
    curr_pic_->pic_output_flag_ = slice_hdr->pic_output_flag;
  }
}

void H265Decoder::CalcPictureOrderCount(const H265PPS* pps,
                                        const H265SliceHeader* slice_hdr) {
  // 8.3.1 Decoding process for picture order count.
  curr_pic_->valid_for_prev_tid0_pic_ =
      !pps->temporal_id && (slice_hdr->nal_unit_type < H265NALU::RADL_N ||
                            slice_hdr->nal_unit_type > H265NALU::RSV_VCL_N14);
  curr_pic_->slice_pic_order_cnt_lsb_ = slice_hdr->slice_pic_order_cnt_lsb;

  // Calculate POC for current picture.
  if ((!slice_hdr->irap_pic || !curr_pic_->no_rasl_output_flag_) &&
      prev_tid0_pic_) {
    const int prev_pic_order_cnt_lsb = prev_tid0_pic_->slice_pic_order_cnt_lsb_;
    const int prev_pic_order_cnt_msb = prev_tid0_pic_->pic_order_cnt_msb_;
    if ((slice_hdr->slice_pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
        ((prev_pic_order_cnt_lsb - slice_hdr->slice_pic_order_cnt_lsb) >=
         (max_pic_order_cnt_lsb_ / 2))) {
      curr_pic_->pic_order_cnt_msb_ =
          prev_pic_order_cnt_msb + max_pic_order_cnt_lsb_;
    } else if ((slice_hdr->slice_pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
               ((slice_hdr->slice_pic_order_cnt_lsb - prev_pic_order_cnt_lsb) >
                (max_pic_order_cnt_lsb_ / 2))) {
      curr_pic_->pic_order_cnt_msb_ =
          prev_pic_order_cnt_msb - max_pic_order_cnt_lsb_;
    } else {
      curr_pic_->pic_order_cnt_msb_ = prev_pic_order_cnt_msb;
    }
  } else {
    curr_pic_->pic_order_cnt_msb_ = 0;
  }
  curr_pic_->pic_order_cnt_val_ =
      curr_pic_->pic_order_cnt_msb_ + slice_hdr->slice_pic_order_cnt_lsb;
}

bool H265Decoder::CalcRefPicPocs(const H265SPS* sps,
                                 const H265PPS* pps,
                                 const H265SliceHeader* slice_hdr) {
  if (slice_hdr->nal_unit_type == H265NALU::IDR_W_RADL ||
      slice_hdr->nal_unit_type == H265NALU::IDR_N_LP) {
    num_poc_st_curr_before_ = num_poc_st_curr_after_ = num_poc_st_foll_ =
        num_poc_lt_curr_ = num_poc_lt_foll_ = 0;
    return true;
  }

  // 8.3.2 - NOTE 2
  const H265StRefPicSet& curr_st_ref_pic_set = slice_hdr->GetStRefPicSet(sps);

  // Equation 8-5.
  int i, j, k;
  for (i = 0, j = 0, k = 0; i < curr_st_ref_pic_set.num_negative_pics; ++i) {
    base::CheckedNumeric<int> poc = curr_pic_->pic_order_cnt_val_;
    poc += curr_st_ref_pic_set.delta_poc_s0[i];
    if (!poc.IsValid()) {
      DVLOG(1) << "Invalid POC";
      return false;
    }
    if (curr_st_ref_pic_set.used_by_curr_pic_s0[i])
      poc_st_curr_before_[j++] = poc.ValueOrDefault(0);
    else
      poc_st_foll_[k++] = poc.ValueOrDefault(0);
  }
  num_poc_st_curr_before_ = j;
  for (i = 0, j = 0; i < curr_st_ref_pic_set.num_positive_pics; ++i) {
    base::CheckedNumeric<int> poc = curr_pic_->pic_order_cnt_val_;
    poc += curr_st_ref_pic_set.delta_poc_s1[i];
    if (!poc.IsValid()) {
      DVLOG(1) << "Invalid POC";
      return false;
    }
    if (curr_st_ref_pic_set.used_by_curr_pic_s1[i])
      poc_st_curr_after_[j++] = poc.ValueOrDefault(0);
    else
      poc_st_foll_[k++] = poc.ValueOrDefault(0);
  }
  num_poc_st_curr_after_ = j;
  num_poc_st_foll_ = k;
  for (i = 0, j = 0, k = 0;
       i < slice_hdr->num_long_term_sps + slice_hdr->num_long_term_pics; ++i) {
    base::CheckedNumeric<int> poc_lt = slice_hdr->poc_lsb_lt[i];
    if (slice_hdr->delta_poc_msb_present_flag[i]) {
      poc_lt += curr_pic_->pic_order_cnt_val_;
      base::CheckedNumeric<int> poc_delta =
          slice_hdr->delta_poc_msb_cycle_lt[i];
      poc_delta *= max_pic_order_cnt_lsb_;
      if (!poc_delta.IsValid()) {
        DVLOG(1) << "Invalid POC";
        return false;
      }
      poc_lt -= poc_delta.ValueOrDefault(0);
      poc_lt -= curr_pic_->pic_order_cnt_val_ & (max_pic_order_cnt_lsb_ - 1);
    }
    if (!poc_lt.IsValid()) {
      DVLOG(1) << "Invalid POC";
      return false;
    }
    if (slice_hdr->used_by_curr_pic_lt[i]) {
      poc_lt_curr_[j] = poc_lt.ValueOrDefault(0);
      curr_delta_poc_msb_present_flag_[j++] =
          slice_hdr->delta_poc_msb_present_flag[i];
    } else {
      poc_lt_foll_[k] = poc_lt.ValueOrDefault(0);
      foll_delta_poc_msb_present_flag_[k++] =
          slice_hdr->delta_poc_msb_present_flag[i];
    }
  }
  num_poc_lt_curr_ = j;
  num_poc_lt_foll_ = k;

  // Check conformance for |num_pic_total_curr|.
  if (slice_hdr->nal_unit_type == H265NALU::CRA_NUT ||
      (slice_hdr->nal_unit_type >= H265NALU::BLA_W_LP &&
       slice_hdr->nal_unit_type <= H265NALU::BLA_N_LP)) {
    if (slice_hdr->num_pic_total_curr) {
      DVLOG(1) << "Invalid value for num_pic_total_curr";
      return false;
    }
  } else if ((slice_hdr->IsBSlice() || slice_hdr->IsPSlice()) &&
             !slice_hdr->num_pic_total_curr) {
    DVLOG(1) << "Invalid value for num_pic_total_curr";
    return false;
  }

  return true;
}

bool H265Decoder::BuildRefPicLists(const H265SPS* sps,
                                   const H265PPS* pps,
                                   const H265SliceHeader* slice_hdr) {
  scoped_refptr<H265Picture> ref_pic_set_lt_curr[kMaxDpbSize];
  scoped_refptr<H265Picture> ref_pic_set_lt_foll[kMaxDpbSize];
  scoped_refptr<H265Picture> ref_pic_set_st_curr_after[kMaxDpbSize];
  scoped_refptr<H265Picture> ref_pic_set_st_curr_before[kMaxDpbSize];
  scoped_refptr<H265Picture> ref_pic_set_st_foll[kMaxDpbSize];

  // Mark everything in the DPB as unused for reference now. When we determine
  // the pics in the ref list, then we will mark them appropriately.
  dpb_.MarkAllUnusedForReference();

  // Equation 8-6.
  // We may be missing reference pictures, if so then we just don't specify
  // them and let the accelerator deal with the missing reference pictures
  // which is covered in the spec.
  int total_ref_pics = 0;
  for (int i = 0; i < num_poc_lt_curr_; ++i) {
    if (!curr_delta_poc_msb_present_flag_[i])
      ref_pic_set_lt_curr[i] = dpb_.GetPicByPocMaskedAndMark(
          poc_lt_curr_[i], sps->max_pic_order_cnt_lsb - 1,
          H265Picture::kLongTermCurr);
    else
      ref_pic_set_lt_curr[i] =
          dpb_.GetPicByPocAndMark(poc_lt_curr_[i], H265Picture::kLongTermCurr);

    if (ref_pic_set_lt_curr[i])
      total_ref_pics++;
  }
  for (int i = 0; i < num_poc_lt_foll_; ++i) {
    if (!foll_delta_poc_msb_present_flag_[i])
      ref_pic_set_lt_foll[i] = dpb_.GetPicByPocMaskedAndMark(
          poc_lt_foll_[i], sps->max_pic_order_cnt_lsb - 1,
          H265Picture::kLongTermFoll);
    else
      ref_pic_set_lt_foll[i] =
          dpb_.GetPicByPocAndMark(poc_lt_foll_[i], H265Picture::kLongTermFoll);

    if (ref_pic_set_lt_foll[i])
      total_ref_pics++;
  }

  // Equation 8-7.
  for (int i = 0; i < num_poc_st_curr_before_; ++i) {
    ref_pic_set_st_curr_before[i] = dpb_.GetPicByPocAndMark(
        poc_st_curr_before_[i], H265Picture::kShortTermCurrBefore);

    if (ref_pic_set_st_curr_before[i])
      total_ref_pics++;
  }
  for (int i = 0; i < num_poc_st_curr_after_; ++i) {
    ref_pic_set_st_curr_after[i] = dpb_.GetPicByPocAndMark(
        poc_st_curr_after_[i], H265Picture::kShortTermCurrAfter);
    if (ref_pic_set_st_curr_after[i])
      total_ref_pics++;
  }
  for (int i = 0; i < num_poc_st_foll_; ++i) {
    ref_pic_set_st_foll[i] =
        dpb_.GetPicByPocAndMark(poc_st_foll_[i], H265Picture::kShortTermFoll);
    if (ref_pic_set_st_foll[i])
      total_ref_pics++;
  }

  // Verify that the total number of reference pictures in the DPB matches the
  // total count of reference pics. This ensures that a picture is not in more
  // than one list, per the spec.
  if (dpb_.GetReferencePicCount() != total_ref_pics) {
    DVLOG(1) << "Conformance problem, reference pic is in more than one list";
    return false;
  }

  ref_pic_list_.clear();
  dpb_.AppendReferencePics(&ref_pic_list_);
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  // 8.3.3 Generation of unavailable reference pictures is something we do not
  // need to handle here. It's handled by the accelerator itself when we do not
  // specify a reference picture that it needs.

  if (slice_hdr->IsPSlice() || slice_hdr->IsBSlice()) {
    // 8.3.4 Decoding process for reference picture lists construction
    int num_rps_curr_temp_list0 =
        std::max(slice_hdr->num_ref_idx_l0_active_minus1 + 1,
                 slice_hdr->num_pic_total_curr);
    scoped_refptr<H265Picture> ref_pic_list_temp0[kMaxDpbSize];

    // Equation 8-8.
    int r_idx = 0;
    while (r_idx < num_rps_curr_temp_list0) {
      for (int i = 0;
           i < num_poc_st_curr_before_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_st_curr_before[i];
      }
      for (int i = 0;
           i < num_poc_st_curr_after_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_st_curr_after[i];
      }
      for (int i = 0; i < num_poc_lt_curr_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_lt_curr[i];
      }
    }

    // Equation 8-9.
    for (r_idx = 0; r_idx <= slice_hdr->num_ref_idx_l0_active_minus1; ++r_idx) {
      ref_pic_list0_.push_back(
          slice_hdr->ref_pic_lists_modification
                  .ref_pic_list_modification_flag_l0
              ? ref_pic_list_temp0[slice_hdr->ref_pic_lists_modification
                                       .list_entry_l0[r_idx]]
              : ref_pic_list_temp0[r_idx]);
    }

    if (slice_hdr->IsBSlice()) {
      int num_rps_curr_temp_list1 =
          std::max(slice_hdr->num_ref_idx_l1_active_minus1 + 1,
                   slice_hdr->num_pic_total_curr);
      scoped_refptr<H265Picture> ref_pic_list_temp1[kMaxDpbSize];

      // Equation 8-10.
      r_idx = 0;
      while (r_idx < num_rps_curr_temp_list1) {
        for (int i = 0;
             i < num_poc_st_curr_after_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_st_curr_after[i];
        }
        for (int i = 0;
             i < num_poc_st_curr_before_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_st_curr_before[i];
        }
        for (int i = 0; i < num_poc_lt_curr_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_lt_curr[i];
        }
      }

      // Equation 8-11.
      for (r_idx = 0; r_idx <= slice_hdr->num_ref_idx_l1_active_minus1;
           ++r_idx) {
        ref_pic_list1_.push_back(
            slice_hdr->ref_pic_lists_modification
                    .ref_pic_list_modification_flag_l1
                ? ref_pic_list_temp1[slice_hdr->ref_pic_lists_modification
                                         .list_entry_l1[r_idx]]
                : ref_pic_list_temp1[r_idx]);
      }
    }
  }

  return true;
}

H265Decoder::H265Accelerator::Status H265Decoder::StartNewFrame(
    const H265SliceHeader* slice_hdr) {
  CHECK(curr_pic_.get());
  DCHECK(slice_hdr);

  curr_pps_id_ = slice_hdr->slice_pic_parameter_set_id;
  const H265PPS* pps = parser_.GetPPS(curr_pps_id_);
  // Slice header parsing already verified this should exist.
  DCHECK(pps);

  curr_sps_id_ = pps->pps_seq_parameter_set_id;
  const H265SPS* sps = parser_.GetSPS(curr_sps_id_);
  // Slice header parsing already verified this should exist.
  DCHECK(sps);

  // If this is from a retry on SubmitFrameMetadata, we should not redo all of
  // these calculations.
  if (!curr_pic_->processed_) {
    // Copy slice/pps variables we need to the picture.
    curr_pic_->nal_unit_type_ = curr_nalu_->nal_unit_type;
    curr_pic_->irap_pic_ = slice_hdr->irap_pic;

    curr_pic_->set_visible_rect(visible_rect_);
    curr_pic_->set_bitstream_id(stream_id_);
    if (sps->GetColorSpace().IsSpecified())
      curr_pic_->set_colorspace(sps->GetColorSpace());
    else
      curr_pic_->set_colorspace(container_color_space_);

    CalcPicOutputFlags(slice_hdr);
    CalcPictureOrderCount(pps, slice_hdr);

    if (!CalcRefPicPocs(sps, pps, slice_hdr)) {
      return H265Accelerator::Status::kFail;
    }

    if (!BuildRefPicLists(sps, pps, slice_hdr)) {
      return H265Accelerator::Status::kFail;
    }

    if (!PerformDpbOperations(sps)) {
      return H265Accelerator::Status::kFail;
    }
    curr_pic_->processed_ = true;
  }

  return accelerator_->SubmitFrameMetadata(sps, pps, slice_hdr, ref_pic_list_,
                                           curr_pic_);
}

H265Decoder::H265Accelerator::Status H265Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_) {
    H265Accelerator::Status result = DecodePicture();
    if (result != H265Accelerator::Status::kOk)
      return result;

    scoped_refptr<H265Picture> pic = curr_pic_;
    curr_pic_ = nullptr;
    FinishPicture(pic);
  }

  return H265Accelerator::Status::kOk;
}

bool H265Decoder::PerformDpbOperations(const H265SPS* sps) {
  // C.5.2.2
  if (curr_pic_->irap_pic_ && curr_pic_->no_rasl_output_flag_ &&
      !curr_pic_->first_picture_) {
    if (!curr_pic_->no_output_of_prior_pics_flag_) {
      OutputAllRemainingPics();
    }
    dpb_.Clear();
  } else {
    int num_to_output;
    do {
      dpb_.DeleteUnused();
      // Get all pictures that haven't been outputted yet.
      H265Picture::Vector not_outputted;
      dpb_.AppendPendingOutputPics(&not_outputted);
      // Sort in output order.
      std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

      // Calculate how many pictures we need to output.
      num_to_output = 0;
      int highest_tid = sps->sps_max_sub_layers_minus1;
      num_to_output = std::max(num_to_output,
                               static_cast<int>(not_outputted.size()) -
                                   sps->sps_max_num_reorder_pics[highest_tid]);
      num_to_output =
          std::max(num_to_output,
                   static_cast<int>(dpb_.size()) -
                       sps->sps_max_dec_pic_buffering_minus1[highest_tid]);

      num_to_output =
          std::min(num_to_output, static_cast<int>(not_outputted.size()));
      if (!num_to_output && dpb_.IsFull()) {
        // This is wrong, we should try to output pictures until we can clear
        // one from the DPB. This is better than failing, but we then may end up
        // with something out of order.
        DVLOG(1) << "Forcibly outputting pictures to make room in DPB.";
        for (const auto& pic : not_outputted) {
          num_to_output++;
          if (pic->ref_ == H265Picture::kUnused)
            break;
        }
      }

      // TODO(jkardatzke): There's another output picture requirement regarding
      // the sps_max_latency_increase_plus1, but I have yet to understand how
      // that could be larger than the sps_max_num_reorder_pics since the actual
      // latency value used is the sum of both.
      not_outputted.resize(num_to_output);
      for (auto& pic : not_outputted) {
        if (!OutputPic(pic))
          return false;
      }

      dpb_.DeleteUnused();
    } while (dpb_.IsFull() && num_to_output);
  }

  if (dpb_.IsFull()) {
    DVLOG(1) << "Could not free up space in DPB for current picture";
    return false;
  }

  // Put the current pic in the DPB.
  dpb_.StorePicture(curr_pic_, H265Picture::kShortTermFoll);
  return true;
}

void H265Decoder::FinishPicture(scoped_refptr<H265Picture> pic) {
  // 8.3.1
  if (pic->valid_for_prev_tid0_pic_)
    prev_tid0_pic_ = pic;

  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  last_slice_hdr_.reset();
}

H265Decoder::H265Accelerator::Status H265Decoder::DecodePicture() {
  DCHECK(curr_pic_.get());
  return accelerator_->SubmitDecode(curr_pic_);
}

bool H265Decoder::OutputPic(scoped_refptr<H265Picture> pic) {
  DCHECK(!pic->outputted_);
  pic->outputted_ = true;

  DVLOG(4) << "Posting output task for POC: " << pic->pic_order_cnt_val_;
  return accelerator_->OutputPicture(std::move(pic));
}

bool H265Decoder::OutputAllRemainingPics() {
  // Output all pictures that are waiting to be outputted.
  H265Picture::Vector to_output;
  dpb_.AppendPendingOutputPics(&to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  for (auto& pic : to_output) {
    if (!OutputPic(std::move(pic)))
      return false;
  }
  return true;
}

bool H265Decoder::Flush() {
  DVLOG(2) << "Decoder flush";

  if (!OutputAllRemainingPics())
    return false;

  dpb_.Clear();
  prev_tid0_pic_ = nullptr;
  return true;
}

}  // namespace media
