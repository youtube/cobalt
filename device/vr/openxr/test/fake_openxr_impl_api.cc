// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl.h>

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/test/openxr_negotiate.h"
#include "device/vr/openxr/test/openxr_test_helper.h"

namespace {
// Global test helper that communicates with the test and contains the mock
// OpenXR runtime state/properties. A reference to this is returned as the
// instance handle through xrCreateInstance.
OpenXrTestHelper g_test_helper;
}  // namespace

// Extension methods

// Mock implementations of openxr runtime.dll APIs.
// Please add new APIs in alphabetical order.

XrResult xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquire_info,
    uint32_t* index) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(acquire_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainImageAcquireInfo is nullptr");
  RETURN_IF(acquire_info->type != XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrAcquireSwapchainImage type invalid");
  RETURN_IF(acquire_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrAcquireSwapchainImage next not nullptr");

  RETURN_IF(index == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrAcquireSwapchainImage index is nullptr");
  *index = g_test_helper.NextSwapchainImageIndex();

  return XR_SUCCESS;
}

XrResult xrAttachSessionActionSets(
    XrSession session,
    const XrSessionActionSetsAttachInfo* attach_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(attach_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSessionActionSetsAttachInfo is nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.AttachActionSets(*attach_info));

  return XR_SUCCESS;
}

XrResult xrBeginFrame(XrSession session,
                      const XrFrameBeginInfo* frame_begin_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(frame_begin_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrFrameBeginInfo is nullptr");
  RETURN_IF(frame_begin_info->type != XR_TYPE_FRAME_BEGIN_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrFrameBeginInfo type invalid");
  RETURN_IF(frame_begin_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrFrameBeginInfo next is not nullptr");
  return g_test_helper.BeginFrame();
}

XrResult xrBeginSession(XrSession session,
                        const XrSessionBeginInfo* begin_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(begin_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSessionBeginInfo is nullptr");
  RETURN_IF(begin_info->type != XR_TYPE_SESSION_BEGIN_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSessionBeginInfo type invalid");

  // Create a list of all the view configurations requested by the client.
  std::vector<XrViewConfigurationType> view_configs = {
      begin_info->primaryViewConfigurationType};
  if (begin_info->next != nullptr) {
    // Secondary views are requested if a next pointer is specified.
    const XrSecondaryViewConfigurationSessionBeginInfoMSFT* second_begin_info =
        reinterpret_cast<
            const XrSecondaryViewConfigurationSessionBeginInfoMSFT*>(
            begin_info->next);
    RETURN_IF(second_begin_info->type !=
                  XR_TYPE_SECONDARY_VIEW_CONFIGURATION_SESSION_BEGIN_INFO_MSFT,
              XR_ERROR_VALIDATION_FAILURE,
              "XrSecondaryViewConfigurationSessionBeginInfoMSFT type invalid");
    RETURN_IF(
        second_begin_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
        "XrSecondaryViewConfigurationSessionBeginInfoMSFT next is not nullptr");

    for (uint32_t i = 0; i < second_begin_info->viewConfigurationCount; i++) {
      view_configs.push_back(
          second_begin_info->enabledViewConfigurationTypes[i]);
    }
  }

  RETURN_IF_XR_FAILED(g_test_helper.BeginSession(view_configs));

  return XR_SUCCESS;
}

XrResult xrCreateAction(XrActionSet action_set,
                        const XrActionCreateInfo* create_info,
                        XrAction* action) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionCreateInfo is nullptr");
  RETURN_IF_XR_FAILED(
      g_test_helper.CreateAction(action_set, *create_info, action));

  return XR_SUCCESS;
}

XrResult xrCreateActionSet(XrInstance instance,
                           const XrActionSetCreateInfo* create_info,
                           XrActionSet* action_set) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionSetCreateInfo is nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateActionSetCreateInfo(*create_info));
  RETURN_IF(action_set == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionSet is nullptr");
  *action_set = g_test_helper.CreateActionSet(*create_info);

  return XR_SUCCESS;
}

XrResult xrCreateActionSpace(XrSession session,
                             const XrActionSpaceCreateInfo* create_info,
                             XrSpace* space) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionSpaceCreateInfo is nullptr");
  RETURN_IF(space == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSpace is nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.CreateActionSpace(*create_info, space));

  return XR_SUCCESS;
}

XrResult xrCreateInstance(const XrInstanceCreateInfo* create_info,
                          XrInstance* instance) {
  DVLOG(2) << __FUNCTION__;

  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrInstanceCreateInfo is nullptr");
  RETURN_IF(create_info->applicationInfo.apiVersion != XR_CURRENT_API_VERSION,
            XR_ERROR_API_VERSION_UNSUPPORTED, "apiVersion unsupported");

  RETURN_IF(create_info->type != XR_TYPE_INSTANCE_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrInstanceCreateInfo type invalid");

  RETURN_IF(create_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrInstanceCreateInfo next is not nullptr");

  RETURN_IF(create_info->createFlags != 0, XR_ERROR_VALIDATION_FAILURE,
            "XrInstanceCreateInfo createFlags is not 0");

  RETURN_IF(
      create_info->enabledApiLayerCount != 0 ||
          create_info->enabledApiLayerNames != nullptr,
      XR_ERROR_VALIDATION_FAILURE,
      "XrInstanceCreateInfo ApiLayer is not supported by this version of test");

  for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
    bool valid_extension = false;
    for (size_t j = 0; j < OpenXrTestHelper::kNumExtensionsSupported; j++) {
      if (strcmp(create_info->enabledExtensionNames[i],
                 OpenXrTestHelper::kExtensions[j]) == 0) {
        valid_extension = true;
        break;
      }
    }

    RETURN_IF_FALSE(valid_extension, XR_ERROR_VALIDATION_FAILURE,
                    "enabledExtensionNames contains invalid extensions");
  }

  RETURN_IF(instance == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrInstance is nullptr");
  *instance = g_test_helper.CreateInstance();

  return XR_SUCCESS;
}

XrResult xrCreateReferenceSpace(XrSession session,
                                const XrReferenceSpaceCreateInfo* create_info,
                                XrSpace* space) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrReferenceSpaceCreateInfo is nullptr");
  RETURN_IF(create_info->type != XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "XrReferenceSpaceCreateInfo type invalid");
  RETURN_IF(create_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrReferenceSpaceCreateInfo next is not nullptr");
  RETURN_IF(
      create_info->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_LOCAL &&
          create_info->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_VIEW &&
          create_info->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE &&
          create_info->referenceSpaceType !=
              XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT,
      XR_ERROR_REFERENCE_SPACE_UNSUPPORTED,
      "XrReferenceSpaceCreateInfo referenceSpaceType invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateXrPosefIsIdentity(
      create_info->poseInReferenceSpace));
  RETURN_IF(space == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSpace is nullptr");
  *space = g_test_helper.CreateReferenceSpace(create_info->referenceSpaceType);

  return XR_SUCCESS;
}

XrResult xrCreateSession(XrInstance instance,
                         const XrSessionCreateInfo* create_info,
                         XrSession* session) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSessionCreateInfo is nullptr");
  RETURN_IF(create_info->type != XR_TYPE_SESSION_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSessionCreateInfo type invalid");
  RETURN_IF(create_info->createFlags != 0, XR_ERROR_VALIDATION_FAILURE,
            "XrSessionCreateInfo createFlags is not 0");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(create_info->systemId));

  const XrGraphicsBindingD3D11KHR* binding =
      static_cast<const XrGraphicsBindingD3D11KHR*>(create_info->next);
  RETURN_IF(binding->type != XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
            XR_ERROR_VALIDATION_FAILURE,
            "XrGraphicsBindingD3D11KHR type invalid");
  RETURN_IF(binding->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrGraphicsBindingD3D11KHR next is not nullptr");
  RETURN_IF(binding->device == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "D3D11Device is nullptr");

  g_test_helper.SetD3DDevice(binding->device);
  RETURN_IF(session == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSession is nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.CreateSession(session));

  return XR_SUCCESS;
}

XrResult xrCreateSwapchain(XrSession session,
                           const XrSwapchainCreateInfo* create_info,
                           XrSwapchain* swapchain) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(create_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo is nullptr");
  RETURN_IF(create_info->type != XR_TYPE_SWAPCHAIN_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSwapchainCreateInfo type invalid");
  RETURN_IF(create_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo next is not nullptr");
  RETURN_IF(create_info->createFlags != 0, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo createFlags is not 0");
  RETURN_IF(create_info->usageFlags != XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo usageFlags is not "
            "XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT");
  RETURN_IF(create_info->format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED,
            "XrSwapchainCreateInfo format unsupported");
  RETURN_IF(create_info->sampleCount != OpenXrTestHelper::kSwapCount,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo sampleCount invalid");
  RETURN_IF(create_info->width == 0, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo width is zero");
  RETURN_IF(create_info->height == 0, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo height is zero");
  RETURN_IF(create_info->faceCount != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo faceCount is not 1");
  RETURN_IF(create_info->arraySize != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo arraySize invalid");
  RETURN_IF(create_info->mipCount != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo mipCount is not 1");

  RETURN_IF(swapchain == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchain is nullptr");
  *swapchain = g_test_helper.CreateSwapchain();

  return XR_SUCCESS;
}

XrResult xrDestroyActionSet(XrActionSet action_set) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.DestroyActionSet(action_set));
  return XR_SUCCESS;
}

XrResult xrDestroyInstance(XrInstance instance) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.DestroyInstance(instance));
  return XR_SUCCESS;
}

XrResult xrDestroySession(XrSession session) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.DestroySession(session));
  return XR_SUCCESS;
}

XrResult xrDestroySpace(XrSpace space) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.DestroySpace(space));
  return XR_SUCCESS;
}

XrResult xrDestroySwapchain(XrSwapchain swapchain) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.DestroySwapchain(swapchain));
  return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frame_end_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(frame_end_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrFrameEndInfo is nullptr");
  RETURN_IF(frame_end_info->type != XR_TYPE_FRAME_END_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrFrameEndInfo type invalid");
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidatePredictedDisplayTime(frame_end_info->displayTime));
  RETURN_IF(frame_end_info->environmentBlendMode !=
                OpenXrTestHelper::kEnvironmentBlendMode,
            XR_ERROR_VALIDATION_FAILURE,
            "XrFrameEndInfo environmentBlendMode invalid");
  // We currently only support one layer per view configuration.
  RETURN_IF(frame_end_info->layerCount != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrFrameEndInfo layerCount invalid");
  RETURN_IF(frame_end_info->layers == nullptr, XR_ERROR_LAYER_INVALID,
            "XrFrameEndInfo has nullptr layers");

  for (uint32_t i = 0; i < frame_end_info->layerCount; i++) {
    const XrCompositionLayerProjection* primary_layer_ptr =
        reinterpret_cast<const XrCompositionLayerProjection*>(
            frame_end_info->layers[i]);
    RETURN_IF_XR_FAILED(g_test_helper.ValidateXrCompositionLayerProjection(
        g_test_helper.PrimaryViewConfig(), *primary_layer_ptr));
  }

  if (frame_end_info->next != nullptr) {
    // If the next pointer is specified, secondary views are active.
    const XrSecondaryViewConfigurationFrameEndInfoMSFT* second_end_info =
        reinterpret_cast<const XrSecondaryViewConfigurationFrameEndInfoMSFT*>(
            frame_end_info->next);

    RETURN_IF(second_end_info->type !=
                  XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_END_INFO_MSFT,
              XR_ERROR_VALIDATION_FAILURE,
              "XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_END_INFO_MSFT type "
              "invalid");
    RETURN_IF(
        second_end_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
        "XrSecondaryViewConfigurationFrameEndInfoMSFT next is not nullptr");

    for (uint32_t i = 0; i < second_end_info->viewConfigurationCount; i++) {
      XrSecondaryViewConfigurationLayerInfoMSFT layer_info =
          second_end_info->viewConfigurationLayersInfo[i];

      RETURN_IF(layer_info.type !=
                    XR_TYPE_SECONDARY_VIEW_CONFIGURATION_LAYER_INFO_MSFT,
                XR_ERROR_VALIDATION_FAILURE,
                "XrSecondaryViewConfigurationLayerInfoMSFT type invalid");
      RETURN_IF(
          layer_info.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
          "XrSecondaryViewConfigurationLayerInfoMSFT next is not nullptr");
      RETURN_IF(
          layer_info.viewConfigurationType == g_test_helper.PrimaryViewConfig(),
          XR_ERROR_LAYER_INVALID,
          "XrSecondaryViewConfigurationLayerInfoMSFT cannot have a "
          "primary view configuration");
      RETURN_IF_XR_FAILED(g_test_helper.ValidateViewConfigType(
          layer_info.viewConfigurationType));
      RETURN_IF(layer_info.environmentBlendMode !=
                    OpenXrTestHelper::kEnvironmentBlendMode,
                XR_ERROR_VALIDATION_FAILURE,
                "XrSecondaryViewConfigurationLayerInfoMSFT "
                "environmentBlendMode invalid");
      // We currently only support one layer per view configuration.
      RETURN_IF(layer_info.layerCount != 1, XR_ERROR_VALIDATION_FAILURE,
                "XrSecondaryViewConfigurationLayerInfoMSFT layerCount invalid");
      RETURN_IF(layer_info.layers == nullptr, XR_ERROR_LAYER_INVALID,
                "XrSecondaryViewConfigurationLayerInfoMSFT has nullptr layers");

      for (uint32_t j = 0; j < layer_info.layerCount; j++) {
        const XrCompositionLayerProjection* secondary_layer_ptr =
            reinterpret_cast<const XrCompositionLayerProjection*>(
                layer_info.layers[j]);
        RETURN_IF_XR_FAILED(g_test_helper.ValidateXrCompositionLayerProjection(
            layer_info.viewConfigurationType, *secondary_layer_ptr));
      }
    }
  }

  RETURN_IF_XR_FAILED(g_test_helper.EndFrame());
  g_test_helper.OnPresentedFrame();
  return XR_SUCCESS;
}

XrResult xrEndSession(XrSession session) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_XR_FAILED(g_test_helper.EndSession());

  return XR_SUCCESS;
}

XrResult xrEnumerateEnvironmentBlendModes(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    uint32_t environment_blend_mode_capacity_input,
    uint32_t* environment_blend_mode_count_output,
    XrEnvironmentBlendMode* environment_blend_modes) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidateViewConfigType(view_configuration_type));

  RETURN_IF(environment_blend_mode_count_output == nullptr,
            XR_ERROR_VALIDATION_FAILURE,
            "environment_blend_mode_count_output is nullptr");
  *environment_blend_mode_count_output = 1;
  if (environment_blend_mode_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(environment_blend_mode_capacity_input != 1,
            XR_ERROR_VALIDATION_FAILURE,
            "environment_blend_mode_capacity_input is neither 0 or 1");
  RETURN_IF(environment_blend_modes == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrEnvironmentBlendMode is nullptr");
  *environment_blend_modes = OpenXrTestHelper::kEnvironmentBlendMode;

  return XR_SUCCESS;
}

// Even thought xrEnumerateInstanceExtensionProperties is not directly called
// in our implementation, it is used inside loader so this function mock is
// needed
XrResult xrEnumerateInstanceExtensionProperties(
    const char* layer_name,
    uint32_t property_capacity_input,
    uint32_t* property_count_output,
    XrExtensionProperties* properties) {
  DVLOG(2) << __FUNCTION__;

  RETURN_IF(
      property_capacity_input < OpenXrTestHelper::kNumExtensionsSupported &&
          property_capacity_input != 0,
      XR_ERROR_SIZE_INSUFFICIENT, "XrExtensionProperties array is too small");

  RETURN_IF(property_count_output == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "property_count_output is nullptr");
  *property_count_output = OpenXrTestHelper::kNumExtensionsSupported;
  if (property_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(
      property_capacity_input != OpenXrTestHelper::kNumExtensionsSupported,
      XR_ERROR_VALIDATION_FAILURE,
      "property_capacity_input is neither 0 or kNumExtensionsSupported");
  RETURN_IF(properties == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrExtensionProperties is nullptr");
  for (uint32_t i = 0; i < OpenXrTestHelper::kNumExtensionsSupported; i++) {
    properties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
    errno_t error = strcpy_s(properties[i].extensionName,
                             std::size(properties[i].extensionName),
                             OpenXrTestHelper::kExtensions[i]);
    DCHECK(error == 0);
    properties[i].extensionVersion = 1;
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateViewConfigurations(
    XrInstance instance,
    XrSystemId system_id,
    uint32_t view_configuration_type_capacity_input,
    uint32_t* view_configuration_type_count_output,
    XrViewConfigurationType* view_configuration_types) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(view_configuration_type_count_output == nullptr,
            XR_ERROR_VALIDATION_FAILURE,
            "view_configuration_type_count_output is nullptr");

  std::vector<XrViewConfigurationType> view_configs =
      g_test_helper.SupportedViewConfigs();
  *view_configuration_type_count_output = view_configs.size();
  if (view_configuration_type_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(view_configuration_types == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "view_configuration_types is nullptr");
  RETURN_IF(view_configuration_type_capacity_input != view_configs.size(),
            XR_ERROR_SIZE_INSUFFICIENT,
            "view_configuration_type_capacity_input size is insufficient");

  for (uint32_t i = 0; i < view_configs.size(); i++) {
    view_configuration_types[i] = view_configs[i];
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateViewConfigurationViews(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    uint32_t view_capacity_input,
    uint32_t* view_count_output,
    XrViewConfigurationView* views) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidateViewConfigType(view_configuration_type));
  RETURN_IF(view_count_output == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "view_count_output is nullptr");

  const std::vector<XrViewConfigurationView>& view_properties =
      g_test_helper.GetViewConfigInfo(view_configuration_type).Properties();
  *view_count_output = view_properties.size();
  if (view_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(view_capacity_input != view_properties.size(),
            XR_ERROR_SIZE_INSUFFICIENT, "view_capacity_input is insufficient");
  RETURN_IF(views == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrViewConfigurationView is nullptr");
  for (uint32_t i = 0; i < view_properties.size(); i++) {
    views[i] = view_properties[i];
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain,
                                    uint32_t image_capacity_input,
                                    uint32_t* image_count_output,
                                    XrSwapchainImageBaseHeader* images) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(image_capacity_input != OpenXrTestHelper::kMinSwapchainBuffering &&
                image_capacity_input != 0,
            XR_ERROR_SIZE_INSUFFICIENT,
            "xrEnumerateSwapchainImages does not equal length returned by "
            "xrCreateSwapchain");

  RETURN_IF(image_count_output == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "image_capacity_input is nullptr");
  *image_count_output = OpenXrTestHelper::kMinSwapchainBuffering;
  if (image_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(image_capacity_input != OpenXrTestHelper::kMinSwapchainBuffering,
            XR_ERROR_VALIDATION_FAILURE,
            "image_capacity_input is neither 0 or kMinSwapchainBuffering");
  RETURN_IF(images == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainImageBaseHeader is nullptr");
  const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>& textures =
      g_test_helper.GetSwapchainTextures();
  DCHECK_EQ(textures.size(), image_capacity_input);

  for (uint32_t i = 0; i < image_capacity_input; i++) {
    XrSwapchainImageD3D11KHR& image =
        reinterpret_cast<XrSwapchainImageD3D11KHR*>(images)[i];

    RETURN_IF(image.type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
              XR_ERROR_VALIDATION_FAILURE,
              "XrSwapchainImageD3D11KHR type invalid");
    RETURN_IF(image.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
              "XrSwapchainImageD3D11KHR next is not nullptr");

    image.texture = textures[i].Get();
  }

  return XR_SUCCESS;
}

__stdcall XrResult xrGetD3D11GraphicsRequirementsKHR(
    XrInstance instance,
    XrSystemId system_id,
    XrGraphicsRequirementsD3D11KHR* graphics_requirements) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(graphics_requirements == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrGraphicsRequirementsD3D11KHR is nullptr");
  RETURN_IF(
      graphics_requirements->type != XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR,
      XR_ERROR_VALIDATION_FAILURE,
      "XrGraphicsRequirementsD3D11KHR type invalid");
  RETURN_IF(graphics_requirements->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrGraphicsRequirementsD3D11KHR next is not nullptr");

  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  DCHECK(SUCCEEDED(hr));
  if (SUCCEEDED(dxgi_factory->EnumAdapters(0, &adapter))) {
    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
    graphics_requirements->adapterLuid = desc.AdapterLuid;

    // Require D3D11.1 to support shared NT handles.
    graphics_requirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_1;

    return XR_SUCCESS;
  }

  RETURN_IF_FALSE(false, XR_ERROR_VALIDATION_FAILURE,
                  "Unable to create query DXGI Adapter");
}

XrResult xrGetActionStateFloat(XrSession session,
                               const XrActionStateGetInfo* get_info,
                               XrActionStateFloat* state) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateGetInfo is nullptr");
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateFloat has wrong type");
  RETURN_IF(get_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateFloat next is not nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(get_info->subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateFloat has subactionPath != nullptr which is not "
            "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStateFloat(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetActionStateBoolean(XrSession session,
                                 const XrActionStateGetInfo* get_info,
                                 XrActionStateBoolean* state) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateGetInfo is nullptr");
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateBoolean get_info has wrong type");
  RETURN_IF(get_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateBoolean next is not nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(get_info->subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateBoolean has subactionPath != nullptr which is not "
            "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStateBoolean(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetActionStateVector2f(XrSession session,
                                  const XrActionStateGetInfo* get_info,
                                  XrActionStateVector2f* state) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateGetInfo is nullptr");
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateVector2f get_info has wrong type");
  RETURN_IF(get_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateVector2f next is not nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(
      get_info->subactionPath != XR_NULL_PATH, XR_ERROR_VALIDATION_FAILURE,
      "xrGetActionStateVector2f has subactionPath != nullptr which is not "
      "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStateVector2f(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetActionStatePose(XrSession session,
                              const XrActionStateGetInfo* get_info,
                              XrActionStatePose* state) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateGetInfo is nullptr");
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStatePose get_info has wrong type");
  RETURN_IF(get_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStatePose next is not nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(get_info->subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStatePose has subactionPath != nullptr which is not "
            "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStatePose(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetCurrentInteractionProfile(
    XrSession session,
    XrPath top_level_user_path,
    XrInteractionProfileState* interaction_profile) {
  DVLOG(1) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(
      g_test_helper.AttachedActionSetsSize() == 0,
      XR_ERROR_ACTIONSET_NOT_ATTACHED,
      "xrGetCurrentInteractionProfile action sets have not been attached yet");
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePath(top_level_user_path));
  RETURN_IF(interaction_profile == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrInteractionProfileState is nullptr");
  RETURN_IF(interaction_profile->type != XR_TYPE_INTERACTION_PROFILE_STATE,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetCurrentInteractionProfile type is not "
            "XR_TYPE_INTERACTION_PROFILE_STATE");
  RETURN_IF(interaction_profile->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrGetCurrentInteractionProfile next is not "
            "nullptr");
  interaction_profile->interactionProfile =
      g_test_helper.GetCurrentInteractionProfile();
  return XR_SUCCESS;
}

XrResult xrGetReferenceSpaceBoundsRect(
    XrSession session,
    XrReferenceSpaceType refernece_space_type,
    XrExtent2Df* bounds) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(refernece_space_type != XR_REFERENCE_SPACE_TYPE_STAGE,
            XR_ERROR_REFERENCE_SPACE_UNSUPPORTED,
            "xrGetReferenceSpaceBoundsRect type is not stage");
  RETURN_IF(bounds == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrExtent2Df is nullptr");
  bounds->width = 0;
  bounds->height = 0;
  return XR_SUCCESS;
}

XrResult xrGetViewConfigurationProperties(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    XrViewConfigurationProperties* configuration_properties) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(
      view_configuration_type != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
      XR_ERROR_VALIDATION_FAILURE, "viewConfigurationType must be stereo");
  RETURN_IF(
      configuration_properties->type == XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
      XR_ERROR_VALIDATION_FAILURE,
      "XrViewConfigurationProperties.type must be "
      "XR_TYPE_VIEW_CONFIGURATION_PROPERTIES");
  RETURN_IF(configuration_properties->next == nullptr,
            XR_ERROR_VALIDATION_FAILURE,
            "XrViewConfigurationProperties.next must be nullptr");
  configuration_properties->viewConfigurationType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  configuration_properties->fovMutable = XR_TRUE;
  return XR_SUCCESS;
}

XrResult xrGetSystem(XrInstance instance,
                     const XrSystemGetInfo* get_info,
                     XrSystemId* system_id) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(get_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSystemGetInfo is nullptr");
  RETURN_IF(get_info->type != XR_TYPE_SYSTEM_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSystemGetInfo type invalid");
  RETURN_IF(get_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSystemGetInfo next is not nullptr");
  RETURN_IF(get_info->formFactor != XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
            XR_ERROR_VALIDATION_FAILURE, "XrSystemGetInfo formFactor invalid");

  RETURN_IF(system_id == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSystemId is nullptr");
  *system_id = g_test_helper.GetSystemId();

  return XR_SUCCESS;
}

XrResult xrGetSystemProperties(XrInstance instance,
                               XrSystemId system_id,
                               XrSystemProperties* system_properties) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(system_properties == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSystemProperties is nullptr");
  RETURN_IF(system_properties->type != XR_TYPE_SYSTEM_PROPERTIES,
            XR_ERROR_VALIDATION_FAILURE, "XrSystemProperties type invalid");
  RETURN_IF(system_properties->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSystemProperties next is not nullptr");

  *system_properties = g_test_helper.GetSystemProperties();
  system_properties->systemId = system_id;

  return XR_SUCCESS;
}

XrResult xrLocateSpace(XrSpace space,
                       XrSpace base_space,
                       XrTime time,
                       XrSpaceLocation* location) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(space));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(base_space));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePredictedDisplayTime(time));

  RETURN_IF(location == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSpaceLocation is nullptr");
  g_test_helper.LocateSpace(space, &(location->pose));

  location->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                            XR_SPACE_LOCATION_POSITION_VALID_BIT |
                            XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
                            XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

  return XR_SUCCESS;
}

XrResult xrLocateViews(XrSession session,
                       const XrViewLocateInfo* view_locate_info,
                       XrViewState* view_state,
                       uint32_t view_capacity_input,
                       uint32_t* view_count_output,
                       XrView* views) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(view_locate_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrViewLocateInfo is nullptr");
  RETURN_IF(view_locate_info->type != XR_TYPE_VIEW_LOCATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrLocateViews view_locate_info type invalid");
  RETURN_IF(view_locate_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrViewLocateInfo next is not nullptr");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateViewConfigType(
      view_locate_info->viewConfigurationType));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePredictedDisplayTime(
      view_locate_info->displayTime));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(view_locate_info->space));
  RETURN_IF(view_state == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrViewState is nullptr");
  RETURN_IF(view_count_output == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "view_count_output is nullptr");

  const device::OpenXrViewConfiguration& view_config =
      g_test_helper.GetViewConfigInfo(view_locate_info->viewConfigurationType);
  const std::vector<XrView>& view_config_views = view_config.Views();
  *view_count_output = view_config_views.size();
  if (view_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(view_capacity_input != view_config_views.size(),
            XR_ERROR_SIZE_INSUFFICIENT,
            "view_capacity_input is neither 0 or OpenXrTestHelper::kViewCount");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateViews(view_capacity_input, views));
  RETURN_IF_FALSE(
      g_test_helper.UpdateViews(view_locate_info->viewConfigurationType, views,
                                view_capacity_input),
      XR_ERROR_VALIDATION_FAILURE, "xrLocateViews UpdateViews failed");
  view_state->viewStateFlags =
      XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;

  return XR_SUCCESS;
}

XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* event_data) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));

  return g_test_helper.PollEvent(event_data);
}

XrResult xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* release_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(release_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainImageReleaseInfo is nullptr");
  RETURN_IF(release_info->type != XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainImageReleaseInfo type invalid");
  RETURN_IF(release_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainImageReleaseInfo next is not nullptr");

  return XR_SUCCESS;
}

XrResult xrSuggestInteractionProfileBindings(
    XrInstance instance,
    const XrInteractionProfileSuggestedBinding* suggested_bindings) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(suggested_bindings == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrInteractionProfileSuggestedBinding is nullptr");
  RETURN_IF(
      suggested_bindings->type != XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
      XR_ERROR_VALIDATION_FAILURE,
      "xrSetInteractionProfileSuggestedBindings type invalid");
  RETURN_IF(suggested_bindings->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "xrSetInteractionProfileSuggestedBindings next is not nullptr");
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidatePath(suggested_bindings->interactionProfile));
  std::string interaction_profile =
      g_test_helper.PathToString(suggested_bindings->interactionProfile);

  RETURN_IF(suggested_bindings->suggestedBindings == nullptr,
            XR_ERROR_VALIDATION_FAILURE,
            "XrInteractionProfileSuggestedBinding has nullptr "
            "XrActionSuggestedBinding");
  RETURN_IF(g_test_helper.AttachedActionSetsSize() != 0,
            XR_ERROR_ACTIONSETS_ALREADY_ATTACHED,
            "xrSuggestInteractionProfileBindings called after "
            "xrAttachSessionActionSets");
  for (uint32_t i = 0; i < suggested_bindings->countSuggestedBindings; i++) {
    XrActionSuggestedBinding suggestedBinding =
        suggested_bindings->suggestedBindings[i];
    RETURN_IF_XR_FAILED(g_test_helper.BindActionAndPath(
        suggested_bindings->interactionProfile, suggestedBinding));
  }

  return XR_SUCCESS;
}

XrResult xrStringToPath(XrInstance instance,
                        const char* path_string,
                        XrPath* path) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(path == nullptr, XR_ERROR_VALIDATION_FAILURE, "path is nullptr");
  *path = g_test_helper.GetPath(path_string);

  return XR_SUCCESS;
}

XrResult xrPathToString(XrInstance instance,
                        XrPath path,
                        uint32_t buffer_capacity_input,
                        uint32_t* buffer_count_output,
                        char* buffer) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePath(path));

  std::string path_string = g_test_helper.PathToString(path);
  RETURN_IF(buffer_count_output == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "buffer_count_output is nullptr");
  // OpenXR spec counts terminating '\0'
  *buffer_count_output = path_string.size() + 1;
  if (buffer_capacity_input == 0) {
    return XR_SUCCESS;
  }

  RETURN_IF(
      buffer_capacity_input <= path_string.size(), XR_ERROR_SIZE_INSUFFICIENT,
      "xrPathToString inputsize is not large enough to hold the output string");
  errno_t error = strcpy_s(buffer, *buffer_count_output, path_string.data());
  DCHECK_EQ(error, 0);

  return XR_SUCCESS;
}

XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* sync_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(g_test_helper.UpdateData(), XR_ERROR_VALIDATION_FAILURE,
                  "xrSyncActionData can't receive data from test");
  RETURN_IF(sync_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionsSyncInfo is nullptr");
  RETURN_IF(sync_info->type != XR_TYPE_ACTIONS_SYNC_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrActionsSyncInfo type invalid");
  RETURN_IF(sync_info->next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionsSyncInfo next is not nullptr");
  RETURN_IF(sync_info->activeActionSets == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionsSyncInfo activeActionSets is nullptr");

  for (uint32_t i = 0; i < sync_info->countActiveActionSets; i++) {
    RETURN_IF(
        sync_info->activeActionSets[i].subactionPath != XR_NULL_PATH,
        XR_ERROR_VALIDATION_FAILURE,
        "xrSyncActionData does not support use of subactionPath for test yet");
    RETURN_IF_XR_FAILED(
        g_test_helper.SyncActionData(sync_info->activeActionSets[i].actionSet));
  }

  return XR_SUCCESS;
}

XrResult xrWaitFrame(XrSession session,
                     const XrFrameWaitInfo* frame_wait_info,
                     XrFrameState* frame_state) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(frame_wait_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrFrameWaitInfo is nullptr");
  RETURN_IF(frame_wait_info->type != XR_TYPE_FRAME_WAIT_INFO,
            XR_ERROR_VALIDATION_FAILURE, "frame_wait_info type invalid");
  RETURN_IF(frame_state == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrFrameState is nullptr");
  RETURN_IF(frame_state->type != XR_TYPE_FRAME_STATE,
            XR_ERROR_VALIDATION_FAILURE, "XR_TYPE_FRAME_STATE type invalid");

  if (frame_state->next != nullptr) {
    // If a next pointer is specified, secondary views are active.
    XrSecondaryViewConfigurationFrameStateMSFT* secondary_frame_state =
        reinterpret_cast<XrSecondaryViewConfigurationFrameStateMSFT*>(
            frame_state->next);
    RETURN_IF(secondary_frame_state->type !=
                  XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_STATE_MSFT,
              XR_ERROR_VALIDATION_FAILURE,
              "XrSecondaryViewConfigurationFrameStateMSFT type invalid");
    RETURN_IF(secondary_frame_state->next != nullptr,
              XR_ERROR_VALIDATION_FAILURE,
              "XrSecondaryViewConfigurationFrameStateMSFT next is not nullptr");
    RETURN_IF(secondary_frame_state->viewConfigurationStates == nullptr,
              XR_ERROR_VALIDATION_FAILURE,
              "XrSecondaryViewConfigurationFrameStateMSFT "
              "viewConfigurationStates must point to an array");

    RETURN_IF_XR_FAILED(g_test_helper.GetSecondaryConfigStates(
        secondary_frame_state->viewConfigurationCount,
        secondary_frame_state->viewConfigurationStates));
  }

  frame_state->predictedDisplayTime = g_test_helper.NextPredictedDisplayTime();

  return XR_SUCCESS;
}

XrResult xrWaitSwapchainImage(XrSwapchain swapchain,
                              const XrSwapchainImageWaitInfo* wait_info) {
  DVLOG(2) << __FUNCTION__;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(wait_info == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainImageWaitInfo is nullptr");
  RETURN_IF(wait_info->type != XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
            XR_ERROR_VALIDATION_FAILURE, "xrWaitSwapchainImage type invalid");
  RETURN_IF(wait_info->type != XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrWaitSwapchainImage next is nullptr");
  RETURN_IF(wait_info->timeout != XR_INFINITE_DURATION,
            XR_ERROR_VALIDATION_FAILURE,
            "xrWaitSwapchainImage timeout not XR_INFINITE_DURATION");

  return XR_SUCCESS;
}

// Getter for extension methods. Casts the correct function dynamically based on
// the method name provided.
// Please add new OpenXR APIs below in alphabetical order.
XrResult XRAPI_PTR xrGetInstanceProcAddr(XrInstance instance,
                                         const char* name,
                                         PFN_xrVoidFunction* function) {
  if (strcmp(name, "xrAcquireSwapchainImage") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrAcquireSwapchainImage);
  } else if (strcmp(name, "xrAttachSessionActionSets") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrAttachSessionActionSets);
  } else if (strcmp(name, "xrBeginFrame") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrBeginFrame);
  } else if (strcmp(name, "xrBeginSession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrBeginSession);
  } else if (strcmp(name, "xrCreateAction") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateAction);
  } else if (strcmp(name, "xrCreateActionSet") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateActionSet);
  } else if (strcmp(name, "xrCreateActionSpace") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateActionSpace);
  } else if (strcmp(name, "xrCreateInstance") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateInstance);
  } else if (strcmp(name, "xrCreateReferenceSpace") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateReferenceSpace);
  } else if (strcmp(name, "xrCreateSession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateSession);
  } else if (strcmp(name, "xrCreateSwapchain") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateSwapchain);
  } else if (strcmp(name, "xrDestroyActionSet") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrDestroyActionSet);
  } else if (strcmp(name, "xrDestroyInstance") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrDestroyInstance);
  } else if (strcmp(name, "xrDestroySession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrDestroySession);
  } else if (strcmp(name, "xrDestroySpace") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrDestroySpace);
  } else if (strcmp(name, "xrDestroySwapchain") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrDestroySwapchain);
  } else if (strcmp(name, "xrEndFrame") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrEndFrame);
  } else if (strcmp(name, "xrEndSession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrEndSession);
  } else if (strcmp(name, "xrEnumerateEnvironmentBlendModes") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateEnvironmentBlendModes);
  } else if (strcmp(name, "xrEnumerateInstanceExtensionProperties") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(
        xrEnumerateInstanceExtensionProperties);
  } else if (strcmp(name, "xrEnumerateSwapchainImages") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateSwapchainImages);
  } else if (strcmp(name, "xrEnumerateViewConfigurations") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateViewConfigurations);
  } else if (strcmp(name, "xrEnumerateViewConfigurationViews") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateViewConfigurationViews);
  } else if (strcmp(name, "xrGetD3D11GraphicsRequirementsKHR") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrGetD3D11GraphicsRequirementsKHR);
  } else if (strcmp(name, "xrGetActionStateFloat") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStateFloat);
  } else if (strcmp(name, "xrGetActionStateBoolean") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStateBoolean);
  } else if (strcmp(name, "xrGetActionStateVector2f") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStateVector2f);
  } else if (strcmp(name, "xrGetActionStatePose") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStatePose);
  } else if (strcmp(name, "xrGetCurrentInteractionProfile") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrGetCurrentInteractionProfile);
  } else if (strcmp(name, "xrGetReferenceSpaceBoundsRect") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrGetReferenceSpaceBoundsRect);
  } else if (strcmp(name, "xrGetViewConfigurationProperties") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrGetViewConfigurationProperties);
  } else if (strcmp(name, "xrGetSystem") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetSystem);
  } else if (strcmp(name, "xrGetSystemProperties") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetSystemProperties);
  } else if (strcmp(name, "xrLocateSpace") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrLocateSpace);
  } else if (strcmp(name, "xrLocateViews") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrLocateViews);
  } else if (strcmp(name, "xrPollEvent") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrPollEvent);
  } else if (strcmp(name, "xrReleaseSwapchainImage") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrReleaseSwapchainImage);
  } else if (strcmp(name, "xrSuggestInteractionProfileBindings") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(
        xrSuggestInteractionProfileBindings);
  } else if (strcmp(name, "xrStringToPath") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrStringToPath);
  } else if (strcmp(name, "xrPathToString") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrPathToString);
  } else if (strcmp(name, "xrSyncActions") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrSyncActions);
  } else if (strcmp(name, "xrWaitFrame") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrWaitFrame);
  } else if (strcmp(name, "xrWaitSwapchainImage") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrWaitSwapchainImage);
  } else {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
  }

  return XR_SUCCESS;
}
