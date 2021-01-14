# Copyright © 2019-2020 Intel Corporation

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

# ===== NOTE =====
# This is a fork of the wrap available at
# https://wrapdb.mesonbuild.com/google-brotli.
#
# The brotli encoder contains static symbols with the same names in different
# translation units, so it does not support unity builds. Since we only need
# the decoder, we have simply removed the brotli_encoder_dep dependency in
# order to depend on brotli without breaking unity builds.
#
# We also define BROTLI_BUILD_PORTABLE whenever b_sanitize is set, even when
# default_library is static.

project(
  'google-brotli',
  'c',
  license : 'MIT',
  version : '1.0.7',
  meson_version : '>= 0.50',
)

cc = meson.get_compiler('c')

dep_m = cc.find_library('m', required : false)
if not cc.has_function('log2', dependencies : dep_m)
  error('Cannot find required log2() function')
endif

inc = include_directories('c/include')

if host_machine.system() == 'linux'
  add_project_arguments('-DOS_LINUX', language : 'c')
elif host_machine.system() == 'freebsd'
  add_project_arguments('-DOS_FREEBSD', language : 'c')
elif host_machine.system() == 'darwin'
  add_project_arguments('-DOS_MACOSX', language : 'c')
endif

c_args = []
install = false
if get_option('default_library') == 'shared'
  c_args += ['-DBROTLI_SHARED_COMPILATION', '-DBROTLI_BUILD_PORTABLE']
  install = true
elif get_option('default_library') == 'both'
  error('both not yet supported.')
elif 'undefined' in get_option('b_sanitize').split(',')
  # XXX Horrible hack XXX
  # brotli depends on undefined behavior otherwise.
  add_project_arguments('-DBROTLI_BUILD_PORTABLE')
endif

libbrotli_common = library(
  'brotli_common',
  ['c/common/dictionary.c', 'c/common/transform.c'],
  c_args : c_args,
  include_directories : inc,
  version : meson.project_version(),
  install : install,
)

libbrotli_decoder = library(
  'brotli_decoder',
  [
    'c/dec/bit_reader.c',
    'c/dec/decode.c',
    'c/dec/huffman.c',
    'c/dec/state.c',
  ],
  c_args : c_args,
  include_directories : inc,
  link_with : libbrotli_common,
  version : meson.project_version(),
  install : install,
)

brotli_decoder_dep = declare_dependency(
  link_with : [libbrotli_common, libbrotli_decoder],
  include_directories : inc,
  version : meson.project_version(),
)
