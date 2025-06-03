// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/picture_layer_tiling.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/raster/raster_source.h"
#include "cc/tiles/prioritized_tile.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

PictureLayerTiling::PictureLayerTiling(
    WhichTree tree,
    const gfx::AxisTransform2d& raster_transform,
    scoped_refptr<RasterSource> raster_source,
    PictureLayerTilingClient* client,
    float min_preraster_distance,
    float max_preraster_distance,
    bool can_use_lcd_text)
    : raster_transform_(raster_transform),
      client_(client),
      tree_(tree),
      raster_source_(raster_source),
      min_preraster_distance_(min_preraster_distance),
      max_preraster_distance_(max_preraster_distance),
      can_use_lcd_text_(can_use_lcd_text) {
  DCHECK(!raster_source->IsSolidColor());
  DCHECK_GE(raster_transform.translation().x(), 0.f);
  DCHECK_LT(raster_transform.translation().x(), 1.f);
  DCHECK_GE(raster_transform.translation().y(), 0.f);
  DCHECK_LT(raster_transform.translation().y(), 1.f);

#if DCHECK_IS_ON()
  gfx::SizeF scaled_source_size(gfx::ScaleSize(
      gfx::SizeF(raster_source_->GetSize()), raster_transform.scale().x(),
      raster_transform.scale().y()));
  gfx::Size floored_size = gfx::ToFlooredSize(scaled_source_size);
  bool is_width_empty =
      !floored_size.width() &&
      !MathUtil::IsWithinEpsilon(scaled_source_size.width(), 1.f);
  bool is_height_empty =
      !floored_size.height() &&
      !MathUtil::IsWithinEpsilon(scaled_source_size.height(), 1.f);
  DCHECK(!is_width_empty && !is_height_empty)
      << "Tiling created with scale too small as contents become empty."
      << " Layer bounds: " << raster_source_->GetSize().ToString()
      << " Raster transform: " << raster_transform_.ToString();
#endif

  gfx::Rect content_bounds_rect =
      EnclosingContentsRectFromLayerRect(gfx::Rect(raster_source_->GetSize()));
  gfx::Size tiling_size = gfx::Size(content_bounds_rect.bottom_right().x(),
                                    content_bounds_rect.bottom_right().y());
  tiling_data_.SetTilingSize(tiling_size);
  gfx::Size tile_size = client_->CalculateTileSize(tiling_size);
  tiling_data_.SetMaxTextureSize(tile_size);
}

PictureLayerTiling::~PictureLayerTiling() = default;

Tile* PictureLayerTiling::CreateTile(const Tile::CreateInfo& info) {
  const int i = info.tiling_i_index;
  const int j = info.tiling_j_index;
  TileMapKey key(i, j);
  DCHECK(!base::Contains(tiles_, key));

  if (!raster_source_->IntersectsRect(info.enclosing_layer_rect, *client_))
    return nullptr;

  all_tiles_done_ = false;
  std::unique_ptr<Tile> tile = client_->CreateTile(info);
  Tile* tile_ptr = tile.get();
  tiles_[key] = std::move(tile);
  return tile_ptr;
}

void PictureLayerTiling::CreateMissingTilesInLiveTilesRect() {
  const PictureLayerTiling* active_twin =
      tree_ == PENDING_TREE ? client_->GetPendingOrActiveTwinTiling(this)
                            : nullptr;
  const Region* invalidation =
      active_twin ? client_->GetPendingInvalidation() : nullptr;

  bool include_borders = false;
  for (TilingData::Iterator iter(&tiling_data_, live_tiles_rect_,
                                 include_borders);
       iter; ++iter) {
    TileMapKey key(iter.index());
    auto find = tiles_.find(key);
    if (find != tiles_.end())
      continue;

    Tile::CreateInfo info = CreateInfoForTile(key.index_x, key.index_y);
    if (ShouldCreateTileAt(info)) {
      Tile* tile = CreateTile(info);

      // If this is the pending tree, then the active twin tiling may contain
      // the previous content ID of these tiles. In that case, we need only
      // partially raster the tile content.
      if (tile && invalidation && TilingMatchesTileIndices(active_twin)) {
        if (const Tile* old_tile =
                active_twin->TileAt(key.index_x, key.index_y)) {
          gfx::Rect tile_rect = tile->content_rect();
          gfx::Rect invalidated;
          for (gfx::Rect rect : *invalidation) {
            gfx::Rect invalid_content_rect =
                EnclosingContentsRectFromLayerRect(rect);
            invalid_content_rect.Intersect(tile_rect);
            invalidated.Union(invalid_content_rect);
          }
          tile->SetInvalidated(invalidated, old_tile->id());
        }
      }
    }
  }
  VerifyLiveTilesRect();
}

void PictureLayerTiling::TakeTilesAndPropertiesFrom(
    PictureLayerTiling* pending_twin,
    const Region& layer_invalidation) {
  SetRasterSourceAndResize(pending_twin->raster_source_);

  RemoveTilesInRegion(layer_invalidation, false /* recreate tiles */);

  resolution_ = pending_twin->resolution_;
  bool create_missing_tiles = false;
  if (live_tiles_rect_.IsEmpty()) {
    live_tiles_rect_ = pending_twin->live_tiles_rect();
    create_missing_tiles = true;
  } else {
    SetLiveTilesRect(pending_twin->live_tiles_rect());
  }

  while (!pending_twin->tiles_.empty()) {
    auto pending_iter = pending_twin->tiles_.begin();
    pending_iter->second->set_tiling(this);
    tiles_[pending_iter->first] = std::move(pending_iter->second);
    pending_twin->tiles_.erase(pending_iter);
  }
  all_tiles_done_ &= pending_twin->all_tiles_done_;

  DCHECK(pending_twin->tiles_.empty());
  pending_twin->all_tiles_done_ = true;

  if (create_missing_tiles)
    CreateMissingTilesInLiveTilesRect();

  VerifyLiveTilesRect();

  SetTilePriorityRects(pending_twin->current_content_to_screen_scale_,
                       pending_twin->current_visible_rect_,
                       pending_twin->current_skewport_rect_,
                       pending_twin->current_soon_border_rect_,
                       pending_twin->current_eventually_rect_,
                       pending_twin->current_occlusion_in_layer_space_);
}

bool PictureLayerTiling::SetRasterSourceAndResize(
    scoped_refptr<RasterSource> raster_source) {
  DCHECK(!raster_source->IsSolidColor());
  gfx::Size old_layer_bounds = raster_source_->GetSize();
  raster_source_ = std::move(raster_source);
  gfx::Size new_layer_bounds = raster_source_->GetSize();
  gfx::Rect content_rect =
      EnclosingContentsRectFromLayerRect(gfx::Rect(new_layer_bounds));
  DCHECK(content_rect.origin() == gfx::Point());
  gfx::Size tile_size = client_->CalculateTileSize(content_rect.size());

  if (tile_size != tiling_data_.max_texture_size()) {
    tiling_data_.SetTilingSize(content_rect.size());
    tiling_data_.SetMaxTextureSize(tile_size);
    // When the tile size changes, the TilingData positions no longer work
    // as valid keys to the TileMap, so just drop all tiles and clear the live
    // tiles rect.
    Reset();
    // When the tile size changes, all tiles and all tile priority rects
    // including the live tiles rect should be updated, therefore return true to
    // notify the caller to call |ComputeTilePriorityRects| to do this.
    return true;
  }

  // When the layer bounds are the same, we need not notify the caller as it
  // will update tiling as needed, so return false.
  if (old_layer_bounds == new_layer_bounds)
    return false;

  // The SetLiveTilesRect() method would drop tiles outside the new bounds,
  // but may do so incorrectly if resizing the tiling causes the number of
  // tiles in the tiling_data_ to change.
  int before_left = tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.x());
  int before_top = tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.y());
  int before_right =
      tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.right() - 1);
  int before_bottom =
      tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.bottom() - 1);

  // The live_tiles_rect_ is clamped to stay within the tiling size as we
  // change it.
  live_tiles_rect_.Intersect(content_rect);
  tiling_data_.SetTilingSize(content_rect.size());

  int after_right = -1;
  int after_bottom = -1;
  if (!live_tiles_rect_.IsEmpty()) {
    after_right =
        tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.right() - 1);
    after_bottom =
        tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.bottom() - 1);
  }

  // There is no recycled twin since this is run on the pending tiling
  // during commit, and on the active tree during activate.
  // Drop tiles outside the new layer bounds if the layer shrank.
  for (int i = after_right + 1; i <= before_right; ++i) {
    for (int j = before_top; j <= before_bottom; ++j)
      TakeTileAt(i, j);
  }
  for (int i = before_left; i <= after_right; ++i) {
    for (int j = after_bottom + 1; j <= before_bottom; ++j)
      TakeTileAt(i, j);
  }

  if (after_right > before_right) {
    DCHECK_EQ(after_right, before_right + 1);
    for (int j = before_top; j <= after_bottom; ++j) {
      Tile::CreateInfo info = CreateInfoForTile(after_right, j);
      if (ShouldCreateTileAt(info))
        CreateTile(info);
    }
  }
  if (after_bottom > before_bottom) {
    // Using the smallest horizontal bound here makes sure we don't
    // create tiles twice and don't iterate into deleted tiles.
    int boundary_right = std::min(after_right, before_right);
    DCHECK_EQ(after_bottom, before_bottom + 1);
    for (int i = before_left; i <= boundary_right; ++i) {
      Tile::CreateInfo info = CreateInfoForTile(i, after_bottom);
      if (ShouldCreateTileAt(info))
        CreateTile(info);
    }
  }
  // We need not notify the caller as it will update tiling as needed, so return
  // false to ensure the existing logic remains unchanged.
  return false;
}

void PictureLayerTiling::Invalidate(const Region& layer_invalidation) {
  DCHECK(tree_ != ACTIVE_TREE || !client_->GetPendingOrActiveTwinTiling(this));
  RemoveTilesInRegion(layer_invalidation, true /* recreate tiles */);
}

void PictureLayerTiling::RemoveTilesInRegion(const Region& layer_invalidation,
                                             bool recreate_tiles) {
  // We only invalidate the active tiling when it's orphaned: it has no pending
  // twin, so it's slated for removal in the future.
  if (live_tiles_rect_.IsEmpty())
    return;

  base::flat_map<TileMapKey, gfx::Rect> remove_tiles;
  gfx::Rect expanded_live_tiles_rect =
      tiling_data_.ExpandRectToTileBounds(live_tiles_rect_);
  for (gfx::Rect layer_rect : layer_invalidation) {
    // The pixels which are invalid in content space.
    gfx::Rect invalid_content_rect =
        EnclosingContentsRectFromLayerRect(layer_rect);
    gfx::Rect coverage_content_rect = invalid_content_rect;
    // Avoid needless work by not bothering to invalidate where there aren't
    // tiles.
    coverage_content_rect.Intersect(expanded_live_tiles_rect);
    if (coverage_content_rect.IsEmpty())
      continue;
    // Since the content_rect needs to invalidate things that only touch a
    // border of a tile, we need to include the borders while iterating.
    bool include_borders = true;
    for (TilingData::Iterator iter(&tiling_data_, coverage_content_rect,
                                   include_borders);
         iter; ++iter) {
      // This also adds the TileMapKey to the map.
      remove_tiles[TileMapKey(iter.index())].Union(invalid_content_rect);
    }
  }

  for (const auto& pair : remove_tiles) {
    const TileMapKey& key = pair.first;
    const gfx::Rect& invalid_content_rect = pair.second;
    // TODO(danakj): This old_tile will not exist if we are committing to a
    // pending tree since there is no tile there to remove, which prevents
    // tiles from knowing the invalidation rect and content id. crbug.com/490847
    std::unique_ptr<Tile> old_tile = TakeTileAt(key.index_x, key.index_y);
    if (recreate_tiles && old_tile) {
      Tile::CreateInfo info = CreateInfoForTile(key.index_x, key.index_y);
      if (Tile* tile = CreateTile(info))
        tile->SetInvalidated(invalid_content_rect, old_tile->id());
    }
  }
}

Tile::CreateInfo PictureLayerTiling::CreateInfoForTile(int i, int j) const {
  gfx::Rect tile_rect = tiling_data_.TileBoundsWithBorder(i, j);
  tile_rect.set_size(tiling_data_.max_texture_size());
  gfx::Rect enclosing_layer_rect =
      EnclosingLayerRectFromContentsRect(tile_rect);
  return Tile::CreateInfo{this,
                          i,
                          j,
                          enclosing_layer_rect,
                          tile_rect,
                          raster_transform_,
                          can_use_lcd_text_};
}

bool PictureLayerTiling::ShouldCreateTileAt(
    const Tile::CreateInfo& info) const {
  const int i = info.tiling_i_index;
  const int j = info.tiling_j_index;
  // Active tree should always create a tile. The reason for this is that active
  // tree represents content that we draw on screen, which means that whenever
  // we check whether a tile should exist somewhere, the answer is yes. This
  // doesn't mean it will actually be created (if raster source doesn't cover
  // the tile for instance). Pending tree, on the other hand, should only be
  // creating tiles that are different from the current active tree, which is
  // represented by the logic in the rest of the function.
  if (tree_ == ACTIVE_TREE)
    return true;

  // If the pending tree has no active twin, then it needs to create all tiles.
  const PictureLayerTiling* active_twin =
      client_->GetPendingOrActiveTwinTiling(this);
  if (!active_twin)
    return true;

  // Pending tree will override the entire active tree if indices don't match.
  if (!TilingMatchesTileIndices(active_twin))
    return true;

  // If our settings don't match the active twin, it means that the active
  // tiles will all be removed when we activate. So we need all the tiles on the
  // pending tree to be created. See
  // PictureLayerTilingSet::CopyTilingsAndPropertiesFromPendingTwin.
  if (can_use_lcd_text() != active_twin->can_use_lcd_text() ||
      raster_transform() != active_twin->raster_transform())
    return true;

  // If the active tree can't create a tile, because of its raster source, then
  // the pending tree should create one.
  if (!active_twin->raster_source()->IntersectsRect(info.enclosing_layer_rect,
                                                    *active_twin->client()))
    return true;

  const Region* layer_invalidation = client_->GetPendingInvalidation();

  // If this tile is invalidated, then the pending tree should create one.
  // Do the intersection test in content space to match the corresponding check
  // on the active tree and avoid floating point inconsistencies.
  for (gfx::Rect layer_rect : *layer_invalidation) {
    gfx::Rect invalid_content_rect =
        EnclosingContentsRectFromLayerRect(layer_rect);
    if (invalid_content_rect.Intersects(info.content_rect))
      return true;
  }
  // If the active tree doesn't have a tile here, but it's in the pending tree's
  // visible rect, then the pending tree should create a tile. This can happen
  // if the pending visible rect is outside of the active tree's live tiles
  // rect. In those situations, we need to block activation until we're ready to
  // display content, which will have to come from the pending tree.
  if (!active_twin->TileAt(i, j) &&
      current_visible_rect_.Intersects(info.content_rect))
    return true;

  // In all other cases, the pending tree doesn't need to create a tile.
  return false;
}

bool PictureLayerTiling::TilingMatchesTileIndices(
    const PictureLayerTiling* twin) const {
  return tiling_data_.max_texture_size() ==
         twin->tiling_data_.max_texture_size();
}

PictureLayerTiling::CoverageIterator::CoverageIterator() = default;

PictureLayerTiling::CoverageIterator::CoverageIterator(
    const PictureLayerTiling* tiling,
    float coverage_scale,
    const gfx::Rect& coverage_rect)
    : tiling_(tiling),
      coverage_rect_(coverage_rect),
      coverage_to_content_(PreScaleAxisTransform2d(tiling->raster_transform(),
                                                   1 / coverage_scale)) {
  DCHECK(tiling_);
  // In order to avoid artifacts in geometry_rect scaling and clamping to ints,
  // the |coverage_scale| should always be at least as big as the tiling's
  // raster scales.
  DCHECK_GE(coverage_scale, tiling_->contents_scale_key());

  // Clamp |coverage_rect| to the bounds of this tiling's raster source.
  coverage_rect_max_bounds_ =
      gfx::ScaleToCeiledSize(tiling->raster_source_->GetSize(), coverage_scale);
  coverage_rect_.Intersect(gfx::Rect(coverage_rect_max_bounds_));
  if (coverage_rect_.IsEmpty())
    return;

  // Find the indices of the texel samples that enclose the rect we want to
  // cover.
  // Because we don't know the target transform at this point, we have to be
  // pessimistic, i.e. assume every point (a pair of real number, not necessary
  // snapped to a pixel sample) inside of the content rect may be sampled.
  // This code maps the boundary points into contents space, then find out the
  // enclosing texture samples. For example, assume we have:
  // coverage_scale : content_scale = 1.23 : 1
  // coverage_rect = (l:123, t:234, r:345, b:456)
  // Then it follows that:
  // content_rect = (l:100.00, t:190.24, r:280.49, b:370.73)
  // Without MSAA, the sample point of a texel is at the center of that texel,
  // thus the sample points we need to cover content_rect are:
  // wanted_texels(sample coordinates) = (l:99.5, t:189.5, r:280.5, b:371.5)
  // Or in integer index:
  // wanted_texels(integer index) = (l:99, t:189, r:280, b:371)
  gfx::RectF content_rect =
      coverage_to_content_.MapRect(gfx::RectF(coverage_rect_));
  content_rect.Offset(-0.5f, -0.5f);
  gfx::Rect wanted_texels = gfx::ToEnclosingRect(content_rect);

  const TilingData& data = tiling_->tiling_data_;
  left_ = data.LastBorderTileXIndexFromSrcCoord(wanted_texels.x());
  top_ = data.LastBorderTileYIndexFromSrcCoord(wanted_texels.y());
  right_ = std::max(
      left_, data.FirstBorderTileXIndexFromSrcCoord(wanted_texels.right()));
  bottom_ = std::max(
      top_, data.FirstBorderTileYIndexFromSrcCoord(wanted_texels.bottom()));

  tile_i_ = left_ - 1;
  tile_j_ = top_;
  ++(*this);
}

PictureLayerTiling::CoverageIterator::~CoverageIterator() = default;

PictureLayerTiling::CoverageIterator&
PictureLayerTiling::CoverageIterator::operator++() {
  if (tile_j_ > bottom_)
    return *this;

  bool first_time = tile_i_ < left_;
  while (true) {
    bool new_row = false;
    tile_i_++;
    if (tile_i_ > right_) {
      tile_i_ = left_;
      tile_j_++;
      new_row = true;
      if (tile_j_ > bottom_) {
        current_tile_ = nullptr;
        break;
      }
    }

    DCHECK_LT(tile_i_, tiling_->tiling_data_.num_tiles_x());
    DCHECK_LT(tile_j_, tiling_->tiling_data_.num_tiles_y());
    current_tile_ = tiling_->TileAt(tile_i_, tile_j_);

    gfx::Rect geometry_rect_candidate = ComputeGeometryRect();

    // This can happen due to floating point inprecision when calculating the
    // |wanted_texels| area in the constructor.
    if (geometry_rect_candidate.IsEmpty())
      continue;

    gfx::Rect last_geometry_rect = current_geometry_rect_;
    current_geometry_rect_ = geometry_rect_candidate;

    if (first_time)
      break;

    // Iteration happens left->right, top->bottom.  Running off the bottom-right
    // edge is handled by the intersection above with dest_rect_.  Here we make
    // sure that the new current geometry rect doesn't overlap with the last.
    int min_left;
    int min_top;
    if (new_row) {
      min_left = coverage_rect_.x();
      min_top = last_geometry_rect.bottom();
    } else {
      min_left = last_geometry_rect.right();
      min_top = last_geometry_rect.y();
    }

    int inset_left = std::max(0, min_left - current_geometry_rect_.x());
    int inset_top = std::max(0, min_top - current_geometry_rect_.y());
    current_geometry_rect_.Inset(
        gfx::Insets::TLBR(inset_top, inset_left, 0, 0));

#if DCHECK_IS_ON()
    // Sometimes we run into an extreme case where we are at the edge of integer
    // precision. When doing so, rect calculations may end up changing values
    // unexpectedly. Unfortunately, there isn't much we can do at this point, so
    // we just do the correctness checks if both y and x offsets are
    // 'reasonable', meaning they are less than the specified value.
    static constexpr int kReasonableOffsetForDcheck = 100'000'000;
    if (!new_row && current_geometry_rect_.x() <= kReasonableOffsetForDcheck &&
        current_geometry_rect_.y() <= kReasonableOffsetForDcheck) {
      DCHECK_EQ(last_geometry_rect.right(), current_geometry_rect_.x());
      DCHECK_EQ(last_geometry_rect.bottom(), current_geometry_rect_.bottom());
      DCHECK_EQ(last_geometry_rect.y(), current_geometry_rect_.y());
    }
#endif

    break;
  }
  return *this;
}

gfx::Rect PictureLayerTiling::CoverageIterator::ComputeGeometryRect() const {
  // Calculate the current geometry rect. As we reserved overlap between tiles
  // to accommodate bilinear filtering and rounding errors in destination
  // space, the geometry rect might overlap on the edges.
  gfx::RectF texel_extent = tiling_->tiling_data_.TexelExtent(tile_i_, tile_j_);
  {
    // Adjust tile extent to accommodate numerical errors.
    //
    // Allow the tile to overreach by 1/1024 texels to avoid seams between
    // tiles. The constant 1/1024 is picked by the fact that with bilinear
    // filtering, the maximum error in color value introduced by clamping
    // error in both u/v axis can't exceed
    // 255 * (1 - (1 - 1/1024) * (1 - 1/1024)) ~= 0.498
    // i.e. The color value can never flip over a rounding threshold.
    constexpr float epsilon = 1.f / 1024.f;
    texel_extent.Inset(-epsilon);
  }

  // Convert texel_extent to coverage scale, which is what we have to report
  // geometry_rect in.
  gfx::Rect candidate =
      gfx::ToEnclosedRect(coverage_to_content_.InverseMapRect(texel_extent));
  {
    // Adjust external edges to cover the whole layer in dest space.
    //
    // For external edges, extend the tile to scaled layer bounds. This is
    // needed to fully cover the coverage space because the sample extent
    // doesn't cover the last 0.5 texel to layer edge, and also the coverage
    // space can be rounded up for up to 1 pixel. This overhang will never be
    // sampled as the AA fragment shader clamps sample coordinate and
    // antialiasing itself.
    const TilingData& data = tiling_->tiling_data_;
    candidate.Inset(gfx::Insets::TLBR(
        tile_j_ ? 0 : -candidate.y(), tile_i_ ? 0 : -candidate.x(),
        (tile_j_ != data.num_tiles_y() - 1)
            ? 0
            : candidate.bottom() - coverage_rect_max_bounds_.height(),
        (tile_i_ != data.num_tiles_x() - 1)
            ? 0
            : candidate.right() - coverage_rect_max_bounds_.width()));
  }

  candidate.Intersect(coverage_rect_);
  return candidate;
}

gfx::Rect PictureLayerTiling::CoverageIterator::geometry_rect() const {
  return current_geometry_rect_;
}

gfx::RectF PictureLayerTiling::CoverageIterator::texture_rect() const {
  auto tex_origin = gfx::PointF(
      tiling_->tiling_data_.TileBoundsWithBorder(tile_i_, tile_j_).origin());

  // Convert from coverage space => content space => texture space.
  gfx::RectF texture_rect(current_geometry_rect_);
  texture_rect = coverage_to_content_.MapRect(texture_rect);
  texture_rect.Offset(-tex_origin.OffsetFromOrigin());

  return texture_rect;
}

std::unique_ptr<Tile> PictureLayerTiling::TakeTileAt(int i, int j) {
  auto found = tiles_.find(TileMapKey(i, j));
  if (found == tiles_.end())
    return nullptr;
  std::unique_ptr<Tile> result = std::move(found->second);
  tiles_.erase(found);
  return result;
}

void PictureLayerTiling::Reset() {
  live_tiles_rect_ = gfx::Rect();
  tiles_.clear();
  all_tiles_done_ = true;
}

void PictureLayerTiling::ComputeTilePriorityRects(
    const gfx::Rect& visible_rect_in_layer_space,
    const gfx::Rect& skewport_in_layer_space,
    const gfx::Rect& soon_border_rect_in_layer_space,
    const gfx::Rect& eventually_rect_in_layer_space,
    float ideal_contents_scale,
    const Occlusion& occlusion_in_layer_space) {
  // If we have, or had occlusions, mark the tiles as 'not done' to ensure that
  // we reiterate the tiles for rasterization.
  if (occlusion_in_layer_space.HasOcclusion() ||
      current_occlusion_in_layer_space_.HasOcclusion()) {
    set_all_tiles_done(false);
  }

  const float content_to_screen_scale =
      ideal_contents_scale / contents_scale_key();

  const gfx::Rect* input_rects[] = {
      &visible_rect_in_layer_space, &skewport_in_layer_space,
      &soon_border_rect_in_layer_space, &eventually_rect_in_layer_space};
  gfx::Rect output_rects[4];
  for (size_t i = 0; i < std::size(input_rects); ++i)
    output_rects[i] = EnclosingContentsRectFromLayerRect(*input_rects[i]);
  // Make sure the eventually rect is aligned to tile bounds.
  output_rects[3] =
      tiling_data_.ExpandRectIgnoringBordersToTileBounds(output_rects[3]);

  SetTilePriorityRects(content_to_screen_scale, output_rects[0],
                       output_rects[1], output_rects[2], output_rects[3],
                       occlusion_in_layer_space);
  SetLiveTilesRect(output_rects[3]);
}

void PictureLayerTiling::SetTilePriorityRects(
    float content_to_screen_scale,
    const gfx::Rect& visible_rect_in_content_space,
    const gfx::Rect& skewport,
    const gfx::Rect& soon_border_rect,
    const gfx::Rect& eventually_rect,
    const Occlusion& occlusion_in_layer_space) {
  current_visible_rect_ = visible_rect_in_content_space;
  current_skewport_rect_ = skewport;
  current_soon_border_rect_ = soon_border_rect;
  current_eventually_rect_ = eventually_rect;
  current_occlusion_in_layer_space_ = occlusion_in_layer_space;
  current_content_to_screen_scale_ = content_to_screen_scale;

  gfx::Rect tiling_rect(tiling_size());
  has_visible_rect_tiles_ = tiling_rect.Intersects(current_visible_rect_);
  has_skewport_rect_tiles_ = tiling_rect.Intersects(current_skewport_rect_);
  has_soon_border_rect_tiles_ =
      tiling_rect.Intersects(current_soon_border_rect_);
  has_eventually_rect_tiles_ = tiling_rect.Intersects(current_eventually_rect_);

  // Note that we use the largest skewport extent from the viewport as the
  // "skewport extent". Also note that this math can't produce negative numbers,
  // since skewport.Contains(visible_rect) is always true.
  max_skewport_extent_in_screen_space_ =
      current_content_to_screen_scale_ *
      std::max(
          {current_visible_rect_.x() - current_skewport_rect_.x(),
           current_skewport_rect_.right() - current_visible_rect_.right(),
           current_visible_rect_.y() - current_skewport_rect_.y(),
           current_skewport_rect_.bottom() - current_visible_rect_.bottom()});
}

void PictureLayerTiling::SetLiveTilesRect(
    const gfx::Rect& new_live_tiles_rect) {
  DCHECK(new_live_tiles_rect.IsEmpty() ||
         gfx::Rect(tiling_size()).Contains(new_live_tiles_rect))
      << "tiling_size: " << tiling_size().ToString()
      << " new_live_tiles_rect: " << new_live_tiles_rect.ToString();
  if (live_tiles_rect_ == new_live_tiles_rect)
    return;

  // Iterate to delete all tiles outside of our new live_tiles rect.
  for (TilingData::DifferenceIterator iter(&tiling_data_, live_tiles_rect_,
                                           new_live_tiles_rect);
       iter; ++iter) {
    TakeTileAt(iter.index_x(), iter.index_y());
  }

  // We don't rasterize non ideal resolution tiles, so there is no need to
  // create any new tiles.
  if (resolution_ == NON_IDEAL_RESOLUTION) {
    live_tiles_rect_.Intersect(new_live_tiles_rect);
    VerifyLiveTilesRect();
    return;
  }

  // Iterate to allocate new tiles for all regions with newly exposed area.
  for (TilingData::DifferenceIterator iter(&tiling_data_, new_live_tiles_rect,
                                           live_tiles_rect_);
       iter; ++iter) {
    Tile::CreateInfo info = CreateInfoForTile(iter.index_x(), iter.index_y());
    if (ShouldCreateTileAt(info))
      CreateTile(info);
  }

  live_tiles_rect_ = new_live_tiles_rect;
  VerifyLiveTilesRect();
}

void PictureLayerTiling::VerifyLiveTilesRect() const {
#if DCHECK_IS_ON()
  for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
    DCHECK(it->second);
    TileMapKey key = it->first;
    DCHECK(key.index_x < tiling_data_.num_tiles_x())
        << this << " " << key.index_x << "," << key.index_y << " num_tiles_x "
        << tiling_data_.num_tiles_x() << " live_tiles_rect "
        << live_tiles_rect_.ToString();
    DCHECK(key.index_y < tiling_data_.num_tiles_y())
        << this << " " << key.index_x << "," << key.index_y << " num_tiles_y "
        << tiling_data_.num_tiles_y() << " live_tiles_rect "
        << live_tiles_rect_.ToString();
    DCHECK(tiling_data_.TileBounds(key.index_x, key.index_y)
               .Intersects(live_tiles_rect_))
        << this << " " << key.index_x << "," << key.index_y << " tile bounds "
        << tiling_data_.TileBounds(key.index_x, key.index_y).ToString()
        << " live_tiles_rect " << live_tiles_rect_.ToString();
  }
#endif
}

bool PictureLayerTiling::IsTileOccludedOnCurrentTree(const Tile* tile) const {
  if (!current_occlusion_in_layer_space_.HasOcclusion())
    return false;
  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());
  gfx::Rect tile_query_rect =
      gfx::IntersectRects(tile_bounds, current_visible_rect_);
  // Explicitly check if the tile is outside the viewport. If so, we need to
  // return false, since occlusion for this tile is unknown.
  if (tile_query_rect.IsEmpty())
    return false;

  tile_query_rect = EnclosingLayerRectFromContentsRect(tile_query_rect);
  return current_occlusion_in_layer_space_.IsOccluded(tile_query_rect);
}

bool PictureLayerTiling::ShouldDecodeCheckeredImagesForTile(
    const Tile* tile) const {
  // If this is the pending tree and the tile is not occluded, any checkered
  // images on this tile should be decoded.
  if (tree_ == PENDING_TREE)
    return !IsTileOccludedOnCurrentTree(tile);

  DCHECK_EQ(tree_, ACTIVE_TREE);
  const PictureLayerTiling* pending_twin =
      client_->GetPendingOrActiveTwinTiling(this);

  // If we don't have a pending twin, then 2 cases are possible. Either we don't
  // have a pending tree, in which case we should be decoding images for tiles
  // which are unoccluded.
  // If we do have a pending tree, then not having a twin implies that this
  // tiling will be evicted upon activation. TODO(khushalsagar): Plumb this
  // information here and return false for this case.
  if (!pending_twin)
    return !IsTileOccludedOnCurrentTree(tile);

  // If the tile will be replaced upon activation, then we don't need to process
  // it for checkered images. Since once the pending tree is activated, it is
  // the new active tree's content that we will invalidate and replace once the
  // decode finishes.
  if (!TilingMatchesTileIndices(pending_twin) ||
      pending_twin->TileAt(tile->tiling_i_index(), tile->tiling_j_index())) {
    return false;
  }

  // Ask the pending twin if this tile will become occluded upon activation.
  return !pending_twin->IsTileOccludedOnCurrentTree(tile);
}

void PictureLayerTiling::UpdateRequiredStatesOnTile(Tile* tile) const {
  tile->set_required_for_activation(IsTileRequiredForActivation(
      tile, [this](const Tile* tile) { return IsTileVisible(tile); },
      IsTileOccluded(tile)));
  tile->set_required_for_draw(IsTileRequiredForDraw(
      tile, [this](const Tile* tile) { return IsTileVisible(tile); }));
}

PrioritizedTile PictureLayerTiling::MakePrioritizedTile(
    Tile* tile,
    PriorityRectType priority_rect_type,
    bool is_tile_occluded) const {
  DCHECK(tile);
  DCHECK(
      raster_source()->IntersectsRect(tile->enclosing_layer_rect(), *client_))
      << "Recording rect: "
      << EnclosingLayerRectFromContentsRect(tile->content_rect()).ToString();

  tile->set_required_for_activation(IsTileRequiredForActivation(
      tile,
      [priority_rect_type](const Tile*) {
        return priority_rect_type == VISIBLE_RECT;
      },
      is_tile_occluded));
  tile->set_required_for_draw(
      IsTileRequiredForDraw(tile, [priority_rect_type](const Tile*) {
        return priority_rect_type == VISIBLE_RECT;
      }));

  const auto& tile_priority =
      ComputePriorityForTile(tile, priority_rect_type, is_tile_occluded);
  DCHECK((!tile->required_for_activation() && !tile->required_for_draw()) ||
         tile_priority.priority_bin == TilePriority::NOW ||
         !client_->HasValidTilePriorities());

  // Note that TileManager will consider this flag but may rasterize the tile
  // anyway (if tile is required for activation for example). We should process
  // the tile for images only if it's further than half of the skewport extent.
  bool process_for_images_only =
      tile_priority.distance_to_visible > min_preraster_distance_ &&
      (tile_priority.distance_to_visible > max_preraster_distance_ ||
       tile_priority.distance_to_visible >
           0.5f * max_skewport_extent_in_screen_space_);

  // If the tile is within max_skewport_extent and a scroll interaction
  // experiencing checkerboarding is currently taking place, then
  // continue to rasterize the tile right now rather than for images only.
  if (tile_priority.distance_to_visible <
          max_skewport_extent_in_screen_space_ &&
      client_->ScrollInteractionInProgress() &&
      client_->CurrentScrollCheckerboardsDueToNoRecording())
    process_for_images_only = false;
  return PrioritizedTile(tile, this, tile_priority, is_tile_occluded,
                         process_for_images_only,
                         ShouldDecodeCheckeredImagesForTile(tile));
}

std::map<const Tile*, PrioritizedTile>
PictureLayerTiling::UpdateAndGetAllPrioritizedTilesForTesting() const {
  std::map<const Tile*, PrioritizedTile> result;
  for (const auto& key_tile_pair : tiles_) {
    Tile* tile = key_tile_pair.second.get();
    PrioritizedTile prioritized_tile = MakePrioritizedTile(
        tile, ComputePriorityRectTypeForTile(tile), IsTileOccluded(tile));
    result.insert(std::make_pair(prioritized_tile.tile(), prioritized_tile));
  }
  return result;
}

TilePriority PictureLayerTiling::ComputePriorityForTile(
    const Tile* tile,
    PriorityRectType priority_rect_type,
    bool is_tile_occluded) const {
  // TODO(vmpstr): See if this can be moved to iterators.
  DCHECK_EQ(ComputePriorityRectTypeForTile(tile), priority_rect_type);
  DCHECK_EQ(TileAt(tile->tiling_i_index(), tile->tiling_j_index()), tile);

  TilePriority::PriorityBin priority_bin;
  if (client_->HasValidTilePriorities()) {
    // Occluded tiles are given a lower PriorityBin to ensure they are evicted
    // before non-occluded tiles.
    priority_bin = is_tile_occluded ? TilePriority::SOON : TilePriority::NOW;
  } else {
    priority_bin = TilePriority::EVENTUALLY;
  }

  switch (priority_rect_type) {
    case VISIBLE_RECT:
    case PENDING_VISIBLE_RECT:
      return TilePriority(resolution_, priority_bin, 0);
    case SKEWPORT_RECT:
    case SOON_BORDER_RECT:
      if (priority_bin < TilePriority::SOON)
        priority_bin = TilePriority::SOON;
      break;
    case EVENTUALLY_RECT:
      priority_bin = TilePriority::EVENTUALLY;
      break;
  }

  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());
  DCHECK_GT(current_content_to_screen_scale_, 0.f);
  float distance_to_visible =
      current_content_to_screen_scale_ *
      current_visible_rect_.ManhattanInternalDistance(tile_bounds);

  return TilePriority(resolution_, priority_bin, distance_to_visible);
}

PictureLayerTiling::PriorityRectType
PictureLayerTiling::ComputePriorityRectTypeForTile(const Tile* tile) const {
  DCHECK_EQ(TileAt(tile->tiling_i_index(), tile->tiling_j_index()), tile);
  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());

  if (current_visible_rect_.Intersects(tile_bounds))
    return VISIBLE_RECT;

  if (pending_visible_rect().Intersects(tile_bounds))
    return PENDING_VISIBLE_RECT;

  if (current_skewport_rect_.Intersects(tile_bounds))
    return SKEWPORT_RECT;

  if (current_soon_border_rect_.Intersects(tile_bounds))
    return SOON_BORDER_RECT;

  DCHECK(current_eventually_rect_.Intersects(tile_bounds));
  return EVENTUALLY_RECT;
}

void PictureLayerTiling::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (const auto& tile_pair : tiles_) {
    Tile* tile = tile_pair.second.get();
    prioritized_tiles->push_back(MakePrioritizedTile(
        tile, ComputePriorityRectTypeForTile(tile), IsTileOccluded(tile)));
  }
}

void PictureLayerTiling::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("num_tiles", base::saturated_cast<int>(tiles_.size()));
  state->SetDouble("content_scale", contents_scale_key());

  state->BeginDictionary("raster_transform");
  state->BeginArray("scale");
  state->AppendDouble(raster_transform_.scale().x());
  state->AppendDouble(raster_transform_.scale().y());
  state->EndArray();
  state->BeginArray("translation");
  state->AppendDouble(raster_transform_.translation().x());
  state->AppendDouble(raster_transform_.translation().y());
  state->EndArray();
  state->EndDictionary();

  MathUtil::AddToTracedValue("visible_rect", current_visible_rect_, state);
  MathUtil::AddToTracedValue("skewport_rect", current_skewport_rect_, state);
  MathUtil::AddToTracedValue("soon_rect", current_soon_border_rect_, state);
  MathUtil::AddToTracedValue("eventually_rect", current_eventually_rect_,
                             state);
  MathUtil::AddToTracedValue("tiling_size", tiling_size(), state);
}

size_t PictureLayerTiling::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
    const Tile* tile = it->second.get();
    amount += tile->GPUMemoryUsageInBytes();
  }
  return amount;
}

gfx::Rect PictureLayerTiling::EnclosingContentsRectFromLayerRect(
    const gfx::Rect& layer_rect) const {
  return ToEnclosingRect(raster_transform_.MapRect(gfx::RectF(layer_rect)));
}

gfx::Rect PictureLayerTiling::EnclosingLayerRectFromContentsRect(
    const gfx::Rect& contents_rect) const {
  return ToEnclosingRect(
      raster_transform_.InverseMapRect(gfx::RectF(contents_rect)));
}

PictureLayerTiling::TileIterator::TileIterator(PictureLayerTiling* tiling)
    : tiling_(tiling), iter_(tiling->tiles_.begin()) {}

PictureLayerTiling::TileIterator::~TileIterator() = default;

Tile* PictureLayerTiling::TileIterator::GetCurrent() {
  return AtEnd() ? nullptr : iter_->second.get();
}

void PictureLayerTiling::TileIterator::Next() {
  if (!AtEnd())
    ++iter_;
}

bool PictureLayerTiling::TileIterator::AtEnd() const {
  return iter_ == tiling_->tiles_.end();
}

}  // namespace cc
