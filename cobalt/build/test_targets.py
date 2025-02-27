#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Calculates test targets based on a root package from GN dependencies."""

import argparse
import collections
import json
import os
import queue

from typing import Dict, List, Set, Tuple

import networkx as nx

# Limit results to the allowlist for now. Once test infra can run more
# we'll extend or remove this list.
# TODO: Move this list to a GN target under //cobalt.
_ALLOWLIST = {
    '//base:base_perftests',
    '//base:base_unittests',
    '//cobalt/renderer:renderer_browsertests',
    '//cobalt:cobalt_unittests',
    '//gpu/gles2_conform_support:gles2_conform_test',
    '//mojo:mojo_perftests',
    '//mojo:mojo_unittests',
    '//net:net_unittests',
    '//skia:skia_unittests',
    '//starboard/nplb:nplb',
    '//third_party/blink/renderer/platform/heap:blink_heap_perftests',
    '//third_party/blink/renderer/platform/heap:blink_heap_unittests',
    '//third_party/blink/renderer/platform/wtf:wtf_unittests',
    '//third_party/blink/renderer/platform:blink_fuzzer_unittests',
    '//third_party/blink/renderer/platform:blink_platform_nocompile_tests',
    '//third_party/blink/renderer/platform:blink_platform_perftests',
    '//third_party/blink/renderer/platform:blink_platform_unittests',
    '//third_party/boringssl:boringssl_crypto_tests',
    '//third_party/boringssl:boringssl_ssl_tests',
    '//third_party/perfetto:perfetto_unittests',
    '//third_party/perfetto:trace_processor_minimal_smoke_tests',
    '//third_party/zlib:zlib_unittests',
    '//url:url_perftests',
    '//url:url_unittests',
}

# Exclude some paths and targets that either are not relevant or don't support
# test sharding.
_EXCLUDE_TESTS = {
    '//android_webview',
    '//base:base_i18n_perftests',
    '//build/rust',
    '//chrome',
    '//components/paint_preview',
    '//components/services/paint_preview_compositor',
    '//components/ukm',
    '//ipc:ipc_perftests',
    '//media/cast',
    '//mojo/core:mojo_core_unittests',
    '//mojo/core:mojo_perftests',
    '//net:net_perftests',
    '//pdf',
    '//testing/libfuzzer/tests:libfuzzer_tests',
    '//third_party/angle',
    '//third_party/dawn',
    '//third_party/pdfium',
    '//third_party/libaddressinput',
    '//third_party/libphonenumber',
    '//third_party/swiftshader',
    '//ui/shell_dialogs',
    '//weblayer',
}

# The two nodes are adjacent in a path that contains many unrelated components.
# Targets in these paths must share an additional path segment to be considered
# related and subject for test target considerations.
_UNRELATED_TARGETS_PATHS = {'components', 'third_party', 'ui'}


def _are_related(node1, node2) -> bool:
  """Two targets are considered related if the share at least one path segment
  or two or more if they are in one of the paths that contain many unrelated
  targets.
  """
  # Remove the target name and split the path into its segments.
  node1_path = node1.split(':')[0].split('/')
  node2_path = node2.split(':')[0].split('/')
  common = os.path.commonprefix([node1_path, node2_path])
  if len(common) == 2:
    # Only the initial // matches.
    return False
  # At least one path segment matches.
  return len(common) > 3 or common[2] not in _UNRELATED_TARGETS_PATHS


def _is_test_target(g, node) -> bool:
  if not any(node == target for target in _ALLOWLIST) or \
    any(node.startswith(path_prefix) for path_prefix in _EXCLUDE_TESTS):
    return False

  # Test targets get a runner target added to its deps. On linux the name of
  # this target is ${target_name}__runner, on android it's
  # ${target_name}__test_runner_script.
  # See src/testing/test.gni for the test() template definition.
  linux_runner = f'{node}__runner'
  android_runner = f'{node}__test_runner_script'
  successors = set(g.successors(node))
  return linux_runner in successors or android_runner in successors


def _create_graph(data: dict) -> Tuple[nx.DiGraph, Dict[str, List[str]]]:
  """Creates a directed graph from 'gn desc' json output.
  Using targets as nodes and creating edges from dependencies.
  """
  g = nx.DiGraph()
  source_map = collections.defaultdict(list)
  for target_name, attributes in data.items():
    node_attributes = {
        'outputs': attributes.get('outputs', []),
        'type': attributes['type'],
    }
    g.add_node(target_name, **node_attributes)
    g.add_edges_from((target_name, dep) for dep in attributes['deps'])
    for source_file in attributes.get('inputs', []):
      source_map[source_file] += [target_name]
    for source_file in attributes.get('sources', []):
      source_map[source_file] += [target_name]
    source_map[target_name.split(':')[0]] += [target_name]
  return g, source_map


def _add_test_target(g: nx.DiGraph, target: str, test_targets: Set[str],
                     executables: Set[str]):
  test_targets.add(target[2:])
  # TODO: This does not work for EG where the "main" test target is the .so
  # file. The runnable binary is generated by the <target_name>_loader target.
  # This also does not yield anything useful for Android where the test
  # target does not produce any outputs.
  executables |= {output[2:] for output in g.nodes[target]['outputs']}


def get_test_targets_from_sources(
    g: nx.DiGraph,
    root_target: str,
    source_files: List[str],
    source_map: Dict,
) -> Tuple[List[str], List[str]]:
  root_target_descendants = set(nx.descendants(g, root_target))
  all_test_targets = {n for n in g.nodes if _is_test_target(g, n)}
  # On android the test template creates an intermediate library target.
  # This must be included to resolve the dependency chain between the targets
  # and the tests.
  all_test_targets_libs = {
      f'{n}__library' for n in all_test_targets if f'{n}__library' in g.nodes
  }

  # For complete coverage we should also follow dependencies through
  # source_sets. Most notable example is media_unittests which is comprised
  # of a collection of source_sets.
  all_source_sets = {n for n in g.nodes if g.nodes[n]['type'] == 'source_set'}

  # Create a subgraph of nodes in the GN graph we care about.
  root_g = g.subgraph({root_target} | root_target_descendants
                      | all_test_targets | all_test_targets_libs
                      | all_source_sets)

  target_queue = queue.Queue()
  test_targets = set()
  executables = set()
  # Get affected targets from the source files provided.
  gni_changes = any(file_path.endswith('.gni') for file_path in source_files)
  if len(source_files) == 0 or gni_changes:
    # If no files were provided or if .gni files were modified assume the entire
    # tree should be considered.
    for target in root_target_descendants:
      target_queue.put(target)
  else:
    for source_file in source_files:
      for target in source_map.get(f'//{source_file}', []):
        target_queue.put(target)
      # Add all targets from modified BUILD.gn files.
      if source_file.endswith('/BUILD.gn'):
        gn_path = f'//{source_file}'[:-len('/BUILD.gn')]
        for target in source_map.get(gn_path, []):
          target_queue.put(target)

  visited = set()
  while target_queue.qsize() > 0:
    target = target_queue.get()
    if target in visited or target not in root_g.nodes:
      continue
    visited.add(target)

    if _is_test_target(g, target):
      _add_test_target(g, target, test_targets, executables)

    for target_dep in root_g.predecessors(target):
      target_queue.put(target_dep)
      if _is_test_target(g, target_dep) and _are_related(target, target_dep):
        _add_test_target(g, target_dep, test_targets, executables)
  return sorted(list(test_targets)), sorted(list(executables))


def main():
  """Command line entry point."""
  parser = argparse.ArgumentParser(
      description='Utility to find test targets from gn dependency graph.')
  parser.add_argument(
      '--gn-json',
      type=str,
      required=True,
      help='Path to the json output of \'gn desc *\'.')
  parser.add_argument(
      '--root-target',
      type=str,
      required=True,
      help='The root target for which to find test targets. A test target is '
      'included if it depends on a target that is in the root target\'s '
      'dependency tree.')
  parser.add_argument(
      '--sources',
      type=lambda arg: arg.split(','),
      default=[],
      help='The source files from which to calculate affected targets, '
      'comma-separated.')
  parser.add_argument(
      '--json-output',
      action='store_true',
      help='The path to which the list should be written in JSON format.'
      'If omitted the targets are printed to stdout in text format.')
  args = parser.parse_args()

  with open(args.gn_json, mode='r', encoding='utf-8') as gn_json_file:
    g, source_map = _create_graph(json.load(gn_json_file))

  test_targets, executables = get_test_targets_from_sources(
      g, args.root_target, args.sources, source_map)

  if args.json_output:
    output = {'test_targets': test_targets, 'executables': executables}
    print(json.dumps(output))
  else:
    print('\n'.join(test_targets))
    print(f'Found {len(test_targets)} test targets')


if __name__ == '__main__':
  main()
