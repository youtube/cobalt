// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/mojom/status_mojom_traits.h"

#include "third_party/chromium/media/base/status_codes.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    media_m96::mojom::StatusDataDataView,
    media_m96::internal::StatusData>::Read(media_m96::mojom::StatusDataDataView data,
                                       media_m96::internal::StatusData* output) {
  output->code = data.code();

  if (!data.ReadGroup(&output->group))
    return false;

  if (!data.ReadMessage(&output->message))
    return false;

  if (!data.ReadFrames(&output->frames))
    return false;

  if (!data.ReadData(&output->data))
    return false;

  std::vector<media_m96::internal::StatusData> causes;
  if (!data.ReadCauses(&causes))
    return false;

  for (const auto& cause : causes)
    output->causes.push_back(cause);

  return true;
}

}  // namespace mojo
