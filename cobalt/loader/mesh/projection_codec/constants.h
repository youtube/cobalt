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

#ifndef COBALT_LOADER_MESH_PROJECTION_CODEC_CONSTANTS_H_
#define COBALT_LOADER_MESH_PROJECTION_CODEC_CONSTANTS_H_

namespace cobalt {
namespace loader {
namespace mesh {
namespace projection_codec {

// These are the string constants that appear as fourcc data in the encoded
// byte stream -- they should always match the public spec.
//
// These really intended for internal use, but they are shared between the
// encoder and decoder, so it seems like the codec header is the right place for
// them.
namespace codec_strings {
static const char kMeshProjBoxType4cc[] = "ytmp";
static const char kMeshProjAlternateBoxType4cc[] = "mshp";
static const char kMeshBoxType4cc[] = "mesh";
static const char kCompressionRaw4cc[] = "raw ";
static const char kCompressionDeflate4cc[] = "dfl8";
}  // namespace codec_strings

// The type geometry type for the mesh. These should match the public spec.
enum MeshGeometryType {
  kTriangles = 0,
  kTriangleStrip = 1,
  kTriangleFan = 2,
};

}  // namespace projection_codec
}  // namespace mesh
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MESH_PROJECTION_CODEC_CONSTANTS_H_
