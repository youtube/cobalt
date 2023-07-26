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

#include "cobalt/loader/mesh/mesh_decoder.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bits.h"
#include "base/memory/ptr_util.h"
#include "cobalt/loader/mesh/projection_codec/projection_decoder.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace mesh {

namespace {

// Builds meshes from calls by the projection decoder.
class MeshDecoderSink : public projection_codec::ProjectionDecoder::Sink {
 public:
  typedef MeshProjection::MeshCollection MeshCollection;
  typedef MeshProjection::MeshCollectionList MeshCollectionList;

  // Decodes the contents of a projection box. (i.e. everything after the
  // full box header.)
  static bool DecodeBoxContents(
      render_tree::ResourceProvider* resource_provider, uint8_t version,
      uint32_t flags, const uint8* data, size_t data_size,
      MeshCollectionList* dest_mesh_collection_list, uint32* crc);

  // Decodes the contents of a projection box. (i.e. everything after the
  // full box header.)
  static scoped_refptr<MeshProjection> DecodeMeshProjectionFromBoxContents(
      render_tree::ResourceProvider* resource_provider, uint8_t version,
      uint32_t flags, const uint8* data, size_t data_size);

  explicit MeshDecoderSink(render_tree::ResourceProvider* resource_provider,
                           MeshCollectionList* dest_mesh_collection_list,
                           uint32* crc);

  ~MeshDecoderSink() override;

  bool IsCached(uint32 crc) override;

  void BeginMeshCollection() override;
  void AddCoordinate(float value) override;
  void AddVertex(const projection_codec::IndexedVert& v) override;

  void BeginMeshInstance() override;
  void SetTextureId(uint8 texture_id) override;
  void SetMeshGeometryType(projection_codec::MeshGeometryType type) override;
  void AddVertexIndex(size_t v_index) override;
  void EndMeshInstance() override;

  void EndMeshCollection() override;

 private:
  render_tree::ResourceProvider* resource_provider_;

  // Components of a mesh projection (possibly several mesh collections).
  MeshCollectionList* mesh_collection_list_;
  uint32* crc_;

  // Components of a mesh collection.
  std::unique_ptr<MeshCollection> mesh_collection_;
  std::vector<float> collection_coordinates_;
  std::vector<projection_codec::IndexedVert> collection_vertices_;

  // Components of a mesh.
  std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > mesh_vertices_;
  render_tree::Mesh::DrawMode mesh_draw_mode_;
  uint8 mesh_texture_id_;

  DISALLOW_COPY_AND_ASSIGN(MeshDecoderSink);
};

MeshDecoderSink::MeshDecoderSink(
    render_tree::ResourceProvider* resource_provider,
    MeshCollectionList* dest_mesh_collection_list, uint32* crc)
    : resource_provider_(resource_provider),
      mesh_collection_list_(dest_mesh_collection_list),
      crc_(crc) {
  DCHECK(resource_provider);
  DCHECK(dest_mesh_collection_list);
  DCHECK(crc);
}

MeshDecoderSink::~MeshDecoderSink() {}

bool MeshDecoderSink::IsCached(uint32 crc) {
  *crc_ = crc;
  return false;
}

void MeshDecoderSink::BeginMeshCollection() {
  CHECK(!mesh_collection_);
  CHECK(collection_coordinates_.empty());
  CHECK(collection_vertices_.empty());

  mesh_collection_.reset(new MeshCollection);
}

void MeshDecoderSink::AddCoordinate(float value) {
  collection_coordinates_.push_back(value);
}

void MeshDecoderSink::AddVertex(const projection_codec::IndexedVert& vertex) {
  collection_vertices_.push_back(vertex);
}

void MeshDecoderSink::BeginMeshInstance() {
  CHECK(!mesh_vertices_);
  mesh_vertices_.reset(new std::vector<render_tree::Mesh::Vertex>);
}

void MeshDecoderSink::SetTextureId(uint8 texture_id) {
  DCHECK_EQ(texture_id, 0);
  mesh_texture_id_ = texture_id;
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

void MeshDecoderSink::AddVertexIndex(size_t index) {
  const projection_codec::IndexedVert& vertex = collection_vertices_[index];
  mesh_vertices_->push_back(render_tree::Mesh::Vertex(
      collection_coordinates_[vertex.x], collection_coordinates_[vertex.y],
      collection_coordinates_[vertex.z], collection_coordinates_[vertex.u],
      collection_coordinates_[vertex.v]));
}

void MeshDecoderSink::EndMeshInstance() {
  mesh_collection_->push_back(resource_provider_->CreateMesh(
      std::move(mesh_vertices_), mesh_draw_mode_));
}

void MeshDecoderSink::EndMeshCollection() {
  mesh_collection_list_->emplace_back(mesh_collection_.release());
  collection_vertices_.clear();
  collection_coordinates_.clear();
}

bool MeshDecoderSink::DecodeBoxContents(
    render_tree::ResourceProvider* resource_provider, uint8_t version,
    uint32_t flags, const uint8* data, size_t data_size,
    MeshCollectionList* dest_mesh_collection_list, uint32* crc) {
  MeshDecoderSink sink(resource_provider, dest_mesh_collection_list, crc);
  return projection_codec::ProjectionDecoder::DecodeBoxContentsToSink(
      version, flags, data, data_size, &sink);
}

scoped_refptr<MeshProjection>
MeshDecoderSink::DecodeMeshProjectionFromBoxContents(
    render_tree::ResourceProvider* resource_provider, uint8_t version,
    uint32_t flags, const uint8* data, size_t data_size) {
  MeshDecoderSink::MeshCollectionList mesh_collection_list;
  uint32 crc = kuint32max;

  if (!DecodeBoxContents(resource_provider, version, flags, data, data_size,
                         &mesh_collection_list, &crc)) {
    return NULL;
  }

  return scoped_refptr<MeshProjection>(
      new MeshProjection(std::move(mesh_collection_list), crc));
}

}  // namespace

MeshDecoder::MeshDecoder(
    render_tree::ResourceProvider* resource_provider,
    const MeshAvailableCallback& mesh_available_callback,
    const loader::Decoder::OnCompleteFunction& load_complete_callback)
    : resource_provider_(resource_provider),
      mesh_available_callback_(mesh_available_callback),
      load_complete_callback_(load_complete_callback),
      is_suspended_(!resource_provider_) {
  DCHECK(!mesh_available_callback_.is_null());
  DCHECK(!load_complete_callback.is_null());
}

void MeshDecoder::DecodeChunk(const char* data, size_t size) {
  if (is_suspended_) {
    DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
    return;
  }

  if (!raw_data_) {
    raw_data_ = base::WrapUnique(new std::vector<uint8>());
  }

  size_t start_size = raw_data_->size();

  raw_data_->resize(start_size + size);
  if (raw_data_->empty()) {
    return;
  }
  memcpy(&((*raw_data_)[start_size]), data, size);
}

void MeshDecoder::Finish() {
  if (is_suspended_) {
    DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
    return;
  }

  if (!resource_provider_) {
    load_complete_callback_.Run(
        std::string("No resource provider was passed to the MeshDecoder."));
    return;
  }

  if (raw_data_->size() <= 4) {
    // Mesh projection boxes cannot be empty, since they carry at least the
    // version and flags.
    load_complete_callback_.Run(
        std::string("MeshDecoder passed an empty buffer, cannot decode."));
    return;
  }

  // Skip version and flags (first 4 bytes of the box).
  scoped_refptr<MeshProjection> mesh_projection =
      MeshDecoderSink::DecodeMeshProjectionFromBoxContents(
          resource_provider_, 0, 0, &raw_data_->at(4), raw_data_->size() - 4);
  if (mesh_projection) {
    mesh_available_callback_.Run(mesh_projection);
    load_complete_callback_.Run(base::nullopt);
  } else {
    // Error must have occurred in MeshDecoderSink::Decode.
    load_complete_callback_.Run(
        std::string("MeshDecoder passed an invalid mesh projection box."));
  }
}

bool MeshDecoder::Suspend() {
  DCHECK(!is_suspended_);
  DCHECK(resource_provider_);

  is_suspended_ = true;
  resource_provider_ = NULL;

  ReleaseRawData();
  return true;
}

void MeshDecoder::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK(is_suspended_);
  DCHECK(!resource_provider_);
  DCHECK(resource_provider);

  is_suspended_ = false;
  resource_provider_ = resource_provider;
}

void MeshDecoder::ReleaseRawData() { raw_data_.reset(); }

}  // namespace mesh
}  // namespace loader
}  // namespace cobalt
