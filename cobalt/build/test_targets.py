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
import os

from typing import Dict, List, Set, Tuple

import networkx as nx

# TODO: Investigate test targets on these lists.
# _ALLOW_TESTS = {
#     '//base:base_perftests',
#     '//base:base_unittests',
#     '//cobalt/renderer:renderer_browsertests',
#     # '//cobalt:cobalt_unittests',
#     # TODO: b/418842688 - Disabled temporarily.
#     # '//gpu/gles2_conform_support:gles2_conform_test',
#     '//mojo:mojo_perftests',
#     '//mojo:mojo_unittests',
#     '//net:net_unittests',
#     '//skia:skia_unittests',
#     '//starboard/nplb:nplb',
#     '//third_party/blink/renderer/platform/heap:blink_heap_perftests',
#     '//third_party/blink/renderer/platform/heap:blink_heap_unittests',
#     '//third_party/blink/renderer/platform/wtf:wtf_unittests',
#     '//third_party/blink/renderer/platform:blink_fuzzer_unittests',
#     '//third_party/blink/renderer/platform:blink_platform_nocompile_tests',
#     '//third_party/blink/renderer/platform:blink_platform_perftests',
#     '//third_party/blink/renderer/platform:blink_platform_unittests',
#     '//third_party/boringssl:boringssl_crypto_tests',
#     '//third_party/boringssl:boringssl_ssl_tests',
#     '//third_party/perfetto:perfetto_unittests',
#     '//third_party/perfetto:trace_processor_minimal_smoke_tests',
#     '//third_party/zlib:zlib_unittests',
#     '//url:url_perftests',
#     '//url:url_unittests',
# }
# _EXCLUDE_TESTS_PREFIX = {
#     '//android_webview',
#     '//base:base_i18n_perftests',
#     '//build/rust',
#     '//chrome',
#     '//components/paint_preview',
#     '//components/services/paint_preview_compositor',
#     '//components/ukm',
#     '//ipc:ipc_perftests',
#     '//media/cast',
#     '//mojo/core:mojo_core_unittests',
#     '//mojo/core:mojo_perftests',
#     '//net:net_perftests',
#     '//pdf',
#     '//testing/libfuzzer/tests:libfuzzer_tests',
#     '//third_party/angle',
#     '//third_party/dawn',
#     '//third_party/pdfium',
#     '//third_party/libaddressinput',
#     '//third_party/libphonenumber',
#     '//third_party/swiftshader',
#     '//ui/shell_dialogs',
#     '//weblayer',
#     # TODO: This is a source_set. Figure out why it's included.
#     '//starboard/common:common_test',
# }

_UNRELATED_PATHS = ['components', 'content', 'skia', 'third_party', 'ui', 'v8']
_INTERMEDIATE_TYPES = ['source_set', 'group']
_LIBRARY_TYPES = ['shared_library', 'static_library']


def _is_test_target(g, target) -> bool:
  # TODO: This should be correct but excludes //media:media_unittests.
  # if target not in g or not g.nodes[target]['unit_test']:
  #   return False

  # TODO: Find better test target determiner. This does not work for modular.

  # To definitely find all test targets we look for the local runner target
  # On linux this target is ${target_name}__runner, on android it's
  # ${target_name}__test_runner_script.
  # See src/testing/test.gni for the test() template definition.
  linux_runner = f'{target}__runner'
  android_runner = f'{target}__test_runner_script'
  successors = set(g.successors(target))
  return linux_runner in successors or android_runner in successors


def _create_graph(data: dict) -> nx.DiGraph:
  """Creates a directed graph from 'gn desc' json output.
  Using targets as nodes and creating edges from dependencies.
  """
  g = nx.DiGraph()
  for target_name, attributes in data.items():
    node_attributes = {
        'outputs': attributes.get('outputs', []),
        'testonly': attributes.get('testonly', False),
        'type': attributes['type'],
        'deps': attributes.get('deps', []),
        'unit_test': 'UNIT_TEST' in attributes.get('defines', []),
    }
    g.add_node(target_name, **node_attributes)
    g.add_edges_from((target_name, dep) for dep in attributes['deps'])
  return g


def _get_path_parts(target: str) -> List[str]:
  """Returns the path components of a GN target, ignoring leading '//'."""
  # Separate path from target name and split by '/', ignoring the leading '//'.
  # //foo/bar:baz -> //foo/bar -> ['', '', 'foo', 'bar'] -> ['foo', 'bar']
  return target.split(':')[0].split('/')[2:]


def _node_type_in(g: nx.DiGraph, n: str, types: List[str]) -> bool:
  """Returns True if the target's type is in the provided list."""
  return g.nodes[n]['type'] in types


def _get_root_slice(g: nx.DiGraph, root_target: str) -> nx.DiGraph:
  """Calculates the slice of the graph containing the root target and its deps.
  """

  def node_filter(n):
    # TODO(find the bug): Remove testonly from cobalt targets.
    return '/cobalt' in n or not g.nodes[n]['testonly']

  def edge_filter(u, v):
    u_path = _get_path_parts(u)
    v_path = _get_path_parts(v)
    # Exclude edges that start and end in _UNRELATED_PATHS.
    return not (u_path[0] in _UNRELATED_PATHS and v_path[0] in _UNRELATED_PATHS)

  root_slice_g = nx.subgraph_view(
      g, filter_node=node_filter, filter_edge=edge_filter)
  # Return a subgraph that only includes nodes reachable from the root target.
  return g.subgraph(nx.dfs_postorder_nodes(root_slice_g, source=root_target))


def _get_test_slice(
    g: nx.DiGraph,
    all_test_targets: Set[str],
) -> Tuple[Set[str], Dict[str, Set[str]]]:
  """Calculates the slice of the graph containing tests and their deps."""
  test_g_nodes = set()
  targets_for_deps = collections.defaultdict(set)
  target_queue = queue.Queue()
  for target in all_test_targets:
    target_queue.put(target)

  visited = set()
  while not target_queue.empty():
    target = target_queue.get()
    if target in visited:
      continue
    visited.add(target)
    test_g_nodes.add(target)

    # Only test targets or intermediate types should include deps.
    if not (target in all_test_targets or
            _node_type_in(g, target, _INTERMEDIATE_TYPES)):
      continue

    for dep in g.nodes[target]['deps']:
      target_path = _get_path_parts(target)
      dep_path = _get_path_parts(dep)

      # TODO: Make this check smarter?
      min_path_length = 2 if target_path[0] in _UNRELATED_PATHS else 1
      if len(os.path.commonprefix([target_path, dep_path])) < min_path_length:
        # Require that at least the first path component matches to include a
        # dep, or the first two for //components, //third_party, //ui.
        continue

      if not (_node_type_in(g, dep, _INTERMEDIATE_TYPES) or _node_type_in(
          g, dep, _LIBRARY_TYPES) and not g.nodes[dep]['testonly']):
        # Only allow source_set and non-testonly library deps.
        continue

      target_queue.put(dep)
      # Use the test targets from the parent node if available,
      # if not it's a test target and should be added itself.
      targets_for_deps[dep] |= targets_for_deps.get(target, {target})
  return test_g_nodes, targets_for_deps


def _find_dependent_tests(
    g: nx.DiGraph,
    root_slice: nx.DiGraph,
    test_slice_nodes: Set[str],
    tests_for_deps: Dict[str, Set[str]],
) -> Tuple[List[str], List[str]]:
  """Finds test targets that depend on the targets in the root slice."""
  overlapping_deps = root_slice.nodes & test_slice_nodes
  test_targets = list(
      set().union(*[tests_for_deps[n] for n in overlapping_deps]))
  executables = [g.nodes[t]['outputs'] for t in test_targets]
  return sorted(test_targets), sorted(executables)


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
  # TODO: Move this to GHA
  # gn refs out/linux-x64x11_devel/ <files> --testonly=true --all
  # parser.add_argument(
  #     '--sources',
  #     type=lambda arg: arg.split(','),
  #     default=[],
  #     help='The source files from which to calculate affected targets, '
  #     'comma-separated.')
  parser.add_argument(
      '--json-output',
      action='store_true',
      help='The path to which the list should be written in JSON format.'
      'If omitted the targets are printed to stdout in text format.')
  args = parser.parse_args()

  with open(args.gn_json, mode='r', encoding='utf-8') as gn_json_file:
    g = _create_graph(json.load(gn_json_file))

  all_test_targets = {n for n in g.nodes if _is_test_target(g, n)}
  root_slice_g = _get_root_slice(g, args.root_target)
  test_slice_nodes, targets_for_deps = _get_test_slice(g, all_test_targets)
  test_targets, executables = _find_dependent_tests(g, root_slice_g,
                                                    test_slice_nodes,
                                                    targets_for_deps)

  # DEBUG
  def set_default(obj):
    if isinstance(obj, set):
      return sorted(list(obj))
    raise TypeError

  paths = collections.defaultdict(set)
  for dep in root_slice_g.nodes & test_slice_nodes:
    paths[' -> '.join(
        nx.shortest_path(root_slice_g, source=args.root_target,
                         target=dep))] |= targets_for_deps[dep]
  print(json.dumps(paths, indent=2, sort_keys=True, default=set_default))

  if args.json_output:
    output = {'test_targets': list(test_targets), 'executables': executables}
    print(json.dumps(output))
  else:
    for test in sorted(test_targets):
      print(test)
    print(f'Found {len(test_targets)} test targets')


if __name__ == '__main__':
  main()
