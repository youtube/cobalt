---
layout: doc
title: "Starboard Module Reference: media.h"
---

Provides media definitions that are common between the Decoder and Player
interfaces.

## Macros ##

### kSbMediaTimeSecond ###

One second in SbMediaTime (90KHz ticks).

## Enums ##

### SbMediaAudioCodec ###

Types of audio elementary streams that can be supported.

#### Values ####

*   `kSbMediaAudioCodecNone`
*   `kSbMediaAudioCodecAac`
*   `kSbMediaAudioCodecOpus`
*   `kSbMediaAudioCodecVorbis`

### SbMediaAudioCodingType ###

Possible audio coding types.

#### Values ####

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

### SbMediaAudioConnector ###

Possible audio connector types.

#### Values ####

*   `kSbMediaAudioConnectorNone`
*   `kSbMediaAudioConnectorAnalog`
*   `kSbMediaAudioConnectorBluetooth`
*   `kSbMediaAudioConnectorHdmi`
*   `kSbMediaAudioConnectorNetwork`
*   `kSbMediaAudioConnectorSpdif`
*   `kSbMediaAudioConnectorUsb`

### SbMediaAudioFrameStorageType ###

Possible audio frame storage types.

#### Values ####

*   `kSbMediaAudioFrameStorageTypeInterleaved`

    The samples of a multi-channel audio stream are stored in one continuous
    buffer. Samples at the same timestamp are stored one after another. For
    example, for a stereo stream with channels L and R that contains samples
    with timestamps 0, 1, 2, etc., the samples are stored in one buffer as "L0
    R0 L1 R1 L2 R2 ...".
*   `kSbMediaAudioFrameStorageTypePlanar`

    The samples of each channel are stored in their own continuous buffer. For
    example, for a stereo stream with channels L and R that contains samples
    with timestamps 0, 1, 2, etc., the samples are stored in two buffers "L0 L1
    L2 ..." and "R0 R1 R2 ...".

### SbMediaAudioSampleType ###

Possible audio sample types.

#### Values ####

*   `kSbMediaAudioSampleTypeInt16`
*   `kSbMediaAudioSampleTypeFloat32`

### SbMediaRangeId ###

This corresponds to the WebM Range enum which is part of WebM color data (see [http://www.webmproject.org/docs/container/#Range](http://www.webmproject.org/docs/container/#Range)
). H.264 only uses a bool, which corresponds to the LIMITED/FULL values. Chrome-
specific values start at 1000.

#### Values ####

*   `kSbMediaRangeIdUnspecified`

    Range is not explicitly specified / unknown.
*   `kSbMediaRangeIdLimited`

    Limited Rec. 709 color range with RGB values ranging from 16 to 235.
*   `kSbMediaRangeIdFull`

    Full RGB color range with RGB valees from 0 to 255.
*   `kSbMediaRangeIdDerived`

    Range is defined by TransferId/MatrixId.
*   `kSbMediaRangeIdLast`

### SbMediaSupportType ###

Indicates how confident the device is that it can play media resources of the
given type. The values are a direct map of the canPlayType() method specified at
the following link: [https://www.w3.org/TR/2011/WD-html5-20110113/video.html#dom-navigator-canplaytype](https://www.w3.org/TR/2011/WD-html5-20110113/video.html#dom-navigator-canplaytype)

#### Values ####

*   `kSbMediaSupportTypeNotSupported`

    The media type cannot be played.
*   `kSbMediaSupportTypeMaybe`

    Cannot determinate if the media type is playable without playing it.
*   `kSbMediaSupportTypeProbably`

    The media type seems to be playable.

### SbMediaType ###

Types of media component streams.

#### Values ####

*   `kSbMediaTypeAudio`

    Value used for audio streams.
*   `kSbMediaTypeVideo`

    Value used for video streams.

### SbMediaVideoCodec ###

Types of video elementary streams that could be supported.

#### Values ####

*   `kSbMediaVideoCodecNone`
*   `kSbMediaVideoCodecH264`
*   `kSbMediaVideoCodecH265`
*   `kSbMediaVideoCodecMpeg2`
*   `kSbMediaVideoCodecTheora`
*   `kSbMediaVideoCodecVc1`
*   `kSbMediaVideoCodecVp10`
*   `kSbMediaVideoCodecVp8`
*   `kSbMediaVideoCodecVp9`

## Typedefs ##

### SbMediaTime ###

Time represented in 90KHz ticks.

#### Definition ####

```
typedef int64_t SbMediaTime
```

## Structs ##

### SbMediaAudioConfiguration ###

A structure describing the audio configuration parameters of a single audio
output.

#### Members ####

*   `int index`

    The platform-defined index of the associated audio output.
*   `SbMediaAudioConnector connector`

    The type of audio connector. Will be the empty `kSbMediaAudioConnectorNone`
    if this device cannot provide this information.
*   `SbTime latency`

    The expected latency of audio over this output, in microseconds, or `0` if
    this device cannot provide this information.
*   `SbMediaAudioCodingType coding_type`

    The type of audio coding used over this connection.
*   `int number_of_channels`

    The number of audio channels currently supported by this device output, or
    `0` if this device cannot provide this information, in which case the caller
    can probably assume stereo output.

### SbMediaAudioHeader ###

An audio sequence header, which is a description of a given audio stream. This,
in hexadecimal string form, acts as a set of instructions to the audio decoder.

The Sequence Header consists of a little-endian hexadecimal encoded
`WAVEFORMATEX` structure followed by an Audio-specific configuration field. The
`WAVEFORMATEX` structure is specified at: [http://msdn.microsoft.com/en-us/library/dd390970(v=vs.85).aspx](http://msdn.microsoft.com/en-us/library/dd390970(v=vs.85).aspx)

#### Members ####

*   `uint16_t format_tag`

    The waveform-audio format type code.
*   `uint16_t number_of_channels`

    The number of audio channels in this format. `1` for mono, `2` for stereo.
*   `uint32_t samples_per_second`

    The sampling rate.
*   `uint32_t average_bytes_per_second`

    The number of bytes per second expected with this format.
*   `uint16_t block_alignment`

    Byte block alignment, e.g, 4.
*   `uint16_t bits_per_sample`

    The bit depth for the stream this represents, e.g. `8` or `16`.
*   `uint16_t audio_specific_config_size`

    The size, in bytes, of the audio_specific_config.
*   `int8_t audio_specific_config`

    The AudioSpecificConfig, as specified in ISO/IEC-14496-3, section 1.6.2.1: [http://read.pudn.com/downloads98/doc/comm/401153/14496/ISO_IEC_14496-3%20Part%203%20Audio/C036083E_SUB1.PDF](http://read.pudn.com/downloads98/doc/comm/401153/14496/ISO_IEC_14496-3%20Part%203%20Audio/C036083E_SUB1.PDF)

### SbMediaColorMetadata ###

HDR (High Dynamic Range) Metadata common for HDR10 and WebM/VP9-based HDR
formats, together with the ColorSpace. HDR reproduces a greater dynamic range of
luminosity than is possible with standard digital imaging. See the Consumer
Electronics Association press release: [https://www.cta.tech/News/Press-Releases/2015/August/CEA-Defines-%E2%80%98HDR-Compatible%E2%80%99-Displays.aspx](https://www.cta.tech/News/Press-Releases/2015/August/CEA-Defines-%E2%80%98HDR-Compatible%E2%80%99-Displays.aspx)

#### Members ####

*   `unsigned int bits_per_channel`

    Number of decoded bits per channel. A value of 0 indicates that the
    BitsPerChannel is unspecified.
*   `unsigned int chroma_subsampling_horizontal`

    The amount of pixels to remove in the Cr and Cb channels for every pixel not
    removed horizontally. Example: For video with 4:2:0 chroma subsampling, the
    `chroma_subsampling_horizontal` should be set to 1.
*   `unsigned int chroma_subsampling_vertical`

    The amount of pixels to remove in the Cr and Cb channels for every pixel not
    removed vertically. Example: For video with 4:2:0 chroma subsampling, the
    `chroma_subsampling_vertical` should be set to 1.
*   `unsigned int cb_subsampling_horizontal`

    The amount of pixels to remove in the Cb channel for every pixel not removed
    horizontally. This is additive with ChromaSubsamplingHorz. Example: For
    video with 4:2:1 chroma subsampling, the `chroma_subsampling_horizontal`
    should be set to 1 and `cb_subsampling_horizontal` should be set to 1.
*   `unsigned int cb_subsampling_vertical`

    The amount of pixels to remove in the Cb channel for every pixel not removed
    vertically. This is additive with `chroma_subsampling_vertical`.
*   `unsigned int chroma_siting_horizontal`

    How chroma is subsampled horizontally. (0: Unspecified, 1: Left Collocated,
    2: Half)
*   `unsigned int chroma_siting_vertical`

    How chroma is subsampled vertically. (0: Unspecified, 1: Top Collocated, 2:
    Half)
*   `SbMediaMasteringMetadata mastering_metadata`

    [HDR Metadata field] SMPTE 2086 mastering data.
*   `unsigned int max_cll`

    [HDR Metadata field] Maximum brightness of a single pixel (Maximum Content
    Light Level) in candelas per square meter (cd/m^2).
*   `unsigned int max_fall`

    [HDR Metadata field] Maximum brightness of a single full frame (Maximum
    Frame-Average Light Level) in candelas per square meter (cd/m^2).
*   `SbMediaPrimaryId primaries`

    [Color Space field] The colour primaries of the video. For clarity, the
    value and meanings for Primaries are adopted from Table 2 of ISO/IEC
    23001-8:2013/DCOR1. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified, 3:
    Reserved, 4: ITU-R BT.470M, 5: ITU-R BT.470BG, 6: SMPTE 170M, 7: SMPTE 240M,
    8: FILM, 9: ITU-R BT.2020, 10: SMPTE ST 428-1, 22: JEDEC P22 phosphors)
*   `SbMediaTransferId transfer`

    [Color Space field] The transfer characteristics of the video. For clarity,
    the value and meanings for TransferCharacteristics 1-15 are adopted from
    Table 3 of ISO/IEC 23001-8:2013/DCOR1. TransferCharacteristics 16-18 are
    proposed values. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified, 3: Reserved,
    4: Gamma 2.2 curve, 5: Gamma 2.8 curve, 6: SMPTE 170M, 7: SMPTE 240M, 8:
    Linear, 9: Log, 10: Log Sqrt, 11: IEC 61966-2-4, 12: ITU-R BT.1361 Extended
    Colour Gamut, 13: IEC 61966-2-1, 14: ITU-R BT.2020 10 bit, 15: ITU-R BT.2020
    12 bit, 16: SMPTE ST 2084, 17: SMPTE ST 428-1 18: ARIB STD-B67 (HLG))
*   `SbMediaMatrixId matrix`

    [Color Space field] The Matrix Coefficients of the video used to derive luma
    and chroma values from red, green, and blue color primaries. For clarity,
    the value and meanings for MatrixCoefficients are adopted from Table 4 of
    ISO/IEC 23001-8:2013/DCOR1. (0:GBR, 1: BT709, 2: Unspecified, 3: Reserved,
    4: FCC, 5: BT470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8: YCOCG, 9: BT2020 Non-
    constant Luminance, 10: BT2020 Constant Luminance)
*   `SbMediaRangeId range`

    [Color Space field] Clipping of the color ranges. (0: Unspecified, 1:
    Broadcast Range, 2: Full range (no clipping), 3: Defined by
    MatrixCoefficients/TransferCharacteristics)
*   `float custom_primary_matrix`

    [Color Space field] Only used if primaries == kSbMediaPrimaryIdCustom. This
    a row-major ordered 3 x 4 submatrix of the 4 x 4 transform matrix. The 4th
    row is completed as (0, 0, 0, 1).

### SbMediaMasteringMetadata ###

SMPTE 2086 mastering data [http://ieeexplore.ieee.org/document/7291707/](http://ieeexplore.ieee.org/document/7291707/)
This standard specifies the metadata items to specify the color volume (the
color primaries, white point, and luminance range) of the display that was used
in mastering video content. The metadata is specified as a set of values
independent of any specific digital representation. Also see the WebM container
guidelines: [https://www.webmproject.org/docs/container/](https://www.webmproject.org/docs/container/)

#### Members ####

*   `float primary_r_chromaticity_x`

    Red X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float primary_r_chromaticity_y`

    Red Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float primary_g_chromaticity_x`

    Green X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float primary_g_chromaticity_y`

    Green Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float primary_b_chromaticity_x`

    Blue X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float primary_b_chromaticity_y`

    Blue Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float white_point_chromaticity_x`

    White X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float white_point_chromaticity_y`

    White Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
*   `float luminance_max`

    Maximum luminance. Shall be represented in candelas per square meter
    (cd/m^2). In range [0, 9999.99].
*   `float luminance_min`

    Minimum luminance. Shall be represented in candelas per square meter
    (cd/m^2). In range [0, 9999.99].

### SbMediaVideoSampleInfo ###

The set of information required by the decoder or player for each video sample.

#### Members ####

*   `bool is_key_frame`

    Indicates whether the associated sample is a key frame (I-frame). Video key
    frames must always start with SPS and PPS NAL units.
*   `int frame_width`

    The frame width of this sample, in pixels. Also could be parsed from the
    Sequence Parameter Set (SPS) NAL Unit. Frame dimensions must only change on
    key frames, but may change on any key frame.
*   `int frame_height`

    The frame height of this sample, in pixels. Also could be parsed from the
    Sequence Parameter Set (SPS) NAL Unit. Frame dimensions must only change on
    key frames, but may change on any key frame.
*   `SbMediaColorMetadata* color_metadata`

    HDR metadata common for HDR10 and WebM/VP9-based HDR formats as well as the
    Color Space, and Color elements: MatrixCoefficients, BitsPerChannel,
    ChromaSubsamplingHorz, ChromaSubsamplingVert, CbSubsamplingHorz,
    CbSubsamplingVert, ChromaSitingHorz, ChromaSitingVert, Range,
    TransferCharacteristics, and Primaries described here: [https://matroska.org/technical/specs/index.html](https://matroska.org/technical/specs/index.html)
    . This will only be specified on frames where the HDR metadata and color /
    color space might have changed (e.g. keyframes).

## Functions ##

### SbMediaCanPlayMimeAndKeySystem ###

Returns information about whether the playback of the specific media described
by `mime` and encrypted using `key_system` can be played.

Note that neither `mime` nor `key_system` can be NULL. This function returns
`kSbMediaSupportNotSupported` if either is NULL.

`mime`: The mime information of the media in the form of `video/webm` or
`video/mp4; codecs="avc1.42001E"`. It may include arbitrary parameters like
"codecs", "channels", etc. Note that the "codecs" parameter may contain more
than one codec, delimited by comma. `key_system`: A lowercase value in fhe form
of "com.example.somesystem" as suggested by [https://w3c.github.io/encrypted-media/#key-system](https://w3c.github.io/encrypted-media/#key-system)) that can
be matched exactly with known DRM key systems of the platform. When `key_system`
is an empty string, the return value is an indication for non-encrypted media.

#### Declaration ####

```
SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char *mime, const char *key_system)
```

### SbMediaGetAudioConfiguration ###

Retrieves the current physical audio configuration of audio output
`output_index` on this device and places it in `out_configuration`, which must
not be NULL.

This function returns `false` if nothing could be determined on this platform or
if `output_index` does not exist on this device.

`out_configuration`: The variable that holds the audio configuration
information.

#### Declaration ####

```
bool SbMediaGetAudioConfiguration(int output_index, SbMediaAudioConfiguration *out_configuration)
```

### SbMediaGetAudioOutputCount ###

Returns the number of audio outputs currently available on this device. Even if
the number of outputs or their audio configurations can't be determined, it is
expected that the platform will at least return a single output that supports at
least stereo.

#### Declaration ####

```
int SbMediaGetAudioOutputCount()
```

### SbMediaIsOutputProtected ###

Indicates whether output copy protection is currently enabled on all capable
outputs. If `true`, then non-protection-capable outputs are expected to be
blanked.

#### Declaration ####

```
bool SbMediaIsOutputProtected()
```

### SbMediaIsSupported ###

Indicates whether this platform supports decoding `video_codec` and
`audio_codec` along with decrypting using `key_system`. If `video_codec` is
`kSbMediaVideoCodecNone` or if `audio_codec` is `kSbMediaAudioCodecNone`, this
function should return `true` as long as `key_system` is supported on the
platform to decode any supported input formats.

`video_codec`: The `SbMediaVideoCodec` being checked for platform compatibility.
`audio_codec`: The `SbMediaAudioCodec` being checked for platform compatibility.
`key_system`: The key system being checked for platform compatibility.

#### Declaration ####

```
bool SbMediaIsSupported(SbMediaVideoCodec video_codec, SbMediaAudioCodec audio_codec, const char *key_system)
```

### SbMediaSetOutputProtection ###

Enables or disables output copy protection on all capable outputs. If enabled,
then non-protection-capable outputs are expected to be blanked.

The return value indicates whether the operation was successful, and the
function returns a success even if the call is redundant in that it doesn't
change the current value.

`enabled`: Indicates whether output protection is enabled (`true`) or disabled.

#### Declaration ####

```
bool SbMediaSetOutputProtection(bool enabled)
```

