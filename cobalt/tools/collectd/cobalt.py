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

import json
import time
import argparse
import random
import websocket

try:
  import collectd
except ImportError:
  collectd = None

conf_ = {'host': 'localhost', 'port': 9222}


def config(conf):
  print('config called {!r}'.format(conf))
  conf_['collectd_conf'] = conf
  collectd.info('config called {!r}'.format(conf))


def reconnect():
  ws = websocket.create_connection('ws://{}:{}'.format(conf_['host'],
                                                       conf_['port']))
  ws.settimeout(3)
  setattr(ws, 'message_id', 1)
  conf_['ws'] = ws


def init():
  conf = conf_['collectd_conf']
  for child in conf.children:
    collectd.info('conf.child key {} values {}'.format(child.key, child.values))
    if child.key.lower() == 'address':
      host, port = child.values[0].split(':')
      conf_['host'] = host
      conf_['port'] = int(port)

  reconnect()


def wait_result(ws, result_id, timeout):
  start_time = time.time()
  messages = []
  matching_result = None
  while True:
    now = time.time()
    if now - start_time > timeout:
      break
    try:
      message = ws.recv()
      parsed_message = json.loads(message)
      messages.append(parsed_message)
      if 'result' in parsed_message and parsed_message['id'] == result_id:
        matching_result = parsed_message
        break
    except:
      break
  return (matching_result, messages)


def exec_js(ws, expr):
  ws.message_id += 1
  call_obj = {
      'id': ws.message_id,
      'method': 'Runtime.evaluate',
      'params': {
          'expression': expr,
          'returnByValue': True
      }
  }
  ws.send(json.dumps(call_obj))
  result, _ = wait_result(ws, ws.message_id, 10)
  return result['result']['result']


def report_value(plugin, type, instance, value, plugin_instance=None):
  vl = collectd.Values(plugin=plugin, type=type, type_instance=instance)
  if plugin_instance:
    vl.plugin_instance = plugin_instance
  vl.dispatch(values=[value])


def report_cobalt_stat(key, collectd_type):
  val = exec_js(conf_['ws'], 'h5vcc.cVal.getValue("' + key + '")')
  try:
    value = int(val['value'])
    # collectd.warning('key {} value: {}'.format(key, value))
    report_value('cobalt', collectd_type, key, value)
  except TypeError:
    collectd.warning('Failed to collect: {} {}'.format(key, val))


# Tuple of a Cobalt Cval, and data type
# Collectd frontends like CGP group values to graphs by the reported data type. The types here are
# simple gauges grouped by their rough estimated orders of magnitude, so that all plots are still
# somewhat discernible.
tracked_stats = [('Cobalt.Lifetime', 'lifetime'),
                 ('Count.DOM.ActiveJavaScriptEvents', 'gauge'),
                 ('Count.DOM.Attrs', 'gauge_10k'),
                 ('Count.DOM.EventListeners', 'gauge_10k'),
                 ('Count.DOM.HtmlCollections', 'gauge_10k'),
                 ('Count.DOM.NodeLists', 'gauge_10k'),
                 ('Count.DOM.NodeMaps', 'gauge_10k'),
                 ('Count.DOM.Nodes', 'gauge_10k'),
                 ('Count.DOM.StringMaps', 'gauge_100'),
                 ('Count.DOM.TokenLists', 'gauge_10k'),
                 ('Count.MainWebModule.DOM.HtmlElement', 'gauge_10k'),
                 ('Count.MainWebModule.DOM.HtmlElement.Document', 'gauge_10k'),
                 ('Count.MainWebModule.DOM.HtmlScriptElement.Execute', 'gauge'),
                 ('Count.Renderer.Rasterize.NewRenderTree', 'gauge_100k'),
                 ('Count.XHR', 'gauge_100'), ('Memory.CPU.Free', 'gauge_1G'),
                 ('Memory.CPU.Used', 'gauge_100M'),
                 ('Memory.DebugConsoleWebModule.DOM.HtmlScriptElement.Execute',
                  'gauge_100k'),
                 ('Memory.Font.LocalTypefaceCache.Capacity', 'gauge_10M'),
                 ('Memory.Font.LocalTypefaceCache.Size', 'gauge_10M'),
                 ('Memory.JS', 'gauge_10M'),
                 ('Memory.MainWebModule.DOM.HtmlScriptElement.Execute',
                  'gauge_1M'), ('Memory.XHR', 'gauge_1M'),
                 ('Memory.MainWebModule.ImageCache.Size', 'gauge_10M'),
                 ('Renderer.SubmissionQueueSize', 'gauge')]


def cobalt_read():
  for name, type in tracked_stats:
    report_cobalt_stat(name, type)


def read(*args, **kwargs):
  try:
    cobalt_read()
  except Exception:
    reconnect()


if collectd:
  collectd.register_config(config)
  collectd.register_init(init)
  collectd.register_read(read)


# Debugcode to verify plugin connection
if __name__ == '__main__':
  class Bunch:
      def __init__(self, **kwds):
          self.__dict__.update(kwds)
  def debugprint(*args,**kwargs):
    print('{!r} {!r}'.format(args,kwargs))
  def debugvalues(**kwargs):
    debugprint(**kwargs)
    return Bunch(dispatch=debugprint,**kwargs)
  collectd = Bunch(info=debugprint,warning=debugprint,Values=debugvalues)
  parser = argparse.ArgumentParser()
  parser.add_argument('--host', default='localhost')
  parser.add_argument('--port', type=int, default=9222)
  args = parser.parse_args()
  conf = Bunch(children=[Bunch( key='address',
    values=['{}:{}'.format(args.host,args.port)])])
  conf_['collectd_conf']  = conf
  init()
  cobalt_read()
