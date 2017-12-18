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

#include "cobalt/cssom/map_to_mesh_function.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "cobalt/cssom/filter_function_list_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

std::string MapToMeshFunction::ToString() const {
  std::string result = "map-to-mesh(";

  if (mesh_spec().mesh_type() == kEquirectangular) {
    result.append("equirectangular");
  } else if (mesh_spec().mesh_type() == kRectangular) {
    result.append("rectangular");
  } else if (mesh_spec().mesh_type() == kUrls) {
    result.append(mesh_spec().mesh_url()->ToString());

    const ResolutionMatchedMeshListBuilder& meshes =
        mesh_spec().resolution_matched_meshes();
    for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
      result.push_back(' ');
      result.append(base::IntToString(meshes[mesh_index]->width_match()));
      result.push_back(' ');
      result.append(base::IntToString(meshes[mesh_index]->height_match()));
      result.push_back(' ');
      result.append(meshes[mesh_index]->mesh_url()->ToString());
    }
  }

  if (mesh_spec().mesh_type() == kRectangular) {
    // Rectangular filters carry neither FOV nor transforms.
    result.append(", none, none, ");
  } else {
    result.append(base::StringPrintf(", %.7grad", horizontal_fov_in_radians()));
    result.append(base::StringPrintf(" %.7grad, ", vertical_fov_in_radians()));

    result.append("matrix3d(");
    for (int col = 0; col <= 3; ++col) {
      for (int row = 0; row <= 3; ++row) {
        if (col > 0 || row > 0) {
          result.append(", ");
        }
        result.append(base::StringPrintf("%.7g", transform()[col][row]));
      }
    }

    result.append("), ");
  }

  result.append(stereo_mode()->ToString());
  result.append(")");

  return result;
}

MapToMeshFunction::ResolutionMatchedMesh::ResolutionMatchedMesh(
    int width_match, int height_match,
    const scoped_refptr<PropertyValue>& mesh_url)
    : width_match_(width_match),
      height_match_(height_match),
      mesh_url_(mesh_url) {
  DCHECK(mesh_url_);
}

bool MapToMeshFunction::ResolutionMatchedMesh::operator==(
    const MapToMeshFunction::ResolutionMatchedMesh& rhs) const {
  return mesh_url()->Equals(*rhs.mesh_url()) &&
         width_match() == rhs.width_match() &&
         height_match() == rhs.height_match();
}

bool MapToMeshFunction::ResolutionMatchedMesh::operator!=(
    const MapToMeshFunction::ResolutionMatchedMesh& rhs) const {
  return !(*this == rhs);
}

const MapToMeshFunction* MapToMeshFunction::ExtractFromFilterList(
    PropertyValue* filter_value) {
  if (filter_value && filter_value != cssom::KeywordValue::GetNone()) {
    const cssom::FilterFunctionListValue::Builder& filter_list =
        base::polymorphic_downcast<cssom::FilterFunctionListValue*>(
            filter_value)
            ->value();

    for (size_t list_index = 0; list_index < filter_list.size(); ++list_index) {
      if (filter_list[list_index]->GetTypeId() ==
          base::GetTypeId<cssom::MapToMeshFunction>()) {
        return base::polymorphic_downcast<const cssom::MapToMeshFunction*>(
            filter_list[list_index]);
      }
    }
  }

  return NULL;
}

bool MapToMeshFunction::operator==(const MapToMeshFunction& rhs) const {
  if (mesh_spec().mesh_type() != rhs.mesh_spec().mesh_type() ||
      horizontal_fov_in_radians() != rhs.horizontal_fov_in_radians() ||
      vertical_fov_in_radians() != rhs.vertical_fov_in_radians() ||
      !stereo_mode()->Equals(*rhs.stereo_mode())) {
    return false;
  }

  if (mesh_spec().mesh_type() == kUrls) {
    if (!mesh_spec().mesh_url()->Equals(*rhs.mesh_spec().mesh_url())) {
      return false;
    }

    const ResolutionMatchedMeshListBuilder& lhs_meshes =
        mesh_spec().resolution_matched_meshes();
    const ResolutionMatchedMeshListBuilder& rhs_meshes =
        rhs.mesh_spec().resolution_matched_meshes();

    if (lhs_meshes.size() != rhs_meshes.size()) {
      return false;
    }

    for (size_t i = 0; i < lhs_meshes.size(); ++i) {
      if (*lhs_meshes[i] != *rhs_meshes[i]) {
        return false;
      }
    }

    for (int col = 0; col <= 3; ++col) {
      if (!glm::all(glm::equal(transform()[col], rhs.transform()[col]))) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace cssom
}  // namespace cobalt
