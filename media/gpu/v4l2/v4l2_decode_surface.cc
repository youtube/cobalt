// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_decode_surface.h"

#include <linux/media.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/macros.h"

namespace media {

V4L2DecodeSurface::V4L2DecodeSurface(V4L2WritableBufferRef input_buffer,
                                     V4L2WritableBufferRef output_buffer,
                                     scoped_refptr<VideoFrame> frame)
    : input_buffer_(std::move(input_buffer)),
      output_buffer_(std::move(output_buffer)),
      video_frame_(std::move(frame)),
      input_record_(input_buffer_.BufferId()),
      output_record_(output_buffer_.BufferId()),
      decoded_(false) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

V4L2DecodeSurface::~V4L2DecodeSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOGF(5) << "Releasing output record id=" << output_record_;
  if (release_cb_)
    std::move(release_cb_).Run();
}

void V4L2DecodeSurface::SetDecoded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!decoded_);
  decoded_ = true;

  // We can now drop references to all reference surfaces for this surface
  // as we are done with decoding.
  reference_surfaces_.clear();

  // And finally execute and drop the decode done callback, if set.
  if (done_cb_)
    std::move(done_cb_).Run();
}

void V4L2DecodeSurface::SetVisibleRect(const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  visible_rect_ = visible_rect;
}

void V4L2DecodeSurface::SetReferenceSurfaces(
    std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(reference_surfaces_.empty());
#if DCHECK_IS_ON()
  for (const auto& ref : reference_surfaces_)
    DCHECK_NE(ref->output_record(), output_record_);
#endif

  reference_surfaces_ = std::move(ref_surfaces);
}

void V4L2DecodeSurface::SetDecodeDoneCallback(base::OnceClosure done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!done_cb_);

  done_cb_ = std::move(done_cb);
}

void V4L2DecodeSurface::SetReleaseCallback(base::OnceClosure release_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!release_cb_);

  release_cb_ = std::move(release_cb);
}

std::string V4L2DecodeSurface::ToString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string out;
  base::StringAppendF(&out, "Buffer %d -> %d. ", input_record_, output_record_);
  base::StringAppendF(&out, "Reference surfaces:");
  for (const auto& ref : reference_surfaces_) {
    DCHECK_NE(ref->output_record(), output_record_);
    base::StringAppendF(&out, " %d", ref->output_record());
  }
  return out;
}

void V4L2ConfigStoreDecodeSurface::PrepareSetCtrls(
    struct v4l2_ext_controls* ctrls) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(ctrls, nullptr);
  DCHECK_GT(config_store_, 0u);

  ctrls->config_store = config_store_;
}

uint64_t V4L2ConfigStoreDecodeSurface::GetReferenceID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Control store uses the output buffer ID as reference.
  return output_record();
}

bool V4L2ConfigStoreDecodeSurface::Submit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(config_store_, 0u);

  input_buffer().SetConfigStore(config_store_);

  if (!std::move(input_buffer()).QueueMMap()) {
    return false;
  }

  switch (output_buffer().Memory()) {
    case V4L2_MEMORY_MMAP:
      return std::move(output_buffer()).QueueMMap();
    case V4L2_MEMORY_DMABUF:
      return std::move(output_buffer()).QueueDMABuf(video_frame());
    default:
      NOTREACHED() << "We should only use MMAP or DMABUF.";
  }

  return false;
}

void V4L2RequestDecodeSurface::PrepareSetCtrls(
    struct v4l2_ext_controls* ctrls) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(ctrls, nullptr);

  request_ref_.ApplyCtrls(ctrls);
}

uint64_t V4L2RequestDecodeSurface::GetReferenceID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the input buffer ID to what the internal representation of
  // the timestamp we submitted will be (tv_usec * 1000).
  return output_record() * 1000;
}

bool V4L2RequestDecodeSurface::Submit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Use the output buffer index as the timestamp.
  // Since the client is supposed to keep the output buffer out of the V4L2
  // queue for as long as it is used as a reference frame, this ensures that
  // all the requests we submit have unique IDs at any point in time.
  struct timeval timestamp = {
      .tv_sec = 0,
      .tv_usec = output_record()
  };
  input_buffer().SetTimeStamp(timestamp);

  if (!std::move(input_buffer()).QueueMMap(&request_ref_)) {
    return false;
  }

  bool result = false;
  switch (output_buffer().Memory()) {
    case V4L2_MEMORY_MMAP:
      result = std::move(output_buffer()).QueueMMap();
      break;
    case V4L2_MEMORY_DMABUF:
      result = std::move(output_buffer()).QueueDMABuf(video_frame());
      break;
    default:
      NOTREACHED() << "We should only use MMAP or DMABUF.";
  }

  if (!result)
    return result;

  return std::move(request_ref_).Submit().has_value();
}

}  // namespace media
