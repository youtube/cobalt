#!/usr/bin/env python

# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Posts an Evergreen update check against Omaha backends."""

import _env  # pylint: disable=unused-import

import argparse
import sys
import requests
import json as simplejson

from starboard.sabi import generate_sabi_id

# The Evergreen application ID on Omaha.
_APP_ID = '{6D4E53F3-CC64-4CB8-B6BD-AB0B8F300E1C}'

# The production Omaha endpoint.
_ENDPOINT = 'https://tools.google.com/service/update2/json'

# The qa Omaha endpoint.
_ENDPOINT_QA = 'https://omaha-qa.sandbox.google.com/service/update2/json'

_HEADERS = {'Content-type': 'application/json'}

# A minified Evergreen update request that would be POST'd to an Omaha backend.
_REQUEST = {
    'request': {
        '@os': 'starboard',
        '@updater': 'cobalt',
        'acceptformat': 'crx2,crx3',
        'app': [{
            'appid': _APP_ID,
            'enabled': True,
            'ping': {
                'r': -2
            },
            'updatecheck': {},
        }],
        'arch': '',
        'chipset': '',
        'dedup': 'cr',
        'firmware': '',
        'hw': {},
        'jsengine': 'v8/8.8.278.8-jit',
        'lang': 'en_US',
        'manufacturer': '',
        'model': '',
        'nacl_arch': '',
        'os': {
            'arch': '',
            'platform': ''
        },
        'prodchannel': 'qa',
        'prodversion': '21',
        'protocol': '3.1',
        'requestid': '',
        'sessionid': '',
        'uastring': '',
        'updaterchannelchanged': 'False',
        'updaterversion': '21',
        'year': ''
    }
}


def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '-b',
      '--brand',
      default='',
      help='Which brand to use.',
  )
  arg_parser.add_argument(
      '-c',
      '--channel',
      default='qa',
      help='Which channel to use.',
  )
  arg_parser.add_argument(
      '-q',
      '--qa',
      action='store_true',
      default=False,
      help='Whether to use the Omaha QA backend.',
  )
  arg_parser.add_argument(
      '-s',
      '--sbversion',
      default='12',
      help='Which Starboard API version to use.',
  )
  arg_parser.add_argument(
      '-v',
      '--version',
      default='1.0.0',
      help='Which Evergreen version to use.',
  )
  args, _ = arg_parser.parse_known_args()

  _REQUEST['request']['SABI'] = generate_sabi_id.DoMain()
  _REQUEST['request']['brand'] = args.brand
  _REQUEST['request']['updaterchannel'] = args.channel
  _REQUEST['request']['sbversion'] = args.sbversion
  _REQUEST['request']['app'][0]['version'] = args.version

  print('Querying: [[ {} ]]'.format(_ENDPOINT_QA if args.qa else _ENDPOINT))
  print('Request:  [[ {} ]]'.format(simplejson.dumps(_REQUEST)))

  print(
      requests.post(
          _ENDPOINT_QA if args.qa else _ENDPOINT,
          data=simplejson.dumps(_REQUEST),
          headers=_HEADERS).text)

  return 0


if __name__ == '__main__':
  sys.exit(main())
