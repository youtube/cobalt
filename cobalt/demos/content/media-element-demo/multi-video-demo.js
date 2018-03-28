// Copyright 2018 Google Inc. All Rights Reserved.
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

// The demo simply plays a video using get parameters in the form of:
//   .../multi-video-demo.html?audio=filename_in_same_folder&video=filename_in_same_folder&instances=2

var kAudioChunkSize = 512 * 1024;
var kVideoChunkSize = 2 * 1024 * 1024;

var kEndOfStreamOffset = -1;

class Player {
  constructor(audio_url, video_url) {
    this.audio_offset = 0;
    this.audio_url = audio_url;
    this.video_offset = 0;
    this.video_url = video_url;
    this.video_tag = this.createVideoElement();
    this.status = this.createStatusElement(this.video_tag);

    this.video_tag.src = '';
    this.video_tag.load();
    this.mediasource = new MediaSource;
    this.mediasource.addEventListener('sourceopen', this.onsourceopen.bind(this));
    this.video_tag.src = window.URL.createObjectURL(this.mediasource);
  }

  onsourceopen() {
    if (this.video_url.endsWith('.mp4')) {
        this.video_source_buffer = this.mediasource.addSourceBuffer(
                                       'video/mp4; codecs="avc1.640028"');
      } else {
        this.video_source_buffer = this.mediasource.addSourceBuffer(
                                       'video/webm; codecs="vp9"');
      }

      this.audio_source_buffer = this.mediasource.addSourceBuffer(
                                     'audio/mp4; codecs="mp4a.40.2"');
      this.tryToDownloadAudioData();
  }

  downloadAndAppend(url, begin, end, source_buffer, callback) {
    var xhr = new XMLHttpRequest;
    xhr.open('GET', url, true);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('load', function(e) {
      var data = new Uint8Array(e.target.response);
      var onupdateend = function() {
        source_buffer.removeEventListener('updateend', onupdateend);
        callback(data.length);
      };
      source_buffer.addEventListener('updateend', onupdateend);
      source_buffer.appendBuffer(data);
      console.log('append ' + data.length + ' bytes from '
                  + url);
    });
    xhr.setRequestHeader('Range', ('bytes=' + begin +'-' + end));
    xhr.send();
  }

  isTrackNeedAudioData() {
    if (this.audio_offset == kEndOfStreamOffset) {
      return false;
    }
    var buffer_range_size = this.audio_source_buffer.buffered.length;
    if (buffer_range_size == 0) {
      return true;
    }
    var start = this.audio_source_buffer.buffered.start(buffer_range_size - 1);
    var end = this.audio_source_buffer.buffered.end(buffer_range_size - 1);
    console.log('audio ' + start + '/' + end + ' ' + this.video_tag.currentTime)
    return end - this.video_tag.currentTime <= 20;
  }

  isTrackNeedVideoData() {
    if (this.video_offset == kEndOfStreamOffset) {
      return false;
    }
    var buffer_range_size = this.video_source_buffer.buffered.length;
    if (buffer_range_size == 0) {
      return true;
    }
    var start = this.video_source_buffer.buffered.start(buffer_range_size - 1);
    var end = this.video_source_buffer.buffered.end(buffer_range_size - 1);
    console.log('video ' + start + '/' + end + ' ' + this.video_tag.currentTime)
    return end - this.video_tag.currentTime <= 20;
  }

  tryToDownloadAudioData() {
    if (!this.isTrackNeedAudioData()) {
      if (this.isTrackNeedVideoData()) {
        this.tryToDownloadVideoData();
      } else {
        window.setTimeout(this.tryToDownloadAudioData.bind(this), 1000);
      }
      return;
    }

    this.downloadAndAppend(
        this.audio_url, this.audio_offset,
        this.audio_offset + kAudioChunkSize - 1, this.audio_source_buffer,
        this.onAudioDataDownloaded.bind(this));
  }

  onAudioDataDownloaded(length) {
    if (length != kAudioChunkSize) {
      this.audio_offset = kEndOfStreamOffset;
    }
    if (this.audio_offset != kEndOfStreamOffset) {
      this.audio_offset += kAudioChunkSize;
    }
    this.tryToDownloadVideoData();
  }

  tryToDownloadVideoData() {
    if (!this.isTrackNeedVideoData()) {
      if (this.isTrackNeedAudioData()) {
        this.tryToDownloadAudioData();
      } else {
        window.setTimeout(this.tryToDownloadVideoData.bind(this), 1000);
      }
      return;
    }

    this.downloadAndAppend(
        this.video_url, this.video_offset,
        this.video_offset + kVideoChunkSize - 1, this.video_source_buffer,
        this.onVideoDataDownloaded.bind(this));
  }

  onVideoDataDownloaded(length) {
    if (length != kVideoChunkSize) {
      this.video_offset = kEndOfStreamOffset;
    }
    if (this.video_offset != kEndOfStreamOffset) {
      this.video_offset += kVideoChunkSize;
    }
    this.tryToDownloadAudioData();
  }

  createVideoElement() {
    var video = document.createElement('video');
    video.autoplay = true;
    video.style.width = '320px';
    video.style.height = '240px';
    document.body.appendChild(video);

    return video;
  }

  createStatusElement(video) {
    var status = document.createElement('div');
    document.body.appendChild(status);
    video.addEventListener('timeupdate', function () {
      status.textContent = 'time: ' + video.currentTime.toFixed(2);
    });

    return status;
  }
}

function main() {
  var get_parameters = window.location.search.substr(1).split('&');
  var audio_url, video_url, instances = 2;
  for (var param of get_parameters) {
    splitted = param.split('=');
    if (splitted[0] == 'audio') {
      audio_url = splitted[1];
    } else if (splitted[0] == 'video') {
      video_url = splitted[1];
    } else if (splitted[0] == 'instances') {
      instances = splitted[1];
    }
  }

  if (audio_url && video_url) {
    for (var i = 0; i < instances; ++i) {
      new Player(audio_url, video_url);
    }
  } else {
    status.textContent = "invalid get parameters " +
                         window.location.search.substr(1);
  }
}

main();
