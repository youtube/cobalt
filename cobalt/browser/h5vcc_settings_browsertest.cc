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

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

// The H5VCC media settings take effect only with Starboard media.
#if BUILDFLAG(USE_STARBOARD_MEDIA)
namespace {

// Evaluates to true if the page has the MediaSource API that it uses to switch
// between SourceBuffers.
constexpr char kHasChangeTo[] = "'changeTo' in MediaSource.prototype";

// Loads the first of two items, buffers the second in standby SourceBuffers,
// and switches to it with MediaSource.changeTo(). Evaluates to "OK", or throws.
constexpr char kChangeToPlayback[] = R"JS(
(async () => {
'use strict';
const video = document.createElement('video');
document.body.appendChild(video);
const AUDIO_TYPE = 'audio/webm; codecs="opus"';
const VIDEO_TYPE = 'video/webm; codecs="vp9"';
// The second item starts here, so the switch shows that playback starts over
// where its media starts.
const SECOND_START = 5;

function check(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

async function fetchMedia(file) {
  const response = await fetch(file);
  return new Uint8Array(await response.arrayBuffer());
}

function append(sourceBuffer, data) {
  return new Promise((resolve, reject) => {
    sourceBuffer.addEventListener('updateend', resolve, {once: true});
    sourceBuffer.addEventListener('error', reject, {once: true});
    sourceBuffer.appendBuffer(data);
  });
}

function waitFor(predicate, message) {
  return new Promise((resolve, reject) => {
    const deadline = performance.now() + 10000;
    const poll = () => {
      if (predicate()) {
        resolve();
      } else if (performance.now() > deadline) {
        reject(new Error('Timed out waiting for ' + message));
      } else {
        setTimeout(poll, 20);
      }
    };
    poll();
  });
}

function activeSourceBuffers(mediaSource) {
  return Array.from({length: mediaSource.activeSourceBuffers.length},
                    (_, i) => mediaSource.activeSourceBuffers[i]);
}

async function run() {
  await h5vcc.settings.set('Media.EnableMultiSourceBufferSwitching', 1);
  check('changeTo' in MediaSource.prototype, 'changeTo() is not exposed');

  const mediaSource = new MediaSource();
  video.src = URL.createObjectURL(mediaSource);
  await new Promise(
      resolve => mediaSource.addEventListener('sourceopen', resolve,
                                              {once: true}));
  const audioData = await fetchMedia('/bear-opus.webm');
  const videoData = await fetchMedia('/bear-vp9.webm');

  // The first item plays now.
  const audio1 = mediaSource.addSourceBuffer(AUDIO_TYPE);
  const video1 = mediaSource.addSourceBuffer(VIDEO_TYPE);
  await append(audio1, audioData);
  await append(video1, videoData);

  // The second item buffers in standby SourceBuffers.
  const audio2 = mediaSource.addSourceBuffer(AUDIO_TYPE, {standby: true});
  const video2 = mediaSource.addSourceBuffer(VIDEO_TYPE, {standby: true});
  audio2.timestampOffset = SECOND_START;
  video2.timestampOffset = SECOND_START;
  await append(audio2, audioData);
  await append(video2, videoData);
  let active = activeSourceBuffers(mediaSource);
  check(active.length == 2 && active.includes(audio1) &&
            active.includes(video1),
        'Standby SourceBuffers are active');

  // The browser test environment doesn't render video, so the test switches
  // while paused, once the pipeline has started.
  await waitFor(() => video.readyState >= HTMLMediaElement.HAVE_METADATA,
                'the first item to load');

  // Two SourceBuffers of the same type can't be changed to.
  let rejected = false;
  try {
    await mediaSource.changeTo([video1, video2]);
  } catch (e) {
    rejected = e.name == 'NotSupportedError';
  }
  check(rejected, 'changeTo() with two video SourceBuffers did not reject');

  // Switch to the second item.
  await mediaSource.changeTo([audio2, video2]);
  active = activeSourceBuffers(mediaSource);
  check(active.length == 2 && active.includes(audio2) &&
            active.includes(video2),
        'activeSourceBuffers was not updated');

  // The SourceBuffers switched away from are read-only.
  let threw = false;
  try {
    audio1.appendBuffer(audioData);
  } catch (e) {
    threw = e.name == 'InvalidStateError';
  }
  check(threw, 'appendBuffer() on a switched-away SourceBuffer did not throw');

  // Playback starts over where the second item's media starts: changeTo()
  // seeks there. (The seek doesn't complete here: the browser test environment
  // doesn't preroll video.)
  check(video.seeking, 'changeTo() did not seek');
  check(video.currentTime >= SECOND_START && video.currentTime < SECOND_START + 1,
        'Playback did not start over at the second item: ' + video.currentTime);
  check(!video.error, 'The video element has an error');

  // The SourceBuffers switched away from can be removed.
  mediaSource.removeSourceBuffer(audio1);
  mediaSource.removeSourceBuffer(video1);
  return 'OK';
}return await run();
})()
)JS";

}  // namespace

using H5vccSettingsBrowserTest = content::ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(H5vccSettingsBrowserTest,
                       MultiSourceBufferSwitchingExposesChangeTo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(), kHasChangeTo).ExtractBool());

  // The setting exposes changeTo() on the current page.
  EXPECT_TRUE(content::EvalJs(
                  shell()->web_contents(),
                  base::StrCat({"h5vcc.settings.set("
                                "'Media.EnableMultiSourceBufferSwitching', 1)"
                                ".then(() => ",
                                kHasChangeTo, ")"}))
                  .ExtractBool());

  // And on pages loaded afterwards.
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/title2.html")));
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(), kHasChangeTo).ExtractBool());
}

// With the setting on, a page can buffer the next item in standby
// SourceBuffers and switch to it with MediaSource.changeTo(). Playback starts
// over where the next item's media starts.
IN_PROC_BROWSER_TEST_F(H5vccSettingsBrowserTest,
                       MultiSourceBufferSwitchingChangeTo) {
  // The media test files, in codecs that every Starboard platform decodes.
  embedded_test_server()->ServeFilesFromSourceDirectory("media/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(content::EvalJs(shell()->web_contents(), kChangeToPlayback), "OK");
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace cobalt
