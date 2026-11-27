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

#ifndef MEDIA_BASE_STARBOARD_SBMEDIA_HISTOGRAMS_H_
#define MEDIA_BASE_STARBOARD_SBMEDIA_HISTOGRAMS_H_

// Names of the Cobalt.Media histograms recorded by Cobalt's Starboard media
// integration (SbPlayer, SbDrm, SbAudioSink, SbMedia, etc.). Must match
// tools/metrics/histograms/metadata/cobalt/histograms.xml.
namespace media {

// keep-sorted start by_regex=k\w+ newline_separated=yes
// Color space of HDR video (more than 8 bits per channel) played by SbPlayer.
inline constexpr char kHdrMatrixHistogram[] = "Cobalt.Media.HDR.Matrix";

inline constexpr char kHdrPrimariesHistogram[] = "Cobalt.Media.HDR.Primaries";

inline constexpr char kHdrRangeHistogram[] = "Cobalt.Media.HDR.Range";

inline constexpr char kHdrTransferHistogram[] = "Cobalt.Media.HDR.Transfer";

// Whether the decoder buffer pool is allocated on demand.
inline constexpr char kIsBufferPoolAllocateOnDemandHistogram[] =
    "Cobalt.Media.IsBufferPoolAllocateOnDemand";

// Latency of SbAudioSinkCreate(). The first call in the process is recorded
// separately from later ones.
inline constexpr char kSbAudioSinkCreateHistogram[] =
    "Cobalt.Media.LatencyTiming.SbAudioSinkCreate";

inline constexpr char kSbAudioSinkCreateSubsequentHistogram[] =
    "Cobalt.Media.LatencyTiming.SbAudioSinkCreate.Subsequent";

// Latency of the Starboard call that creates the player: SbPlayerCreate(), or
// SbUrlPlayerCreate() for the tvOS URL player. Every call is recorded,
// including ones that fail to create a valid player. The first call in the
// process is recorded separately from later ones.
inline constexpr char kSbPlayerCreateHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreate";

inline constexpr char kSbPlayerCreateSubsequentHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreate.Subsequent";

// Time from the call that creates the player until the player first reports
// kSbPlayerStateInitialized. Split the same way as SbPlayerCreate.
inline constexpr char kSbPlayerCreateToInitializedHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreateToInitialized";

inline constexpr char kSbPlayerCreateToInitializedSubsequentHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreateToInitialized.Subsequent";

// Latency of SbPlayerDestroy(), split by the last state reported by the
// player.
inline constexpr char kSbPlayerDestroyBeforePrerollingHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.BeforePrerolling";

inline constexpr char kSbPlayerDestroyEndOfStreamHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.EndOfStream";

inline constexpr char kSbPlayerDestroyHistogramPrefix[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy";

inline constexpr char kSbPlayerDestroyPrerollingHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.Prerolling";

inline constexpr char kSbPlayerDestroyPresentingHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.Presenting";

// Color space of SDR video (8 bits per channel or fewer) played by SbPlayer.
inline constexpr char kSdrMatrixHistogram[] = "Cobalt.Media.SDR.Matrix";

inline constexpr char kSdrPrimariesHistogram[] = "Cobalt.Media.SDR.Primaries";

inline constexpr char kSdrRangeHistogram[] = "Cobalt.Media.SDR.Range";

inline constexpr char kSdrTransferHistogram[] = "Cobalt.Media.SDR.Transfer";
// keep-sorted end

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_SBMEDIA_HISTOGRAMS_H_
