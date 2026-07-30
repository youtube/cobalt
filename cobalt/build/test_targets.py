#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
import queue

from typing import Dict, List, Set, Tuple

import networkx as nx

# Limit results to the allowlist for now. Once test infra can run more
# we'll extend or remove this list.
# TODO: Move this list to a GN target under //cobalt.
_ALLOW_TESTS = {
    '//base:base_perftests',
    '//base:base_unittests',
    '//cobalt/renderer:renderer_browsertests',
    # TODO: b/418842688 - Broken due to missing files.
    # '//cobalt:cobalt_unittests',
    # TODO: b/418842688 - Disabled temporarily.
    # '//gpu/gles2_conform_support:gles2_conform_test',
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
_EXCLUDE_TESTS_PREFIX = {
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
    # TODO: This is a source_set. Figure out why it's included.
    '//starboard/common:common_test',
}


def _is_test_target(g, target) -> bool:
  if (any(not target in _ALLOW_TESTS or target.startswith(path_prefix)
          for path_prefix in _EXCLUDE_TESTS_PREFIX)):
    return False

  # Starboard tests include a dependency on starboard_test_main, which contains
  # the runner.
  if ('//starboard:starboard_test_main' in g.nodes[target]['deps'] and
      not '__library' in target):
    return True

  # To definitely find all test targets we look for the local runner target
  # On linux this target is ${target_name}__runner, on android it's
  # ${target_name}__test_runner_script.
  # See src/testing/test.gni for the test() template definition.
  linux_runner = f'{target}__runner'
  android_runner = f'{target}__test_runner_script'
  successors = set(g.successors(target))
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
        'sources': attributes.get('sources', []),
    }
    g.add_node(target_name, **node_attributes)
    g.add_edges_from((target_name, dep) for dep in attributes['deps'])
    for source_file in attributes.get('inputs', []):
      source_map[source_file] += [target_name]
    for source_file in attributes.get('sources', []):
      source_map[source_file] += [target_name]
    # Also keep a list of targets for each path. This is used below to find
    # targets for an edited BUILD.gn file.
    source_map[target_name.split(':')[0]] += [target_name]
  return g, source_map


def _get_initial_targets(
    all_test_targets: Set[str],
    source_files: List[str],
    source_map: Dict,
) -> queue.Queue:
  """Gets the initial targets to consider based on source files."""
  target_queue = queue.Queue()
  gni_changes = any(
      file_path.endswith('.gni') or file_path.endswith('.gn')
      for file_path in source_files)
  github_changes = any(
      file_path.startswith('.github') for file_path in source_files)
  # Consider the entire tree if no files were provided, or if any special
  # files were modified.
  if len(source_files) == 0 or gni_changes or github_changes:
    for target in all_test_targets:
      target_queue.put(target)
  else:
    for source_file in source_files:
      for target in source_map.get(f'//{source_file}', []):
        target_queue.put(target)
      # Add all targets from modified BUILD.gn files.
      if source_file.endswith('BUILD.gn'):
        gn_path = f'//{source_file}'[:-len('BUILD.gn')]
        for target in source_map.get(gn_path, []):
          target_queue.put(target)
  return target_queue


def _create_relevant_subgraph(g: nx.DiGraph, all_test_targets: Set[str],
                              root_target: str) -> nx.DiGraph:
  """Creates a subgraph of the GN graph containing relevant nodes.

  TODO: The graph returned by this does not seem to be meaningfully different
        from `g`. Investigate why to reduce unnecessarily running targets.

  Args:
    g: The full GN dependency graph.
    root_target: The root target for which to find test targets.

  Returns:
    A subgraph containing the root target, its descendants, all test targets,
    intermediate library targets, and source_sets.
  """
  relevant_nodes = {root_target} | all_test_targets

  root_target_descendants = set(nx.descendants(g, root_target))
  relevant_nodes |= root_target_descendants

  # On android the test template creates an intermediate library target.
  # This must be included to resolve the dependency chain between the targets
  # and the tests.
  all_test_targets_libs = set()
  for target in all_test_targets:
    for successor in g.successors(target):
      if successor.startswith(f'{target}__library'):
        all_test_targets_libs.add(successor)
  relevant_nodes |= all_test_targets_libs

  # For complete coverage we should also follow dependencies through
  # source_sets. Most notable example is media_unittests which is comprised
  # of a collection of source_sets.
  all_source_sets = {n for n in g.nodes if g.nodes[n]['type'] == 'source_set'}
  relevant_nodes |= all_source_sets
  return g.subgraph(relevant_nodes)


def _find_dependent_tests(
    g: nx.DiGraph,
    root_g: nx.DiGraph,
    target_queue: queue.Queue,
) -> None:
  """Finds test targets that depend on the targets in the queue."""
  test_targets = set()
  executables = set()
  visited = set()
  while target_queue.qsize() > 0:
    target = target_queue.get()
    if target in visited:
      continue
    visited.add(target)

    if _is_test_target(g, target):
      # Ninja target name is the same as the gn target path sans the leading //.
      test_targets.add(target.lstrip('/'))
      # TODO: This does not work for EG where the "main" test target is the .so
      # file. The runnable binary is generated by the <target_name>_loader
      # target. This also does not yield anything useful for Android where the
      # test target does not produce any outputs.
      # There is a workaround for this in the CI on-host test job.
      executables |= {
          output.lstrip('/') for output in g.nodes[target]['outputs']
      }
    elif target in root_g.nodes:
      # Add all targets that depend on this current target to the queue.
      # This could potentially be filtered on node type.
      for predecessor in root_g.predecessors(target):
        target_queue.put(predecessor)

  return sorted(list(test_targets)), sorted(list(executables))


def get_test_targets_from_sources(
    g: nx.DiGraph,
    root_target: str,
    source_files: List[str],
    source_map: Dict[str, List[str]],
) -> Tuple[List[str], List[str]]:
  all_test_targets = {n for n in g.nodes if _is_test_target(g, n)}
  root_g = _create_relevant_subgraph(g, all_test_targets, root_target)

  # Find the tests that depend on the affected targets.
  target_queue = _get_initial_targets(all_test_targets, source_files,
                                      source_map)
  return _find_dependent_tests(g, root_g, target_queue)


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
