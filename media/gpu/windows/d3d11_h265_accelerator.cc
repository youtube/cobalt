// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_h265_accelerator.h"

#include <windows.h>

#include <algorithm>
#include <map>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_log.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/media_buildflags.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

namespace {

using H265DecoderStatus = H265Decoder::H265Accelerator::Status;

// Converts SubsampleEntry to D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK.
void AppendSubsamples(
    const std::vector<SubsampleEntry>& from,
    std::vector<D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK>* to) {
  for (const auto& from_entry : from) {
    D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK subsample = {};
    subsample.ClearSize = from_entry.clear_bytes;
    subsample.EncryptedSize = from_entry.cypher_bytes;
    to->push_back(subsample);
  }
}

}  // namespace

class D3D11H265Picture : public H265Picture {
 public:
  D3D11H265Picture(D3D11PictureBuffer* picture)
      : picture(picture), picture_index_(picture->picture_index()) {
    picture->set_in_picture_use(true);
  }

  raw_ptr<D3D11PictureBuffer> picture;
  size_t picture_index_;

  D3D11H265Picture* AsD3D11H265Picture() override { return this; }

 protected:
  ~D3D11H265Picture() override;
};

D3D11H265Picture::~D3D11H265Picture() {
  picture->set_in_picture_use(false);
}

D3D11H265Accelerator::D3D11H265Accelerator(
    D3D11VideoDecoderClient* client,
    MediaLog* media_log,
    ComD3D11VideoDevice video_device,
    std::unique_ptr<VideoContextWrapper> video_context)
    : client_(client),
      media_log_(media_log),
      video_device_(video_device),
      video_context_(std::move(video_context)) {
  DCHECK(client);
  DCHECK(media_log_);
  client->SetDecoderCB(base::BindRepeating(
      &D3D11H265Accelerator::SetVideoDecoder, base::Unretained(this)));
}

D3D11H265Accelerator::~D3D11H265Accelerator() {}

scoped_refptr<H265Picture> D3D11H265Accelerator::CreateH265Picture() {
  D3D11PictureBuffer* picture = client_->GetPicture();
  if (!picture) {
    return nullptr;
  }
  return base::MakeRefCounted<D3D11H265Picture>(picture);
}

bool D3D11H265Accelerator::IsChromaSamplingSupported(
    VideoChromaSampling chroma_sampling) {
  return chroma_sampling == VideoChromaSampling::k420 ||
         chroma_sampling == VideoChromaSampling::k422 ||
         chroma_sampling == VideoChromaSampling::k444;
}

H265DecoderStatus D3D11H265Accelerator::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic) {
  const bool is_encrypted = pic->decrypt_config();
  if (is_encrypted) {
    RecordFailure("Cannot find decrypt context for the frame.",
                  D3D11Status::Codes::kCryptoConfigFailed);
    return H265DecoderStatus::kFail;
  }

  HRESULT hr;
  for (;;) {
    D3D11H265Picture* d3d11_pic = pic->AsD3D11H265Picture();
    if (!d3d11_pic)
      return H265DecoderStatus::kFail;

    ID3D11VideoDecoderOutputView* output_view = nullptr;
    auto result = d3d11_pic->picture->AcquireOutputView();
    if (result.has_value()) {
      output_view = std::move(result).value();
    } else {
      RecordFailure(std::move(result).error());
      return H265DecoderStatus::kFail;
    }

    hr = video_context_->DecoderBeginFrame(video_decoder_.Get(), output_view, 0,
                                           nullptr);
    if (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING) {
      base::PlatformThread::YieldCurrentThread();
    } else if (!SUCCEEDED(hr)) {
      RecordFailure("DecoderBeginFrame failed",
                    D3D11Status::Codes::kDecoderBeginFrameFailed, hr);
      return H265DecoderStatus::kFail;
    } else {
      break;
    }
  }

  use_scaling_lists_ = sps->scaling_list_enabled_flag;

  poc_index_into_ref_pic_list_.clear();
  for (size_t i = 0; i < media::kMaxRefPicListSize; i++) {
    ref_frame_list_[i].bPicEntry = 0xFF;
    ref_frame_pocs_[i] = 0;
  }

  // |ref_pic_list| contains the set of pictures as described
  // in HEVC spec section 8.3.2, from the lists RefPicSetLtCurr,
  // RefPicSetLtFoll, RefPicSetStCurrBefore, RefPicSetStCurrAfter
  // and RefPicSetStFoll. When submitting a slice, will use information
  // in ref_pic_list0 and ref_pic_list1 to fill POCs of corresponding
  // list in picture param.
  if (ref_pic_list.size() > kMaxRefPicListSize) {
    DLOG(ERROR) << "Invalid fef pic list size.";
    return H265DecoderStatus::kFail;
  }

  int i = 0;
  for (auto& it : ref_pic_list) {
    if (!it)
      continue;
    D3D11H265Picture* our_ref_pic = it->AsD3D11H265Picture();
    if (!our_ref_pic)
      continue;
    ref_frame_list_[i].Index7Bits = our_ref_pic->picture_index_;
    ref_frame_list_[i].AssociatedFlag = our_ref_pic->IsLongTermRef();
    ref_frame_pocs_[i] = our_ref_pic->pic_order_cnt_val_;
    poc_index_into_ref_pic_list_[our_ref_pic->pic_order_cnt_val_] = i;
    i++;
  }
  slice_info_.clear();
  return RetrieveBitstreamBuffer() ? H265DecoderStatus::kOk
                                   : H265DecoderStatus::kFail;
}

bool D3D11H265Accelerator::RetrieveBitstreamBuffer() {
  DCHECK(!bitstream_buffer_bytes_);
  DCHECK(!bitstream_buffer_size_);

  current_offset_ = 0;
  void* buffer;
  UINT buffer_size;
  HRESULT hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, &buffer_size,
      &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("GetDecoderBuffer (Bitstream) failed",
                  D3D11Status::Codes::kGetBitstreamBufferFailed, hr);
    return false;
  }
  bitstream_buffer_bytes_ = (uint8_t*)buffer;
  bitstream_buffer_size_ = buffer_size;

  return true;
}

void D3D11H265Accelerator::FillPicParamsWithConstants(
    DXVA_PicParams_HEVC_Rext* pic) {
  // According to DXVA spec section 2.2, this optional 1-bit flag
  // has no meaning when used for CurrPic so always configure to 0.
  pic->main.CurrPic.AssociatedFlag = 0;

  // num_tile_columns_minus1 and num_tile_rows_minus1 will only
  // be set if tiles are enabled. Set to 0 by default.
  pic->main.num_tile_columns_minus1 = 0;
  pic->main.num_tile_rows_minus1 = 0;

  // Host decoder may set this to 1 if sps_max_num_reorder_pics is 0,
  // but there is no requirement that NoPicReorderingFlag must be
  // derived from it. So we always set it to 0 here.
  pic->main.NoPicReorderingFlag = 0;

  // Must be set to 0 in absence of indication whether B slices are used
  // or not, and it does not affect the decoding process.
  pic->main.NoBiPredFlag = 0;

  // Shall be set to 0 and accelerators shall ignore its value.
  pic->main.ReservedBits1 = 0;

  // Bit field added to enable DWORD alignment and should be set to 0.
  pic->main.ReservedBits2 = 0;

  // Should always be set to 0.
  pic->main.ReservedBits3 = 0;

  // Should be set to 0 and ignored by accelerators
  pic->main.ReservedBits4 = 0;

  // Should always be set to 0.
  pic->main.ReservedBits5 = 0;

  // Should always be set to 0.
  pic->main.ReservedBits6 = 0;

  // Should always be set to 0.
  pic->main.ReservedBits7 = 0;
}

#define ARG_SEL(_1, _2, NAME, ...) NAME
#define SPS_TO_PP1(a) (pic_param->main).a = sps->a;
#define SPS_TO_PPEXT(a) pic_param->a = sps->a;
#define SPS_TO_PP2(a, b) (pic_param->main).a = sps->b;
#define SPS_TO_PP(...) ARG_SEL(__VA_ARGS__, SPS_TO_PP2, SPS_TO_PP1)(__VA_ARGS__)
void D3D11H265Accelerator::PicParamsFromSPS(DXVA_PicParams_HEVC_Rext* pic_param,
                                            const H265SPS* sps) {
  // Refer to formula 7-14 and 7-16 of HEVC spec.
  int min_cb_log2_size_y = sps->log2_min_luma_coding_block_size_minus3 + 3;
  (pic_param->main).PicWidthInMinCbsY =
      sps->pic_width_in_luma_samples >> min_cb_log2_size_y;
  (pic_param->main).PicHeightInMinCbsY =
      sps->pic_height_in_luma_samples >> min_cb_log2_size_y;
  // wFormatAndSequenceInfoFlags from SPS
  SPS_TO_PP(chroma_format_idc);
  SPS_TO_PP(separate_colour_plane_flag);
  SPS_TO_PP(bit_depth_luma_minus8);
  SPS_TO_PP(bit_depth_chroma_minus8);
  SPS_TO_PP(log2_max_pic_order_cnt_lsb_minus4);

  if (sps->profile_tier_level.general_profile_idc == 4) {
    is_rext_ = true;
  }
  // HEVC DXVA spec does not clearly state which slot
  // in sps->sps_max_dec_pic_buffering_minus1 should
  // be used here. However section A4.1 of HEVC spec
  // requires the slot of highest tid to be used for
  // indicating the maximum DPB size if level is not
  // 8.5.
  int highest_tid = sps->sps_max_sub_layers_minus1;
  (pic_param->main).sps_max_dec_pic_buffering_minus1 =
      sps->sps_max_dec_pic_buffering_minus1[highest_tid];

  SPS_TO_PP(log2_min_luma_coding_block_size_minus3);
  SPS_TO_PP(log2_diff_max_min_luma_coding_block_size);

  // DXVA spec names them differently with HEVC spec.
  SPS_TO_PP(log2_min_transform_block_size_minus2,
            log2_min_luma_transform_block_size_minus2);
  SPS_TO_PP(log2_diff_max_min_transform_block_size,
            log2_diff_max_min_luma_transform_block_size);

  SPS_TO_PP(max_transform_hierarchy_depth_inter);
  SPS_TO_PP(max_transform_hierarchy_depth_intra);
  SPS_TO_PP(num_short_term_ref_pic_sets);
  SPS_TO_PP(num_long_term_ref_pics_sps);

  // dwCodingParamToolFlags extracted from SPS
  SPS_TO_PP(scaling_list_enabled_flag);
  SPS_TO_PP(amp_enabled_flag);
  SPS_TO_PP(sample_adaptive_offset_enabled_flag);
  SPS_TO_PP(pcm_enabled_flag);

  if (sps->pcm_enabled_flag) {
    SPS_TO_PP(pcm_sample_bit_depth_luma_minus1);
    SPS_TO_PP(pcm_sample_bit_depth_chroma_minus1);
    SPS_TO_PP(log2_min_pcm_luma_coding_block_size_minus3);
    SPS_TO_PP(log2_diff_max_min_pcm_luma_coding_block_size);
    SPS_TO_PP(pcm_loop_filter_disabled_flag);
  }
  SPS_TO_PP(long_term_ref_pics_present_flag);
  SPS_TO_PP(sps_temporal_mvp_enabled_flag);
  SPS_TO_PP(strong_intra_smoothing_enabled_flag);

  if (sps->sps_range_extension_flag) {
    SPS_TO_PPEXT(transform_skip_rotation_enabled_flag);
    SPS_TO_PPEXT(transform_skip_context_enabled_flag);
    SPS_TO_PPEXT(implicit_rdpcm_enabled_flag);
    SPS_TO_PPEXT(explicit_rdpcm_enabled_flag);
    SPS_TO_PPEXT(extended_precision_processing_flag);
    SPS_TO_PPEXT(intra_smoothing_disabled_flag);
    SPS_TO_PPEXT(high_precision_offsets_enabled_flag);
    SPS_TO_PPEXT(persistent_rice_adaptation_enabled_flag);
    SPS_TO_PPEXT(cabac_bypass_alignment_enabled_flag);
  }
}
#undef SPS_TO_PP
#undef SPS_TO_PPEXT
#undef SPS_TO_PP2
#undef SPS_TO_PP1

#define PPS_TO_PPEXT(a) pic_param->a = pps->a;
#define PPS_TO_PP1(a) (pic_param->main).a = pps->a;
#define PPS_TO_PP2(a, b) (pic_param->main).a = pps->b;
#define PPS_TO_PP(...) ARG_SEL(__VA_ARGS__, PPS_TO_PP2, PPS_TO_PP1)(__VA_ARGS__)
void D3D11H265Accelerator::PicParamsFromPPS(DXVA_PicParams_HEVC_Rext* pic_param,
                                            const H265PPS* pps) {
  PPS_TO_PP(num_ref_idx_l0_default_active_minus1);
  PPS_TO_PP(num_ref_idx_l1_default_active_minus1);
  PPS_TO_PP(init_qp_minus26);

  // dwCodingParamToolFlags from PPS
  PPS_TO_PP(dependent_slice_segments_enabled_flag);
  PPS_TO_PP(output_flag_present_flag);
  PPS_TO_PP(num_extra_slice_header_bits);
  PPS_TO_PP(sign_data_hiding_enabled_flag);
  PPS_TO_PP(cabac_init_present_flag);

  // dwCodingSettingPicturePropertyFlags from PPS
  PPS_TO_PP(constrained_intra_pred_flag);
  PPS_TO_PP(transform_skip_enabled_flag);
  PPS_TO_PP(cu_qp_delta_enabled_flag);
  PPS_TO_PP(pps_slice_chroma_qp_offsets_present_flag);
  PPS_TO_PP(weighted_pred_flag);
  PPS_TO_PP(weighted_bipred_flag);
  PPS_TO_PP(transquant_bypass_enabled_flag);
  PPS_TO_PP(tiles_enabled_flag);
  PPS_TO_PP(entropy_coding_sync_enabled_flag);
  PPS_TO_PP(uniform_spacing_flag);
  if (pps->tiles_enabled_flag)
    PPS_TO_PP(loop_filter_across_tiles_enabled_flag);
  PPS_TO_PP(pps_loop_filter_across_slices_enabled_flag);
  PPS_TO_PP(deblocking_filter_override_enabled_flag);
  PPS_TO_PP(pps_deblocking_filter_disabled_flag);
  PPS_TO_PP(lists_modification_present_flag);
  PPS_TO_PP(slice_segment_header_extension_present_flag);

  PPS_TO_PP(pps_cb_qp_offset);
  PPS_TO_PP(pps_cr_qp_offset);
  if (pps->tiles_enabled_flag) {
    PPS_TO_PP(num_tile_columns_minus1);
    PPS_TO_PP(num_tile_rows_minus1);
    if (!pps->uniform_spacing_flag) {
      for (int i = 0; i <= pps->num_tile_columns_minus1; i++) {
        PPS_TO_PP(column_width_minus1[i]);
      }
      for (int j = 0; j <= pps->num_tile_rows_minus1; j++) {
        PPS_TO_PP(row_height_minus1[j]);
      }
    }
  }
  PPS_TO_PP(diff_cu_qp_delta_depth);
  PPS_TO_PP(pps_beta_offset_div2);
  PPS_TO_PP(pps_tc_offset_div2);
  PPS_TO_PP(log2_parallel_merge_level_minus2);

  if (pps->pps_range_extension_flag) {
    PPS_TO_PPEXT(cross_component_prediction_enabled_flag);
    PPS_TO_PPEXT(chroma_qp_offset_list_enabled_flag);
    if (pps->chroma_qp_offset_list_enabled_flag) {
      PPS_TO_PPEXT(diff_cu_chroma_qp_offset_depth);
      PPS_TO_PPEXT(chroma_qp_offset_list_len_minus1);
      for (int i = 0; i <= pps->chroma_qp_offset_list_len_minus1; i++) {
        PPS_TO_PPEXT(cb_qp_offset_list[i]);
        PPS_TO_PPEXT(cr_qp_offset_list[i]);
      }
    }
    PPS_TO_PPEXT(log2_sao_offset_scale_luma);
    PPS_TO_PPEXT(log2_sao_offset_scale_chroma);
    if (pps->transform_skip_enabled_flag) {
      PPS_TO_PPEXT(log2_max_transform_skip_block_size_minus2);
    }
  }
  return;
}
#undef PPS_TO_PPEXT
#undef PPS_TO_PP
#undef PPS_TO_PP2
#undef PPS_TO_PP1
#undef ARG_SEL

void D3D11H265Accelerator::PicParamsFromSliceHeader(
    DXVA_PicParams_HEVC_Rext* pic_param,
    const H265SPS* sps,
    const H265SliceHeader* slice_hdr) {
  // IDR_W_RADL and IDR_N_LP NALUs do not contain st_rps in slice header.
  // Otherwise if short_term_ref_pic_set_sps_flag is 1, host decoder
  // shall set ucNumDeltaPocsOfRefRpsIdx to 0.
  if (slice_hdr->short_term_ref_pic_set_sps_flag) {
    pic_param->main.ucNumDeltaPocsOfRefRpsIdx = 0;
    pic_param->main.wNumBitsForShortTermRPSInSlice = 0;
  } else {
    pic_param->main.ucNumDeltaPocsOfRefRpsIdx =
        slice_hdr->st_ref_pic_set.rps_idx_num_delta_pocs;
    pic_param->main.wNumBitsForShortTermRPSInSlice = slice_hdr->st_rps_bits;
  }
  pic_param->main.IrapPicFlag = slice_hdr->irap_pic;
  auto nal_unit_type = slice_hdr->nal_unit_type;
  pic_param->main.IdrPicFlag = (nal_unit_type == H265NALU::IDR_W_RADL ||
                                nal_unit_type == H265NALU::IDR_N_LP);
  pic_param->main.IntraPicFlag = slice_hdr->irap_pic;
}

void D3D11H265Accelerator::PicParamsFromPic(DXVA_PicParams_HEVC_Rext* pic_param,
                                            D3D11H265Picture* pic) {
  pic_param->main.CurrPicOrderCntVal = pic->pic_order_cnt_val_;
  pic_param->main.CurrPic.Index7Bits = pic->picture_index_;
}

bool D3D11H265Accelerator::PicParamsFromRefLists(
    DXVA_PicParams_HEVC_Rext* pic_param,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before) {
  constexpr int kDxvaInvalidRefPicIndex = 0xFF;
  constexpr unsigned kStLtRpsSize = 8;

  std::fill_n(pic_param->main.RefPicSetStCurrBefore, kStLtRpsSize,
              kDxvaInvalidRefPicIndex);
  std::fill_n(pic_param->main.RefPicSetStCurrAfter, kStLtRpsSize,
              kDxvaInvalidRefPicIndex);
  std::fill_n(pic_param->main.RefPicSetLtCurr, kStLtRpsSize,
              kDxvaInvalidRefPicIndex);
  std::copy(ref_frame_pocs_, ref_frame_pocs_ + kMaxRefPicListSize - 1,
            pic_param->main.PicOrderCntValList);

  size_t idx = 0;
  for (auto& it : ref_pic_set_st_curr_before) {
    if (!it)
      continue;
    auto poc = it->pic_order_cnt_val_;
    auto poc_index = poc_index_into_ref_pic_list_[poc];
    if (poc_index < 0) {
      DLOG(ERROR) << "Invalid index of POC for RefPicSetStCurrBefore.";
      return false;
    }
    if (idx > kStLtRpsSize - 1) {
      DLOG(ERROR) << "Invalid RefPicSetStCurrBefore size.";
      return false;
    }
    pic_param->main.RefPicSetStCurrBefore[idx++] = poc_index;
  }
  idx = 0;
  for (auto& it : ref_pic_set_st_curr_after) {
    if (!it)
      continue;
    auto poc = it->pic_order_cnt_val_;
    auto poc_index = poc_index_into_ref_pic_list_[poc];
    if (poc_index < 0) {
      DLOG(ERROR) << "Invalid index of POC for RefPicSetStCurrAfter.";
      return false;
    }
    if (idx > kStLtRpsSize - 1) {
      DLOG(ERROR) << "Invalid RefPicSetStCurrAfter size.";
      return false;
    }
    pic_param->main.RefPicSetStCurrAfter[idx++] = poc_index;
  }
  idx = 0;
  for (auto& it : ref_pic_set_lt_curr) {
    if (!it)
      continue;
    auto poc = it->pic_order_cnt_val_;
    auto poc_index = poc_index_into_ref_pic_list_[poc];
    if (poc_index < 0) {
      DLOG(ERROR) << "Invalid index of POC for RefPicSetLtCurr.";
      return false;
    }
    if (idx > kStLtRpsSize - 1) {
      DLOG(ERROR) << "Invalid RefPicSetLtCurr size.";
      return false;
    }
    pic_param->main.RefPicSetLtCurr[idx++] = poc_index;
  }

  return true;
}

H265DecoderStatus D3D11H265Accelerator::SubmitSlice(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list0,
    const H265Picture::Vector& ref_pic_list1,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DXVA_PicParams_HEVC_Rext pic_param = {};

  D3D11H265Picture* d3d11_pic = pic->AsD3D11H265Picture();
  if (!d3d11_pic) {
    return H265DecoderStatus::kFail;
  }

  FillPicParamsWithConstants(&pic_param);
  PicParamsFromSPS(&pic_param, sps);
  PicParamsFromPPS(&pic_param, pps);
  PicParamsFromSliceHeader(&pic_param, sps, slice_hdr);
  PicParamsFromPic(&pic_param, d3d11_pic);
  memcpy(pic_param.main.RefPicList, ref_frame_list_,
         sizeof pic_param.main.RefPicList);

  if (!PicParamsFromRefLists(&pic_param, ref_pic_set_lt_curr,
                             ref_pic_set_st_curr_after,
                             ref_pic_set_st_curr_before)) {
    return H265DecoderStatus::kFail;
  }

  pic_param.main.StatusReportFeedbackNumber =
      current_status_report_feedback_num_++;

  UINT buffer_size;
  void* buffer;
  HRESULT hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
      &buffer_size, &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("GetDecoderBuffer (PictureParams) failed",
                  D3D11Status::Codes::kGetPicParamBufferFailed, hr);
    return H265DecoderStatus::kFail;
  }

  memcpy(buffer, &pic_param, sizeof(pic_param));
  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (PictureParams) failed",
                  D3D11Status::Codes::kReleasePicParamBufferFailed, hr);
    return H265DecoderStatus::kFail;
  }

  // Fill up the quantitization matrix data structure when
  // pps->scaling_list_enabled is true. See section 4.2
  // of DXVA spec for HEVC.
  if (use_scaling_lists_) {
    DXVA_Qmatrix_HEVC iq_matrix_buf = {};
    const H265ScalingListData* scaling_lists =
        pps->pps_scaling_list_data_present_flag ? &pps->scaling_list_data
                                                : &sps->scaling_list_data;

    static_assert(std::is_same<decltype(iq_matrix_buf.ucScalingLists0),
                               decltype(scaling_lists->scaling_list_4x4)>()
                      .value);
    memcpy(iq_matrix_buf.ucScalingLists0, scaling_lists->scaling_list_4x4,
           sizeof iq_matrix_buf.ucScalingLists0);

    static_assert(std::is_same<decltype(iq_matrix_buf.ucScalingLists1),
                               decltype(scaling_lists->scaling_list_8x8)>()
                      .value);
    memcpy(iq_matrix_buf.ucScalingLists1, scaling_lists->scaling_list_8x8,
           sizeof iq_matrix_buf.ucScalingLists1);

    static_assert(std::is_same<decltype(iq_matrix_buf.ucScalingLists2),
                               decltype(scaling_lists->scaling_list_16x16)>()
                      .value);
    memcpy(iq_matrix_buf.ucScalingLists2, scaling_lists->scaling_list_16x16,
           sizeof iq_matrix_buf.ucScalingLists2);

    static_assert(
        std::is_same<
            std::remove_reference_t<decltype(iq_matrix_buf.ucScalingLists3[0])>,
            std::remove_const_t<std::remove_reference_t<
                decltype(scaling_lists->scaling_list_32x32[0])>>>()
            .value);
    memcpy(iq_matrix_buf.ucScalingLists3[0],
           scaling_lists->scaling_list_32x32[0],
           sizeof(iq_matrix_buf.ucScalingLists3[0]));
    memcpy(iq_matrix_buf.ucScalingLists3[1],
           scaling_lists->scaling_list_32x32[3],
           sizeof(iq_matrix_buf.ucScalingLists3[1]));

    static_assert(
        std::is_same<decltype(iq_matrix_buf.ucScalingListDCCoefSizeID2),
                     decltype(scaling_lists->scaling_list_dc_coef_16x16)>()
            .value);
    memcpy(iq_matrix_buf.ucScalingListDCCoefSizeID2,
           scaling_lists->scaling_list_dc_coef_16x16,
           sizeof(iq_matrix_buf.ucScalingListDCCoefSizeID2));
    iq_matrix_buf.ucScalingListDCCoefSizeID3[0] =
        scaling_lists->scaling_list_dc_coef_32x32[0];
    iq_matrix_buf.ucScalingListDCCoefSizeID3[1] =
        scaling_lists->scaling_list_dc_coef_32x32[3];

    hr = video_context_->GetDecoderBuffer(
        video_decoder_.Get(),
        D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, &buffer_size,
        &buffer);
    if (!SUCCEEDED(hr)) {
      RecordFailure("GetDecoderBuffer (QuantMatrix) failed",
                    D3D11Status::Codes::kGetQuantBufferFailed, hr);
      return H265DecoderStatus::kFail;
    }
    memcpy(buffer, &iq_matrix_buf, sizeof(iq_matrix_buf));
    hr = video_context_->ReleaseDecoderBuffer(
        video_decoder_.Get(),
        D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX);
    if (!SUCCEEDED(hr)) {
      RecordFailure("ReleaseDecoderBuffer (QuantMatrix) failed",
                    D3D11Status::Codes::kReleaseQuantBufferFailed, hr);
      return H265DecoderStatus::kFail;
    }
  }

  // Ideally all slices in a frame are put in the same bitstream buffer.
  // However the bitstream buffer may not fit all the data, so split on the
  // necessary boundaries.
  size_t out_bitstream_size = size + 3;
  size_t remaining_bitstream = out_bitstream_size;
  size_t start_location = 0;

  if (pic->decrypt_config()) {
    // For now, the entire frame has to fit into the bitstream buffer. This way
    // the subsample ClearSize adjustment below should work.
    if (bitstream_buffer_size_ < remaining_bitstream) {
      RecordFailure("Input slice NALU (" +
                        base::NumberToString(remaining_bitstream) +
                        ") too big to fit in the bistream buffer (" +
                        base::NumberToString(bitstream_buffer_size_) + ").",
                    D3D11Status::Codes::kBitstreamBufferSliceTooBig);
      return H265DecoderStatus::kFail;
    }

    AppendSubsamples(subsamples, &subsamples_);
    if (!subsamples.empty()) {
      // Follow same logic as D3D11H264Accelerator.
      subsamples_[subsamples_.size() - subsamples.size()].ClearSize += 3;
    }
  }

  while (remaining_bitstream > 0) {
    if (bitstream_buffer_size_ < remaining_bitstream &&
        slice_info_.size() > 0) {
      if (!SubmitSliceData())
        return H265DecoderStatus::kFail;

      if (!RetrieveBitstreamBuffer())
        return H265DecoderStatus::kFail;
    }

    size_t bytes_to_copy = remaining_bitstream;
    bool contains_end = true;
    if (bytes_to_copy > bitstream_buffer_size_) {
      bytes_to_copy = bitstream_buffer_size_;
      contains_end = false;
    }
    size_t real_bytes_to_copy = bytes_to_copy;
    uint8_t* out_start = bitstream_buffer_bytes_;
    if (bytes_to_copy >= 3 && start_location == 0) {
      *(out_start++) = 0;
      *(out_start++) = 0;
      *(out_start++) = 1;
      real_bytes_to_copy -= 3;
    }
    memcpy(out_start, data + start_location, real_bytes_to_copy);

    DXVA_Slice_HEVC_Short slice_info = {};
    slice_info.BSNALunitDataLocation = (UINT)current_offset_;
    slice_info.SliceBytesInBuffer = (UINT)bytes_to_copy;
    if (contains_end && start_location == 0)
      slice_info.wBadSliceChopping = 0;
    else if (!contains_end && start_location == 0)
      slice_info.wBadSliceChopping = 1;
    else if (contains_end && start_location != 0)
      slice_info.wBadSliceChopping = 2;
    else
      slice_info.wBadSliceChopping = 3;

    slice_info_.push_back(slice_info);
    bitstream_buffer_size_ -= bytes_to_copy;
    current_offset_ += bytes_to_copy;
    start_location += bytes_to_copy;
    remaining_bitstream -= bytes_to_copy;
    bitstream_buffer_bytes_ += bytes_to_copy;
  }

  return H265DecoderStatus::kOk;
}

bool D3D11H265Accelerator::SubmitSliceData() {
  if (slice_info_.size() <= 0) {
    DLOG(ERROR) << "Invalid slice info size.";
    return false;
  }

  UINT buffer_size;
  void* buffer;

  HRESULT hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
      &buffer_size, &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("GetDecoderBuffer (SliceControl) failed",
                  D3D11Status::Codes::kGetSliceControlBufferFailed, hr);
    return false;
  }

  if (sizeof(slice_info_[0]) * slice_info_.size() > buffer_size) {
    DLOG(ERROR) << "Slice info size larger than decoder buffer size.";
    return false;
  }
  memcpy(buffer, &slice_info_[0], sizeof(slice_info_[0]) * slice_info_.size());
  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (SliceControl) failed",
                  D3D11Status::Codes::kReleaseSliceControlBufferFailed, hr);
    return false;
  }

  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (BitStream) failed",
                  D3D11Status::Codes::kReleaseBitstreamBufferFailed, hr);
    return false;
  }

  VideoContextWrapper::VideoBufferWrapper buffers[4] = {};
  buffers[0].BufferType = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
  buffers[0].DataOffset = 0;
  if (is_rext_) {
    buffers[0].DataSize = sizeof(DXVA_PicParams_HEVC_Rext);
  } else {
    buffers[0].DataSize = sizeof(DXVA_PicParams_HEVC);
  }
  buffers[1].BufferType = D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
  buffers[1].DataOffset = 0;
  buffers[1].DataSize = sizeof(slice_info_[0]) * slice_info_.size();
  buffers[2].BufferType = D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
  buffers[2].DataOffset = 0;
  buffers[2].DataSize = current_offset_;
  if (use_scaling_lists_) {
    buffers[3].BufferType =
        D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX;
    buffers[3].DataOffset = 0;
    buffers[3].DataSize = sizeof(DXVA_Qmatrix_HEVC);
  }

  if (!frame_iv_.empty()) {
    buffers[2].pIV = frame_iv_.data();
    buffers[2].IVSize = frame_iv_.size();
    // Subsmaples matter iff there is IV, for decryption.
    if (!subsamples_.empty()) {
      buffers[2].pSubSampleMappingBlock = subsamples_.data();
      buffers[2].SubSampleMappingCount = subsamples_.size();
    }
  }

  hr = video_context_->SubmitDecoderBuffers(
      video_decoder_.Get(),
      use_scaling_lists_ ? std::size(buffers) : std::size(buffers) - 1,
      buffers);
  current_offset_ = 0;
  slice_info_.clear();
  bitstream_buffer_bytes_ = nullptr;
  bitstream_buffer_size_ = 0;
  frame_iv_.clear();
  subsamples_.clear();
  if (!SUCCEEDED(hr)) {
    RecordFailure("SubmitDecoderBuffers failed",
                  D3D11Status::Codes::kSubmitDecoderBuffersFailed, hr);
    return false;
  }

  return true;
}

H265DecoderStatus D3D11H265Accelerator::SubmitDecode(
    scoped_refptr<H265Picture> pic) {
  if (!SubmitSliceData())
    return H265DecoderStatus::kFail;

  HRESULT hr = video_context_->DecoderEndFrame(video_decoder_.Get());
  if (!SUCCEEDED(hr)) {
    RecordFailure("DecoderEndFrame failed",
                  D3D11Status::Codes::kDecoderEndFrameFailed, hr);
    return H265DecoderStatus::kFail;
  }

  return H265DecoderStatus::kOk;
}

void D3D11H265Accelerator::Reset() {
  if (!bitstream_buffer_bytes_)
    return;

  HRESULT hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);

  bitstream_buffer_bytes_ = nullptr;
  bitstream_buffer_size_ = 0;
  current_offset_ = 0;
  CHECK(SUCCEEDED(hr));
}

bool D3D11H265Accelerator::OutputPicture(scoped_refptr<H265Picture> pic) {
  D3D11H265Picture* our_pic = pic->AsD3D11H265Picture();
  return our_pic && client_->OutputResult(our_pic, our_pic->picture);
}

void D3D11H265Accelerator::RecordFailure(const std::string& reason,
                                         D3D11Status::Codes code,
                                         HRESULT hr) const {
  std::string hr_string;
  if (!SUCCEEDED(hr))
    hr_string = ": " + logging::SystemErrorCodeToString(hr);

  DLOG(ERROR) << reason << hr_string;
  MEDIA_LOG(ERROR, media_log_) << hr_string << ": " << reason;
}

void D3D11H265Accelerator::RecordFailure(D3D11Status error) const {
  RecordFailure(error.message(), error.code());
}

void D3D11H265Accelerator::SetVideoDecoder(ComD3D11VideoDecoder video_decoder) {
  video_decoder_ = std::move(video_decoder);
}

}  // namespace media
