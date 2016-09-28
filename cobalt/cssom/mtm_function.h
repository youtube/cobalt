/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_CSSOM_MTM_FUNCTION_H_
#define COBALT_CSSOM_MTM_FUNCTION_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/filter_function.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/url_value.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace cobalt {
namespace cssom {

// Represent an MTM filter.
class MTMFunction : public FilterFunction {
 public:
  // A resolution-matched mesh specifier.
  class ResolutionMatchedMesh {
   public:
    ResolutionMatchedMesh(int width_match, int height_match,
                          const scoped_refptr<PropertyValue>& mesh_url);
    int width_match() const { return width_match_; }
    int height_match() const { return height_match_; }
    const scoped_refptr<PropertyValue>& mesh_url() const { return mesh_url_; }
    bool operator==(const MTMFunction::ResolutionMatchedMesh& rhs) const;
    bool operator!=(const MTMFunction::ResolutionMatchedMesh& rhs) const;

   private:
    const int width_match_;
    const int height_match_;
    const scoped_refptr<PropertyValue> mesh_url_;
  };
  typedef ScopedVector<ResolutionMatchedMesh> ResolutionMatchedMeshListBuilder;

  MTMFunction(const scoped_refptr<PropertyValue>& mesh_url,
              ResolutionMatchedMeshListBuilder resolution_matched_meshes,
              float horizontal_fov, float vertical_fov,
              const glm::mat4& transform);

  ~MTMFunction() OVERRIDE {}

  const scoped_refptr<PropertyValue>& mesh_url() const { return mesh_url_; }
  const ResolutionMatchedMeshListBuilder& resolution_matched_meshes() const {
    return resolution_matched_meshes_;
  }

  float horizontal_fov() const { return horizontal_fov_; }
  float vertical_fov() const { return vertical_fov_; }
  const glm::mat4& transform() const { return transform_; }

  std::string ToString() const OVERRIDE;

  bool operator==(const MTMFunction& rhs) const;

  static const MTMFunction* ExtractFromFilterList(PropertyValue* filter_list);

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(MTMFunction);

 private:
  const scoped_refptr<PropertyValue> mesh_url_;
  const ResolutionMatchedMeshListBuilder resolution_matched_meshes_;
  const float horizontal_fov_;
  const float vertical_fov_;
  const glm::mat4 transform_;

  DISALLOW_COPY_AND_ASSIGN(MTMFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_MTM_FUNCTION_H_
