// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_CONSTANTS_H_
#define MEDIA_WEBM_WEBM_CONSTANTS_H_

namespace media {

// WebM element IDs.
// This is a subset of the IDs in the Matroska spec.
// http://www.matroska.org/technical/specs/index.html
const int kWebMIdAspectRatioType = 0x54B3;
const int kWebMIdAudio = 0xE1;
const int kWebMIdBitDepth = 0x6264;
const int kWebMIdBlock = 0xA1;
const int kWebMIdBlockGroup = 0xA0;
const int kWebMIdChannels = 0x9F;
const int kWebMIdCluster = 0x1f43b675;
const int kWebMIdCodecID = 0x86;
const int kWebMIdCodecName = 0x258688;
const int kWebMIdCodecPrivate = 0x63A2;
const int kWebMIdCRC32 = 0xBF;
const int kWebMIdCues = 0x1C53BB6B;
const int kWebMIdDateUTC = 0x4461;
const int kWebMIdDefaultDuration = 0x23E383;
const int kWebMIdDisplayHeight = 0x54BA;
const int kWebMIdDisplayUnit = 0x54B2;
const int kWebMIdDisplayWidth = 0x54B0;
const int kWebMIdDuration = 0x4489;
const int kWebMIdEBML = 0x1A45DFA3;
const int kWebMIdFlagDefault = 0x88;
const int kWebMIdFlagEnabled = 0xB9;
const int kWebMIdFlagForced = 0x55AA;
const int kWebMIdFlagInterlaced = 0x9A;
const int kWebMIdFlagLacing = 0x9C;
const int kWebMIdInfo = 0x1549A966;
const int kWebMIdLanguage = 0x22B59C;
const int kWebMIdMuxingApp = 0x4D80;
const int kWebMIdName = 0x536E;
const int kWebMIdOutputSamplingFrequency = 0x78B5;
const int kWebMIdPixelCropBottom = 0x54AA;
const int kWebMIdPixelCropLeft = 0x54CC;
const int kWebMIdPixelCropRight = 0x54DD;
const int kWebMIdPixelCropTop = 0x54BB;
const int kWebMIdPixelHeight = 0xBA;
const int kWebMIdPixelWidth = 0xB0;
const int kWebMIdPrevSize = 0xAB;
const int kWebMIdSamplingFrequency = 0xB5;
const int kWebMIdSeekHead = 0x114D9B74;
const int kWebMIdSegment = 0x18538067;
const int kWebMIdSegmentUID = 0x73A4;
const int kWebMIdSimpleBlock = 0xA3;
const int kWebMIdStereoMode = 0x53B8;
const int kWebMIdTimecode = 0xE7;
const int kWebMIdTimecodeScale = 0x2AD7B1;
const int kWebMIdTitle = 0x7BA9;
const int kWebMIdTrackEntry = 0xAE;
const int kWebMIdTrackNumber = 0xD7;
const int kWebMIdTrackType = 0x83;
const int kWebMIdTrackUID = 0x73C5;
const int kWebMIdTracks = 0x1654AE6B;
const int kWebMIdVideo = 0xE0;
const int kWebMIdVoid = 0xEC;
const int kWebMIdWritingApp = 0x5741;

// Default timecode scale if the TimecodeScale element is
// not specified in the INFO element.
const int kWebMDefaultTimecodeScale = 1000000;

// Values for TrackType element.
const int kWebMTrackTypeVideo = 1;
const int kWebMTrackTypeAudio = 2;

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_CONSTANTS_H_
