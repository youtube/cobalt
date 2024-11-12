// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "url/gurl.h"

namespace media_router {

namespace {

// If a friendly name does not exist, falls back to use model name + last 4
// digits of the UUID as the friendly name.
std::string ComputeFriendlyName(const std::string& unique_id,
                                const std::string& model_name) {
  if (model_name.empty() || unique_id.length() < 4)
    return std::string();

  std::string trimmed_unique_id = unique_id.substr(unique_id.length() - 4);
  return base::StringPrintf("%s [%s]", model_name.c_str(),
                            trimmed_unique_id.c_str());
}

void NotifyParsingError(SafeDialDeviceDescriptionParser::ParseCallback callback,
                        SafeDialDeviceDescriptionParser::ParsingResult error) {
  std::move(callback).Run(ParsedDialDeviceDescription(), error);
}

}  // namespace

SafeDialDeviceDescriptionParser::SafeDialDeviceDescriptionParser() = default;

SafeDialDeviceDescriptionParser::~SafeDialDeviceDescriptionParser() = default;

void SafeDialDeviceDescriptionParser::Parse(const std::string& xml_text,
                                            const GURL& app_url,
                                            ParseCallback callback) {
  DCHECK(callback);
  GetDataDecoder().ParseXml(
      xml_text, data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&SafeDialDeviceDescriptionParser::OnXmlParsingDone,
                     weak_factory_.GetWeakPtr(), std::move(callback), app_url));
}

void SafeDialDeviceDescriptionParser::OnXmlParsingDone(
    SafeDialDeviceDescriptionParser::ParseCallback callback,
    const GURL& app_url,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_dict()) {
    std::move(callback).Run(
        ParsedDialDeviceDescription(),
        SafeDialDeviceDescriptionParser::ParsingResult::kInvalidXml);
    return;
  }

  bool unique_device = true;
  const base::Value* device_element = data_decoder::FindXmlElementPath(
      *result, {"root", "device"}, &unique_device);
  if (!device_element) {
    NotifyParsingError(
        std::move(callback),
        SafeDialDeviceDescriptionParser::ParsingResult::kInvalidXml);
    return;
  }
  DCHECK(unique_device);

  ParsedDialDeviceDescription device_description;
  static constexpr size_t kArraySize = 4U;
  static constexpr std::array<const char* const, kArraySize> kNodeNames{
      {"UDN", "friendlyName", "modelName", "deviceType"}};
  static constexpr std::array<ParsingResult, kArraySize> kParsingErrors{
      {ParsingResult::kFailedToReadUdn,
       ParsingResult::kFailedToReadFriendlyName,
       ParsingResult::kFailedToReadModelName,
       ParsingResult::kFailedToReadDeviceType}};
  const std::array<std::string*, kArraySize> kFields{
      {&device_description.unique_id, &device_description.friendly_name,
       &device_description.model_name, &device_description.device_type}};

  for (size_t i = 0; i < kArraySize; i++) {
    const base::Value* value =
        data_decoder::GetXmlElementChildWithTag(*device_element, kNodeNames[i]);
    if (value) {
      DCHECK_EQ(1, data_decoder::GetXmlElementChildrenCount(*device_element,
                                                            kNodeNames[i]));
      bool parsed = data_decoder::GetXmlElementText(*value, kFields[i]);
      if (!parsed) {
        NotifyParsingError(std::move(callback), kParsingErrors[i]);
        return;
      }
    }
  }

  if (device_description.friendly_name.empty()) {
    device_description.friendly_name = ComputeFriendlyName(
        device_description.unique_id, device_description.model_name);
  }

  device_description.app_url = app_url;

  std::move(callback).Run(device_description, ParsingResult::kSuccess);
}

}  // namespace media_router
