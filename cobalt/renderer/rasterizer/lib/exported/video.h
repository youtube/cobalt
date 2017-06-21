// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_VIDEO_H_
#define COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_VIDEO_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Functions to be called to set callbacks for the updates to video rendering
// parameters. When the host application is notified that the graphics context
// has been created, it should install the callbacks within the same call stack
// of CbLibOnGraphicsContextCreated. Failing to do so will crash the application
// on debug mode and no-op on production builds.

typedef enum {
  kCbLibVideoProjectionTypeNone = 0,  // When no offscreen video is playing.
  kCbLibVideoProjectionTypeRectangular = 1,
  kCbLibVideoProjectionTypeMesh = 2,
} CbLibVideoProjectionType;

// If projection_type is Rectangular or Mesh, this signals the start of off-
// screen video playback. If it is None, this signals the end of playback.
typedef void (*CbLibVideoUpdateProjectionTypeCallback)(
    void* context, CbLibVideoProjectionType projection_type);

SB_EXPORT_PLATFORM void CbLibVideoSetOnUpdateProjectionType(
    void* context, CbLibVideoUpdateProjectionTypeCallback callback);

typedef enum {
  kCbLibVideoMeshDrawModeTriangles = 0,
  kCbLibVideoMeshDrawModeTriangleStrip = 1,
  kCbLibVideoMeshDrawModeTriangleFan = 2,
} CbLibVideoMeshDrawMode;

typedef struct {
  int vertex_count;
  CbLibVideoMeshDrawMode draw_mode;
  // Memory owned and deleted by Cobalt. The array holds |count| tuples of
  // interleaved position and texture coordinates in single precision floating
  // point numbers, X, Y, Z, U, V.
  const float* vertices;
} CbLibVideoMesh;

// Called when two new video meshes are detected; each should be applied to the
// view of the corresponding eye on stereo rendering.
typedef void (*CbLibVideoUpdateMeshesCallback)(void* context,
                                               CbLibVideoMesh left_eye_mesh,
                                               CbLibVideoMesh right_eye_mesh);

SB_EXPORT_PLATFORM void CbLibVideoSetOnUpdateMeshes(
    void* context, CbLibVideoUpdateMeshesCallback callback);

typedef enum {
  kCbLibVideoStereoModeMono = 0,
  kCbLibVideoStereoModeStereoLeftRight = 1,
  kCbLibVideoStereoModeStereoTopBottom = 2,
  kCbLibVideoStereoModeStereoLeftRightUnadjustedCoordinates = 3,
} CbLibVideoStereoMode;

// Called when the layout of the different stereo views changes on the video
// texture.
typedef void (*CbLibVideoUpdateStereoModeCallback)(
    void* context, CbLibVideoStereoMode stereo_mode);

SB_EXPORT_PLATFORM void CbLibVideoSetOnUpdateStereoMode(
    void* context, CbLibVideoUpdateStereoModeCallback callback);

// Called to set the RGB video texture ID.
typedef void (*CbLibVideoUpdateRgbTextureIdCallback)(void* context, int id);

SB_EXPORT_PLATFORM void CbLibVideoSetOnUpdateRgbTextureId(
    void* context, CbLibVideoUpdateRgbTextureIdCallback callback);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_VIDEO_H_
