# Test vectors

Test vectors are grouped with a common prefix. For example test_000012 has
several files associated with it.

-   Metadata describing the test vector:`test_000012.textproto`.
-   Standalone IAMF bitstream: `test_000012.iamf`.
-   Fragmented MP4 file: `test_000012_f.mp4`.
-   Standalone MP4 file: `test_000012_s.mp4`.
-   Decoded WAV file (per substream x): `test_000012_decoded_substream_x.wav`
-   Rendered WAV file (per `mix_presentation_id` x, sub mix index y, layout
    index z): `test_000012_rendered_id_x_sub_mix_y_layout_z.wav`

## .textproto files

Theses file describe metadata about the test vector to encode an
[IA Sequence](https://aomediacodec.github.io/iamf/#standalone-ia-sequence).

-   `is_valid`: True when the encoder can produce an IA Sequence where all mixes
    would be understood by a compliant decoder. False when one or more mixes
    exercise fields or features which would cause mixes to be ignored.
-   `is_valid_to_decode`: True when an IAMF-compliant decoder could decode at
    least one mix of the associated IA Sequence ("should-pass"). False when all
    mixes are non-conformant and may fail to be decoded ("should-fail"). The
    IAMF spec does not specify what happens when requirements are violated; a
    robust system may still attempt to process and create output for
    "should-fail" tests.
-   `human_readable_descriptions`: A short description of what is being tested
    and why.
-   `mp4_fixed_timestamp`: The timestamp within the MP4 file. Can be safely
    ignored.
-   `primary_tested_spec_sections`: A list of the main sections being tested. In
    the form `X.Y.Z/class_or_field_name` to represent the `class_or_field_name`
    in the IAMF specification Section `X.Y.Z` is being tested.
-   `base_test`: The recommended textproto to diff against.
-   Other fields refer to the OBUs and data within the test vector.

# Input WAV files

Test vectors may have multiple substreams with several input .wav files. These
.wav files may be shared with other test vectors. The .textproto file has a
section which input wav file associated with each substream.

## Summary

Title                                                | Summary                                                                                                             | Channels | Sample Rate | Format    | Duration
---------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | -------- | ----------- | --------- | --------
`audiolab-acoustic-guitar_2OA_470_ALLRAD_0.5s`       | Short clip of a guitar playing using 7.1.4.                                                                         | 12       | 48kHz       | pcm_s16le | 500ms
`audiolab-acoustic-guitar_2OA_470_ALLRAD_5s.wav`     | Short clip of a guitar playing using 7.1.4.                                                                         | 12       | 48kHz       | pcm_s16le | 5s
`audiolab-acoustic-guitar_2OA_470_ALLRAD_concat.wav` | Clip of a guitar playing which which is repeated once using 7.1.4.                                                  | 12       | 48kHz       | pcm_s16le | 22.77s
`dialog_clip_stereo.wav`                             | English dialog.                                                                                                     | 2        | 48kHz       | pcm_s16le | 5s
`Mechanism_5s_32bit.wav`                             | Mechanical noises using 7.1.4.                                                                                      | 12       | 48kHz       | pcm_s32le | 5s
`Mechanism_5s.wav`                                   | Mechanical noises using 7.1.4.                                                                                      | 12       | 48kHz       | pcm_s16le | 5s
`sample1_48kHz_stereo.wav`                           | Sawtooth wave.                                                                                                      | 2        | 48kHz       | pcm_s16le | 5s
`sawtooth_10000_foa_48kHz.wav`                       | Sawtooth wave using first-order ambisonics.                                                                         | 4        | 48kHz       | pcm_s16le | 500ms
`sawtooth_10000_stereo_44100hz_s16le.wav`            | Sawtooth wave.                                                                                                      | 2        | 44.1kHz     | pcm_s16le | 500ms
`sawtooth_10000_stereo_48kHz_s24le.wav`              | Sawtooth wave.                                                                                                      | 2        | 48kHz       | pcm_s24le | 500ms
`sawtooth_10000_stereo_48kHz.wav`                    | Sawtooth wave.                                                                                                      | 2        | 48kHz       | pcm_s16le | 500ms
`sawtooth_100_stereo.wav`                            | Sawtooth wave.                                                                                                      | 2        | 16kHz       | pcm_s16le | 500ms
`sawtooth_8000_toa_48kHz.wav`                        | Sawtooth wave using third-order ambisonics.                                                                         | 16       | 48kHz       | pcm_s16le | 500ms
`sine_1000_16kHz_512ms.wav`                          | Sine wave.                                                                                                          | 2        | 16kHz       | pcm_s16le | 512ms
`sine_1000_16khz_512ms_s32le.wav`                    | Sine wave.                                                                                                          | 1        | 16kHz       | pcm_s32le | 512ms
`sine_1000_48kHz_512ms.wav`                          | Sine wave.                                                                                                          | 2        | 48kHz       | pcm_s16le | 512ms
`sine_1000_48kHz.wav`                                | Sine wave.                                                                                                          | 2        | 48kHz       | pcm_s16le | 500ms
`sine_1000_4oa_48kHz.wav`                            | Sine wave using fourth-order ambisonics.                                                                            | 25       | 48kHz       | pcm_s16le | 5000ms
`stereo_8_samples_48khz_s16le.wav`                   | Tiny test file. The first channel encodes 1, 2, ... 8. The second channel encodes 65535, 65534, ... 65528.          | 2        | 48kHz       | pcm_s16le | 8 samples
`stereo_8_samples_48khz_s24le.wav`                   | Tiny test file. The first channel encodes 1, 2, ... 8. The second channel encodes 16777216, 16777215, ... 16777209. | 2        | 48kHz       | pcm_s24le | 8 samples
`Transport_TOA_5s.wav`                               | Short clip of vehicles driving by using third-order ambisonics.                                                     | 16       | 48kHz       | pcm_s16le | 5s
