// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/video_util.h"

#include <cmath>

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/media/base/yuv_convert.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

math::Size GetNaturalSize(const math::Size& visible_size,
                          int aspect_ratio_numerator,
                          int aspect_ratio_denominator) {
  if (aspect_ratio_denominator == 0 || aspect_ratio_numerator < 0 ||
      aspect_ratio_denominator < 0)
    return math::Size();

  double aspect_ratio =
      aspect_ratio_numerator / static_cast<double>(aspect_ratio_denominator);

  return math::Size(round(visible_size.width() * aspect_ratio),
                    visible_size.height());
}

void RotatePlaneByPixels(const uint8_t* src, uint8_t* dest, int width,
                         int height,
                         int rotation,  // Clockwise.
                         bool flip_vert, bool flip_horiz) {
  DCHECK((width > 0) && (height > 0) && ((width & 1) == 0) &&
         ((height & 1) == 0) && (rotation >= 0) && (rotation < 360) &&
         (rotation % 90 == 0));

  // Consolidate cases. Only 0 and 90 are left.
  if (rotation == 180 || rotation == 270) {
    rotation -= 180;
    flip_vert = !flip_vert;
    flip_horiz = !flip_horiz;
  }

  int num_rows = height;
  int num_cols = width;
  int src_stride = width;
  // During pixel copying, the corresponding incremental of dest pointer
  // when src pointer moves to next row.
  int dest_row_step = width;
  // During pixel copying, the corresponding incremental of dest pointer
  // when src pointer moves to next column.
  int dest_col_step = 1;

  if (rotation == 0) {
    if (flip_horiz) {
      // Use pixel copying.
      dest_col_step = -1;
      if (flip_vert) {
        // Rotation 180.
        dest_row_step = -width;
        dest += height * width - 1;
      } else {
        dest += width - 1;
      }
    } else {
      if (flip_vert) {
        // Fast copy by rows.
        dest += width * (height - 1);
        for (int row = 0; row < height; ++row) {
          memcpy(dest, src, width);
          src += width;
          dest -= width;
        }
      } else {
        memcpy(dest, src, width * height);
      }
      return;
    }
  } else if (rotation == 90) {
    int offset;
    if (width > height) {
      offset = (width - height) / 2;
      src += offset;
      num_rows = num_cols = height;
    } else {
      offset = (height - width) / 2;
      src += width * offset;
      num_rows = num_cols = width;
    }

    dest_col_step = (flip_vert ? -width : width);
    dest_row_step = (flip_horiz ? 1 : -1);
    if (flip_horiz) {
      if (flip_vert) {
        dest += (width > height ? width * (height - 1) + offset
                                : width * (height - offset - 1));
      } else {
        dest += (width > height ? offset : width * offset);
      }
    } else {
      if (flip_vert) {
        dest += (width > height ? width * height - offset - 1
                                : width * (height - offset) - 1);
      } else {
        dest +=
            (width > height ? width - offset - 1 : width * (offset + 1) - 1);
      }
    }
  } else {
    NOTREACHED();
  }

  // Copy pixels.
  for (int row = 0; row < num_rows; ++row) {
    const uint8_t* src_ptr = src;
    uint8_t* dest_ptr = dest;
    for (int col = 0; col < num_cols; ++col) {
      *dest_ptr = *src_ptr++;
      dest_ptr += dest_col_step;
    }
    src += src_stride;
    dest += dest_row_step;
  }
}

}  // namespace media
}  // namespace cobalt
