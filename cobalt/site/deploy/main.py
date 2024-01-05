# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Redirects traffic to the corresponding doc on the new devsite."""

import os


def app(environ, start_response):
  """Transforms the requested path to the matching document on
  https://developers.google.com/youtube/cobalt/ and redirects.

  This function is invoked by gunicorn in the App Engine default entrypoint:
    entrypoint: gunicorn -b :$PORT main:app

  Changing the file or function name requires configuring a matching entrypoint.
  """
  base_url = 'https://developers.google.com'
  path_prefix = '/youtube/cobalt/docs'
  # Remove extension from filename if present.
  doc_path = os.path.splitext(environ['RAW_URI'])[0]

  start_response('301 Moved Permanently',
                 [('Location', f'{base_url}{path_prefix}{doc_path}')])
  return []
