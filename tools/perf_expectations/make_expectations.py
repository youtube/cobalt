#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import math
import optparse
import re
import simplejson
import subprocess
import sys
import time
import urllib2


__version__ = '1.0'
DEFAULT_EXPECTATIONS_FILE = 'perf_expectations.json'
DEFAULT_VARIANCE = 0.05
USAGE = ''


def ReadFile(filename):
  try:
    file = open(filename, 'r')
  except IOError, e:
    print >> sys.stderr, ('I/O Error reading file %s(%s): %s' %
                          (filename, e.errno, e.strerror))
    raise e
  contents = file.read()
  file.close()
  return contents


def ConvertJsonIntoDict(string):
  """Read a JSON string and convert its contents into a Python datatype."""
  if len(string) == 0:
    print >> sys.stderr, ('Error could not parse empty string')
    raise Exception('JSON data missing')

  try:
    json = simplejson.loads(string)
  except ValueError, e:
    print >> sys.stderr, ('Error parsing string: "%s"' % string)
    raise e
  return json


# Floating point representation of last time we fetched a URL.
last_fetched_at = None
def FetchUrlContents(url):
  global last_fetched_at
  if last_fetched_at and ((time.time() - last_fetched_at) <= 0.5):
    # Sleep for half a second to avoid overloading the server.
    time.sleep(0.5)
  try:
    last_fetched_at = time.time()
    connection = urllib2.urlopen(url)
  except urllib2.HTTPError, e:
    if e.code == 404:
      return None
    raise e
  text = connection.read().strip()
  connection.close()
  return text


def WriteJson(filename, data, keys):
  """Write a list of |keys| in |data| to the file specified in |filename|."""
  try:
    file = open(filename, 'w')
  except IOError, e:
    print >> sys.stderr, ('I/O Error writing file %s(%s): %s' %
                          (filename, e.errno, e.strerror))
    return False
  jsondata = []
  for key in keys:
    rowdata = []
    for subkey in ['reva', 'revb', 'improve', 'regress']:
      if subkey in data[key]:
        rowdata.append('"%s": %s' % (subkey, data[key][subkey]))
    jsondata.append('"%s": {%s}' % (key, ', '.join(rowdata)))
  jsondata.append('"load": true')
  json = '{%s\n}' % ',\n '.join(jsondata)
  file.write(json + '\n')
  file.close()
  return True


def Main(args):
  parser = optparse.OptionParser(usage=USAGE, version=__version__)
  options, args = parser.parse_args(args)

  # Get the list of summaries for a test.
  base_url = 'http://build.chromium.org/f/chromium/perf'
  perf = ConvertJsonIntoDict(ReadFile(DEFAULT_EXPECTATIONS_FILE))

  # Fetch graphs.dat for this combination.
  perfkeys = perf.keys()
  # In perf_expectations.json, ignore the 'load' key.
  perfkeys.remove('load')
  perfkeys.sort()

  write_new_expectations = False
  for key in perfkeys:
    value = perf[key]
    variance = DEFAULT_VARIANCE

    # Skip expectations that are missing a reva or revb.  We can't generate
    # expectations for those.
    if not(value.has_key('reva') and value.has_key('revb')):
      print '%s (skipping, missing revision range)' % key
      continue
    revb = int(value['revb'])
    reva = int(value['reva'])

    # Ensure that reva is less than revb.
    if reva > revb:
      temp = reva
      reva = revb
      revb = temp

    # Get the system/test/graph/tracename and reftracename for the current key.
    matchData = re.match(r'^([^/]+)\/([^/]+)\/([^/]+)\/([^/]+)$', key)
    if not matchData:
      print '%s (skipping, cannot parse key)' % key
      continue
    system = matchData.group(1)
    test = matchData.group(2)
    graph = matchData.group(3)
    tracename = matchData.group(4)
    reftracename = tracename + '_ref'

    # Create the summary_url and get the json data for that URL.
    # FetchUrlContents() may sleep to avoid overloading the server with
    # requests.
    summary_url = '%s/%s/%s/%s-summary.dat' % (base_url, system, test, graph)
    summaryjson = FetchUrlContents(summary_url)
    if not summaryjson:
      print '%s (skipping, missing json data)' % key
      continue

    summarylist = summaryjson.split('\n')
    trace_values = {}
    for trace in [tracename, reftracename]:
      trace_values.setdefault(trace, {})

    # Find the high and low values for each of the traces.
    scanning = False
    printed_error = False
    for line in summarylist:
      json = ConvertJsonIntoDict(line)
      if int(json['rev']) <= revb:
        scanning = True
      if int(json['rev']) < reva:
        break

      # We found the upper revision in the range.  Scan for trace data until we
      # find the lower revision in the range.
      if scanning:
        for trace in [tracename, reftracename]:
          if trace not in json['traces']:
            if not printed_error:
              print '%s (error)' % key
              printed_error = True
            print '  trace %s missing' % trace
            continue
          if type(json['traces'][trace]) != type([]):
            if not printed_error:
              print '%s (error)' % key
              printed_error = True
            print '  trace %s format not recognized' % trace
            continue
          try:
            tracevalue = float(json['traces'][trace][0])
          except ValueError:
            if not printed_error:
              print '%s (error)' % key
              printed_error = True
            print '  trace %s value error: %s' % (
                trace, str(json['traces'][trace][0]))
            continue

          for bound in ['high', 'low']:
            trace_values[trace].setdefault(bound, tracevalue)

          trace_values[trace]['high'] = max(trace_values[trace]['high'],
                                            tracevalue)
          trace_values[trace]['low'] = min(trace_values[trace]['low'],
                                           tracevalue)

    if 'high' not in trace_values[tracename]:
      print '%s (skipping, no suitable traces matched)' % key
      continue

    # Calculate assuming high deltas are regressions and low deltas are
    # improvements.
    regress = (float(trace_values[tracename]['high']) -
               float(trace_values[reftracename]['low']))
    improve = (float(trace_values[tracename]['low']) -
               float(trace_values[reftracename]['high']))
    # If the existing values assume regressions are low deltas relative to
    # improvements, swap our regress and improve.  This value must be a
    # scores-like result.
    if perf[key]['regress'] < perf[key]['improve']:
      temp = regress
      regress = improve
      improve = temp
    if regress < improve:
      regress = int(math.floor(regress - abs(regress*variance)))
      improve = int(math.ceil(improve + abs(improve*variance)))
    else:
      improve = int(math.floor(improve - abs(improve*variance)))
      regress = int(math.ceil(regress + abs(regress*variance)))

    if (perf[key]['regress'] == regress and perf[key]['improve'] == improve):
      print '%s (no change)' % key
      continue

    write_new_expectations = True
    print key
    print '  before = %s' % perf[key]
    print '  traces = %s' % trace_values
    perf[key]['regress'] = regress
    perf[key]['improve'] = improve
    print '  after = %s' % perf[key]

  if write_new_expectations:
    print 'writing expectations... ',
    WriteJson(DEFAULT_EXPECTATIONS_FILE, perf, perfkeys)
    print 'done'
  else:
    print 'no updates made'


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
