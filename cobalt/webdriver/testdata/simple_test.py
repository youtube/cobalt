#!/usr/bin/env python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Simple script for Webdriver Wire Protocol communication."""

import binascii
import json
import time
import requests

# This is a simple script for Webdriver Wire Protocol communication.
#   https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol

WEBDRIVER_HOST = 'http://localhost:4444'

GET = 'GET'
POST = 'POST'
DELETE = 'DELETE'

# WebDriver Response Status Codes:
# https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Response-Status-Codes

RESPONSE_STATUS_CODES = {
    0: 'Success',
    6: 'No Such Driver',
    7: 'No Such Element',
    8: 'No Such Frame',
    9: 'Unknown Command',
    10: 'Stale Element Reference',
    11: 'Element Not Visible',
    12: 'Invalid Element State',
    13: 'Unknown Error',
    15: 'Element Is Not Selectable',
    17: 'JavaScript Error',
    19: 'XPath Lookup Error',
    21: 'Timeout',
    23: 'No Such Window',
    24: 'Invalid Cookie Domain',
    25: 'Unable To Set Cookie',
    26: 'Unexpected Alert Open',
    27: 'No Alert Open Error',
    28: 'Script Timeout',
    29: 'Invalid Element Coordinates',
    30: 'IME Not Available',
    31: 'IME Engine Activation Failed',
    32: 'Invalid Selector',
    33: 'Session Not Created Exception',
    34: 'Move Target Out Of Bounds'
}


def Request(request_type, path='', parameters=None):
  """Perform a WebDriver JSON Wire Protocol Request.

  Args:
    request_type: GET, POST, or DELETE
    path: optional path string for the request
    parameters: optional parameter dictionary for the request

  Returns:
    The dictionary returned by the WebDriver server
  """
  url = '%s/%s' % (WEBDRIVER_HOST, path)
  headers = {'content-type': 'application/json', 'Accept-Charset': 'UTF-8'}
  if request_type == GET:
    request = requests.get(url, data=json.dumps(parameters), headers=headers)
  if request_type == POST:
    request = requests.post(url, data=json.dumps(parameters), headers=headers)
  if request_type == DELETE:
    request = requests.delete(url, data=json.dumps(parameters), headers=headers)
  result = request.text if request.headers[
      'content-type'] == 'text/plain' else request.json()
  if request.status_code == 200:
    return result
  else:
    print('*** Error %d %s: \"%s\"' %
          (request.status_code, RESPONSE_STATUS_CODES[result['status']]
           if isinstance(result, dict) else 'unknown',
           result['value']['message'] if isinstance(result, dict) else result))
    print('*** Error %d: %s' % (request.status_code, result))
  return None


def SessionRequest(session_id, request_type, path=None, parameters=None):
  """Perform a WebDriver JSON Wire Protocol Session Request.

  Args:
    session_id: Value for ':sessionId' for the request
    request_type: GET, POST, or DELETE
    path: optional path string for the request
    parameters: optional parameter dictionary for the request

  Returns:
    The dictionary returned by the WebDriver server
  """
  if path:
    return Request(request_type, 'session/%s/%s' % (session_id, path),
                   parameters)
  return Request(request_type, 'session/%s' % (session_id), parameters)


def ElementRequest(session_id,
                   element_id,
                   request_type,
                   path=None,
                   parameters=None):
  return SessionRequest(session_id, request_type,
                        'element/%s/%s' % (element_id[u'ELEMENT'], path),
                        parameters)


def GetSessionID():
  """Retrieve a WebDriver JSON Wire Protocol Session.

  Returns:
    The value for ':sessionId' to be used for Session Requests
  """
  request = Request(POST, 'session', {'desiredCapabilities': {}})
  if request:
    session_id = request[u'sessionId']
  else:
    # If creating a new session id fails, use an already existing session.
    request = Request(GET, 'sessions')
    if request:
      session_id = request['value'][0]
  return session_id


def DeleteSession(session_id):
  """Delete a WebDriver JSON Wire Protocol Session.

  Args:
    session_id: Value for ':sessionId' for the request

  Returns:
    The dictionary returned by the WebDriver server
  """
  return SessionRequest(session_id, DELETE)


def GetScreenShot(session_id, filename):
  """Retrieve a Screenshot.

  Args:
    session_id: Value for ':sessionId' for the request
    filename: The filename to write the PNG screenshot to
  """
  request = SessionRequest(session_id, GET, 'screenshot')
  if request:
    with open(filename, 'w') as f:
      f.write(binascii.a2b_base64(request['value']))
      f.close()


def GetElementScreenShot(session_id, element_id, filename):
  """Retrieve a Screenshot.

  Args:
    session_id: Value for ':sessionId' for the request
    filename: The filename to write the PNG screenshot to
  """
  request = ElementRequest(session_id, element_id, GET, 'screenshot')
  if request:
    with open(filename, 'w') as f:
      f.write(binascii.a2b_base64(request['value']))
      f.close()


def GetActiveElement(session_id):
  return SessionRequest(session_id, POST, 'element/active')['value']


def Moveto(session_id, element, xoffset, yoffset):
  return SessionRequest(session_id, POST, 'moveto', {
      u'element': element,
      u'xoffset': xoffset,
      u'yoffset': yoffset
  })


def Click(session_id, button):
  return SessionRequest(session_id, POST, 'click', {u'button': button})


def Buttondown(session_id, button):
  return SessionRequest(session_id, POST, 'buttondown', {u'button': button})


def Buttonup(session_id, button):
  return SessionRequest(session_id, POST, 'buttonup', {u'button': button})


def ElementName(session_id, element_id):
  return ElementRequest(session_id, element_id, GET, 'name')


def ElementText(session_id, element_id):
  return ElementRequest(session_id, element_id, GET, 'text')


def ElementClick(session_id, element_id, button):
  return ElementRequest(session_id, element_id, POST, 'click',
                        {u'button': button})


def ElementKeys(session_id, element_id, keys):
  return ElementRequest(session_id, element_id, POST, 'value', {u'value': keys})


def ElementFind(session_id, using, value):
  result = SessionRequest(session_id, POST, 'element', {
      u'using': using,
      u'value': value
  })
  return None if result is None else result['value']


def MouseTest():
  # Do a simple test that hovers the mouse to the right from the active
  # element, then clicks on the element of class 'trending'.
  session_id = GetSessionID()
  try:
    active_element = GetActiveElement(session_id)
    print('active_element : %s' % active_element)

    for xoffset in range(0, 1900, 20):
      print('Moveto: %s' % Moveto(session_id, active_element, xoffset, 200))
      time.sleep(0.05)

    selected_element = ElementFind(session_id, 'class name',
                                   'ytlr-tile-renderer--focused')
    print('selected_element : %s' % selected_element)

    print('ElementClick: %s' % ElementClick(session_id, selected_element, 0))

  except KeyboardInterrupt:
    print('Bye')

  DeleteSession(session_id)


def ElementScreenShotTest():
  # Do a simple test that hovers the mouse to the right from the active
  # element, then clicks on the element of class 'trending'.
  session_id = GetSessionID()
  try:
    selected_element = ElementFind(session_id, 'class name',
                                   'ytlr-tile-renderer--focused')
    print('Selected List element : %s' % selected_element)

    # Write screenshots for the selected element, until interrupted.
    while True:
      selected_element = ElementFind(session_id, 'class name',
                                     'ytlr-tile-renderer--focused')
      print('Selected List element : %s' % selected_element)
      if selected_element is not None:
        print('GetElementScreenShot: %s' % GetElementScreenShot(
            session_id, selected_element,
            'element-' + selected_element['ELEMENT'] + '.png'))

  except KeyboardInterrupt:
    print('Bye')

  DeleteSession(session_id)


def ElementUniqueTest():
  # Do a simple test that keeps running as long as the element IDs stay the
  # same.
  session_id = GetSessionID()
  try:
    initial_active_element = GetActiveElement(session_id)
    print('initial active_element : %s' % initial_active_element)

    initial_selected_element = ElementFind(session_id, 'class name',
                                           'ytlr-tile-renderer--focused')
    print('Selected List element : %s' % initial_selected_element)
    while True:
      active_element = GetActiveElement(session_id)
      print('active_element : %s' % active_element)

      if initial_active_element != active_element:
        break

      selected_element = ElementFind(session_id, 'class name',
                                     'ytlr-tile-renderer--focused')
      print('Selected List element : %s' % selected_element)

      if initial_selected_element != selected_element:
        break

      time.sleep(1)

  except KeyboardInterrupt:
    print('Bye')


def main():
  MouseTest()
  ElementScreenShotTest()
  # ElementUniqueTest()


if __name__ == '__main__':
  main()
