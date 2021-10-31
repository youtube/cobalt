// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/blitter.h"
#include "starboard/common/log.h"

namespace {
// Round the given float to the nearest integer.
int round(float x) {
  return static_cast<int>(x + 0.5f);
}

// A periodic function that follows the same sawtooth pattern as the modulus
// function in both the positive and negative domain.
int positive_mod(int x, int m) {
  if (x >= 0) {
    return x % m;
  } else {
    int mod_out = (1 - (x / m)) * m + x;
    return mod_out == m ? 0 : mod_out;
  }
}

// The Tiler class is a helper class to determine how to chop up into tiles a
// source image into a destination rectangle.  The Tiler will pre-compute all
// the information needed to determine coordinates of individual tiles.  This
// makes it easy to have every SbBlitterBlitRectToRectTiled() call implemented
// by a SbBlitterBlitRectsToRects() call with one rect per tile.
class Tiler {
 public:
  Tiler(const SbBlitterRect& src_rect,
        const SbBlitterRect& dst_rect,
        int src_image_width,
        int src_image_height);

  // Returns the number of tiles in the respective dimensions, including partial
  // tiles.
  int num_x_tiles() const;
  int num_y_tiles() const;

  // Returns the rectangles for the source rectangle for the given tile
  // coordinate.
  SbBlitterRect src_rect(int x, int y) const;
  // Returns the rectangles for the destination rectangle for the given tile
  // coordinate.
  SbBlitterRect dst_rect(int x, int y) const;

 private:
  float dst_x(int x_index) const;
  float dst_y(int y_index) const;

  // Saved input parameters.
  int source_image_width_;
  int source_image_height_;
  SbBlitterRect dst_rect_;

  // The positions within the source image of the first tile, where we start.
  int src_first_x_;
  int src_first_y_;

  // The positions within the source image of the last tile, where we end.
  int src_last_y_;
  int src_last_x_;

  // The number of tiles in each direction, counting partial tiles.
  int num_x_tiles_;
  int num_y_tiles_;

  // The size of the first (possibly partial) tile, in destination pixels.
  float dst_x_first_width_;
  float dst_y_first_height_;

  // The size of a full tile, in destination pixels.
  float dst_tile_width_;
  float dst_tile_height_;
};

Tiler::Tiler(const SbBlitterRect& src_rect,
             const SbBlitterRect& dst_rect,
             int src_image_width,
             int src_image_height) {
  // Precompute all our helper values here in the constructor.
  source_image_width_ = src_image_width;
  source_image_height_ = src_image_height;
  dst_rect_ = dst_rect;

  src_first_x_ = positive_mod(src_rect.x, src_image_width);
  int num_x_full_tiles =
      (src_rect.width -
       (src_first_x_ != 0 ? (src_image_width - src_first_x_) : 0)) /
      src_image_width;
  src_last_x_ = positive_mod(src_rect.width - (src_image_width - src_first_x_),
                             src_image_width);
  if (src_last_x_ == 0) {
    src_last_x_ = src_image_width;
  }

  num_x_tiles_ =
      (num_x_full_tiles > 0
           ? num_x_full_tiles + (src_first_x_ != 0 ? 1 : 0) +
                 (src_last_x_ != src_image_width ? 1 : 0)
           : (src_first_x_ + src_rect.width > src_image_width ? 2 : 1));

  src_first_y_ = positive_mod(src_rect.y, src_image_height);
  int num_y_full_tiles =
      (src_rect.height -
       (src_first_y_ != 0 ? (src_image_height - src_first_y_) : 0)) /
      src_image_height;
  src_last_y_ = positive_mod(
      src_rect.height - (src_image_height - src_first_y_), src_image_height);
  if (src_last_y_ == 0) {
    src_last_y_ = src_image_height;
  }

  num_y_tiles_ =
      (num_y_full_tiles > 0
           ? num_y_full_tiles + (src_first_y_ != 0 ? 1 : 0) +
                 (src_last_y_ != src_image_height ? 1 : 0)
           : (src_first_y_ + src_rect.height > src_image_height ? 2 : 1));

  dst_tile_width_ =
      (static_cast<float>(src_image_width) / src_rect.width) * dst_rect.width;
  dst_x_first_width_ =
      dst_tile_width_ *
      (static_cast<float>(src_image_width - src_first_x_) / src_image_width);

  dst_tile_height_ = (static_cast<float>(src_image_height) / src_rect.height) *
                     dst_rect.height;
  dst_y_first_height_ =
      dst_tile_height_ *
      (static_cast<float>(src_image_height - src_first_y_) / src_image_height);
}

int Tiler::num_x_tiles() const {
  return num_x_tiles_;
}

int Tiler::num_y_tiles() const {
  return num_y_tiles_;
}

SbBlitterRect Tiler::src_rect(int x, int y) const {
  SbBlitterRect ret;

  ret.x = (x == 0 ? src_first_x_ : 0);
  ret.width =
      (x == num_x_tiles() - 1 ? src_last_x_ : source_image_width_) - ret.x;

  ret.y = (y == 0 ? src_first_y_ : 0);
  ret.height =
      (y == num_y_tiles() - 1 ? src_last_y_ : source_image_height_) - ret.y;

  return ret;
}

SbBlitterRect Tiler::dst_rect(int x, int y) const {
  SbBlitterRect ret;
  int x1 = round(dst_x(x));
  int x2 = round(dst_x(x + 1));
  int y1 = round(dst_y(y));
  int y2 = round(dst_y(y + 1));

  ret.x = x1;
  ret.y = y1;
  ret.width = x2 - x1;
  ret.height = y2 - y1;

  return ret;
}

float Tiler::dst_x(int x_index) const {
  if (x_index == 0) {
    return static_cast<float>(dst_rect_.x);
  } else if (x_index == num_x_tiles()) {
    return static_cast<float>(dst_rect_.x + dst_rect_.width);
  } else if (x_index == 1) {
    return dst_rect_.x + dst_x_first_width_;
  } else {
    return dst_rect_.x + dst_x_first_width_ + dst_tile_width_ * (x_index - 1);
  }
}

float Tiler::dst_y(int y_index) const {
  if (y_index == 0) {
    return static_cast<float>(dst_rect_.y);
  } else if (y_index == num_y_tiles()) {
    return static_cast<float>(dst_rect_.y + dst_rect_.height);
  } else if (y_index == 1) {
    return dst_rect_.y + dst_y_first_height_;
  } else {
    return dst_rect_.y + dst_y_first_height_ + dst_tile_height_ * (y_index - 1);
  }
}

}  // namespace

// Implement the tiling by splitting our draw area into individual tiles
// and rendering each one explicitly via SbBlitterBlitRectsToRects().
bool SbBlitterBlitRectToRectTiled(SbBlitterContext context,
                                  SbBlitterSurface source_surface,
                                  SbBlitterRect src_rect,
                                  SbBlitterRect dst_rect) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsSurfaceValid(source_surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid texture.";
    return false;
  }
  if (src_rect.width <= 0 || src_rect.height <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Source width and height must both be "
                   << "greater than 0.";
    return false;
  }
  if (dst_rect.width < 0 || dst_rect.height < 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Destination width and height must "
                   << "both be greater than or equal to 0.";
    return false;
  }

  SbBlitterSurfaceInfo surface_info;
  SbBlitterGetSurfaceInfo(source_surface, &surface_info);

  Tiler tiler(src_rect, dst_rect, surface_info.width, surface_info.height);

  int num_tiles = tiler.num_x_tiles() * tiler.num_y_tiles();
  SbBlitterRect* src_rects = new SbBlitterRect[num_tiles];
  SbBlitterRect* dst_rects = new SbBlitterRect[num_tiles];

  int current_tile = 0;
  for (int y = 0; y < tiler.num_y_tiles(); ++y) {
    for (int x = 0; x < tiler.num_x_tiles(); ++x) {
      src_rects[current_tile] = tiler.src_rect(x, y);
      dst_rects[current_tile] = tiler.dst_rect(x, y);
      ++current_tile;
    }
  }

  bool result = SbBlitterBlitRectsToRects(context, source_surface, src_rects,
                                          dst_rects, num_tiles);

  delete[] src_rects;
  delete[] dst_rects;

  return result;
}
