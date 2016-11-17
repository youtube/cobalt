/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_SWITCHES_H_
#define COBALT_BROWSER_SWITCHES_H_

namespace cobalt {
namespace browser {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
extern const char kAllowHttp[];
extern const char kAudioDecoderStub[];
extern const char kCspMode[];
extern const char kDebugConsoleMode[];
extern const char kDisableWebmVp9[];
extern const char kEnableWebDriver[];
extern const char kExtraWebFileDir[];
extern const char kFakeMicrophone[];
extern const char kIgnoreCertificateErrors[];
extern const char kInputFuzzer[];
extern const char kMinLogLevel[];
extern const char kNullAudioStreamer[];
extern const char kNullSavegame[];
extern const char kPartialLayout[];
extern const char kProxy[];
extern const char kRemoteDebuggingPort[];
extern const char kShutdownAfter[];
extern const char kStubImageDecoder[];
extern const char kTimedTrace[];
extern const char kVideoContainerSizeOverride[];
extern const char kVideoDecoderStub[];
extern const char kWebDriverPort[];
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

extern const char kImageCacheSizeInBytes[];
extern const char kInitialURL[];
extern const char kRemoteTypefaceCacheSizeInBytes[];
extern const char kScratchSurfaceCacheSizeInBytes[];
extern const char kSkiaCacheSizeInBytes[];
extern const char kSoftwareSurfaceCacheSizeInBytes[];
extern const char kSurfaceCacheSizeInBytes[];
extern const char kViewport[];

}  // namespace switches
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SWITCHES_H_
