// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_scene.h"
#include "base/check_op.h"

namespace device {

OpenXrScene::OpenXrScene(const device::OpenXrExtensionHelper& extensions,
                         XrSceneObserverMSFT scene_observer)
    : extensions_(extensions),
      scene_(XR_NULL_HANDLE,
             OpenXrExtensionHandleTraits<XrSceneMSFT>(
                 extensions.ExtensionMethods().xrDestroySceneMSFT)) {
  XrSceneCreateInfoMSFT create_info{XR_TYPE_SCENE_CREATE_INFO_MSFT};
  XrResult create_scene_result =
      extensions.ExtensionMethods().xrCreateSceneMSFT(
          scene_observer, &create_info,
          OpenXrExtensionHandle<XrSceneMSFT>::Receiver(scene_).get());
  DCHECK(XR_SUCCEEDED(create_scene_result));
}
OpenXrScene::~OpenXrScene() = default;

XrResult OpenXrScene::GetPlanes(std::vector<OpenXrScenePlane>& out_planes) {
  XrSceneComponentsGetInfoMSFT get_info{XR_TYPE_SCENE_COMPONENTS_GET_INFO_MSFT};
  get_info.componentType = XR_SCENE_COMPONENT_TYPE_PLANE_MSFT;

  static const std::vector<OpenXrSceneObject::Type> plane_filters{
      XR_SCENE_OBJECT_TYPE_BACKGROUND_MSFT, XR_SCENE_OBJECT_TYPE_WALL_MSFT,
      XR_SCENE_OBJECT_TYPE_FLOOR_MSFT,      XR_SCENE_OBJECT_TYPE_CEILING_MSFT,
      XR_SCENE_OBJECT_TYPE_PLATFORM_MSFT,   XR_SCENE_OBJECT_TYPE_INFERRED_MSFT};

  XrSceneObjectTypesFilterInfoMSFT types_filter{
      XR_TYPE_SCENE_OBJECT_TYPES_FILTER_INFO_MSFT};
  types_filter.objectTypeCount = static_cast<uint32_t>(plane_filters.size());
  types_filter.objectTypes = plane_filters.data();
  device::InsertExtensionStruct(get_info, types_filter);

  // Before we get back the array of components/planes, we need to query
  // for the size of the array and has to do the allocation first.
  XrSceneComponentsMSFT scene_components{XR_TYPE_SCENE_COMPONENTS_MSFT};
  RETURN_IF_XR_FAILED(extensions_->ExtensionMethods().xrGetSceneComponentsMSFT(
      scene_.get(), &get_info, &scene_components));
  const uint32_t count = scene_components.componentCountOutput;

  // The output size (count) is the same for XrSceneComponentMSFT and
  // XrScenePlaneMSFT. It's a mapping 1:1 since we are getting back
  // plane-type components.
  std::vector<XrSceneComponentMSFT> components(count);
  scene_components.componentCapacityInput = components.size();
  scene_components.components = components.data();

  std::vector<XrScenePlaneMSFT> planes(count);
  XrScenePlanesMSFT scenePlanes{XR_TYPE_SCENE_PLANES_MSFT};
  scenePlanes.scenePlaneCount = planes.size();
  scenePlanes.scenePlanes = planes.data();
  device::InsertExtensionStruct(scene_components, scenePlanes);

  RETURN_IF_XR_FAILED(extensions_->ExtensionMethods().xrGetSceneComponentsMSFT(
      scene_.get(), &get_info, &scene_components));
  // The count should stay the same
  DCHECK_EQ(count, scene_components.componentCountOutput);

  out_planes.clear();
  out_planes.reserve(count);
  for (uint32_t k = 0; k < count; k++) {
    out_planes.emplace_back(components[k], planes[k]);
  }
  return XR_SUCCESS;
}

XrResult OpenXrScene::LocateObjects(XrSpace base_space,
                                    XrTime time,
                                    std::vector<OpenXrScenePlane>& planes) {
  std::vector<XrUuidMSFT> plane_ids;
  for (auto& plane : planes) {
    plane_ids.emplace_back(plane.id_);
  }

  XrSceneComponentsLocateInfoMSFT locate_info{
      XR_TYPE_SCENE_COMPONENTS_LOCATE_INFO_MSFT};
  locate_info.baseSpace = base_space;
  locate_info.time = time;
  locate_info.componentIdCount = static_cast<uint32_t>(plane_ids.size());
  locate_info.componentIds = static_cast<const XrUuidMSFT*>(plane_ids.data());

  std::vector<XrSceneComponentLocationMSFT> plane_locations;
  plane_locations.resize(plane_ids.size());
  XrSceneComponentLocationsMSFT component_locations{
      XR_TYPE_SCENE_COMPONENT_LOCATIONS_MSFT};
  component_locations.locationCount =
      static_cast<uint32_t>(plane_locations.size());
  component_locations.locations = plane_locations.data();

  XrResult locate_result =
      extensions_->ExtensionMethods().xrLocateSceneComponentsMSFT(
          scene_.get(), &locate_info, &component_locations);

  if (XR_SUCCEEDED(locate_result)) {
    for (size_t i = 0; i < planes.size(); i++) {
      planes[i].location_ = plane_locations[i];
    }
  }

  return locate_result;
}

}  // namespace device
