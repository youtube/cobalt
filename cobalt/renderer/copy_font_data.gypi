# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file is meant to be included by a GYP target that needs to copy the
# platform specific font data into <(PRODUCT_DIR)/content.

{
  'variables': {
    'static_contents_dir': '<(DEPTH)/lbshell/content',
    'fonts_dir': '<(static_contents_dir)/fonts',

    'input_fonts': [
      '<(fonts_dir)/DroidNaskh-Bold.ttf',
      '<(fonts_dir)/DroidNaskh-Regular.ttf',
      '<(fonts_dir)/DroidSans-Bold.ttf',
      '<(fonts_dir)/DroidSans.ttf',
      '<(fonts_dir)/DroidSansArmenian.ttf',
      '<(fonts_dir)/DroidSansEthiopic-Bold.ttf',
      '<(fonts_dir)/DroidSansEthiopic-Regular.ttf',
      '<(fonts_dir)/DroidSansFallbackFull.ttf',
      '<(fonts_dir)/DroidSansGeorgian.ttf',
      '<(fonts_dir)/DroidSansHebrew-Bold.ttf',
      '<(fonts_dir)/DroidSansHebrew-Regular.ttf',
      '<(fonts_dir)/DroidSansJapanese.ttf',
      '<(fonts_dir)/DroidSansThai.ttf',
      '<(fonts_dir)/Lohit-Bengali.ttf',
      '<(fonts_dir)/Lohit-Devanagari.ttf',
      '<(fonts_dir)/Lohit-Gujarati.ttf',
      '<(fonts_dir)/Lohit-Kannada.ttf',
      '<(fonts_dir)/Lohit-Malayalam.ttf',
      '<(fonts_dir)/Lohit-Oriya.ttf',
      '<(fonts_dir)/Lohit-Punjabi.ttf',
      '<(fonts_dir)/Lohit-Tamil.ttf',
      '<(fonts_dir)/Lohit-Telugu.ttf',
      '<(fonts_dir)/OpenSans-Regular.ttf',
      # TODO(***REMOVED***): Remove the following three items and font files when @font-face is implemented.
      '<(fonts_dir)/icons.ttf',
      '<(fonts_dir)/Roboto-Bold.ttf',
      '<(fonts_dir)/Roboto-Regular.ttf',
    ],
  },

  'copies': [
    {
      'destination': '<(PRODUCT_DIR)/content/data/fonts',
      'files': [ '<@(input_fonts)' ],
    },
  ],
}
