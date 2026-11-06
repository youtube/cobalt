// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/splash/splash_wasm_selector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "cobalt/shell/browser/splash/splash_wasm_runner.h"
#include "cobalt/shell/common/shell_switches.h"

namespace content {

SplashSelection::SplashSelection() = default;
SplashSelection::SplashSelection(SplashSelection&&) = default;
SplashSelection& SplashSelection::operator=(SplashSelection&&) = default;
SplashSelection::~SplashSelection() = default;

SplashWasmSelector::SplashWasmSelector(BrowserContext* browser_context,
                                       const GURL& splash_origin,
                                       std::u16string cache_name)
    : reader_(browser_context, splash_origin, std::move(cache_name)) {}

SplashWasmSelector::~SplashWasmSelector() = default;

// static
std::string SplashWasmSelector::BuildInputJson(const SplashSelectorInput& input,
                                               base::Time now) {
  base::Value::Dict dict;
  dict.Set("abi_version", static_cast<int>(kSplashSelectorAbiVersion));
  dict.Set("now_ms", now.InMillisecondsFSinceUnixEpoch());
  dict.Set("locale", input.locale);
  dict.Set("topic", input.topic);
  dict.Set("screen_width", input.screen_width);
  dict.Set("screen_height", input.screen_height);
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

void SplashWasmSelector::Select(const SplashSelectorInput& input,
                                Callback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  selection_ = SplashSelection();
  input_json_ = BuildInputJson(input, base::Time::Now());
  reader_.Read(
      kSelectorKey, kMaxSelectorSize,
      base::BindOnce(&SplashWasmSelector::OnSelectorRead,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void SplashWasmSelector::OnSelectorRead(
    base::TimeTicks start,
    base::expected<std::vector<uint8_t>, SplashCacheReader::Error> result) {
  selection_.read_selector_time = base::TimeTicks::Now() - start;
  if (!result.has_value()) {
    selection_.selector_error =
        std::string("Selector ") +
        SplashCacheReader::ErrorToString(result.error());
    return ReadVideo(kFallbackKey);
  }
  const base::TimeTicks run_start = base::TimeTicks::Now();
  base::expected<std::string, std::string> key =
      RunSplashSelectorWasm(result.value(), input_json_, instruction_limit_);
  selection_.run_selector_time = base::TimeTicks::Now() - run_start;
  if (!key.has_value()) {
    selection_.selector_error = key.error();
    return ReadVideo(kFallbackKey);
  }
  selection_.selected_by_wasm = true;
  ReadVideo(key.value());
}

void SplashWasmSelector::ReadVideo(const std::string& key) {
  reader_.Read(
      key, switches::kMaxSplashContentSize,
      base::BindOnce(&SplashWasmSelector::OnVideoRead,
                     weak_factory_.GetWeakPtr(), key, base::TimeTicks::Now()));
}

void SplashWasmSelector::OnVideoRead(
    const std::string& key,
    base::TimeTicks start,
    base::expected<std::vector<uint8_t>, SplashCacheReader::Error> result) {
  selection_.read_video_time += base::TimeTicks::Now() - start;
  if (result.has_value()) {
    selection_.key = key;
    selection_.video = std::move(result.value());
    return Finish();
  }
  LOG(WARNING) << "Splash video " << key << ": "
               << SplashCacheReader::ErrorToString(result.error());
  if (key != kFallbackKey) {
    if (selection_.selected_by_wasm) {
      selection_.selected_by_wasm = false;
      selection_.selector_error = "Selected video " + key + " not found";
    }
    return ReadVideo(kFallbackKey);
  }
  Finish();
}

void SplashWasmSelector::Finish() {
  LOG(INFO) << "Splash selection: key='" << selection_.key << "' ("
            << selection_.video.size()
            << " bytes), by_wasm=" << selection_.selected_by_wasm
            << ", selector_error='" << selection_.selector_error
            << "', read_selector=" << selection_.read_selector_time
            << ", run_selector=" << selection_.run_selector_time
            << ", read_video=" << selection_.read_video_time;
  std::move(callback_).Run(std::move(selection_));
}

}  // namespace content
