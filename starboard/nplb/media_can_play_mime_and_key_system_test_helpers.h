// Copyright 2021 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_NPLB_MEDIA_CAN_PLAY_MIME_AND_KEY_SYSTEM_TEST_HELPERS_H_
#define STARBOARD_NPLB_MEDIA_CAN_PLAY_MIME_AND_KEY_SYSTEM_TEST_HELPERS_H_

namespace starboard {
namespace nplb {

struct SbMediaCanPlayMimeAndKeySystemParam {
  const char* mime;
  const char* key_system;
};

static SbMediaCanPlayMimeAndKeySystemParam kWarmupQueryParams[] = {
    {"audio/mp4; codecs=\"mp4a.40.2\"", ""},
    {"audio/webm; codecs=\"opus\"", ""},
    {"video/webm; codecs=\"avc1.64002a\"", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\";", ""},
    {"video/webm; codecs=\"av01.0.09M.08\"", ""},
    {"video/webm; codecs=\"av01.0.17M.10.0.110.09.16.09.0\"", ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"", "com.widevine.alpha"},
    {"video/webm; codecs=\"avc1.64002a\"", "com.widevine.alpha"},
};

// Query params from https://youtu.be/iXvy8ZeCs5M.
static SbMediaCanPlayMimeAndKeySystemParam kSdrQueryParams[] = {
    {"video/mp4; codecs=\"avc1.42001E\"", ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"", ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"", ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.99.99.00\"", ""},
    {"audio/webm; codecs=\"opus\"", ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"audio/webm; codecs=\"opus\"; channels=99", ""},
    {"video/mp4; codecs=av01.0.05M.08", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; width=640", ""},
    {"video/webm; codecs=\"vp9\"; width=99999", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; height=360", ""},
    {"video/webm; codecs=\"vp9\"; height=99999", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; framerate=30", ""},
    {"video/webm; codecs=\"vp9\"; framerate=9999", ""},
    {"video/webm; codecs=\"vp9\"; width=3840; height=2160; bitrate=2000000",
     ""},
    {"video/webm; codecs=\"vp9\"; width=3840; height=2160; bitrate=20000000",
     ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; bitrate=300000", ""},
    {"video/webm; codecs=\"vp9\"; bitrate=2000000000", ""},
    {"video/mp4; codecs=\"avc1.4d4015\"; width=426; height=240; framerate=24; "
     "bitrate=233713",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=640; height=360; framerate=24; "
     "bitrate=422012",
     ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d400c\"; width=256; height=144; framerate=24; "
     "bitrate=110487",
     ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; eotf=bt709", ""},
    {"video/webm; codecs=\"vp9\"; eotf=catavision", ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=426; "
     "height=240; framerate=24; bitrate=191916; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=640; "
     "height=360; framerate=24; bitrate=400973; eotf=bt709",
     ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"video/mp4; codecs=\"av01.0.00M.08\"; width=256; height=144; "
     "framerate=24; "
     "bitrate=76146; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.00M.08\"; width=426; height=240; "
     "framerate=24; "
     "bitrate=156234; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.01M.08\"; width=640; height=360; "
     "framerate=24; "
     "bitrate=302046; eotf=bt709",
     ""},
    {"audio/webm; codecs=\"opus\"", ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"", ""},
};

// Query params from https://youtu.be/1La4QzGeaaQ.
static SbMediaCanPlayMimeAndKeySystemParam kHdrQueryParams[] = {
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"", ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.99.99.00\"", ""},
    {"audio/webm; codecs=\"opus\"", ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"audio/webm; codecs=\"opus\"; channels=99", ""},
    {"video/mp4; codecs=av01.0.05M.08", ""},
    {"video/mp4; codecs=av99.0.05M.08", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; height=360", ""},
    {"video/webm; codecs=\"vp9\"; height=99999", ""},
    {"video/webm; codecs=\"vp9\"; width=3840; height=2160; bitrate=2000000",
     ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; bitrate=300000", ""},
    {"video/webm; codecs=\"vp9\"; bitrate=2000000000", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; width=640", ""},
    {"video/webm; codecs=\"vp9\"; width=99999", ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; framerate=30", ""},
    {"video/webm; codecs=\"vp9\"; framerate=9999", ""},
    {"video/mp4; codecs=\"avc1.4d4015\"; width=426; height=240; framerate=30; "
     "bitrate=296736",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=640; height=360; framerate=30; "
     "bitrate=700126",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=854; height=480; framerate=30; "
     "bitrate=1357113",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=30; "
     "bitrate=2723992",
     ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d400c\"; width=256; height=144; framerate=30; "
     "bitrate=123753",
     ""},
    {"video/webm; codecs=\"vp9\"", ""},
    {"video/webm; codecs=\"vp9\"; eotf=bt709", ""},
    {"video/webm; codecs=\"vp9\"; eotf=catavision", ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=426; "
     "height=240; framerate=30; bitrate=202710; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=640; "
     "height=360; framerate=30; bitrate=427339; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=30; bitrate=782821; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=30; bitrate=1542503; eotf=bt709",
     ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d4020\"; width=1280; height=720; framerate=60; "
     "bitrate=3488936",
     ""},
    {"video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
     "framerate=60; "
     "bitrate=5833750",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=60; bitrate=2676194; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=60; bitrate=4461346; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=2560; "
     "height=1440; framerate=60; bitrate=13384663; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=3840; "
     "height=2160; framerate=60; bitrate=26752474; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=256; "
     "height=144; framerate=60; bitrate=245561",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=426; "
     "height=240; framerate=60; bitrate=500223",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=640; "
     "height=360; framerate=60; bitrate=1064485",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=854; "
     "height=480; framerate=60; bitrate=1998847",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=1280; "
     "height=720; framerate=60; bitrate=4556353",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=1920; "
     "height=1080; framerate=60; bitrate=6946958",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=2560; "
     "height=1440; framerate=60; bitrate=16930005",
     ""},
    {"video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=3840; "
     "height=2160; framerate=60; bitrate=30184402",
     ""},
    {"video/mp4; codecs=\"av01.0.00M.10.0.110.09.16.09.0\"; width=256; "
     "height=144; framerate=30; bitrate=89195; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.00M.10.0.110.09.16.09.0\"; width=426; "
     "height=240; framerate=30; bitrate=172861; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.01M.10.0.110.09.16.09.0\"; width=640; "
     "height=360; framerate=30; bitrate=369517; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.04M.10.0.110.09.16.09.0\"; width=854; "
     "height=480; framerate=30; bitrate=695606; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.08M.10.0.110.09.16.09.0\"; width=1280; "
     "height=720; framerate=60; bitrate=2017563; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.09M.10.0.110.09.16.09.0\"; width=1920; "
     "height=1080; framerate=60; bitrate=3755257; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.12M.10.0.110.09.16.09.0\"; width=2560; "
     "height=1440; framerate=60; bitrate=8546165; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.13M.10.0.110.09.16.09.0\"; width=3840; "
     "height=2160; framerate=60; bitrate=17537773; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.17M.10.0.110.09.16.09.0\"; width=7680; "
     "height=4320; framerate=60; bitrate=37270368; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.00M.10.0.110.09.16.09.0\"; width=256; "
     "height=144; framerate=60; bitrate=193907; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.01M.10.0.110.09.16.09.0\"; width=426; "
     "height=240; framerate=60; bitrate=400353; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.04M.10.0.110.09.16.09.0\"; width=640; "
     "height=360; framerate=60; bitrate=817812; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.05M.10.0.110.09.16.09.0\"; width=854; "
     "height=480; framerate=60; bitrate=1558025; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.08M.10.0.110.09.16.09.0\"; width=1280; "
     "height=720; framerate=60; bitrate=4167668; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.09M.10.0.110.09.16.09.0\"; width=1920; "
     "height=1080; framerate=60; bitrate=6870811; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.12M.10.0.110.09.16.09.0\"; width=2560; "
     "height=1440; framerate=60; bitrate=17316706; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.13M.10.0.110.09.16.09.0\"; width=3840; "
     "height=2160; framerate=60; bitrate=31942925; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.17M.10.0.110.09.16.09.0\"; width=7680; "
     "height=4320; framerate=60; bitrate=66038840; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"av01.0.17M.10.0.110.09.16.09.0\"; width=7680; "
     "height=4320; framerate=60; bitrate=45923436; eotf=smpte2084",
     ""},
    {"video/mp4; codecs=\"avc1.4d4015\"; width=426; height=240; framerate=24; "
     "bitrate=160590",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=640; height=360; framerate=24; "
     "bitrate=255156",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=490890",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=24; "
     "bitrate=1000556",
     ""},
    {"video/mp4; codecs=\"avc1.640028\"; width=1920; height=1080; "
     "framerate=24; "
     "bitrate=1810004",
     ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d400c\"; width=256; height=144; framerate=24; "
     "bitrate=82746",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=426; "
     "height=240; framerate=24; bitrate=178701; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=640; "
     "height=360; framerate=24; bitrate=371303; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=579918; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=999223; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=1814623; eotf=bt709",
     ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
};

// Query params from https://youtu.be/1mSzHxMpji0.
static SbMediaCanPlayMimeAndKeySystemParam kDrmQueryParams[] = {
    {"video/mp4; codecs=\"avc1.4d4015\"; width=426; height=240; framerate=24; "
     "bitrate=281854",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=640; height=360; framerate=24; "
     "bitrate=637760",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=1164612",
     ""},
    {"video/mp4; codecs=\"avc1.640028\"; width=1920; height=1080; "
     "framerate=24; "
     "bitrate=4362827",
     ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d400c\"; width=256; height=144; framerate=24; "
     "bitrate=138907",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=1746306",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=3473564",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=24; "
     "bitrate=3481130",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=24; "
     "bitrate=5789806",
     ""},
    {"video/mp4; codecs=\"avc1.640028\"; width=1920; height=1080; "
     "framerate=24; "
     "bitrate=5856175",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=2629046; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=1328071; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=2375894; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=426; "
     "height=240; framerate=24; bitrate=229634; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=640; "
     "height=360; framerate=24; bitrate=324585; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=639196; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=1055128; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"audio/mp4; codecs=\"ec-3\"; channels=6", ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=2111149; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=3709033; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=3679792; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=5524689; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"audio/mp4; codecs=\"ac-3\"; channels=6", ""},
    {"video/mp4; codecs=\"avc1.4d4015\"; width=426; height=240; framerate=24; "
     "bitrate=281854",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=640; height=360; framerate=24; "
     "bitrate=637760",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=1164612",
     ""},
    {"video/mp4; codecs=\"avc1.640028\"; width=1920; height=1080; "
     "framerate=24; "
     "bitrate=4362827",
     ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d400c\"; width=256; height=144; framerate=24; "
     "bitrate=138907",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=1746306",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=3473564",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=24; "
     "bitrate=3481130",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=24; "
     "bitrate=5789806",
     ""},
    {"video/mp4; codecs=\"avc1.640028\"; width=1920; height=1080; "
     "framerate=24; "
     "bitrate=5856175",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=2629046; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=1328071; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=2375894; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=426; "
     "height=240; framerate=24; bitrate=229634; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=640; "
     "height=360; framerate=24; bitrate=324585; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=639196; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=1055128; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"audio/mp4; codecs=\"ec-3\"; channels=6", ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=2111149; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=3709033; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=3679792; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=5524689; eotf=bt709; "
     "cryptoblockformat=subsample",
     ""},
    {"audio/mp4; codecs=\"ac-3\"; channels=6", ""},
    {"video/mp4; codecs=\"avc1.4d4015\"; width=426; height=240; framerate=24; "
     "bitrate=149590",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=640; height=360; framerate=24; "
     "bitrate=261202",
     ""},
    {"video/mp4; codecs=\"avc1.4d401e\"; width=854; height=480; framerate=24; "
     "bitrate=368187",
     ""},
    {"video/mp4; codecs=\"avc1.4d401f\"; width=1280; height=720; framerate=24; "
     "bitrate=676316",
     ""},
    {"video/mp4; codecs=\"avc1.640028\"; width=1920; height=1080; "
     "framerate=24; "
     "bitrate=2691722",
     ""},
    {"audio/mp4; codecs=\"mp4a.40.2\"; channels=2", ""},
    {"video/mp4; codecs=\"avc1.4d400c\"; width=256; height=144; framerate=24; "
     "bitrate=84646",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=426; "
     "height=240; framerate=24; bitrate=192698; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=640; "
     "height=360; framerate=24; bitrate=342403; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=854; "
     "height=480; framerate=24; bitrate=514976; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1280; "
     "height=720; framerate=24; bitrate=852689; eotf=bt709",
     ""},
    {"video/webm; codecs=\"vp09.00.51.08.01.01.01.01.00\"; width=1920; "
     "height=1080; framerate=24; bitrate=2389269; eotf=bt709",
     ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"audio/webm; codecs=\"opus\"; channels=2", ""},
    {"video/mp4; codecs=\"av01.0.00M.08\"; width=256; height=144; "
     "framerate=24; "
     "bitrate=74957; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.00M.08\"; width=426; height=240; "
     "framerate=24; "
     "bitrate=148691; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.01M.08\"; width=640; height=360; "
     "framerate=24; "
     "bitrate=305616; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.04M.08\"; width=854; height=480; "
     "framerate=24; "
     "bitrate=577104; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.05M.08\"; width=1280; height=720; "
     "framerate=24; bitrate=989646; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"av01.0.08M.08\"; width=1920; height=1080; "
     "framerate=24; bitrate=1766589; eotf=bt709",
     ""},
    {"video/mp4; codecs=\"avc1.4d4015\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d4015\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d401e\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d401e\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d401f\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d401f\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.640028\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.640028\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d400c\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"avc1.4d400c\"", "com.widevine.alpha"},
    {"video/mp4; codecs=\"vp09.00.51.08.01.01.01.01.00\"",
     "com.widevine.alpha"},
    {"video/mp4; codecs=\"vp09.00.51.08.01.01.01.01.00\"",
     "com.widevine.alpha"},
    {"video/mp4; codecs=\"audio/mp4; codecs=\"mp4a.40.2\"",
     "com.widevine.alpha"},
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_MEDIA_CAN_PLAY_MIME_AND_KEY_SYSTEM_TEST_HELPERS_H_
