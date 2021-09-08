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
        os.path.dirname(__file__), '..', '..', '..', 'third_party',
        'websocket-client'))
import websocket  # pylint: disable=wrong-import-position

# Set to True to add additional logging to debug the test.
_DEBUG = False


class DebuggerCommandError(Exception):
  """Exception when an error response is received for a command."""

  def __init__(self, error):
    code = '[{}] '.format(error['code']) if 'code' in error else ''
    super(DebuggerCommandError, self).__init__(code + error['message'])


class DebuggerEventError(Exception):
  """Exception when an unexpected event is received."""

  def __init__(self, expected, actual):
    super(DebuggerEventError,
          self).__init__('Waiting for {} but got {}'.format(expected, actual))


class JavaScriptError(Exception):
  """Exception when a JavaScript exception occurs in an evaluation."""

  def __init__(self, exception_details):
    # All the fields we care about are optional, so gracefully fallback.
    ex = exception_details.get('exception', {})
    fallback = ex.get('className', 'Unknown error') + ' (No description)'
    msg = ex.get('description', fallback)
    super(JavaScriptError, self).__init__(msg)


class DebuggerConnection(object):
  """Connection to debugger over a WebSocket.

  Runs in the owner's thread by receiving and storing messages as needed when
  waiting for a command response or event.
  """

  def __init__(self, ws_url, timeout=1):
    self.ws_url = ws_url
    self.timeout = timeout
    self.last_id = 0
    self.commands = {}
    self.responses = {}
    self.events = []

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
      except websocket.WebSocketTimeoutException:
        method = self.commands[command_id]['method']
        raise DebuggerCommandError(
            {'message': 'Timeout waiting for response to ' + method})
    self.commands.pop(command_id)
    return self.responses.pop(command_id)

  def wait_event(self, method):
    """Wait for an event to arrive.

    Fails the test with an exception if the WebSocket times out without
    receiving the event, or another event arrives first.
    """

    # "Debugger.scriptParsed" events get sent as artifacts of the debugger
    # backend implementation running its own injected code, so they are ignored
    # unless that's what we're waiting for.
    allow_script_parsed = (method == 'Debugger.scriptParsed')

    # Pop already-received events from the event queue.
    def _next_event():
      while self.events:
        event = self.events.pop(0)
        if allow_script_parsed or event['method'] != 'Debugger.scriptParsed':
          return event
      return None

    # Take the next event in the queue. If the queue is empty, receive messages
    # until an event arrives and is put in the queue for us to take.
    while True:
      event = _next_event()
      if event:
        break
      try:
        self._receive_message()
      except websocket.WebSocketTimeoutException:
        raise DebuggerEventError(method, 'None (timeout)')
    if method != event['method']:
      raise DebuggerEventError(method, event['method'])
    return event

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

  def enable_dom(self):
    """Helper to enable the 'DOM' domain and get the document."""
    self.run_command('DOM.enable')
    self.wait_event('DOM.documentUpdated')
    doc_response = self.run_command('DOM.getDocument')
    return doc_response['result']['root']

  def evaluate_js(self, expression):
    """Helper for the 'Runtime.evaluate' command to run some JavaScript."""
    if _DEBUG:
      logging.debug('JavaScript eval -------- %s', expression)
    response = self.run_command('Runtime.evaluate', {
        'contextId': self.context_id,
        'expression': expression,
    })
    if 'exceptionDetails' in response['result']:
      raise JavaScriptError(response['result']['exceptionDetails'])
    return response['result']


class WebDebuggerTest(black_box_tests.BlackBoxTestCase):
  """Test interaction with the web debugger over a WebSocket."""

  def set_up_with(self, cm):
    val = cm.__enter__()
    self.addCleanup(cm.__exit__, None, None, None)
    return val

  def setUp(self):
    cobalt_vars = self.cobalt_config.GetVariables(self.launcher_params.config)
    if not cobalt_vars['enable_debugger']:
      self.skipTest('DevTools is disabled on this platform')

    self.server = self.set_up_with(
        ThreadedWebServer(binding_address=self.GetBindingAddress()))
    url = self.server.GetURL(file_name='testdata/web_debugger.html')
    self.runner = self.set_up_with(self.CreateCobaltRunner(url=url))
    self.debugger = self.set_up_with(self.create_debugger_connection())
    self.runner.WaitForJSTestsSetup()
    self.debugger.enable_runtime()

  def tearDown(self):
    self.debugger.evaluate_js('onEndTest()')
    self.assertTrue(self.runner.JSTestsSucceeded())
    unprocessed_events = [
        e['method']
        for e in self.debugger.events
        if e['method'] != 'Debugger.scriptParsed'
    ]
    self.assertEqual([], unprocessed_events)

  def create_debugger_connection(self):
    devtools_url = self.runner.GetCval('Cobalt.Server.DevTools')
    parts = list(urlparse.urlsplit(devtools_url))
    parts[0] = 'ws'  # scheme
    parts[2] = '/devtools/page/cobalt'  # path
    ws_url = urlparse.urlunsplit(parts)
    return DebuggerConnection(ws_url)

  def test_runtime(self):
    # Evaluate a simple expression.
    eval_result = self.debugger.evaluate_js('6 * 7')
    self.assertEqual(42, eval_result['result']['value'])

    # Set an attribute and read it back w/ WebDriver.
    self.debugger.evaluate_js(
        'document.body.setAttribute("web_debugger", "tested")')
    self.assertEqual(
        'tested',
        self.runner.UniqueFind('body').get_attribute('web_debugger'))

    # Log to console, and check we get the console event.
    self.debugger.evaluate_js('console.log("hello")')
    console_event = self.debugger.wait_event('Runtime.consoleAPICalled')
    self.assertEqual('hello', console_event['params']['args'][0]['value'])

  def test_dom_tree(self):
    doc_root = self.debugger.enable_dom()
    self.assertEqual('#document', doc_root['nodeName'])

    doc_url = doc_root['documentURL']
    # remove query params (cert_scope, etc.)
    doc_url = doc_url.split('?')[0]
    self.assertEqual(self.runner.url, doc_url)

    # document: <html><head></head><body></body></html>
    html_node = doc_root['children'][0]
    body_node = html_node['children'][1]
    self.assertEqual('BODY', body_node['nodeName'])

    # <body>
    #   <h1><span>Web debugger</span></h1>
    #   <div#test>
    #     <div#A><div#A1/><div#A2/></div#A>
    #     <div#B><span#B1/></div#B>
    #     <span#C/>
    #   </div#test>
    #   <script/>
    # </body>

    # Request children of BODY including the entire subtree.
    self.debugger.run_command(
        'DOM.requestChildNodes',
        {
            'nodeId': body_node['nodeId'],
            'depth': -1,  # entire subtree
        })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(3, len(child_nodes_event['params']['nodes']))

    h1 = child_nodes_event['params']['nodes'][0]
    span_h1 = h1['children'][0]
    text_h1 = span_h1['children'][0]
    self.assertEqual(1, len(h1['children']))
    self.assertEqual(1, len(span_h1['children']))
    self.assertNotIn('children', text_h1)
    self.assertEqual(1, h1['childNodeCount'])
    self.assertEqual(1, span_h1['childNodeCount'])  # non-whitespace text
    self.assertEqual(0, text_h1['childNodeCount'])
    self.assertEqual('H1', h1['nodeName'])
    self.assertEqual('SPAN', span_h1['nodeName'])
    self.assertEqual('#text', text_h1['nodeName'])
    self.assertEqual('Web debugger', text_h1['nodeValue'])

    div_test = child_nodes_event['params']['nodes'][1]
    div_a = div_test['children'][0]
    div_a1 = div_a['children'][0]
    div_a2 = div_a['children'][1]
    div_b = div_test['children'][1]
    span_b1 = div_b['children'][0]
    text_b1 = span_b1['children'][0]  # 1 deeper than requested
    span_c = div_test['children'][2]
    text_c = span_c['children'][0]
    self.assertEqual(3, len(div_test['children']))
    self.assertEqual(2, len(div_a['children']))
    self.assertNotIn('children', div_a1)
    self.assertNotIn('children', div_a2)
    self.assertEqual(1, len(div_b['children']))
    self.assertEqual(1, len(span_b1['children']))
    self.assertNotIn('children', text_b1)
    self.assertEqual(1, len(span_c['children']))
    self.assertNotIn('children', text_c)
    self.assertEqual(3, div_test['childNodeCount'])
    self.assertEqual(2, div_a['childNodeCount'])
    self.assertEqual(0, div_a1['childNodeCount'])
    self.assertEqual(0, div_a2['childNodeCount'])
    self.assertEqual(1, div_b['childNodeCount'])
    self.assertEqual(0, span_b1['childNodeCount'])  # whitespace text
    self.assertEqual(0, text_b1['childNodeCount'])
    self.assertEqual(0, span_c['childNodeCount'])  # whitespace text
    self.assertEqual(0, text_c['childNodeCount'])
    self.assertEqual(['id', 'test'], div_test['attributes'])
    self.assertEqual(['id', 'A'], div_a['attributes'])
    self.assertEqual(['id', 'A1'], div_a1['attributes'])
    self.assertEqual(['id', 'A2'], div_a2['attributes'])
    self.assertEqual(['id', 'B'], div_b['attributes'])
    self.assertEqual(['id', 'B1'], span_b1['attributes'])
    self.assertEqual(['id', 'C'], span_c['attributes'])
    self.assertEqual(3, text_b1['nodeType'])  # Text
    self.assertEqual('', text_b1['nodeValue'].strip())
    self.assertEqual(3, text_c['nodeType'])  # Text
    self.assertEqual('', text_c['nodeValue'].strip())

    # Request children of BODY to depth 2.
    # Not reporting children of <div#A> & <div#B>.
    # Reporting lone text child of <span#C>
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': body_node['nodeId'],
        'depth': 2,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(3, len(child_nodes_event['params']['nodes']))

    h1 = child_nodes_event['params']['nodes'][0]
    span_h1 = h1['children'][0]
    text_h1 = span_h1['children'][0]  # 1 deeper than requested
    self.assertEqual('H1', h1['nodeName'])
    self.assertEqual('SPAN', span_h1['nodeName'])
    self.assertEqual('#text', text_h1['nodeName'])
    self.assertEqual('Web debugger', text_h1['nodeValue'])

    div_test = child_nodes_event['params']['nodes'][1]
    div_a = div_test['children'][0]
    div_b = div_test['children'][1]
    span_c = div_test['children'][2]
    text_c = span_c['children'][0]  # 1 deeper than requested
    self.assertEqual(3, len(div_test['children']))
    self.assertNotIn('children', div_a)
    self.assertNotIn('children', div_b)
    self.assertEqual(1, len(span_c['children']))
    self.assertNotIn('children', text_c)
    self.assertEqual(3, div_test['childNodeCount'])
    self.assertEqual(2, div_a['childNodeCount'])
    self.assertEqual(1, div_b['childNodeCount'])
    self.assertEqual(0, span_c['childNodeCount'])  # whitespace text
    self.assertEqual(['id', 'test'], div_test['attributes'])
    self.assertEqual(['id', 'A'], div_a['attributes'])
    self.assertEqual(['id', 'B'], div_b['attributes'])
    self.assertEqual(['id', 'C'], span_c['attributes'])
    self.assertEqual(3, text_c['nodeType'])  # Text
    self.assertEqual('', text_c['nodeValue'].strip())

    # Request children of BODY to default depth of 1.
    # Not reporting children of <div#test>
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': body_node['nodeId'],
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(3, len(child_nodes_event['params']['nodes']))

    h1 = child_nodes_event['params']['nodes'][0]
    div_test = child_nodes_event['params']['nodes'][1]
    self.assertNotIn('children', h1)
    self.assertNotIn('children', div_test)
    self.assertEqual(1, h1['childNodeCount'])
    self.assertEqual(3, div_test['childNodeCount'])
    self.assertEqual(['id', 'test'], div_test['attributes'])

  def test_dom_remote_object(self):
    doc_root = self.debugger.enable_dom()
    html_node = doc_root['children'][0]
    body_node = html_node['children'][1]
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': body_node['nodeId'],
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    div_test = child_nodes_event['params']['nodes'][1]

    # Get <div#A1> as a remote object, and request it as a node.
    # This sends 'DOM.setChildNodes' events for unknown nodes on the path from
    # <div#test>, which is the nearest ancestor that was already reported.
    eval_result = self.debugger.evaluate_js('document.getElementById("A1")')
    node_response = self.debugger.run_command('DOM.requestNode', {
        'objectId': eval_result['result']['objectId'],
    })

    # Event reporting the children of <div#test>
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(div_test['nodeId'],
                     child_nodes_event['params']['parentId'])
    div_a = child_nodes_event['params']['nodes'][0]
    div_b = child_nodes_event['params']['nodes'][1]
    self.assertEqual(['id', 'A'], div_a['attributes'])
    self.assertEqual(['id', 'B'], div_b['attributes'])

    # Event reporting the children of <div#A>
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(div_a['nodeId'], child_nodes_event['params']['parentId'])
    div_a1 = child_nodes_event['params']['nodes'][0]
    div_a2 = child_nodes_event['params']['nodes'][1]
    self.assertEqual(['id', 'A1'], div_a1['attributes'])
    self.assertEqual(['id', 'A2'], div_a2['attributes'])

    # The node we requested now has an ID as reported the last event.
    self.assertEqual(div_a1['nodeId'], node_response['result']['nodeId'])

    # Round trip resolving test div to an object, then back to a node.
    resolve_response = self.debugger.run_command('DOM.resolveNode', {
        'nodeId': div_test['nodeId'],
    })
    node_response = self.debugger.run_command('DOM.requestNode', {
        'objectId': resolve_response['result']['object']['objectId'],
    })
    self.assertEqual(div_test['nodeId'], node_response['result']['nodeId'])

  def test_dom_childlist_mutation(self):
    doc_root = self.debugger.enable_dom()
    # document: <html><head></head><body></body></html>
    html_node = doc_root['children'][0]
    head_node = html_node['children'][0]
    body_node = html_node['children'][1]
    self.assertEqual('BODY', body_node['nodeName'])

    # getDocument should return HEAD and BODY, but none of their children.
    self.assertEqual(3, head_node['childNodeCount'])  # title, script, script
    self.assertEqual(3, body_node['childNodeCount'])  # h1, div, script
    self.assertNotIn('children', head_node)
    self.assertNotIn('children', body_node)

    # Request 1 level of children in the BODY
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': body_node['nodeId'],
        'depth': 1,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(body_node['childNodeCount'],
                     len(child_nodes_event['params']['nodes']))
    h1 = child_nodes_event['params']['nodes'][0]
    div_test = child_nodes_event['params']['nodes'][1]
    self.assertEqual(['id', 'test'], div_test['attributes'])
    self.assertEqual(1, h1['childNodeCount'])  # span
    self.assertEqual(3, div_test['childNodeCount'])  # div A, div B, span C
    self.assertNotIn('children', h1)
    self.assertNotIn('children', div_test)

    # Insert a node as a child of a node whose children aren't yet reported.
    self.debugger.evaluate_js(
        'elem = document.createElement("span");'
        'elem.id = "child";'
        'elem.textContent = "foo";'
        'document.getElementById("test").appendChild(elem);')
    count_event = self.debugger.wait_event('DOM.childNodeCountUpdated')
    self.assertEqual(div_test['nodeId'], count_event['params']['nodeId'])
    self.assertEqual(div_test['childNodeCount'] + 1,
                     count_event['params']['childNodeCount'])

    # Remove a child from a node whose children aren't yet reported.
    self.debugger.evaluate_js('elem = document.getElementById("test");'
                              'elem.removeChild(elem.lastChild);')
    count_event = self.debugger.wait_event('DOM.childNodeCountUpdated')
    self.assertEqual(div_test['nodeId'], count_event['params']['nodeId'])
    self.assertEqual(div_test['childNodeCount'],
                     count_event['params']['childNodeCount'])

    # Request the children of the test div to repeat the insert/remove tests
    # after its children have been reported.
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': div_test['nodeId'],
        'depth': 1,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    self.assertEqual(div_test['nodeId'],
                     child_nodes_event['params']['parentId'])
    self.assertEqual(div_test['childNodeCount'],
                     len(child_nodes_event['params']['nodes']))

    # Insert a node as a child of a node whose children have been reported.
    self.debugger.evaluate_js(
        'elem = document.createElement("span");'
        'elem.id = "child";'
        'elem.textContent = "foo";'
        'document.getElementById("test").appendChild(elem);')
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(div_test['nodeId'],
                     inserted_event['params']['parentNodeId'])
    self.assertEqual(child_nodes_event['params']['nodes'][-1]['nodeId'],
                     inserted_event['params']['previousNodeId'])

    # Remove a child from a node whose children have been reported.
    self.debugger.evaluate_js('elem = document.getElementById("test");'
                              'elem.removeChild(elem.lastChild);')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(div_test['nodeId'],
                     removed_event['params']['parentNodeId'])
    self.assertEqual(inserted_event['params']['node']['nodeId'],
                     removed_event['params']['nodeId'])

    # Move a subtree to another part of the DOM that has not yet been reported.
    # (Get the original children of <div#test> to depth 1)
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': div_test['nodeId'],
        'depth': 1,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    orig_num_children = len(child_nodes_event['params']['nodes'])
    orig_div_a = child_nodes_event['params']['nodes'][0]
    orig_span_c = child_nodes_event['params']['nodes'][2]
    self.assertEqual(['id', 'A'], orig_div_a['attributes'])
    self.assertEqual(['id', 'C'], orig_span_c['attributes'])
    # (Move <span#C> into <div#A>)
    self.debugger.evaluate_js('a = document.getElementById("A");'
                              'c = document.getElementById("C");'
                              'a.appendChild(c);')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(orig_span_c['nodeId'], removed_event['params']['nodeId'])
    count_event = self.debugger.wait_event('DOM.childNodeCountUpdated')
    self.assertEqual(orig_div_a['nodeId'], count_event['params']['nodeId'])
    self.assertEqual(orig_div_a['childNodeCount'] + 1,
                     count_event['params']['childNodeCount'])
    # (Check the moved children of <div#test>)
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': div_test['nodeId'],
        'depth': 2,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    div_a = child_nodes_event['params']['nodes'][0]
    moved_span_c = div_a['children'][2]
    self.assertEqual(orig_num_children - 1,
                     len(child_nodes_event['params']['nodes']))
    self.assertEqual(['id', 'C'], moved_span_c['attributes'])
    self.assertNotEqual(orig_span_c['nodeId'], moved_span_c['nodeId'])

    # Move a subtree to another part of the DOM that has already been reported.
    # (Move <div#B> into <div#A>)
    orig_div_b = child_nodes_event['params']['nodes'][1]
    orig_span_b1 = orig_div_b['children'][0]
    self.debugger.evaluate_js('a = document.getElementById("A");'
                              'b = document.getElementById("B");'
                              'a.appendChild(b);')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(orig_div_b['nodeId'], removed_event['params']['nodeId'])
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    moved_div_b = inserted_event['params']['node']
    self.assertEqual(['id', 'B'], moved_div_b['attributes'])
    self.assertNotEqual(orig_div_b['nodeId'], moved_div_b['nodeId'])
    # (Check the children of moved <div#B>)
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': moved_div_b['nodeId'],
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    moved_span_b1 = child_nodes_event['params']['nodes'][0]
    self.assertEqual(['id', 'B1'], moved_span_b1['attributes'])
    self.assertNotEqual(orig_span_b1['nodeId'], moved_span_b1['nodeId'])

    # Replace a subtree with innerHTML
    # (replace all children of <div#B>)
    inner_html = "<div id='D'>\\n</div>"
    self.debugger.evaluate_js('b = document.getElementById("B");'
                              'b.innerHTML = "%s"' % inner_html)
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(moved_span_b1['nodeId'], removed_event['params']['nodeId'])
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(moved_div_b['nodeId'],
                     inserted_event['params']['parentNodeId'])
    div_d = inserted_event['params']['node']
    self.assertEqual(['id', 'D'], div_d['attributes'])
    self.assertEqual(0, div_d['childNodeCount'])  # Whitespace not reported.

  def test_dom_text_mutation(self):
    doc_root = self.debugger.enable_dom()
    # document: <html><head></head><body></body></html>
    html_node = doc_root['children'][0]
    body_node = html_node['children'][1]

    # Request 2 levels of children in the BODY
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': body_node['nodeId'],
        'depth': 2,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    div_test = child_nodes_event['params']['nodes'][1]

    # Unrequested lone text node at depth+1 in <h1><span>.
    h1 = child_nodes_event['params']['nodes'][0]
    h1_span = h1['children'][0]
    h1_text = h1_span['children'][0]
    self.assertEqual(h1['childNodeCount'], len(h1['children']))
    self.assertEqual(3, h1_text['nodeType'])  # Text
    self.assertEqual('Web debugger', h1_text['nodeValue'])

    # Unrequested lone whitespace text node at depth+1 in <span#B1>
    div_b = div_test['children'][1]
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': div_b['nodeId'],
        'depth': 1,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    span_b1 = child_nodes_event['params']['nodes'][0]
    text_b1 = span_b1['children'][0]
    self.assertEqual(3, text_b1['nodeType'])  # Text
    self.assertEqual('', text_b1['nodeValue'].strip())

    # Changing a text node's value mutates character data.
    self.debugger.evaluate_js('text = document.getElementById("B1").firstChild;'
                              'text.nodeValue = "Hello";')
    data_event = self.debugger.wait_event('DOM.characterDataModified')
    self.assertEqual(text_b1['nodeId'], data_event['params']['nodeId'])
    self.assertEqual('Hello', data_event['params']['characterData'])

    # Setting whitespace in a lone text node reports it removed.
    self.debugger.evaluate_js('text = document.getElementById("B1").firstChild;'
                              'text.nodeValue = "";')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(span_b1['nodeId'], removed_event['params']['parentNodeId'])
    self.assertEqual(text_b1['nodeId'], removed_event['params']['nodeId'])

    # Setting text in a lone whitespace text node reports it inserted.
    self.debugger.evaluate_js('text = document.getElementById("B1").firstChild;'
                              'text.nodeValue = "Revived";')
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(span_b1['nodeId'],
                     inserted_event['params']['parentNodeId'])
    self.assertEqual(0, inserted_event['params']['previousNodeId'])
    self.assertEqual(3, inserted_event['params']['node']['nodeType'])  # Text
    self.assertEqual('Revived', inserted_event['params']['node']['nodeValue'])
    self.assertNotEqual(text_b1['nodeId'],
                        inserted_event['params']['node']['nodeId'])

    # Setting text in a whitespace sibling text node reports it inserted.
    self.debugger.evaluate_js(
        'text = document.getElementById("B1").nextSibling;'
        'text.nodeValue = "Filled";')
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(div_b['nodeId'], inserted_event['params']['parentNodeId'])
    self.assertEqual(span_b1['nodeId'],
                     inserted_event['params']['previousNodeId'])
    self.assertEqual(3, inserted_event['params']['node']['nodeType'])  # Text
    self.assertEqual('Filled', inserted_event['params']['node']['nodeValue'])
    self.assertNotEqual(text_b1['nodeId'],
                        inserted_event['params']['node']['nodeId'])

    # Setting whitespace in a sibling text node reports it removed.
    self.debugger.evaluate_js(
        'text = document.getElementById("B1").nextSibling;'
        'text.nodeValue = "";')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(div_b['nodeId'], removed_event['params']['parentNodeId'])
    self.assertEqual(inserted_event['params']['node']['nodeId'],
                     removed_event['params']['nodeId'])

    # Setting textContent removes all children and inserts a new text node.
    self.debugger.evaluate_js(
        'document.getElementById("B").textContent = "One";')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(div_b['nodeId'], removed_event['params']['parentNodeId'])
    self.assertEqual(span_b1['nodeId'], removed_event['params']['nodeId'])
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(div_b['nodeId'], inserted_event['params']['parentNodeId'])
    self.assertEqual(0, inserted_event['params']['previousNodeId'])
    self.assertEqual(3, inserted_event['params']['node']['nodeType'])  # Text
    self.assertEqual('One', inserted_event['params']['node']['nodeValue'])

    # Setting empty textContent removes all children without inserting anything.
    self.debugger.evaluate_js('document.getElementById("B").textContent = "";')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(div_b['nodeId'], removed_event['params']['parentNodeId'])
    self.assertEqual(inserted_event['params']['node']['nodeId'],
                     removed_event['params']['nodeId'])

    # Setting textContent over empty text only inserts a new text node.
    self.debugger.evaluate_js(
        'document.getElementById("B").textContent = "Two";')
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(div_b['nodeId'], inserted_event['params']['parentNodeId'])
    self.assertEqual(0, inserted_event['params']['previousNodeId'])
    self.assertEqual(3, inserted_event['params']['node']['nodeType'])  # Text
    self.assertEqual('Two', inserted_event['params']['node']['nodeValue'])

    # Setting textContent over other text replaces the text nodes.
    self.debugger.evaluate_js(
        'document.getElementById("B").textContent = "Three";')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(div_b['nodeId'], removed_event['params']['parentNodeId'])
    self.assertEqual(inserted_event['params']['node']['nodeId'],
                     removed_event['params']['nodeId'])
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(div_b['nodeId'], inserted_event['params']['parentNodeId'])
    self.assertEqual(0, inserted_event['params']['previousNodeId'])
    self.assertEqual(3, inserted_event['params']['node']['nodeType'])  # Text
    self.assertEqual('Three', inserted_event['params']['node']['nodeValue'])

    # Setting whitespace textContent removes all children without any insertion.
    self.debugger.evaluate_js(
        'document.getElementById("B").textContent = "\\n";')
    removed_event = self.debugger.wait_event('DOM.childNodeRemoved')
    self.assertEqual(div_b['nodeId'], removed_event['params']['parentNodeId'])
    self.assertEqual(inserted_event['params']['node']['nodeId'],
                     removed_event['params']['nodeId'])

    # Setting textContent over whitespace text only inserts a new text node.
    self.debugger.evaluate_js(
        'document.getElementById("B").textContent = "Four";')
    inserted_event = self.debugger.wait_event('DOM.childNodeInserted')
    self.assertEqual(div_b['nodeId'], inserted_event['params']['parentNodeId'])
    self.assertEqual(0, inserted_event['params']['previousNodeId'])
    self.assertEqual(3, inserted_event['params']['node']['nodeType'])  # Text
    self.assertEqual('Four', inserted_event['params']['node']['nodeValue'])

  def test_dom_attribute_mutation(self):
    doc_root = self.debugger.enable_dom()
    # document: <html><head></head><body></body></html>
    html_node = doc_root['children'][0]
    body_node = html_node['children'][1]

    # Request 1 level of children in the BODY
    self.debugger.run_command('DOM.requestChildNodes', {
        'nodeId': body_node['nodeId'],
        'depth': 1,
    })
    child_nodes_event = self.debugger.wait_event('DOM.setChildNodes')
    h1 = child_nodes_event['params']['nodes'][0]

    def assert_attr_modified(statement, name, value):
      self.debugger.evaluate_js(
          'elem = document.getElementsByTagName("H1")[0];' + statement)
      attr_event = self.debugger.wait_event('DOM.attributeModified')
      self.assertEqual(attr_event['params']['nodeId'], h1['nodeId'])
      self.assertEqual(attr_event['params']['name'], name)
      self.assertEqual(attr_event['params']['value'], value)

    def assert_attr_removed(statement, name):
      self.debugger.evaluate_js(
          'elem = document.getElementsByTagName("H1")[0];' + statement)
      attr_event = self.debugger.wait_event('DOM.attributeRemoved')
      self.assertEqual(attr_event['params']['nodeId'], h1['nodeId'])
      self.assertEqual(attr_event['params']['name'], name)

    # Add a normal attribute.
    assert_attr_modified('elem.id = "foo"', 'id', 'foo')
    # Change a normal attribute.
    assert_attr_modified('elem.id = "bar"', 'id', 'bar')
    # Change a normal attribute with setAttribute.
    assert_attr_modified('elem.setAttribute("id", "baz")', 'id', 'baz')
    # Remove a normal attribute.
    assert_attr_removed('elem.removeAttribute("id")', 'id')

    # Change the className (from 'name', as set in web_debugger.html).
    assert_attr_modified('elem.className = "zzyzx"', 'class', 'zzyzx')

    # Change a dataset value (from 'robot', as set in web_debugger.html).
    assert_attr_modified('elem.dataset.who = "person"', 'data-who', 'person')
    # Add a dataset value.
    assert_attr_modified('elem.dataset.what = "thing"', 'data-what', 'thing')
    # Remove a data attribute with delete operator.
    # TODO: Fix this - MutationObserver is not reporting this mutation,
    # assert_attr_removed('delete elem.dataset.who', 'data-who')

    # Change a data attribute with setAttribute.
    assert_attr_modified('elem.setAttribute("data-what", "object")',
                         'data-what', 'object')
    # Add a data attribute with setAttribute.
    assert_attr_modified('elem.setAttribute("data-where", "place")',
                         'data-where', 'place')
    # Remove a data attribute with removeAttribute.
    assert_attr_removed('elem.removeAttribute("data-what")', 'data-what')

  def assert_paused(self, expected_stacks):
    """Checks that the debugger is paused at a breakpoint.

    Waits for the expected |Debugger.paused| event from hitting the breakpoint
    and then asserts that the expected_stacks match the call stacks in that
    event. Execution is always resumed before returning so that more JS can be
    evaluated, as needed to continue or end the test.

    Args:
      expected_stacks: A list of lists of strings with the expected function
        names in the call stacks in a series of asynchronous executions.
    """
    # Expect an anonymous frame from the backend eval that's running the code
    # we sent in the "Runtime.evaluate" command.
    expected_stacks[-1] += ['']

    paused_event = self.debugger.wait_event('Debugger.paused')
    try:
      call_stacks = []
      # First the main stack where the breakpoint was hit.
      call_frames = paused_event['params']['callFrames']
      call_stack = [frame['functionName'] for frame in call_frames]
      call_stacks.append(call_stack)
      # Then asynchronous stacks that preceded the main stack.
      async_trace = paused_event['params'].get('asyncStackTrace')
      while async_trace:
        call_frames = async_trace['callFrames']
        call_stack = [frame['functionName'] for frame in call_frames]
        call_stacks.append(call_stack)
        async_trace = async_trace.get('parent')
      self.assertEqual(expected_stacks, call_stacks)
    finally:
      # We must resume in order to avoid hanging if something goes wrong.
      self.debugger.run_command('Debugger.resume')
    self.debugger.wait_event('Debugger.resumed')

  def test_debugger_breakpoint(self):
    self.debugger.run_command('Debugger.enable')

    # Get the ID and source of our JavaScript test utils.
    script_id = ''
    while not script_id:
      script_event = self.debugger.wait_event('Debugger.scriptParsed')
      script_url = script_event['params']['url']
      if script_url.endswith('web_debugger_test_utils.js'):
        script_id = script_event['params']['scriptId']
    source_response = self.debugger.run_command('Debugger.getScriptSource', {
        'scriptId': script_id,
    })
    script_source = source_response['result']['scriptSource'].splitlines()

    # Set a breakpoint on the asyncBreak() function.
    line_number = next(n for n, l in enumerate(script_source)
                       if l.startswith('function asyncBreak'))
    self.debugger.run_command('Debugger.setBreakpoint', {
        'location': {
            'scriptId': script_id,
            'lineNumber': line_number,
        },
    })
    self.debugger.run_command('Debugger.setAsyncCallStackDepth', {
        'maxDepth': 99,
    })

    # Check the breakpoint within a SetTimeout() callback.
    self.debugger.evaluate_js('testSetTimeout()')
    self.assert_paused([
        [
            'asyncBreak',
            'asyncC',
            'timeoutB',
        ],
        [
            'asyncB',
            'timeoutA',
        ],
        [
            'asyncA',
            'testSetTimeout',
        ],
    ])

    # Check the breakpoint within a promise "then" after being resolved.
    self.debugger.evaluate_js('testPromise()')
    self.assert_paused([
        [
            'asyncBreak',
            'promiseThen',
        ],
        [
            'waitPromise',
            'testPromise',
        ],
    ])

    # Check the breakpoint after async await for a promise to resolve.
    self.debugger.evaluate_js('testAsyncFunction()')
    self.assert_paused([
        [
            'asyncBreak',
            'asyncAwait',
        ],
        [
            'asyncAwait',
            'testAsyncFunction',
        ],
    ])

    # Check the breakpoint within an XHR event handler.
    self.debugger.evaluate_js('testXHR(window.location.href)')
    self.assert_paused([
        [
            'asyncBreak',
            'fileLoaded',
        ],
        [
            'doXHR',
            'testXHR',
        ],
    ])

    # Check the breakpoint within a MutationObserver.
    self.debugger.evaluate_js('testMutate()')
    self.assert_paused([
        [
            'asyncBreak',
            'mutationCallback',
        ],
        [
            'doSetAttribute',
            'testMutate',
        ],
    ])

    # Check the breakpoint within an animation callback.
    self.debugger.evaluate_js('testAnimationFrame()')
    self.assert_paused([
        [
            'asyncBreak',
            'animationFrameCallback',
        ],
        [
            'doRequestAnimationFrame',
            'testAnimationFrame',
        ],
    ])

    # Check the breakpoint on a media callback going through EventQueue.
    self.debugger.evaluate_js('testMediaSource()')
    self.assert_paused([
        [
            'asyncBreak',
            'sourceOpenCallback',
        ],
        [
            'setSourceListener',
            'testMediaSource',
        ],
    ])
