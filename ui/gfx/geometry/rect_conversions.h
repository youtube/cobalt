// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_RECT_CONVERSIONS_H_
#define UI_GFX_GEOMETRY_RECT_CONVERSIONS_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {

// Returns the smallest Rect that encloses the given RectF if possible.
// The returned Rect is larger than or equal to the input RectF, unless the
// the geometry values exceed int range and are clamped to int.
GEOMETRY_EXPORT Rect ToEnclosingRect(const RectF& rect);

// Similar to ToEnclosingRect(), but for each edge, if the distance between the
// edge and the nearest integer grid is smaller than |error|, the edge is
// snapped to the integer grid. Unlike ToNearestRect() which only accepts
// integer rect with or without floating point error, this function also accepts
// non-integer rect.
GEOMETRY_EXPORT Rect ToEnclosingRectIgnoringError(const RectF& rect,
                                                  float error);

// Returns the largest Rect that is enclosed by the given RectF if possible.
// The returned rect is smaller than or equal to the input rect, but if
// the input RectF is too small and no enclosed Rect exists, the returned
// rect is an empty Rect at |ToCeiledPoint(rect.origin())|.
GEOMETRY_EXPORT Rect ToEnclosedRect(const RectF& rect);

// Similar to ToEnclosedRect(), but for each edge, if the distance between the
// edge and the nearest integer grid is smaller than |error|, the edge is
// snapped to the integer grid. Unlike ToNearestRect() which only accepts
// integer rect with or without floating point error, this function also accepts
// non-integer rect.
GEOMETRY_EXPORT Rect ToEnclosedRectIgnoringError(const RectF& rect,
                                                 float error);

// Returns the Rect after snapping the corners of the RectF to an integer grid.
// This should only be used when the RectF you provide is expected to be an
// integer rect with floating point error. If it is an arbitrary RectF, then
// you should use a different method.
GEOMETRY_EXPORT Rect ToNearestRect(const RectF& rect);

// Returns true if the Rect produced after snapping the corners of the RectF
// to an integer grid is withing |distance|.
GEOMETRY_EXPORT bool IsNearestRectWithinDistance(const gfx::RectF& rect,
                                                 float distance);

// Returns the Rect after rounding the corners of the RectF to an integer grid.
GEOMETRY_EXPORT gfx::Rect ToRoundedRect(const gfx::RectF& rect);

// Returns a Rect obtained by flooring the values of the given RectF.
// Please prefer the previous two functions in new code.
GEOMETRY_EXPORT Rect ToFlooredRectDeprecated(const RectF& rect);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RECT_CONVERSIONS_H_
