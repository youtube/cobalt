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
    print '*** Error %d: %s' % (request.status_code, result['value']['message']
                                if isinstance(result, dict) else result)
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
  return SessionRequest(session_id, request_type, 'element/%s/%s' %
                        (element_id[u'ELEMENT'], path), parameters)


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
  return SessionRequest(session_id, POST, 'element',
                        {u'using': using,
                         u'value': value})['value']


def MouseTest():
  # Do a simple test that hovers the mouse to the right from the active
  # element, then clicks on the element of class 'trending'.
  session_id = GetSessionID()
  active_element = GetActiveElement(session_id)
  print 'active_element : %s' % active_element

  for xoffset in range(0, 1900, 20):
    print 'Moveto: %s' % Moveto(session_id, active_element, xoffset, 200)
    time.sleep(0.05)

  trending_element = ElementFind(session_id, 'class name', 'trending')
  print 'trending_element : %s' % trending_element

  print 'ElementClick: %s' % ElementClick(session_id, trending_element, 0)

  DeleteSession(session_id)


def main():
  MouseTest()


if __name__ == '__main__':
  main()
