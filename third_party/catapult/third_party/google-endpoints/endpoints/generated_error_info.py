# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Automatically generated mapping of error codes."""

# pylint: disable=g-bad-name

import collections


_ErrorInfo = collections.namedtuple(
    '_ErrorInfo', ['http_status', 'rpc_status', 'reason', 'domain'])

_UNSUPPORTED_ERROR = _ErrorInfo(404,
                                404,
                                'unsupportedProtocol',
                                'global')
_BACKEND_ERROR = _ErrorInfo(503,
                            -32099,
                            'backendError',
                            'global')
_ERROR_MAP = {
    400: _ErrorInfo(400, 400, 'badRequest', 'global'),
    401: _ErrorInfo(401, 401, 'required', 'global'),
    402: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    403: _ErrorInfo(403, 403, 'forbidden', 'global'),
    404: _ErrorInfo(404, 404, 'notFound', 'global'),
    405: _ErrorInfo(501, 501, 'unsupportedMethod', 'global'),
    406: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    407: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    408: _ErrorInfo(503, -32099, 'backendError', 'global'),
    409: _ErrorInfo(409, 409, 'conflict', 'global'),
    410: _ErrorInfo(410, 410, 'deleted', 'global'),
    411: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    412: _ErrorInfo(412, 412, 'conditionNotMet', 'global'),
    413: _ErrorInfo(413, 413, 'uploadTooLarge', 'global'),
    414: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    415: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    416: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    417: _ErrorInfo(404, 404, 'unsupportedProtocol', 'global'),
    }


def get_error_info(lily_status):
  """Get info that would be returned by the server for this HTTP status.

  Args:
    lily_status: An integer containing the HTTP status returned by the SPI.

  Returns:
    An _ErrorInfo object containing information that would be returned by the
    live server for the provided lily_status.
  """
  if lily_status >= 500:
    return _BACKEND_ERROR

  return _ERROR_MAP.get(lily_status, _UNSUPPORTED_ERROR)
