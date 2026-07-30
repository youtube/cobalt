// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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


// Dictionary mapping string descriptions to media file descriptions in the
// form of [contentType, url, maxVideoCapabilities (for videos only), licenseUrl]
const MEDIA_FILES = {
  'av1_720p_60fps_drm': {
    contentType: 'video/mp4; codecs="av01.0.05M.08"',
    url: 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/av1-senc/sdr_720p60.mp4',
    maxVideoCapabilities: 'width=1280; height=720',
    licenseUrl: 'https://dash-mse-test.appspot.com/api/drm/widevine?drm_system=widevine&source=YOUTUBE&ip=0.0.0.0&ipbits=0&expire=19000000000&key=ik0&sparams=ip,ipbits,expire,drm_system,source,video_id&video_id=6508f99557a8385f&signature=5153900DAC410803EC269D252DAAA82BA6D8B825.495E631E406584A8EFCB4E9C9F3D45F6488B94E4',
  },
  // 40 MB
  'h264_720p_24fps_drm': {
    contentType: 'video/mp4; codecs="avc1.640028"',
    url: 'http://yt-dash-mse-test.commondatastorage.googleapis.com/media/oops_cenc-20121114-145-no-clear-start.mp4',
    maxVideoCapabilities: 'width=1280; height=720',
    licenseUrl: 'https://dash-mse-test.appspot.com/api/drm/widevine?drm_system=widevine&source=YOUTUBE&ip=0.0.0.0&ipbits=0&expire=19000000000&key=test_key1&sparams=ip,ipbits,expire,drm_system,source,video_id&video_id=03681262dc412c06&signature=9C4BE99E6F517B51FED1F0B3B31966D3C5DAB9D6.6A1F30BB35F3A39A4CA814B731450D4CBD198FFD',
  },
  // 38 MB
  'h264_720p_60fps_drm': {
    contentType: 'video/mp4; codecs="avc1.640028"',
    url: 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/drml3NoHdcp_h264_720p_60fps_cenc.mp4',
    maxVideoCapabilities: 'width=1280; height=720',
    licenseUrl: 'https://dash-mse-test.appspot.com/api/drm/widevine?drm_system=widevine&source=YOUTUBE&ip=0.0.0.0&ipbits=0&expire=19000000000&key=test_key1&sparams=ip,ipbits,expire,drm_system,source,video_id&video_id=03681262dc412c06&signature=9C4BE99E6F517B51FED1F0B3B31966D3C5DAB9D6.6A1F30BB35F3A39A4CA814B731450D4CBD198FFD',
  },
  // 32 MB
  'vp9_720p_60fps_drm': {
    contentType: 'video/webm; codecs="vp9"',
    url: 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/drml3NoHdcp_vp9_720p_60fps_enc.webm',
    maxVideoCapabilities: 'width=1280; height=720',
    licenseUrl: 'https://dash-mse-test.appspot.com/api/drm/widevine?drm_system=widevine&source=YOUTUBE&ip=0.0.0.0&ipbits=0&expire=19000000000&key=test_key1&sparams=ip,ipbits,expire,drm_system,source,video_id&video_id=f320151fa3f061b2&signature=81E7B33929F9F35922F7D2E96A5E7AC36F3218B2.673F553EE51A48438AE5E707AEC87A071B4FEF65'
  },
  // 1 MB
  // Mono won't work with tunnel mode on many devices.
  'aac_mono_drm': {
    contentType: 'audio/mp4; codecs="mp4a.40.2"',
    url: 'http://yt-dash-mse-test.commondatastorage.googleapis.com/media/oops_cenc-20121114-148.mp4',
    licenseUrl: 'https://dash-mse-test.appspot.com/api/drm/widevine?drm_system=widevine&source=YOUTUBE&ip=0.0.0.0&ipbits=0&expire=19000000000&key=test_key1&sparams=ip,ipbits,expire,drm_system,source,video_id&video_id=03681262dc412c06&signature=9C4BE99E6F517B51FED1F0B3B31966D3C5DAB9D6.6A1F30BB35F3A39A4CA814B731450D4CBD198FFD',
  },
  // 2.8 MB
  'aac_clear': {
    contentType: 'audio/mp4; codecs="mp4a.40.2"',
    url: 'http://yt-dash-mse-test.commondatastorage.googleapis.com/media/car-20120827-8c.mp4',
  },
  // 1.7 MB
  'opus_clear': {
    contentType: 'audio/webm; codecs="opus"',
    url: 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/car_opus_med.webm',
  },
};

mediaCache = {}

function fetchArrayBuffer(method, url, body, callback) {
  var xhr = new XMLHttpRequest();
  xhr.responseType = 'arraybuffer';
  xhr.addEventListener('load', function() {
    callback(xhr.response);
  });
  xhr.open(method, url);
  xhr.send(body);
}

async function fetchMediaData(mediaFileId) {
  if (mediaFileId in mediaCache) {
    return mediaCache[mediaFileId];
  }

  const response = await fetch(MEDIA_FILES[mediaFileId].url);
  mediaCache[mediaFileId] = await response.arrayBuffer();
  return mediaCache[mediaFileId];
}

function extractLicense(licenseArrayBuffer) {
  var licenseArray = new Uint8Array(licenseArrayBuffer);
  var licenseStartIndex = licenseArray.length - 2;
  while (licenseStartIndex >= 0) {
    if (licenseArray[licenseStartIndex] == 13 &&
        licenseArray[licenseStartIndex + 1] == 10) {
      licenseStartIndex += 2;
      break;
    }
    --licenseStartIndex;
  }

  return licenseArray.subarray(licenseStartIndex);
}

async function createMediaKeySystem(isPrimaryVideo, audioContentType, videoContentType) {
  const keySystems = isPrimaryVideo ? ['com.widevine.alpha'] : ['com.youtube.widevine.l3', 'com.widevine.alpha'];
  for (keySystem of keySystems) {
    try {
      mediaKeySystemAccess = await navigator.requestMediaKeySystemAccess(keySystem, [{
        'initDataTypes': ['cenc', 'webm'],
        'audioCapabilities': [{'contentType': audioContentType}],
        'videoCapabilities': [{'contentType': videoContentType}]}]);
      return mediaKeySystemAccess.createMediaKeys();
    } catch {
      console.log('create keySystem ' + keySystem + ' failed.')
      continue;
    }
  }
}

function createTunnelModeContentType(videoContentType, tunnelModeAttributeValue) {
  return videoContentType + '; tunnelmode=' + tunnelModeAttributeValue;
}

function isTunnelModeSupported(videoContentType) {
  if (!MediaSource.isTypeSupported(videoContentType)) {
    // If the content type isn't supported at all, it won't be supported in
    // tunnel mode.
    return false;
  }
  if (MediaSource.isTypeSupported(createTunnelModeContentType(videoContentType, 'invalid'))) {
    // The implementation doesn't understand the `tunnelmode` attribute.
    return false;
  }
  return MediaSource.isTypeSupported(createTunnelModeContentType(videoContentType, 'true'));
}

async function play(videoElementId, videoFileId, optionalAudioFileId) {
  const isPrimaryVideo = videoElementId == 'primary-video';

  videoContentType = MEDIA_FILES[videoFileId].contentType;
  if (isTunnelModeSupported(videoContentType)) {
    videoContentType = createTunnelModeContentType(videoContentType, 'true');
  }

  var mediaKeys = await createMediaKeySystem(isPrimaryVideo, optionalAudioFileId ? MEDIA_FILES[optionalAudioFileId].contentType : MEDIA_FILES['opus_clear'].contentType, videoContentType);
  var videoElement = document.getElementById(videoElementId);

  if (!isPrimaryVideo && videoElement.setMaxVideoCapabilities) {
    videoElement.setMaxVideoCapabilities(MEDIA_FILES[videoFileId].maxVideoCapabilities);
  }

  videoElement.setMediaKeys(mediaKeys);

  mediaKeySession = mediaKeys.createSession();
  var licenseServerUrl = MEDIA_FILES[videoFileId].licenseUrl;
  mediaKeySession.addEventListener('message', function(messageEvent) {
    fetchArrayBuffer('POST', licenseServerUrl, messageEvent.message,
      function(licenseArrayBuffer) {
        mediaKeySession.update(extractLicense(licenseArrayBuffer));
      });
  });

  videoElement.addEventListener('encrypted', function(encryptedEvent) {
    mediaKeySession.generateRequest(
        encryptedEvent.initDataType, encryptedEvent.initData);
  });

  var mediaSource = new MediaSource();
  mediaSource.addEventListener('sourceopen', async function() {
    var videoSourceBuffer = mediaSource.addSourceBuffer(videoContentType);
    var audioSourceBuffer;

    if (optionalAudioFileId) {
      audioSourceBuffer = mediaSource.addSourceBuffer(MEDIA_FILES[optionalAudioFileId].contentType);
    }

    var videoArrayBuffer = await fetchMediaData(videoFileId);
    videoSourceBuffer.appendBuffer(videoArrayBuffer);

    if (audioSourceBuffer) {
      var audioArrayBuffer = await fetchMediaData(optionalAudioFileId);
      audioSourceBuffer.appendBuffer(audioArrayBuffer);
    }
  });

  videoElement.src = URL.createObjectURL(mediaSource);
  videoElement.play();
}

function getGetParameters() {
  var parsedParameters = {};

  const urlComponents = window.location.href.split('?');
  if (urlComponents.length < 2) {
    return parsedParameters;
  }

  const query = urlComponents[1];
  const parameters = query.split('&');

  for (parameter of parameters) {
    const split = parameter.split('=');
    if (split.length == 0) {
      continue;
    }
    if (split.length == 1) {
      parsedParameters[split[0]] = '';
    } else {
      parsedParameters[split[0]] = split[1];
    }
  }

  return parsedParameters;
}

function populateMediaFileIds() {
  var mediaFileIds = [];
  const getParameters = getGetParameters();

  mediaFileIds['video0'] = getParameters['video0'] ?? 'vp9_720p_60fps_drm';
  mediaFileIds['video1'] = getParameters['video1'] ?? 'h264_720p_24fps_drm';
  mediaFileIds['video2'] = getParameters['video2'] ?? 'vp9_720p_60fps_drm';
  mediaFileIds['video3'] = getParameters['video3'] ?? 'h264_720p_24fps_drm';
  mediaFileIds['audio'] = getParameters['audio'] ?? 'opus_clear';

  return mediaFileIds;
}

async function prefetchMediaData(mediaFileIds) {
  for (mediaFileId of Object.keys(mediaFileIds)) {
    await fetchMediaData(mediaFileIds[mediaFileId]);
  }
}

async function main() {
  if (window.h5vcc && window.h5vcc.settings) {
    h5vcc.settings.set('MediaSource.EnableAvoidCopyingArrayBuffer', 1);
  }

  const mediaFileIds = populateMediaFileIds();
  await prefetchMediaData(mediaFileIds);

  play('primary-video', mediaFileIds['video0'], mediaFileIds['audio']);
  window.setTimeout(function() {
    play('secondary-video-1', mediaFileIds['video1']);
  }, 10000);
  window.setTimeout(function() {
    play('secondary-video-2', mediaFileIds['video2']);
  }, 20000);
  window.setTimeout(function() {
    play('secondary-video-3', mediaFileIds['video3']);
  }, 30000);
}


main();
