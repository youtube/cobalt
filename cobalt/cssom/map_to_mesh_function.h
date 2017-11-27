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

#ifndef COBALT_CSSOM_MAP_TO_MESH_FUNCTION_H_
#define COBALT_CSSOM_MAP_TO_MESH_FUNCTION_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/filter_function.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/url_value.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace cobalt {
namespace cssom {

// Represent a map-to-mesh filter.
class MapToMeshFunction : public FilterFunction {
 public:
  // A resolution-matched mesh specifier.
  class ResolutionMatchedMesh {
   public:
    ResolutionMatchedMesh(int width_match, int height_match,
                          const scoped_refptr<PropertyValue>& mesh_url);
    int width_match() const { return width_match_; }
    int height_match() const { return height_match_; }
    const scoped_refptr<PropertyValue>& mesh_url() const { return mesh_url_; }
    bool operator==(const MapToMeshFunction::ResolutionMatchedMesh& rhs) const;
    bool operator!=(const MapToMeshFunction::ResolutionMatchedMesh& rhs) const;

   private:
    const int width_match_;
    const int height_match_;
    const scoped_refptr<PropertyValue> mesh_url_;
  };

  // Type of the source of the mesh: either a built-in mesh type or a custom
  // mesh.
  enum MeshSpecType {
    // Built-in equirectangular mesh.
    kEquirectangular,
    // List of custom binary mesh URLs.
    kUrls
  };
  typedef ScopedVector<ResolutionMatchedMesh> ResolutionMatchedMeshListBuilder;

  // Contains the specification of the mesh source.
  class MeshSpec {
   public:
    explicit MeshSpec(MeshSpecType mesh_type) : mesh_type_(mesh_type) {
      // Check that this is a built-in mesh type.
      DCHECK_EQ(mesh_type, kEquirectangular);
    }

    MeshSpec(MeshSpecType mesh_type,
             const scoped_refptr<PropertyValue>& mesh_url,
             ResolutionMatchedMeshListBuilder resolution_matched_meshes)
        : mesh_type_(mesh_type),
          mesh_url_(mesh_url),
          resolution_matched_meshes_(resolution_matched_meshes.Pass()) {
      DCHECK_EQ(mesh_type_, kUrls);
      DCHECK(mesh_url);
    }

    const scoped_refptr<PropertyValue>& mesh_url() const {
      DCHECK_EQ(mesh_type_, kUrls);
      return mesh_url_;
    }
    const ResolutionMatchedMeshListBuilder& resolution_matched_meshes() const {
      DCHECK_EQ(mesh_type_, kUrls);
      return resolution_matched_meshes_;
    }
    MeshSpecType mesh_type() const { return mesh_type_; }

   private:
    MeshSpecType mesh_type_;
    scoped_refptr<PropertyValue> mesh_url_;
    ResolutionMatchedMeshListBuilder resolution_matched_meshes_;

    DISALLOW_COPY_AND_ASSIGN(MeshSpec);
  };

  MapToMeshFunction(scoped_ptr<MeshSpec> mesh_spec,
                    float horizontal_fov_in_radians,
                    float vertical_fov_in_radians, const glm::mat4& transform,
                    const scoped_refptr<KeywordValue>& stereo_mode)
      : mesh_spec_(mesh_spec.Pass()),
        horizontal_fov_in_radians_(horizontal_fov_in_radians),
        vertical_fov_in_radians_(vertical_fov_in_radians),
        transform_(transform),
        stereo_mode_(stereo_mode) {
    DCHECK(mesh_spec_);
    DCHECK(stereo_mode_);
  }

  // Alternate constructor for mesh URL lists.
  MapToMeshFunction(const scoped_refptr<PropertyValue>& mesh_url,
                    ResolutionMatchedMeshListBuilder resolution_matched_meshes,
                    float horizontal_fov_in_radians,
                    float vertical_fov_in_radians, const glm::mat4& transform,
                    const scoped_refptr<KeywordValue>& stereo_mode)
      : mesh_spec_(
            new MeshSpec(kUrls, mesh_url, resolution_matched_meshes.Pass())),
        horizontal_fov_in_radians_(horizontal_fov_in_radians),
        vertical_fov_in_radians_(vertical_fov_in_radians),
        transform_(transform),
        stereo_mode_(stereo_mode) {
    DCHECK(stereo_mode_);
  }

  // Alternate constructor for built-in meshes.
  MapToMeshFunction(MeshSpecType spec_type, float horizontal_fov_in_radians,
                    float vertical_fov_in_radians, const glm::mat4& transform,
                    const scoped_refptr<KeywordValue>& stereo_mode)
      : mesh_spec_(new MeshSpec(spec_type)),
        horizontal_fov_in_radians_(horizontal_fov_in_radians),
        vertical_fov_in_radians_(vertical_fov_in_radians),
        transform_(transform),
        stereo_mode_(stereo_mode) {
    DCHECK(stereo_mode_);
  }

  ~MapToMeshFunction() override {}

  const MeshSpec& mesh_spec() const { return *mesh_spec_; }
  float horizontal_fov_in_radians() const { return horizontal_fov_in_radians_; }
  float vertical_fov_in_radians() const { return vertical_fov_in_radians_; }
  const glm::mat4& transform() const { return transform_; }
  const scoped_refptr<KeywordValue>& stereo_mode() const {
    return stereo_mode_;
  }

  std::string ToString() const override;

  bool operator==(const MapToMeshFunction& rhs) const;

  static const MapToMeshFunction* ExtractFromFilterList(
      PropertyValue* filter_list);

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(MapToMeshFunction);

 private:
  const scoped_ptr<MeshSpec> mesh_spec_;
  const float horizontal_fov_in_radians_;
  const float vertical_fov_in_radians_;
  const glm::mat4 transform_;
  const scoped_refptr<KeywordValue> stereo_mode_;

  DISALLOW_COPY_AND_ASSIGN(MapToMeshFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_MAP_TO_MESH_FUNCTION_H_
