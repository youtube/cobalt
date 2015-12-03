/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_BASE_SHELL_FILTER_GRAPH_LOG_CONSTANTS_H_
#define MEDIA_BASE_SHELL_FILTER_GRAPH_LOG_CONSTANTS_H_

namespace media {

// 4 bytes for object type and 4 bytes for signal type following mp4-like
// binary conventions of packed UTF-8 characters in machine-endian quads.

// this file is also parsed by the python-based log pretty-printer tool.
// to support easy python parsing, all values should be in hex, and all
// names should be prefixed with one of ObjectId,Event,or State.

static const uint32 kObjectIdBufferFactory = 0x62756672;       // 'bufr'
static const uint32 kObjectIdDemuxer = 0x646d7578;             // 'dmux'
static const uint32 kObjectIdAudioDemuxerStream = 0x75617364;  // 'dsau'
static const uint32 kObjectIdVideoDemuxerStream = 0x69767364;  // 'dsvi'
static const uint32 kObjectIdAudioDecoder = 0x61646563;        // 'adec'
static const uint32 kObjectIdAudioRenderer = 0x61726e64;       // 'arnd'
static const uint32 kObjectIdAudioSink = 0x6b6e6973;           // 'sink'
static const uint32 kObjectIdVideoDecoder = 0x76646563;        // 'vdec'
static const uint32 kObjectIdVideoRenderer = 0x76726e64;       // 'vrnd'
static const uint32 kObjectIdGraphics = 0x67726166;            // 'graf'

static const uint32 kEventArrayAllocationError = 0x61796572;      // 'ayer'
static const uint32 kEventArrayAllocationDeferred = 0x61797774;   // 'aywt'
static const uint32 kEventArrayAllocationReclaim = 0x61797263;    // 'ayrc'
static const uint32 kEventArrayAllocationRequest = 0x61797270;    // 'ayrq'
static const uint32 kEventArrayAllocationSuccess = 0x61796f6b;    // 'ayok'
static const uint32 kEventAudioClock = 0x61636c6b;                // 'aclk'
static const uint32 kEventBufferAllocationError = 0x62666572;     // 'bfer'
static const uint32 kEventBufferAllocationDeferred = 0x62667774;  // 'bfwt'
static const uint32 kEventBufferAllocationReclaim = 0x62667263;   // 'bfrc'
static const uint32 kEventBufferAllocationRequest = 0x62667270;   // 'bfrq'
static const uint32 kEventBufferAllocationSuccess = 0x62666f6b;   // 'bfok'
static const uint32 kEventConstructor = 0x63746f72;               // 'ctor'
static const uint32 kEventDataDecoded = 0x64617461;               // 'data'
static const uint32 kEventDecode = 0x64636f64;                    // 'dcod'
static const uint32 kEventDownloadAudio = 0x6c646175;             // 'ldau'
static const uint32 kEventDownloadVideo = 0x6c647664;             // 'ldvd
static const uint32 kEventDropFrame = 0x64726f70;                 // 'drop'
static const uint32 kEventEndOfStreamReceived = 0x656f7372;       // 'eosr'
static const uint32 kEventEndOfStreamSent = 0x656f7373;           // 'eoss'
static const uint32 kEventEnqueue = 0x6e717565;                   // 'nque'
static const uint32 kEventFatalError = 0x65726f72;                // 'eror'
static const uint32 kEventFlush = 0x666c7368;                     // 'flsh'
static const uint32 kEventFrameComposite = 0x64726177;            // 'draw'
static const uint32 kEventFrameFlip = 0x666c6970;                 // 'flip'
static const uint32 kEventFreeInputBuffer = 0x6672696e;           // 'frin'
static const uint32 kEventFreeOutputBuffer = 0x66726f75;          // 'frou'
static const uint32 kEventInitialize = 0x696e6974;                // 'init'
static const uint32 kEventOutputBufferFull = 0x66756c6c;          // 'full'
static const uint32 kEventPause = 0x70617573;                     // 'paus'
static const uint32 kEventPlay = 0x706c6179;                      // 'play'
static const uint32 kEventPop = 0x706f7020;                       // 'pop '
static const uint32 kEventPreroll = 0x70726f6c;                   // 'prol'
static const uint32 kEventPush = 0x70757368;                      // 'push'
static const uint32 kEventRead = 0x72656164;                      // 'read'
static const uint32 kEventRender = 0x726e6472;                    // 'rndr'
static const uint32 kEventRequestAudio = 0x72657161;              // 'reqa'
static const uint32 kEventRequestInterrupt = 0x69726570;          // 'ireq'
static const uint32 kEventRequestVideo = 0x72657176;              // 'reqv'
static const uint32 kEventReset = 0x72736574;                     // 'rset'
static const uint32 kEventResume = 0x7273756d;                    // 'rsum'
static const uint32 kEventSeek = 0x7365656b;                      // 'seek'
static const uint32 kEventStart = 0x73747274;                     // 'strt'
static const uint32 kEventStop = 0x73746f70;                      // 'stop'
static const uint32 kEventTimeCallback = 0x74696d65;              // 'time'
static const uint32 kEventUnderflow = 0x75666c77;                 // 'uflw'
static const uint32 kEventViewHostComposite = 0x76686365;         // 'vhce'
static const uint32 kEventWebKitComposite = 0x776b6365;           // 'wkce'

// instead of timestamp the following state flags log individual pipeline
// state information.
// two uint32s of buffer queue size and read cb queue size
static const uint32 kStateDemuxerStreamQueues = 0x73657571;  // 'ques'
// one uint32 either zero or one depending on state, and a zero
static const uint32 kStateDemuxerStreamBuffering = 0x66667562;  // 'buff'

}  // namespace media

#endif  // MEDIA_BASE_SHELL_FILTER_GRAPH_LOG_CONSTANTS_H_
