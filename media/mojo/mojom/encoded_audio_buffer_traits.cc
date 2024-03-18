// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/encoded_audio_buffer_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::EncodedAudioBufferDataView,
                  media::EncodedAudioBuffer>::
    Read(media::mojom::EncodedAudioBufferDataView input,
         media::EncodedAudioBuffer* output) {
  media::AudioParameters params;
  if (!input.ReadParams(&params))
    return false;

  base::TimeDelta ts;
  if (!input.ReadTimestamp(&ts))
    return false;
  base::TimeTicks timestamp = ts + base::TimeTicks();

  base::TimeDelta duration;
  if (!input.ReadDuration(&duration))
    return false;

  mojo::ArrayDataView<uint8_t> data_view;
  input.GetDataDataView(&data_view);
  if (data_view.is_null())
    return false;

  size_t encoded_data_size = data_view.size();
  std::unique_ptr<uint8_t[]> encoded_data(new uint8_t[encoded_data_size]);
  memcpy(encoded_data.get(), data_view.data(), encoded_data_size);

  *output = media::EncodedAudioBuffer(params, std::move(encoded_data),
                                      encoded_data_size, timestamp, duration);
  return true;
}

}  // namespace mojo
