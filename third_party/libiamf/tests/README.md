# Test vectors
Test vectors are grouped with a common prefix. For example test_000012 has several files associated with it.

- Metadata describing the test vector:`test_000012.textproto`.
- Standalone IAMF bitstream: `test_000012.iamf`.
- Fragmented MP4 file: `test_000012_f.mp4`.
- Standalone MP4 file: `test_000012_s.mp4`.
- Decoded WAV file (per substream x): `test_000012_decoded_substream_x.wav`
- Rendered WAV file (per `mix_presentation_id` x, sub mix index y, layout index z): `test_000012_rendered_id_x_sub_mix_y_layout_z.wav`

## .textproto files
These file describe metadata about the test vector.

- `is_valid`: True for conformant bitstreams ("should-pass"). False for non-conformant bitstreams ("should-fail").
- `human_readable_descriptions`: A short description of what is being tested and why.
- `mp4_fixed_timestamp`: The timestamp within the MP4 file. Can be safely ignored.
- `primary_tested_spec_sections`: A list of the main sections being tested. In the form `X.Y.Z/class_or_field_name` to represent the `class_or_field_name` in the IAMF specification Section `X.Y.Z` is being tested.
- `base_test`: The recommended textproto to diff against.
- Other fields refer to the OBUs and data within the test vector.

# Input WAV files

Test vectors may have multiple substreams with several input .wav files. These .wav files may be shared with other test vectors. The .textproto file has a section which input wav file associated with each substream.
