// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

function getRepresentation(keySystem, audioMime, videoMime, encryptionScheme) {
  var representation = keySystem;
  if (typeof audioMime !== 'undefined') {
    representation += ', ' + audioMime;
  }
  if (typeof videoMime !== 'undefined') {
    representation += ', ' + videoMime;
  }
  if (typeof encryptionScheme !== 'undefined') {
    representation += ', encryptionscheme="' + encryptionScheme + '"';
  }
  return representation;
}

function checkForSupport(keySystem, audioMime, videoMime, encryptionScheme,
                         expectedResult) {
  var configs = [{
    initDataTypes: ['cenc', 'sinf', 'webm'],
    audioCapabilities: [],
    videoCapabilities: [],
  }];
  if (typeof audioMime !== 'undefined') {
    configs[0].audioCapabilities.push(
        {contentType: audioMime, encryptionScheme: encryptionScheme});
  }
  if (typeof videoMime !== 'undefined') {
    configs[0].videoCapabilities.push(
        {contentType: videoMime, encryptionScheme: encryptionScheme});
  }
  var representation = getRepresentation(keySystem, audioMime, videoMime,
                                         encryptionScheme);
  navigator.requestMediaKeySystemAccess(keySystem, configs)
      .then(onKeySystemAccess.bind(this, representation, expectedResult),
            onFailure.bind(this, representation, expectedResult));
}

function addQueryResult(query, result, expectedResult) {
  var row = document.createElement('div');

  var cell = document.createElement('span');
  cell.className = 'query';
  cell.textContent = query + ' => ';
  row.appendChild(cell);

  cell = document.createElement('span');
  cell.className = result == expectedResult ? 'result-success' :
                                              'result-failure';
  cell.textContent = result;
  row.appendChild(cell);

  document.body.appendChild(row);
}

function onKeySystemAccess(representation, expectedResult, keySystemAccess) {
  addQueryResult(representation, 'supported', expectedResult);
}

function onFailure(representation, expectedResult) {
  addQueryResult(representation, 'not supported', expectedResult);
}

checkForSupport('com.widevine.alpha.invalid', 'audio/mp4; codecs="mp4a.40.2"',
                undefined, undefined, 'not supported');
checkForSupport('com.widevine.alpha.invalid', undefined,
                'video/webm; codecs="vp9"', undefined, 'not supported');

// 'invalid-scheme' is not a valid scheme.
checkForSupport('com.widevine.alpha', 'audio/mp4; codecs="mp4a.40.2"',
                undefined, 'invalid-scheme', 'not supported');
checkForSupport('com.widevine.alpha', undefined, 'video/webm; codecs="vp9"',
                'invalid-scheme', 'not supported');

checkForSupport('com.widevine.alpha', 'audio/mp4; codecs="mp4a.40.2"',
                undefined, undefined, 'supported');
checkForSupport('com.widevine.alpha', undefined, 'video/webm; codecs="vp9"',
                undefined, 'supported');

// Empty string is not a valid scheme.
checkForSupport('com.widevine.alpha', 'audio/mp4; codecs="mp4a.40.2"',
                undefined, '', 'not supported');
checkForSupport('com.widevine.alpha', undefined, 'video/webm; codecs="vp9"',
                '', 'not supported');

checkForSupport('com.widevine.alpha', 'audio/mp4; codecs="mp4a.40.2"',
                undefined, 'cenc', 'supported');
checkForSupport('com.widevine.alpha', undefined, 'video/webm; codecs="vp9"',
                'cenc', 'supported');

checkForSupport('com.widevine.alpha', 'audio/mp4; codecs="mp4a.40.2"',
                undefined, 'cbcs', 'supported');
checkForSupport('com.widevine.alpha', undefined, 'video/webm; codecs="vp9"',
                'cbcs', 'supported');

checkForSupport('com.widevine.alpha', 'audio/mp4; codecs="mp4a.40.2"',
                 undefined, 'cbcs-1-9', 'supported');
checkForSupport('com.widevine.alpha', undefined, 'video/webm; codecs="vp9"',
                'cbcs-1-9', 'supported');
