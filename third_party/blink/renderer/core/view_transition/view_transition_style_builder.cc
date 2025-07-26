// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_style_builder.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

const char* kGroupTagName = "html::view-transition-group";
const char* kImagePairTagName = "html::view-transition-image-pair";
const char* kNewImageTagName = "html::view-transition-new";
const char* kOldImageTagName = "html::view-transition-old";
const char* kKeyframeNamePrefix = "-ua-view-transition-group-anim-";

}  // namespace

void ViewTransitionStyleBuilder::AddUAStyle(const String& style) {
  builder_.Append(style);
}

String ViewTransitionStyleBuilder::Build() {
  return builder_.ReleaseString();
}

void ViewTransitionStyleBuilder::AddSelector(const String& name,
                                             const String& tag) {
  builder_.Append(name);
  builder_.Append("(");
  builder_.Append(tag);
  builder_.Append(")");
}

void ViewTransitionStyleBuilder::AddRules(const String& selector,
                                          const String& tag,
                                          const String& rules) {
  AddSelector(selector, tag);
  builder_.Append("{ ");
  builder_.Append(rules);
  builder_.Append(" }");
}

void ViewTransitionStyleBuilder::AddAnimations(
    AnimationType type,
    const String& tag,
    const ContainerProperties& source_properties,
    const CapturedCssProperties& animated_css_properties,
    const gfx::Transform& parent_inverse_transform) {
  switch (type) {
    case AnimationType::kOldOnly:
      AddRules(kOldImageTagName, tag,
               "animation-name: -ua-view-transition-fade-out");
      break;

    case AnimationType::kNewOnly:
      AddRules(kNewImageTagName, tag,
               "animation-name: -ua-view-transition-fade-in");
      break;

    case AnimationType::kBoth:
      AddRules(kOldImageTagName, tag,
               "animation-name: -ua-view-transition-fade-out, "
               "-ua-mix-blend-mode-plus-lighter");

      AddRules(kNewImageTagName, tag,
               "animation-name: -ua-view-transition-fade-in, "
               "-ua-mix-blend-mode-plus-lighter");

      AddRules(kImagePairTagName, tag, "isolation: isolate;\n");

      const String& animation_name =
          AddKeyframes(tag, source_properties, animated_css_properties,
                       parent_inverse_transform);
      StringBuilder rule_builder;
      rule_builder.Append("animation-name: ");
      rule_builder.Append(animation_name);
      rule_builder.Append(";\n");
      rule_builder.Append("animation-timing-function: ease;\n");
      rule_builder.Append("animation-delay: 0s;\n");
      rule_builder.Append("animation-iteration-count: 1;\n");
      rule_builder.Append("animation-direction: normal;\n");
      AddRules(kGroupTagName, tag, rule_builder.ReleaseString());
      if (!source_properties.box_geometry) {
        break;
      }

      StringBuilder keyframe_name_builder;
      keyframe_name_builder.Append("-ua-view-transition-content-geometry-");
      keyframe_name_builder.Append(tag);
      String image_pair_animation_name = keyframe_name_builder.ReleaseString();
      StringBuilder image_pair_animation_properties_builder;
      image_pair_animation_properties_builder.Append("animation-name: ");
      image_pair_animation_properties_builder.Append(image_pair_animation_name);
      image_pair_animation_properties_builder.Append(";\n");
      image_pair_animation_properties_builder.Append(
          "animation-delay: inherit;\n"
          "animation-direction: inherit;\n"
          "animation-iteration-count: inherit;\n"
          "animation-timing-function: inherit;\n");
      AddRules(kImagePairTagName, tag,
               image_pair_animation_properties_builder.ReleaseString());
      builder_.Append("@keyframes ");
      builder_.Append(image_pair_animation_name);
      builder_.AppendFormat(
          R"CSS({
        from {
          width: %.3fpx;
          height: %.3fpx;
        } }
      )CSS",
          source_properties.box_geometry->content_box.Width().ToFloat(),
          source_properties.box_geometry->content_box.Height().ToFloat());
      break;
  }
}

namespace {
std::string GetTransformString(
    const ViewTransitionStyleBuilder::ContainerProperties& properties,
    const gfx::Transform& parent_inverse_transform) {
  gfx::Transform applied_transform(parent_inverse_transform);
  applied_transform.PreConcat(properties.snapshot_matrix);
  return ComputedStyleUtils::ValueForTransform(applied_transform, 1, false)
      ->CssText()
      .Utf8();
}
}  // namespace

String ViewTransitionStyleBuilder::AddKeyframes(
    const String& tag,
    const ContainerProperties& source_properties,
    const CapturedCssProperties& animated_css_properties,
    const gfx::Transform& parent_inverse_transform) {
  String keyframe_name = [&tag]() {
    StringBuilder builder;
    builder.Append(kKeyframeNamePrefix);
    builder.Append(tag);
    return builder.ReleaseString();
  }();

  builder_.Append("@keyframes ");
  builder_.Append(keyframe_name);
  builder_.AppendFormat(
      R"CSS({
        from {
          transform: %s;
          width: %.3fpx;
          height: %3fpx;
      )CSS",
      GetTransformString(source_properties, parent_inverse_transform).c_str(),
      source_properties.GroupSize().width.ToFloat(),
      source_properties.GroupSize().height.ToFloat());

  for (const auto& [id, value] : animated_css_properties) {
    builder_.AppendFormat(
        "%s: %s;\n",
        CSSProperty::Get(id).GetPropertyNameAtomicString().Utf8().c_str(),
        value.Utf8().c_str());
  }
  builder_.Append("}}");
  return keyframe_name;
}

void ViewTransitionStyleBuilder::AddContainerStyles(
    const String& tag,
    const ContainerProperties& properties,
    const CapturedCssProperties& captured_css_properties,
    const gfx::Transform& parent_inverse_transform) {
  StringBuilder group_rule_builder;
  group_rule_builder.AppendFormat(
      R"CSS(
        width: %.3fpx;
        height: %.3fpx;
        transform: %s;
      )CSS",
      properties.GroupSize().width.ToFloat(),
      properties.GroupSize().height.ToFloat(),
      GetTransformString(properties, parent_inverse_transform).c_str());
  for (const auto& [id, value] : captured_css_properties) {
    group_rule_builder.AppendFormat(
        "%s: %s;\n",
        CSSProperty::Get(id).GetPropertyNameAtomicString().Utf8().c_str(),
        value.Utf8().c_str());
  }

  if (properties.box_geometry) {
    StringBuilder image_pair_rule_builder;
    image_pair_rule_builder.AppendFormat(
        "width: %.3fpx;\n"
        "height: %.3fpx;\n"
        "position: relative;\n"
        "display: block;\n"
        "inset: unset;\n",
        properties.box_geometry->content_box.Width().ToFloat(),
        properties.box_geometry->content_box.Height().ToFloat());
    AddRules(kImagePairTagName, tag, image_pair_rule_builder.ReleaseString());
  }
  AddRules(kGroupTagName, tag, group_rule_builder.ReleaseString());
}

}  // namespace blink
