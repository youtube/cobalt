// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <algorithm>

#include "base/bits.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/loader/mesh/mesh_decoder.h"
#include "cobalt/loader/mesh/projection_codec/projection_decoder.h"
#include "third_party/glm/glm/vec2.hpp"
#include "third_party/glm/glm/vec3.hpp"

namespace cobalt {
namespace loader {
namespace mesh {

namespace {

// Builds meshes from calls by the projection decoder.
class MeshDecoderSink : public projection_codec::ProjectionDecoder::Sink {
 public:
  typedef std::vector<scoped_refptr<render_tree::Mesh> > MeshCollection;
  typedef ScopedVector<MeshCollection> MeshCollectionList;

  // Decode a complete box that includes the header.
  static bool Decode(const uint8* data, size_t data_size,
                     MeshCollectionList* dest_mesh_collection_list);

  // Decodes the contents of a projection box. (i.e. everything after the
  // full box header.)
  static bool DecodeBoxContents(uint8_t version, uint32_t flags,
                                const uint8* data, size_t data_size,
                                MeshCollectionList* dest_mesh_collection_list);

  explicit MeshDecoderSink(MeshCollectionList* dest_mesh_collection_list);

  ~MeshDecoderSink() OVERRIDE;

  bool IsCached(uint32 crc) OVERRIDE;

  void BeginMeshCollection() OVERRIDE;
  void AddCoordinate(float value) OVERRIDE;
  void AddVertex(const projection_codec::IndexedVert& v) OVERRIDE;

  void BeginMeshInstance() OVERRIDE;
  void SetTextureId(uint8 texture_id) OVERRIDE;
  void SetMeshGeometryType(projection_codec::MeshGeometryType type) OVERRIDE;
  void AddVertexIndex(size_t v_index) OVERRIDE;
  void EndMeshInstance() OVERRIDE;

  void EndMeshCollection() OVERRIDE;

  static scoped_refptr<render_tree::Mesh> ExtractSingleMesh(
      const MeshCollectionList& list);

 private:
  MeshCollectionList* mesh_collection_list_;  // not owned

  std::vector<float> coordinates_;
  std::vector<render_tree::Mesh::Vertex> vertices_;
  scoped_ptr<MeshCollection> mesh_collection_;
  std::vector<render_tree::Mesh::Vertex> mesh_vertices_;
  render_tree::Mesh::DrawMode mesh_draw_mode_;
  uint32 crc_;

  DISALLOW_COPY_AND_ASSIGN(MeshDecoderSink);
};

MeshDecoderSink::MeshDecoderSink(MeshCollectionList* dest_mesh_collection_list)
    : mesh_collection_list_(dest_mesh_collection_list) {}

MeshDecoderSink::~MeshDecoderSink() {}

bool MeshDecoderSink::IsCached(uint32 crc) {
  crc_ = crc;
  return false;
}

void MeshDecoderSink::BeginMeshCollection() {
  CHECK(!mesh_collection_);

  coordinates_.clear();
  vertices_.clear();

  mesh_collection_.reset(new MeshCollection);
}

void MeshDecoderSink::AddCoordinate(float value) {
  coordinates_.push_back(value);
}

void MeshDecoderSink::AddVertex(const projection_codec::IndexedVert& v) {
  render_tree::Mesh::Vertex vertex;
  vertex.position_coord =
      glm::vec3(coordinates_[v.x], coordinates_[v.y], coordinates_[v.z]);
  vertex.texture_coord = glm::vec2(coordinates_[v.u], coordinates_[v.v]);
  vertices_.push_back(vertex);
}

void MeshDecoderSink::BeginMeshInstance() { CHECK(mesh_vertices_.empty()); }

void MeshDecoderSink::SetTextureId(uint8 texture_id) {
  // No texture id other than the default video texture supported.
  DCHECK(!texture_id);
}

void MeshDecoderSink::SetMeshGeometryType(
    projection_codec::MeshGeometryType geometry_type) {
  switch (geometry_type) {
    case projection_codec::kTriangles:
      mesh_draw_mode_ = render_tree::Mesh::kDrawModeTriangles;
      break;
    case projection_codec::kTriangleStrip:
      mesh_draw_mode_ = render_tree::Mesh::kDrawModeTriangleStrip;
      break;
    case projection_codec::kTriangleFan:
      mesh_draw_mode_ = render_tree::Mesh::kDrawModeTriangleFan;
      break;
    default:
      NOTREACHED() << "Unrecognized mesh geometry emitted by "
                      "ProjectionDecoder.";
  }
}

void MeshDecoderSink::AddVertexIndex(size_t v_index) {
  mesh_vertices_.push_back(vertices_[v_index]);
}

void MeshDecoderSink::EndMeshInstance() {
  mesh_collection_->push_back(scoped_refptr<render_tree::Mesh>(
      new render_tree::Mesh(mesh_vertices_, mesh_draw_mode_, crc_)));
}

void MeshDecoderSink::EndMeshCollection() {
  mesh_collection_list_->push_back(mesh_collection_.release());
  coordinates_.clear();
  vertices_.clear();
}

bool MeshDecoderSink::Decode(const uint8* data, size_t data_size,
                             MeshCollectionList* dest_mesh_collection_list) {
  MeshDecoderSink sink(dest_mesh_collection_list);
  return projection_codec::ProjectionDecoder::DecodeToSink(data, data_size,
                                                           &sink);
}

bool MeshDecoderSink::DecodeBoxContents(
    uint8_t version, uint32_t flags, const uint8* data, size_t data_size,
    MeshCollectionList* dest_mesh_collection_list) {
  MeshDecoderSink sink(dest_mesh_collection_list);
  return projection_codec::ProjectionDecoder::DecodeBoxContentsToSink(
      version, flags, data, data_size, &sink);
}

scoped_refptr<render_tree::Mesh> MeshDecoderSink::ExtractSingleMesh(
    const MeshCollectionList& list) {
  if (list.size() == 1) {
    const MeshCollection& collection = *list[0];

    if (collection.size() == 1) {
      return collection[0];
    }
  }

  return NULL;
}

}  // namespace

MeshDecoder::MeshDecoder(render_tree::ResourceProvider* resource_provider,
                         const SuccessCallback& success_callback,
                         const ErrorCallback& error_callback)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      error_callback_(error_callback),
      suspended_(false) {
  DCHECK(resource_provider_);
  DCHECK(!success_callback_.is_null());
  DCHECK(!error_callback_.is_null());
}

void MeshDecoder::DecodeChunk(const char* data, size_t size) {
  if (!raw_data_) {
    raw_data_ = make_scoped_ptr(new std::vector<uint8>());
  }

  size_t start_size = raw_data_->size();

  raw_data_->resize(start_size + size);
  memcpy(&((*raw_data_)[start_size]), data, size);
}

void MeshDecoder::ReleaseRawData() { raw_data_.reset(); }

void MeshDecoder::Finish() {
  if (suspended_) {
    DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
    return;
  }

  if (!resource_provider_) {
    error_callback_.Run("No resource provider was passed to the MeshDecoder.");
    return;
  }

  if (raw_data_->size() <= 4) {
    // Mesh projection boxes cannot be empty, since they carry at least the
    // version and flags.
    error_callback_.Run("MeshDecoder passed an empty buffer, cannot decode.");
    return;
  }

  std::string error_string;
  MeshDecoderSink::MeshCollectionList mesh_collection_list;

  if (MeshDecoderSink::DecodeBoxContents(
          0, 0, &raw_data_->at(4),  // Skip version and flags.
          raw_data_->size(), &mesh_collection_list)) {
    scoped_refptr<render_tree::Mesh> mesh =
        MeshDecoderSink::ExtractSingleMesh(mesh_collection_list);

    if (mesh) {
      success_callback_.Run(mesh);
      return;
    } else {
      error_callback_.Run(
          "MeshDecoder found unexpected number of meshes in "
          "projection box.");
    }
  }

  // Error must hace occured in MeshDecoderSink::Decode.
  error_callback_.Run("MeshDecoder passed an invalid mesh projection box.");
}

bool MeshDecoder::Suspend() {
  DCHECK(!suspended_);
  suspended_ = true;
  resource_provider_ = NULL;
  ReleaseRawData();
  return true;
}

void MeshDecoder::Resume(render_tree::ResourceProvider* resource_provider) {
  if (!suspended_) {
    DCHECK_EQ(resource_provider_, resource_provider);
    return;
  }

  suspended_ = false;
  resource_provider_ = resource_provider;
}

}  // namespace mesh
}  // namespace loader
}  // namespace cobalt
