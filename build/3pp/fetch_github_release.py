# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import os
import pathlib
import re
import urllib.request


def _fetch_json(url):
    return json.load(urllib.request.urlopen(url))


def _latest(api_url, install_scripts=None):
    # Make the version change every time this file changes.
    md5 = hashlib.md5()
    md5.update(pathlib.Path(__file__).read_bytes())
    import __main__
    md5.update(pathlib.Path(__main__.__file__).read_bytes())

    if install_scripts:
        for path in install_scripts:
            md5.update(pathlib.Path(path).read_bytes())
    file_hash = md5.hexdigest()[:10]

    release = _fetch_json(f'{api_url}/releases/latest')['tag_name']
    print('{}.{}'.format(release, file_hash))


def _get_url(api_url,
             artifact_filename=None,
             artifact_extension=None,
             artifact_regex=None):
    # Split off our md5 hash.
    version = os.environ['_3PP_VERSION'].rsplit('.', 1)[0]
    json_dict = _fetch_json(f'{api_url}/releases/tags/{version}')
    urls = [x['browser_download_url'] for x in json_dict['assets']]

    if artifact_regex:
        urls = [x for x in urls if re.search(artifact_regex, x)]

    if len(urls) != 1:
        raise Exception('len(urls) != 1: \n' + '\n'.join(urls))

    partial_manifest = {
        'url': urls,
        'ext': artifact_extension or '',
    }
    if artifact_filename:
        partial_manifest['name'] = [artifact_filename]

    print(json.dumps(partial_manifest))


def main(*,
         project,
         artifact_filename=None,
         artifact_extension=None,
         artifact_regex=None,
         install_scripts=None,
         extract_extension=None):
    """The fetch.py script for a 3pp module.

    Args:
      project: GitHub username for the repo. e.g. "google/protobuf".
      artifact_filename: The name for the downloaded file. Required when not
          setting "unpack_archive: true" in 3pp.pb.
      artifact_extension: File extension of file being downloaded. Required when
          setting "unpack_archive: true" in 3pp.pb.
      artifact_regex: A regex to use to identify the desired artifact from the
          list of artifacts on the release.
      install_scripts: List of script to add to the md5 of the version. The main
          module and this module are always included.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('latest', 'get_url'))
    args = parser.parse_args()

    api_url = f'https://api.github.com/repos/{project}'
    if args.action == 'latest':
        _latest(api_url, install_scripts=install_scripts)
    else:
        _get_url(api_url,
                 artifact_filename=artifact_filename,
                 artifact_extension=artifact_extension,
                 artifact_regex=artifact_regex)


if __name__ == '__main__':
    main()
