---
layout: doc
title: "Starboard Module Reference: media.h"
---

Provides media definitions that are common between the Decoder and Player
interfaces.

## Enums

### SbMediaAudioCodec

Types of audio elementary streams that can be supported.

**Values**

*   `kSbMediaAudioCodecNone`
*   `kSbMediaAudioCodecAac`
*   `kSbMediaAudioCodecOpus`
*   `kSbMediaAudioCodecVorbis`

### SbMediaAudioCodingType

Possible audio coding types.

**Values**

*   `kSbMediaAudioCodingTypeNone`
*   `kSbMediaAudioCodingTypeAac`
*   `kSbMediaAudioCodingTypeAc3`
*   `kSbMediaAudioCodingTypeAtrac`
*   `kSbMediaAudioCodingTypeBitstream`
*   `kSbMediaAudioCodingTypeDolbyDigitalPlus`
*   `kSbMediaAudioCodingTypeDts`
*   `kSbMediaAudioCodingTypeMpeg1`
*   `kSbMediaAudioCodingTypeMpeg2`
*   `kSbMediaAudioCodingTypeMpeg3`
*   `kSbMediaAudioCodingTypePcm`

### SbMediaAudioConnector

Possible audio connector types.

**Values**

*   `kSbMediaAudioConnectorNone`
*   `kSbMediaAudioConnectorAnalog`
*   `kSbMediaAudioConnectorBluetooth`
*   `kSbMediaAudioConnectorHdmi`
*   `kSbMediaAudioConnectorNetwork`
*   `kSbMediaAudioConnectorSpdif`
*   `kSbMediaAudioConnectorUsb`

### SbMediaAudioFrameStorageType

Possible audio frame storage types.

**Values**

*   `kSbMediaAudioFrameStorageTypeInterleaved` - The samples of a multi-channel audio stream are stored in one continuousbuffer. Samples at the same timestamp are stored one after another. Forexample, for a stereo stream with channels L and R that contains sampleswith timestamps 0, 1, 2, etc., the samples are stored in one buffer as"L0 R0 L1 R1 L2 R2 ...".
*   `kSbMediaAudioFrameStorageTypePlanar` - The samples of each channel are stored in their own continuous buffer.For example, for a stereo stream with channels L and R that containssamples with timestamps 0, 1, 2, etc., the samples are stored in twobuffers "L0 L1 L2 ..." and "R0 R1 R2 ...".

### SbMediaAudioSampleType

Possible audio sample types.

**Values**

*   `kSbMediaAudioSampleTypeInt16`
*   `kSbMediaAudioSampleTypeFloat32`

### SbMediaMatrixId

**Values**

*   `kSbMediaMatrixIdRgb` - The first 0-255 values should match the H264 specification (see Table E-5Matrix Coefficients in https://www.itu.int/rec/T-REC-H.264/en).
*   `kSbMediaMatrixIdBt709`
*   `kSbMediaMatrixIdUnspecified`
*   `kSbMediaMatrixIdReserved`
*   `kSbMediaMatrixIdFcc`
*   `kSbMediaMatrixIdBt470Bg`
*   `kSbMediaMatrixIdSmpte170M`
*   `kSbMediaMatrixIdSmpte240M`
*   `kSbMediaMatrixIdYCgCo`
*   `kSbMediaMatrixIdBt2020NonconstantLuminance`
*   `kSbMediaMatrixIdBt2020ConstantLuminance`
*   `kSbMediaMatrixIdYDzDx`
*   `kSbMediaMatrixIdLastStandardValue`
*   `kSbMediaMatrixIdUnknown` - Chrome-specific values start at 1000
*   `kSbMediaMatrixIdLast`

### SbMediaPrimaryId

**Values**

*   `kSbMediaPrimaryIdReserved0` - The first 0-255 values should match the H264 specification (see Table E-3Colour Primaries in https://www.itu.int/rec/T-REC-H.264/en).
*   `kSbMediaPrimaryIdBt709`
*   `kSbMediaPrimaryIdUnspecified`
*   `kSbMediaPrimaryIdReserved`
*   `kSbMediaPrimaryIdBt470M`
*   `kSbMediaPrimaryIdBt470Bg`
*   `kSbMediaPrimaryIdSmpte170M`
*   `kSbMediaPrimaryIdSmpte240M`
*   `kSbMediaPrimaryIdFilm`
*   `kSbMediaPrimaryIdBt2020`
*   `kSbMediaPrimaryIdSmpteSt4281`
*   `kSbMediaPrimaryIdSmpteSt4312`
*   `kSbMediaPrimaryIdSmpteSt4321`
*   `kSbMediaPrimaryIdLastStandardValue`
*   `kSbMediaPrimaryIdUnknown` - Chrome-specific values start at 1000.
*   `kSbMediaPrimaryIdXyzD50`
*   `kSbMediaPrimaryIdCustom`

### SbMediaRangeId

This corresponds to the WebM Range enum which is part of WebM color data
(see http://www.webmproject.org/docs/container/#Range).
H.264 only uses a bool, which corresponds to the LIMITED/FULL values.
Chrome-specific values start at 1000.

**Values**

*   `kSbMediaRangeIdUnspecified` - Range is not explicitly specified / unknown.
*   `kSbMediaRangeIdLimited` - Limited Rec. 709 color range with RGB values ranging from 16 to 235.
*   `kSbMediaRangeIdFull` - Full RGB color range with RGB valees from 0 to 255.
*   `kSbMediaRangeIdDerived` - Range is defined by TransferId/MatrixId.

### SbMediaSupportType

Indicates how confident the device is that it can play media resources
of the given type. The values are a direct map of the canPlayType() method
specified at the following link:
https://www.w3.org/TR/2011/WD-html5-20110113/video.html#dom-navigator-canplaytype

**Values**

*   `kSbMediaSupportTypeNotSupported` - The media type cannot be played.
*   `kSbMediaSupportTypeMaybe` - Cannot determinate if the media type is playable without playing it.
*   `kSbMediaSupportTypeProbably` - The media type seems to be playable.

### SbMediaTransferId

**Values**

*   `kSbMediaTransferIdReserved0` - The first 0-255 values should match the H264 specification (see Table E-4Transfer Characteristics in https://www.itu.int/rec/T-REC-H.264/en).
*   `kSbMediaTransferIdBt709`
*   `kSbMediaTransferIdUnspecified`
*   `kSbMediaTransferIdReserved`
*   `kSbMediaTransferIdGamma22`
*   `kSbMediaTransferIdGamma28`
*   `kSbMediaTransferIdSmpte170M`
*   `kSbMediaTransferIdSmpte240M`
*   `kSbMediaTransferIdLinear`
*   `kSbMediaTransferIdLog`
*   `kSbMediaTransferIdLogSqrt`
*   `kSbMediaTransferIdIec6196624`
*   `kSbMediaTransferIdBt1361Ecg`
*   `kSbMediaTransferIdIec6196621`
*   `kSbMediaTransferId10BitBt2020`
*   `kSbMediaTransferId12BitBt2020`
*   `kSbMediaTransferIdSmpteSt2084`
*   `kSbMediaTransferIdSmpteSt4281`
*   `kSbMediaTransferIdAribStdB67` - AKA hybrid-log gamma, HLG.
*   `kSbMediaTransferIdLastStandardValue`
*   `kSbMediaTransferIdUnknown` - Chrome-specific values start at 1000.
*   `kSbMediaTransferIdGamma24`
*   `kSbMediaTransferIdSmpteSt2084NonHdr` - This is an ad-hoc transfer function that decodes SMPTE 2084 contentinto a 0-1 range more or less suitable for viewing on a non-hdrdisplay.
*   `kSbMediaTransferIdCustom` - TODO: Need to store an approximation of the gamma function(s).
*   `kSbMediaTransferIdLast`

### SbMediaType

Types of media component streams.

**Values**

*   `kSbMediaTypeAudio` - Value used for audio streams.
*   `kSbMediaTypeVideo` - Value used for video streams.

### SbMediaVideoCodec

Types of video elementary streams that could be supported.

**Values**

*   `kSbMediaVideoCodecNone`
*   `kSbMediaVideoCodecH264`
*   `kSbMediaVideoCodecH265`
*   `kSbMediaVideoCodecMpeg2`
*   `kSbMediaVideoCodecTheora`
*   `kSbMediaVideoCodecVc1`
*   `kSbMediaVideoCodecVp10`
*   `kSbMediaVideoCodecVp8`
*   `kSbMediaVideoCodecVp9`

## Structs

### SbMediaAudioConfiguration

A structure describing the audio configuration parameters of a single audio
output.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>index</code></td>    <td>The platform-defined index of the associated audio output.</td>  </tr>
  <tr>
    <td><code>SbMediaAudioConnector</code><br>        <code>connector</code></td>    <td>The type of audio connector. Will be the empty <code>kSbMediaAudioConnectorNone</code>
if this device cannot provide this information.</td>  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>latency</code></td>    <td>The expected latency of audio over this output, in microseconds, or <code>0</code> if
this device cannot provide this information.</td>  </tr>
  <tr>
    <td><code>SbMediaAudioCodingType</code><br>        <code>coding_type</code></td>    <td>The type of audio coding used over this connection.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>number_of_channels</code></td>    <td>The number of audio channels currently supported by this device output, or
<code>0</code> if this device cannot provide this information, in which case the
caller can probably assume stereo output.</td>  </tr>
</table>

### SbMediaAudioHeader

An audio sequence header, which is a description of a given audio stream.
This, in hexadecimal string form, acts as a set of instructions to the audio
decoder.<br>
The Sequence Header consists of a little-endian hexadecimal encoded
`WAVEFORMATEX` structure followed by an Audio-specific configuration field.
The `WAVEFORMATEX` structure is specified at:
http://msdn.microsoft.com/en-us/library/dd390970(v=vs.85).aspx

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>uint16_t</code><br>        <code>format_tag</code></td>    <td>The waveform-audio format type code.</td>  </tr>
  <tr>
    <td><code>uint16_t</code><br>        <code>number_of_channels</code></td>    <td>The number of audio channels in this format. <code>1</code> for mono, <code>2</code> for stereo.</td>  </tr>
  <tr>
    <td><code>uint32_t</code><br>        <code>samples_per_second</code></td>    <td>The sampling rate.</td>  </tr>
  <tr>
    <td><code>uint32_t</code><br>        <code>average_bytes_per_second</code></td>    <td>The number of bytes per second expected with this format.</td>  </tr>
  <tr>
    <td><code>uint16_t</code><br>        <code>block_alignment</code></td>    <td>Byte block alignment, e.g, 4.</td>  </tr>
  <tr>
    <td><code>uint16_t</code><br>        <code>bits_per_sample</code></td>    <td>The bit depth for the stream this represents, e.g. <code>8</code> or <code>16</code>.</td>  </tr>
  <tr>
    <td><code>uint16_t</code><br>        <code>audio_specific_config_size</code></td>    <td>The size, in bytes, of the audio_specific_config.</td>  </tr>
  <tr>
    <td><code>const</code><br>        <code>void* audio_specific_config</code></td>    <td>The AudioSpecificConfig, as specified in ISO/IEC-14496-3, section 1.6.2.1:
http://read.pudn.com/downloads98/doc/comm/401153/14496/ISO_IEC_14496-3%20Part%203%20Audio/C036083E_SUB1.PDF</td>  </tr>
  <tr>
    <td><code>int8_t</code><br>        <code>audio_specific_config[8]</code></td>    <td></td>  </tr>
</table>

### SbMediaColorMetadata

HDR (High Dynamic Range) Metadata common for HDR10 and
WebM/VP9-based HDR formats, together with the ColorSpace. HDR
reproduces a greater dynamic range of luminosity than is possible
with standard digital imaging. See the Consumer Electronics
Association press release:
https://www.cta.tech/News/Press-Releases/2015/August/CEA-Defines-%E2%80%98HDR-Compatible%E2%80%99-Displays.aspx

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int bits_per_channel</code></td>    <td>Number of decoded bits per channel. A value of 0 indicates that
the BitsPerChannel is unspecified.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int chroma_subsampling_horizontal</code></td>    <td>The amount of pixels to remove in the Cr and Cb channels for
every pixel not removed horizontally. Example: For video with
4:2:0 chroma subsampling, the <code>chroma_subsampling_horizontal</code> should be set
to 1.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int chroma_subsampling_vertical</code></td>    <td>The amount of pixels to remove in the Cr and Cb channels for
every pixel not removed vertically. Example: For video with
4:2:0 chroma subsampling, the <code>chroma_subsampling_vertical</code> should be set
to 1.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int cb_subsampling_horizontal</code></td>    <td>The amount of pixels to remove in the Cb channel for every pixel
not removed horizontally. This is additive with
ChromaSubsamplingHorz. Example: For video with 4:2:1 chroma
subsampling, the <code>chroma_subsampling_horizontal</code> should be set to 1 and
<code>cb_subsampling_horizontal</code> should be set to 1.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int cb_subsampling_vertical</code></td>    <td>The amount of pixels to remove in the Cb channel for every pixel
not removed vertically. This is additive with
<code>chroma_subsampling_vertical</code>.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int chroma_siting_horizontal</code></td>    <td>How chroma is subsampled horizontally. (0: Unspecified, 1: Left
Collocated, 2: Half)</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int chroma_siting_vertical</code></td>    <td>How chroma is subsampled vertically. (0: Unspecified, 1: Top
Collocated, 2: Half)</td>  </tr>
  <tr>
    <td><code>SbMediaMasteringMetadata</code><br>        <code>mastering_metadata</code></td>    <td>[HDR Metadata field] SMPTE 2086 mastering data.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int max_cll</code></td>    <td>[HDR Metadata field] Maximum brightness of a single pixel (Maximum
Content Light Level) in candelas per square meter (cd/m^2).</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int max_fall</code></td>    <td>[HDR Metadata field] Maximum brightness of a single full frame
(Maximum Frame-Average Light Level) in candelas per square meter
(cd/m^2).</td>  </tr>
  <tr>
    <td><code>SbMediaPrimaryId</code><br>        <code>primaries</code></td>    <td>[Color Space field] The colour primaries of the video. For
clarity, the value and meanings for Primaries are adopted from
Table 2 of ISO/IEC 23001-8:2013/DCOR1. (0: Reserved, 1: ITU-R
BT.709, 2: Unspecified, 3: Reserved, 4: ITU-R BT.470M, 5: ITU-R
BT.470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8: FILM, 9: ITU-R
BT.2020, 10: SMPTE ST 428-1, 22: JEDEC P22 phosphors)</td>  </tr>
  <tr>
    <td><code>SbMediaTransferId</code><br>        <code>transfer</code></td>    <td>[Color Space field] The transfer characteristics of the
video. For clarity, the value and meanings for
TransferCharacteristics 1-15 are adopted from Table 3 of ISO/IEC
23001-8:2013/DCOR1. TransferCharacteristics 16-18 are proposed
values. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified, 3:
Reserved, 4: Gamma 2.2 curve, 5: Gamma 2.8 curve, 6: SMPTE 170M,
7: SMPTE 240M, 8: Linear, 9: Log, 10: Log Sqrt, 11: IEC
61966-2-4, 12: ITU-R BT.1361 Extended Colour Gamut, 13: IEC
61966-2-1, 14: ITU-R BT.2020 10 bit, 15: ITU-R BT.2020 12 bit,
16: SMPTE ST 2084, 17: SMPTE ST 428-1 18: ARIB STD-B67 (HLG))</td>  </tr>
  <tr>
    <td><code>SbMediaMatrixId</code><br>        <code>matrix</code></td>    <td>[Color Space field] The Matrix Coefficients of the video used to
derive luma and chroma values from red, green, and blue color
primaries. For clarity, the value and meanings for
MatrixCoefficients are adopted from Table 4 of ISO/IEC
23001-8:2013/DCOR1. (0:GBR, 1: BT709, 2: Unspecified, 3:
Reserved, 4: FCC, 5: BT470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8:
YCOCG, 9: BT2020 Non-constant Luminance, 10: BT2020 Constant
Luminance)</td>  </tr>
  <tr>
    <td><code>SbMediaRangeId</code><br>        <code>range</code></td>    <td>[Color Space field] Clipping of the color ranges. (0:
Unspecified, 1: Broadcast Range, 2: Full range (no clipping), 3:
Defined by MatrixCoefficients/TransferCharacteristics)</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>custom_primary_matrix[12]</code></td>    <td>[Color Space field] Only used if primaries ==
kSbMediaPrimaryIdCustom.  This a row-major ordered 3 x 4
submatrix of the 4 x 4 transform matrix.  The 4th row is
completed as (0, 0, 0, 1).</td>  </tr>
</table>

### SbMediaMasteringMetadata

SMPTE 2086 mastering data
http://ieeexplore.ieee.org/document/7291707/
This standard specifies the metadata items to specify the color
volume (the color primaries, white point, and luminance range) of
the display that was used in mastering video content. The metadata
is specified as a set of values independent of any specific digital
representation.
Also see the WebM container guidelines:
https://www.webmproject.org/docs/container/

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>float</code><br>        <code>primary_r_chromaticity_x</code></td>    <td>Red X chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>primary_r_chromaticity_y</code></td>    <td>Red Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>primary_g_chromaticity_x</code></td>    <td>Green X chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>primary_g_chromaticity_y</code></td>    <td>Green Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>primary_b_chromaticity_x</code></td>    <td>Blue X chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>primary_b_chromaticity_y</code></td>    <td>Blue Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>white_point_chromaticity_x</code></td>    <td>White X chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>white_point_chromaticity_y</code></td>    <td>White Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>luminance_max</code></td>    <td>Maximum luminance. Shall be represented in candelas per square
meter (cd/m^2). In range [0, 9999.99].</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>luminance_min</code></td>    <td>Minimum luminance. Shall be represented in candelas per square
meter (cd/m^2). In range [0, 9999.99].</td>  </tr>
</table>

### SbMediaVideoSampleInfo

The set of information required by the decoder or player for each video
sample.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>bool</code><br>        <code>is_key_frame</code></td>    <td>Indicates whether the associated sample is a key frame (I-frame).
Video key frames must always start with SPS and PPS NAL units.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>frame_width</code></td>    <td>The frame width of this sample, in pixels. Also could be parsed from the
Sequence Parameter Set (SPS) NAL Unit. Frame dimensions must only change on
key frames, but may change on any key frame.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>frame_height</code></td>    <td>The frame height of this sample, in pixels. Also could be parsed from the
Sequence Parameter Set (SPS) NAL Unit. Frame dimensions must only change on
key frames, but may change on any key frame.</td>  </tr>
</table>

## Functions

### SbMediaCanPlayMimeAndKeySystem

**Description**

Returns information about whether the playback of the specific media
described by `mime` and encrypted using `key_system` can be played.<br>
Note that neither `mime` nor `key_system` can be NULL. This function returns
`kSbMediaSupportNotSupported` if either is NULL.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMediaCanPlayMimeAndKeySystem-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMediaCanPlayMimeAndKeySystem-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMediaCanPlayMimeAndKeySystem-declaration">
<pre>
SB_EXPORT SbMediaSupportType
SbMediaCanPlayMimeAndKeySystem(const char* mime, const char* key_system);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMediaCanPlayMimeAndKeySystem-stub">

```
#include "starboard/media.h"

SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char* mime,
                                                  const char* key_system) {
  SB_UNREFERENCED_PARAMETER(mime);
  SB_UNREFERENCED_PARAMETER(key_system);
  return kSbMediaSupportTypeNotSupported;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>mime</code></td>
    <td>The mime information of the media in the form of <code>video/webm</code>
or <code>video/mp4; codecs="avc1.42001E"</code>. It may include arbitrary parameters
like "codecs", "channels", etc.  Note that the "codecs" parameter may
contain more than one codec, delimited by comma.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>key_system</code></td>
    <td>A lowercase value in fhe form of "com.example.somesystem"
as suggested by https://w3c.github.io/encrypted-media/#key-system
that can be matched exactly with known DRM key systems of the platform.
When <code>key_system</code> is an empty string, the return value is an indication for
non-encrypted media.</td>
  </tr>
</table>

### SbMediaGetAudioConfiguration

**Description**

Retrieves the current physical audio configuration of audio output
`output_index` on this device and places it in `out_configuration`,
which must not be NULL.<br>
This function returns `false` if nothing could be determined on this
platform or if `output_index` does not exist on this device.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMediaGetAudioConfiguration-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMediaGetAudioConfiguration-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMediaGetAudioConfiguration-declaration">
<pre>
SB_EXPORT bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMediaGetAudioConfiguration-stub">

```
#include "starboard/media.h"

bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) {
  SB_UNREFERENCED_PARAMETER(output_index);
  SB_UNREFERENCED_PARAMETER(out_configuration);
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>output_index</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbMediaAudioConfiguration*</code><br>        <code>out_configuration</code></td>
    <td>The variable that holds the audio configuration
information.</td>
  </tr>
</table>

### SbMediaGetAudioOutputCount

**Description**

Returns the number of audio outputs currently available on this device.
Even if the number of outputs or their audio configurations can't be
determined, it is expected that the platform will at least return a single
output that supports at least stereo.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMediaGetAudioOutputCount-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMediaGetAudioOutputCount-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMediaGetAudioOutputCount-declaration">
<pre>
SB_EXPORT int SbMediaGetAudioOutputCount();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMediaGetAudioOutputCount-stub">

```
#include "starboard/media.h"

int SbMediaGetAudioOutputCount() {
  return 0;
}
```

  </div>
</div>

### SbMediaIsOutputProtected

**Description**

Indicates whether output copy protection is currently enabled on all capable
outputs. If `true`, then non-protection-capable outputs are expected to be
blanked.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMediaIsOutputProtected-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMediaIsOutputProtected-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMediaIsOutputProtected-declaration">
<pre>
SB_EXPORT bool SbMediaIsOutputProtected();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMediaIsOutputProtected-stub">

```
#include "starboard/media.h"

#include "starboard/log.h"

bool SbMediaIsOutputProtected() {
  return false;
}
```

  </div>
</div>

### SbMediaIsSupported

**Description**

Indicates whether this platform supports decoding `video_codec` and
`audio_codec` along with decrypting using `key_system`. If `video_codec` is
`kSbMediaVideoCodecNone` or if `audio_codec` is `kSbMediaAudioCodecNone`,
this function should return `true` as long as `key_system` is supported on
the platform to decode any supported input formats.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMediaIsSupported-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMediaIsSupported-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMediaIsSupported-declaration">
<pre>
SB_EXPORT bool SbMediaIsSupported(SbMediaVideoCodec video_codec,
                                  SbMediaAudioCodec audio_codec,
                                  const char* key_system);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMediaIsSupported-stub">

```
#include "starboard/media.h"

SB_EXPORT bool SbMediaIsSupported(SbMediaVideoCodec /*video_codec*/,
                                  SbMediaAudioCodec /*audio_codec*/,
                                  const char* /*key_system*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMediaVideoCodec</code><br>        <code>video_codec</code></td>
    <td>The <code>SbMediaVideoCodec</code> being checked for platform
compatibility.</td>
  </tr>
  <tr>
    <td><code>SbMediaAudioCodec</code><br>        <code>audio_codec</code></td>
    <td>The <code>SbMediaAudioCodec</code> being checked for platform
compatibility.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>key_system</code></td>
    <td>The key system being checked for platform compatibility.</td>
  </tr>
</table>

### SbMediaSetOutputProtection

**Description**

Enables or disables output copy protection on all capable outputs. If
enabled, then non-protection-capable outputs are expected to be blanked.<br>
The return value indicates whether the operation was successful, and the
function returns a success even if the call is redundant in that it doesn't
change the current value.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMediaSetOutputProtection-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMediaSetOutputProtection-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMediaSetOutputProtection-declaration">
<pre>
SB_EXPORT bool SbMediaSetOutputProtection(bool enabled);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMediaSetOutputProtection-stub">

```
#include "starboard/media.h"

#include "starboard/log.h"

bool SbMediaSetOutputProtection(bool enabled) {
  SB_UNREFERENCED_PARAMETER(enabled);
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>bool</code><br>        <code>enabled</code></td>
    <td>Indicates whether output protection is enabled (<code>true</code>) or
disabled.</td>
  </tr>
</table>

