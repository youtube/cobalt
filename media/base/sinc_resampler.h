// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SINC_RESAMPLER_H_
#define MEDIA_BASE_SINC_RESAMPLER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

// SincResampler is a high-quality single-channel sample-rate converter.
class MEDIA_EXPORT SincResampler {
 public:
  // Callback type for providing more data into the resampler.  Expects |frames|
  // of data to be rendered into |destination|; zero padded if not enough frames
  // are available to satisfy the request.
  typedef base::Callback<void(float* destination, int frames)> ReadCB;

  // Constructs a SincResampler with the specified |read_cb|, which is used to
  // acquire audio data for resampling.  |io_sample_rate_ratio| is the ratio of
  // input / output sample rates.
  SincResampler(double io_sample_rate_ratio, const ReadCB& read_cb);
  virtual ~SincResampler();

  // Resample |frames| of data from |read_cb_| into |destination|.
  void Resample(float* destination, int frames);

  // The maximum size in frames that guarantees Resample() will only make a
  // single call to |read_cb_| for more data.
  int ChunkSize();

 private:
  void InitializeKernel();

  // The ratio of input / output sample rates.
  double io_sample_rate_ratio_;

  // An index on the source input buffer with sub-sample precision.  It must be
  // double precision to avoid drift.
  double virtual_source_idx_;

  // The buffer is primed once at the very beginning of processing.
  bool buffer_primed_;

  // Source of data for resampling.
  ReadCB read_cb_;

  // Contains kKernelOffsetCount kernels back-to-back, each of size kKernelSize.
  // The kernel offsets are sub-sample shifts of a windowed sinc shifted from
  // 0.0 to 1.0 sample.
  scoped_array<float> kernel_storage_;

  // Data from the source is copied into this buffer for each processing pass.
  scoped_array<float> input_buffer_;

  // Pointers to the various regions inside |input_buffer_|.  See the diagram at
  // the top of the .cc file for more information.
  float* const r0_;
  float* const r1_;
  float* const r2_;
  float* const r3_;
  float* const r4_;
  float* const r5_;
};

}  // namespace media

#endif  // MEDIA_BASE_SINC_RESAMPLER_H_
