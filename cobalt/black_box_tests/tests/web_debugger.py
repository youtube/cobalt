# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

"""Connect to the web debugger and run some commands."""

# pylint: disable=g-doc-args,g-doc-return-or-yield,g-doc-exception

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import,g-bad-import-order

import json
import logging
import os
import sys
import urlparse

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), '..', '..', '..',
        'third_party', 'websocket-client'))
import websocket  # pylint: disable=g-bad-import-order,g-import-not-at-top

# Set to True to add additional logging to debug the test.
_DEBUG = False


class DebuggerCommandError(Exception):
  """Exception when an error response is received for a command."""

  def __init__(self, error):
    code = '[{}] '.format(error['code']) if 'code' in error else ''
    super(DebuggerCommandError, self).__init__(code + error['message'])


class DebuggerConnection(object):
  """Connection to debugger over a WebSocket.

  Runs in the owner's thread by receiving and storing messages as needed when
  waiting for a command response or event.
  """

  def __init__(self, ws_url, timeout=3):
    self.ws_url = ws_url
    self.timeout = timeout
    self.last_id = 0
    self.commands = dict()
    self.responses = dict()
    self.events = list()

  def __enter__(self):
    websocket.enableTrace(_DEBUG)
    logging.debug('Debugger WebSocket: %s', self.ws_url)
    self.ws = websocket.create_connection(self.ws_url, timeout=self.timeout)
    return self

  def __exit__(self, exc_type, exc_value, exc_traceback):
    self.ws.close()

  def run_command(self, method, params=None):
    """Runs a debugger command and waits for the response.

    Fails the test with an exception if the debugger backend returns an error
    response, or the command times out without receiving a response.
    """
    response = self.wait_response(self.send_command(method, params))
    if 'error' in response:
      raise DebuggerCommandError(response['error'])
    return response

  def send_command(self, method, params=None):
    """Sends an async debugger command and returns the command id."""
    self.last_id += 1
    msg = {
        'id': self.last_id,
        'method': method,
    }
    if params:
      msg['params'] = params
    if _DEBUG:
      logging.debug('send >>>>>>>>\n%s\n>>>>>>>>',
                    json.dumps(msg, sort_keys=True, indent=4))
    logging.debug('send command: %s', method)
    self.ws.send(json.dumps(msg))
    self.commands[self.last_id] = msg
    return self.last_id

  def wait_response(self, command_id):
    """Wait for the response for a command to arrive.

    Fails the test with an exception if the WebSocket times out without
    receiving the response.
    """
    assert command_id in self.commands
    # Receive messages until the response we want arrives.
    # It may have already arrived before we even enter the loop.
    while command_id not in self.responses:
      try:
        self._receive_message()
      except websocket.WebSocketTimeoutException as err:
        # Show which command response we were waiting for that timed out.
        err.args = ('Command', self.commands[command_id])
        raise
    self.commands.pop(command_id)
    return self.responses.pop(command_id)

  def wait_event(self, method):
    """Wait for an event to arrive.

    Fails the test with an exception if the WebSocket times out without
    receiving the event.
    """
    # Check if we already received the event.
    for event in self.events:
      if event['method'] == method:
        self.events.remove(event)
        return event
    # Receive messages until the event we want arrives.
    while not self.events or self.events[-1]['method'] != method:
      try:
        self._receive_message()
      except websocket.WebSocketTimeoutException as err:
        # Show which event we were waiting for that timed out.
        err.args = ('Event', method)
        raise
    return self.events.pop()

  def _receive_message(self):
    """Receives one message and stores it in either responses or events."""
    msg = json.loads(self.ws.recv())
    if _DEBUG:
      logging.debug('recv <<<<<<<<\n%s\n<<<<<<<<',
                    json.dumps(msg, sort_keys=True, indent=4))
    if 'id' in msg:
      self.responses[msg['id']] = msg
    elif 'method' in msg:
      logging.debug('receive event: %s', msg['method'])
      self.events.append(msg)
    else:
      raise ValueError('Unexpected debugger message', msg)

  def enable_runtime(self):
    """Helper to enable the 'Runtime' domain and save the context_id."""
    self.run_command('Runtime.enable')
    context_event = self.wait_event('Runtime.executionContextCreated')
    self.context_id = context_event['params']['context']['id']
    assert self.context_id > 0

  def evaluate_js(self, expression):
    """Helper for the 'Runtime.evaluate' command to run some JavaScript."""
    return self.run_command('Runtime.evaluate', {
        'contextId': self.context_id,
        'expression': expression,
    })


class WebDebuggerTest(black_box_tests.BlackBoxTestCase):
  """Test interaction with the web debugger over a WebSocket."""

  def setUp(self):
    platform_vars = self.platform_config.GetVariables(self.device_params.config)
    if platform_vars['javascript_engine'] != 'v8':
      self.skipTest('DevTools requires V8')

    cobalt_vars = self.cobalt_config.GetVariables(self.device_params.config)
    if not cobalt_vars['enable_debugger']:
      self.skipTest('DevTools is disabled on this platform')

  def create_debugger_connection(self, runner):
    devtools_url = runner.GetCval('Cobalt.Server.DevTools')
    parts = list(urlparse.urlsplit(devtools_url))
    parts[0] = 'ws'  # scheme
    parts[2] = '/devtools/page/cobalt'  # path
    ws_url = urlparse.urlunsplit(parts)
    return DebuggerConnection(ws_url)

  def test_runtime(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/web_debugger.html')
      with self.CreateCobaltRunner(url=url) as runner:
        with self.create_debugger_connection(runner) as debugger:
          runner.WaitForJSTestsSetup()
          debugger.enable_runtime()

          # Evaluate a simple expression.
          eval_response = debugger.evaluate_js('6 * 7')
          self.assertEqual(42, eval_response['result']['result']['value'])

          # Set an attribute and read it back w/ WebDriver.
          debugger.evaluate_js(
              'document.body.setAttribute("web_debugger", "tested")')
          self.assertEqual(
              'tested',
              runner.UniqueFind('body').get_attribute('web_debugger'))

          # Log to console, and check we get the console event.
          debugger.evaluate_js('console.log("hello")')
          console_event = debugger.wait_event('Runtime.consoleAPICalled')
          self.assertEqual('hello', console_event['params']['args'][0]['value'])

          # End the test.
          debugger.evaluate_js('onEndTest()')
          self.assertTrue(runner.JSTestsSucceeded())

  def test_dom(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/web_debugger.html')
      with self.CreateCobaltRunner(url=url) as runner:
        with self.create_debugger_connection(runner) as debugger:
          runner.WaitForJSTestsSetup()
          debugger.enable_runtime()
          debugger.run_command('DOM.enable')

          doc_response = debugger.run_command('DOM.getDocument')
          doc_root = doc_response['result']['root']
          self.assertEqual('#document', doc_root['nodeName'])

          doc_url = doc_root['documentURL']
          # remove query params (cert_scope, etc.)
          doc_url = doc_url.split('?')[0]
          self.assertEqual(url, doc_url)

          # document: <html><head></head><body></body></html>
          html_node = doc_root['children'][0]
          body_node = html_node['children'][1]
          self.assertEqual('BODY', body_node['nodeName'])

          # body:
          #   <h1><span>Web debugger</span></h1>
          #   <div#test>
          #     <div#A><div#A1/><div#A2/></div#A>
          #     <div#B/>
          #   </div#test>
          debugger.run_command('DOM.requestChildNodes', {
              'nodeId': body_node['nodeId'],
              'depth': -1,  # entire subtree
          })
          child_nodes_event = debugger.wait_event('DOM.setChildNodes')

          h1 = child_nodes_event['params']['nodes'][0]
          span = h1['children'][0]
          text = span['children'][0]
          self.assertEqual('H1', h1['nodeName'])
          self.assertEqual('SPAN', span['nodeName'])
          self.assertEqual('#text', text['nodeName'])
          self.assertEqual('Web debugger', text['nodeValue'])

          test_div = child_nodes_event['params']['nodes'][1]
          child_a = test_div['children'][0]
          child_a1 = child_a['children'][0]
          child_a2 = child_a['children'][1]
          child_b = test_div['children'][1]
          self.assertEqual(2, test_div['childNodeCount'])
          self.assertEqual(2, child_a['childNodeCount'])
          self.assertEqual(0, child_b['childNodeCount'])
          self.assertEqual(['id', 'test'], test_div['attributes'])
          self.assertEqual(['id', 'A'], child_a['attributes'])
          self.assertEqual(['id', 'A1'], child_a1['attributes'])
          self.assertEqual(['id', 'A2'], child_a2['attributes'])
          self.assertEqual(['id', 'B'], child_b['attributes'])
          self.assertEqual([], child_b['children'])

          # Repeat, but only to depth 2 - not reporting children of A & B.
          debugger.run_command('DOM.requestChildNodes', {
              'nodeId': body_node['nodeId'],
              'depth': 2,
          })
          child_nodes_event = debugger.wait_event('DOM.setChildNodes')

          test_div = child_nodes_event['params']['nodes'][1]
          child_a = test_div['children'][0]
          child_b = test_div['children'][1]
          self.assertFalse('children' in child_a)
          self.assertFalse('children' in child_b)
          self.assertEqual(2, test_div['childNodeCount'])
          self.assertEqual(2, child_a['childNodeCount'])
          self.assertEqual(0, child_b['childNodeCount'])
          self.assertEqual(['id', 'test'], test_div['attributes'])
          self.assertEqual(['id', 'A'], child_a['attributes'])
          self.assertEqual(['id', 'B'], child_b['attributes'])

          # Repeat, to default depth of 1 - not reporting children of "#test".
          debugger.run_command('DOM.requestChildNodes', {
              'nodeId': body_node['nodeId'],
          })
          child_nodes_event = debugger.wait_event('DOM.setChildNodes')

          test_div = child_nodes_event['params']['nodes'][1]
          self.assertFalse('children' in test_div)
          self.assertEqual(2, test_div['childNodeCount'])
          self.assertEqual(['id', 'test'], test_div['attributes'])

          # Get the test div as a remote object, and request it as a node.
          # This sends a 'DOM.setChildNodes' event for each node up to the root.
          eval_result = debugger.evaluate_js('document.getElementById("test")')
          node_response = debugger.run_command('DOM.requestNode', {
              'objectId': eval_result['result']['result']['objectId'],
          })
          self.assertEqual(test_div['nodeId'],
                           node_response['result']['nodeId'])

          # Event reporting the requested <div#test>
          node_event = debugger.wait_event('DOM.setChildNodes')
          self.assertEqual(test_div['nodeId'],
                           node_event['params']['nodes'][0]['nodeId'])
          self.assertEqual(body_node['nodeId'],
                           node_event['params']['parentId'])

          # Event reporting the parent <body>
          node_event = debugger.wait_event('DOM.setChildNodes')
          self.assertEqual(body_node['nodeId'],
                           node_event['params']['nodes'][0]['nodeId'])
          self.assertEqual(html_node['nodeId'],
                           node_event['params']['parentId'])

          # Event reporting the parent <html>
          node_event = debugger.wait_event('DOM.setChildNodes')
          self.assertEqual(html_node['nodeId'],
                           node_event['params']['nodes'][0]['nodeId'])
          self.assertEqual(doc_root['nodeId'], node_event['params']['parentId'])

          # Round trip resolving test div to an object, then back to a node.
          resolve_response = debugger.run_command('DOM.resolveNode', {
              'nodeId': test_div['nodeId'],
          })
          node_response = debugger.run_command('DOM.requestNode', {
              'objectId': resolve_response['result']['object']['objectId'],
          })
          self.assertEqual(test_div['nodeId'],
                           node_response['result']['nodeId'])

          # Event reporting the requested <div#test>
          node_event = debugger.wait_event('DOM.setChildNodes')
          self.assertEqual(test_div['nodeId'],
                           node_event['params']['nodes'][0]['nodeId'])
          self.assertEqual(body_node['nodeId'],
                           node_event['params']['parentId'])
          # Ignore the other two events reporting the parents.
          node_event = debugger.wait_event('DOM.setChildNodes')
          node_event = debugger.wait_event('DOM.setChildNodes')

          # End the test.
          debugger.evaluate_js('onEndTest()')
          self.assertTrue(runner.JSTestsSucceeded())
