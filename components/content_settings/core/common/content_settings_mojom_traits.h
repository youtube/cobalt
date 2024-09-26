// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_MOJOM_TRAITS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_MOJOM_TRAITS_H_

#include <string>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings.mojom-shared.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content_settings::mojom::PatternPartsDataView,
                    ContentSettingsPattern::PatternParts> {
  static const std::string& scheme(
      const ContentSettingsPattern::PatternParts& r) {
    return r.scheme;
  }

  static bool is_scheme_wildcard(
      const ContentSettingsPattern::PatternParts& r) {
    return r.is_scheme_wildcard;
  }

  static const std::string& host(
      const ContentSettingsPattern::PatternParts& r) {
    return r.host;
  }

  static bool has_domain_wildcard(
      const ContentSettingsPattern::PatternParts& r) {
    return r.has_domain_wildcard;
  }

  static const std::string& port(
      const ContentSettingsPattern::PatternParts& r) {
    return r.port;
  }

  static bool is_port_wildcard(const ContentSettingsPattern::PatternParts& r) {
    return r.is_port_wildcard;
  }

  static const std::string& path(
      const ContentSettingsPattern::PatternParts& r) {
    return r.path;
  }

  static bool is_path_wildcard(const ContentSettingsPattern::PatternParts& r) {
    return r.is_path_wildcard;
  }

  static bool Read(content_settings::mojom::PatternPartsDataView data,
                   ContentSettingsPattern::PatternParts* out);
};

template <>
struct StructTraits<content_settings::mojom::ContentSettingsPatternDataView,
                    ContentSettingsPattern> {
  static const ContentSettingsPattern::PatternParts& parts(
      const ContentSettingsPattern& r) {
    return r.parts_;
  }

  static bool is_valid(const ContentSettingsPattern& r) { return r.is_valid_; }

  static bool Read(content_settings::mojom::ContentSettingsPatternDataView data,
                   ContentSettingsPattern* out);
};

template <>
struct EnumTraits<content_settings::mojom::ContentSetting, ContentSetting> {
  static content_settings::mojom::ContentSetting ToMojom(
      ContentSetting setting);

  static bool FromMojom(content_settings::mojom::ContentSetting setting,
                        ContentSetting* out);
};

template <>
struct StructTraits<
    content_settings::mojom::ContentSettingPatternSourceDataView,
    ContentSettingPatternSource> {
  static const ContentSettingsPattern& primary_pattern(
      const ContentSettingPatternSource& r) {
    return r.primary_pattern;
  }

  static const ContentSettingsPattern& secondary_pattern(
      const ContentSettingPatternSource& r) {
    return r.secondary_pattern;
  }

  static const base::Value& setting_value(
      const ContentSettingPatternSource& r) {
    return r.setting_value;
  }

  static const base::Time& expiration(const ContentSettingPatternSource& r) {
    return r.metadata.expiration;
  }

  static const std::string& source(const ContentSettingPatternSource& r) {
    return r.source;
  }

  static bool incognito(const ContentSettingPatternSource& r) {
    return r.incognito;
  }

  static bool Read(
      content_settings::mojom::ContentSettingPatternSourceDataView data,
      ContentSettingPatternSource* out);
};

template <>
struct StructTraits<
    content_settings::mojom::RendererContentSettingRulesDataView,
    RendererContentSettingRules> {
  static const std::vector<ContentSettingPatternSource>& image_rules(
      const RendererContentSettingRules& r) {
    return r.image_rules;
  }

  static const std::vector<ContentSettingPatternSource>& script_rules(
      const RendererContentSettingRules& r) {
    return r.script_rules;
  }

  static const std::vector<ContentSettingPatternSource>& popup_redirect_rules(
      const RendererContentSettingRules& r) {
    return r.popup_redirect_rules;
  }

  static const std::vector<ContentSettingPatternSource>& mixed_content_rules(
      const RendererContentSettingRules& r) {
    return r.mixed_content_rules;
  }

  static const std::vector<ContentSettingPatternSource>&
  auto_dark_content_rules(const RendererContentSettingRules& r) {
    return r.auto_dark_content_rules;
  }

  static bool Read(
      content_settings::mojom::RendererContentSettingRulesDataView data,
      RendererContentSettingRules* out);
};

}  // namespace mojo

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_MOJOM_TRAITS_H_
