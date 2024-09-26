#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script which gathers the JSON results merged from multiple
swarming shards of a step on the waterfall.

This is used to feed in the per-test times of previous runs of tests
to the browser_test_runner's sharding algorithm, to improve shard
distribution.
"""

import argparse
import json
import logging
import sys

import six  # pylint: disable=import-error

# //content/test/gpu is Python 3-only at this point, but
# //testing/scripts/test_buildbucket_api_gpu_use_cases.py does import this file
# via Python 2 on bots during the "get compile targets for scripts" step. So,
# keep this compatibility in for now.
# pylint: disable=wrong-import-position
if six.PY3:
  import urllib.request as ulib
else:
  import urllib2 as ulib  # pylint: disable=import-error
# pylint: enable=wrong-import-position


def GetBuildData(method, request):
  # Explorable via RPC explorer:
  # https://cr-buildbucket.appspot.com/rpcexplorer/services/
  #   buildbucket.v2.Builds/{GetBuild|SearchBuilds}
  assert method in ['GetBuild', 'SearchBuilds']

  # The Python docs are wrong. It's fine for this payload to be just
  # a JSON string.
  headers = {'content-type': 'application/json', 'accept': 'application/json'}
  logging.debug('Making request:')
  logging.debug('%s', request)
  if not isinstance(request, bytes):
    request = request.encode('utf-8')
  url = ulib.Request(
      'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/' + method,
      request, headers)
  conn = ulib.urlopen(url)
  result = conn.read().decode('utf-8')
  conn.close()
  # Result is a multi-line string the first line of which is
  # deliberate garbage and the rest of which is a JSON payload.
  return json.loads(''.join(result.splitlines()[1:]))


def GetJsonForBuildSteps(bot, build):
  request = json.dumps({
      'builder': {
          'project': 'chromium',
          'bucket': 'ci',
          'builder': bot
      },
      'buildNumber': build,
      'fields': 'steps.*.name,steps.*.logs'
  })
  return GetBuildData('GetBuild', request)


def GetJsonForLatestGreenBuildSteps(bot):
  fields = [
      'builds.*.number',
      'builds.*.steps.*.name',
      'builds.*.steps.*.logs',
  ]
  request = json.dumps({
      'predicate': {
          'builder': {
              'project': 'chromium',
              'bucket': 'ci',
              'builder': bot
          },
          'status': 'SUCCESS'
      },
      'fields': ','.join(fields),
      'pageSize': 1
  })
  builds_json = GetBuildData('SearchBuilds', request)
  if 'builds' not in builds_json:
    raise ValueError('Returned json data does not have "builds"')
  builds = builds_json['builds']
  assert len(builds) == 1
  return builds[0]


def JsonLoadFromUrl(url):
  conn = ulib.urlopen(url + '?format=raw')
  result = conn.read()
  conn.close()
  return json.loads(result)


def FindStepLogURL(steps, step_name, log_name):
  # The format of this JSON-encoded protobuf is defined here:
  # https://chromium.googlesource.com/infra/luci/luci-go/+/main/
  #   buildbucket/proto/step.proto
  # It's easiest to just use the RPC explorer to fetch one and see
  # what's desired to extract.
  for step in steps:
    if 'name' not in step or 'logs' not in step:
      continue
    if step['name'].startswith(step_name):
      for log in step['logs']:
        if log.get('name') == log_name:
          return log.get('viewUrl')
  return None


def ExtractTestTimes(node, node_name, dest, delim):
  if 'times' in node:
    dest[node_name] = sum(node['times']) / len(node['times'])
  else:
    for k in node.keys():
      if isinstance(node[k], dict):
        test_name = node_name + delim + k if node_name else k
        ExtractTestTimes(node[k], test_name, dest, delim)


def GatherResults(bot, build, step):
  if build is None:
    build_json = GetJsonForLatestGreenBuildSteps(bot)
    build = build_json.get('number')
  else:
    build_json = GetJsonForBuildSteps(bot, build)

  logging.debug('Fetched information from bot %s, build %s', bot, build)

  if 'steps' not in build_json:
    raise ValueError('Returned Json data do not have "steps"')

  json_output = FindStepLogURL(build_json['steps'], step, 'json.output')
  if not json_output:
    raise ValueError(
        'Unable to find json.output from step starting with %s' % step)
  logging.debug('json.output for step starting with %s: %s', step, json_output)

  merged_json = JsonLoadFromUrl(json_output)
  extracted_times = {'times': {}}
  ExtractTestTimes(merged_json['tests'], '', extracted_times['times'],
                   merged_json['path_delimiter'])

  return extracted_times, merged_json


def main():
  rest_args = sys.argv[1:]
  parser = argparse.ArgumentParser(
      description="""
Gather JSON results from a run of a Swarming test.

Example invocation to fetch the WebGL 1.0 test runtimes from Linux FYI
Release (NVIDIA):

gather_swarming_json_results.py \
  --step webgl_conformance_gl_passthrough_tests \
  --output=../data/gpu/webgl_conformance_tests_output.json

Example invocation to fetch the WebGL 2.0 runtimes:
gather_swarming_json_results.py \
  --step webgl2_conformance_gl_passthrough_tests \
  --output=../data/gpu/webgl2_conformance_tests_output.json

""",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      default=False,
      help='Enable verbose output')
  parser.add_argument(
      '--bot',
      default='Linux FYI Release (NVIDIA)',
      help='Which bot to examine')
  parser.add_argument(
      '--build',
      type=int,
      help='Which build to fetch. If not specified, use '
      'the latest successful build.')
  parser.add_argument('--step',
                      default='webgl2_conformance_gl_passthrough_tests',
                      help='Which step to fetch (treated as a prefix)')
  parser.add_argument(
      '--output',
      metavar='FILE',
      default='output.json',
      help='Name of output file; contains only test run times')
  parser.add_argument(
      '--full-output',
      metavar='FILE',
      help='Name of complete output file if desired')

  options = parser.parse_args(rest_args)
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)

  if sys.version_info < (2, 7, 10):
    logging.warning('Script does not work with Python older than 2.7.10')
    return 0

  extracted_times, merged_json = GatherResults(options.bot, options.build,
                                               options.step)

  logging.debug('Saving output to %s', options.output)
  with open(options.output, 'w') as f:
    json.dump(
        extracted_times, f, sort_keys=True, indent=2, separators=(',', ': '))

  if options.full_output is not None:
    logging.debug('Saving full output to %s', options.full_output)
    with open(options.full_output, 'w') as f:
      json.dump(
          merged_json, f, sort_keys=True, indent=2, separators=(',', ': '))

  return 0


if __name__ == '__main__':
  sys.exit(main())
