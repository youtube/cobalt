# Copyright (c) 2021 Benjamin A. Beasley
# Copyright (c) 2019-2020 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# This wrap is based closely on the google-brotli wrap.

# ===== NOTE =====
# This is a fork of the wrap available at
# https://wrapdb.mesonbuild.com/google-woff2.
#
# The woff2 encoder relies on the brotli encoder, which contains static symbols
# with the same names in different translation units, so it does not support
# unity builds. Since we only need the decoder, we have simply removed the
# woff2_encoder_dep dependency in order to depend on woff2 without breaking
# unity builds (via the transitive dependency on brotli).

project(
  'google-woff2',
  'cpp',
  license : 'MIT',
  version : '1.0.2',
  meson_version : '>= 0.55',
)

inc = include_directories('include')

if not get_option('canonical_prefixes')
  add_project_arguments('-no-canonical-prefixes', language : 'cpp')
endif
if not get_option('noisy_logging')
  add_project_arguments('-DFONT_COMPRESSION_BIN', language : 'cpp')
endif
add_project_arguments('-D__STDC_FORMAT_MACROS', language : 'cpp')
add_project_arguments('-fno-omit-frame-pointer', language : 'cpp')
if host_machine.system() == 'darwin'
  add_project_arguments('-DOS_MACOSX', language : 'cpp')
endif

# From https://github.com/khaledhosny/ots/:
if 'undefined' in get_option('b_sanitize').split(',')
  # woff2 depends on undefined behavior otherwise.
  add_project_arguments('-U__BYTE_ORDER__', language : 'cpp')
endif

libwoff2_common = library(
  'woff2_common',
  ['src/table_tags.cc', 'src/variable_length.cc', 'src/woff2_common.cc'],
  include_directories : inc,
  version : meson.project_version(),
)

woff2_common_dep = declare_dependency(
  link_with : libwoff2_common,
  include_directories : inc,
  version : meson.project_version(),
)
meson.override_dependency('libwoff2common', woff2_common_dep)

woff2_info = executable(
  'woff2_info',
  ['src/woff2_info.cc'],
  include_directories : inc,
  link_with : libwoff2_common)
meson.override_find_program('woff2_info', woff2_info)

libwoff2_decoder = library(
  'woff2_decoder',
  ['src/woff2_dec.cc', 'src/woff2_out.cc'],
  include_directories : inc,
  link_with : libwoff2_common,
  dependencies : [dependency('libbrotlidec',
                             fallback : ['google-brotli',
                                         'brotli_decoder_dep'])],
  version : meson.project_version(),
)

woff2_decoder_dep = declare_dependency(
  link_with : [libwoff2_common, libwoff2_decoder],
  include_directories : inc,
  version : meson.project_version(),
)
meson.override_dependency('libwoff2dec', woff2_decoder_dep)

woff2_decompress = executable(
  'woff2_decompress',
  ['src/woff2_decompress.cc'],
  include_directories : inc,
  link_with : [libwoff2_common, libwoff2_decoder])
meson.override_find_program('woff2_decompress', woff2_decompress)
