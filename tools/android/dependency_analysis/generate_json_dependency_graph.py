#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool to run jdeps and process its output into a JSON file."""

import argparse
import functools
import logging
import math
import multiprocessing
import pathlib
import subprocess
import sys

from typing import Dict, Optional, Tuple, Union

import class_dependency
import package_dependency
import serialization
import target_dependency

_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
sys.path.append(str(_SRC_PATH / 'build/android'))
from pylib import constants

sys.path.append(str(_SRC_PATH / 'tools/android'))
from python_utils import git_metadata_utils, subprocess_utils

_IGNORED_JAR_PATHS = [
    # This matches org_ow2_asm_asm_commons and org_ow2_asm_asm_analysis, both of
    # which fail jdeps (not sure why).
    'third_party/android_deps/libs/org_ow2_asm_asm',
]


def _relsrc(path: Union[str, pathlib.Path], src_path: pathlib.Path):
    return pathlib.Path(path).relative_to(src_path)


def _is_relative_to(path: pathlib.Path, other_path: pathlib.Path):
    """This replicates pathlib.Path.is_relative_to.

    Since bots still run python3.8, they do not have access to is_relative_to,
    which was introduced in python3.9.
    """
    try:
        path.relative_to(other_path)
        return True
    except ValueError:
        # This error is expected when path is not a subpath of other_path.
        return False


def class_is_interesting(name: str, prefixes: Tuple[str]):
    """Checks if a jdeps class is a class we are actually interested in."""
    if not prefixes or name.startswith(prefixes):
        return True
    return False


class JavaClassJdepsParser:
    """A parser for jdeps class-level dependency output."""

    def __init__(self):
        self._graph = class_dependency.JavaClassDependencyGraph()

    @property
    def graph(self):
        """The dependency graph of the jdeps output.

        Initialized as empty and updated using parse_raw_jdeps_output.
        """
        return self._graph

    def parse_raw_jdeps_output(self, build_target: str, jdeps_output: str,
                               prefixes: Tuple[str]):
        """Parses the entirety of the jdeps output."""
        for line in jdeps_output.split('\n'):
            self.parse_line(build_target, line, prefixes)

    def parse_line(self,
                   build_target: str,
                   line: str,
                   prefixes: Tuple[str] = ('org.chromium.', )):
        """Parses a line of jdeps output.

        The assumed format of the line starts with 'name_1 -> name_2'.
        """
        parsed = line.split()
        if len(parsed) <= 3:
            return
        if parsed[2] == 'not' and parsed[3] == 'found':
            return
        if parsed[1] != '->':
            return

        dep_from = parsed[0]
        dep_to = parsed[2]
        if not class_is_interesting(dep_from, prefixes):
            return

        key_from, nested_from = class_dependency.split_nested_class_from_key(
            dep_from)
        from_node: class_dependency.JavaClass = self._graph.add_node_if_new(
            key_from)
        from_node.add_build_target(build_target)

        if not class_is_interesting(dep_to, prefixes):
            return

        key_to, nested_to = class_dependency.split_nested_class_from_key(
            dep_to)

        self._graph.add_node_if_new(key_to)
        if key_from != key_to:  # Skip self-edges (class-nested dependency)
            self._graph.add_edge_if_new(key_from, key_to)
        if nested_from is not None:
            from_node.add_nested_class(nested_from)
        if nested_to is not None:
            from_node.add_nested_class(nested_to)


def _calculate_cache_path(filepath: pathlib.Path, src_path: pathlib.Path,
                          build_output_dir: pathlib.Path) -> pathlib.Path:
    """Return a cache path for jdeps that is always in the output dir.

    Also ensures that the cache file's parent directories exist if the original
    file was not already in the output dir.

    Example:
    - Given:
      src_path = /cr/src
      build_output_dir = /cr/src/out/Debug
    - filepath = /cr/src/out/Debug/a/d/file.jar
      Returns: /cr/src/out/Debug/a/d/file.jdeps_cache
    - filepath = /cr/src/out/Debug/../../b/c/file.jar
      Returns: /cr/src/out/Debug/jdeps_cache/b/c/file.jdeps_cache
    """
    filepath = filepath.resolve(strict=True)
    if _is_relative_to(filepath, build_output_dir):
        return filepath.with_suffix('.jdeps_cache')
    assert src_path in filepath.parents, f'Jar file not under src: {filepath}'
    jdeps_cache_dir = build_output_dir / 'jdeps_cache'
    relpath = filepath.relative_to(src_path)
    cache_path = jdeps_cache_dir / relpath.with_suffix('.jdeps_cache')
    # The parent dirs may not exist since this path is re-parented from //src to
    # //src/out/Dir.
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    return cache_path


def _run_jdeps(jdeps_path: pathlib.Path, src_path: pathlib.Path,
               build_output_dir: pathlib.Path,
               filepath: pathlib.Path) -> Optional[str]:
    """Runs jdeps on the given filepath and returns the output.

    Uses a simple file cache for the output of jdeps. If the jar file's mtime is
    older than the jdeps cache then just use the cached content instead.
    Otherwise jdeps is run again and the output used to update the file cache.

    Tested Nov 2nd, 2022:
    - With all cache hits, script takes 13 seconds.
    - Without the cache, script takes 1 minute 14 seconds.
    """
    # Some __compile_java targets do not generate a .jar file, skipping these
    # does not affect correctness.
    if not filepath.exists():
        return None

    cache_path = _calculate_cache_path(filepath, src_path, build_output_dir)
    if (cache_path.exists()
            and cache_path.stat().st_mtime > filepath.stat().st_mtime):
        logging.debug(
            f'Found valid jdeps cache at {_relsrc(cache_path, src_path)}')
        with cache_path.open() as f:
            return f.read()

    # Cache either doesn't exist or is older than the jar file.
    logging.debug(
        f'Running jdeps and parsing output for {_relsrc(filepath, src_path)}')
    output = subprocess_utils.run_command([
        str(jdeps_path),
        '-R',
        '-verbose:class',
        '--multi-release',  # Some jars support multiple JDK releases.
        'base',
        str(filepath),
    ])
    with cache_path.open('w') as f:
        f.write(output)
    return output


def _run_gn_desc_list_dependencies(build_output_dir: pathlib.Path, target: str,
                                   gn_path: str,
                                   src_path: pathlib.Path) -> str:
    """Runs gn desc to list all jars that a target depends on.

    This includes direct and indirect dependencies."""
    return subprocess_utils.run_command(
        [gn_path, 'desc', '--all',
         str(build_output_dir), target, 'deps'],
        cwd=src_path)


JarTargetDict = Dict[str, pathlib.Path]


def _should_ignore(jar_path: str) -> bool:
    for ignored_jar_path in _IGNORED_JAR_PATHS:
        if ignored_jar_path in jar_path:
            return True
    return False


def run_and_parse_list_java_targets(build_output_dir: pathlib.Path,
                                    j_value: Optional[str], show_ninja: bool,
                                    src_path: pathlib.Path) -> JarTargetDict:
    """Runs list_java_targets.py to find all jars generated in the build.

    Returns a dict similar to parse_original_targets_and_jars.
    """
    # pylint: disable=line-too-long
    # Example output with build_output_dir as 'out/Debug':
    # //gpu:gpu_benchmark: obj/gpu/gpu_benchmark__apk.processed.jar
    # //media/midi:midi_java: obj/media/midi/midi_java.javac.jar
    # //clank/third_party/google3:clock_java: ../../clank/third_party/google3/libs/clock.jar
    # pylint: enable=line-too-long
    cmd = [
        str(src_path / 'build' / 'android' / 'list_java_targets.py'),
        '-C',
        str(build_output_dir),
        '--gn-labels',  # Adds the // prefix.
        '--query',
        'deps_info.unprocessed_jar_path',
    ]
    if j_value:
        cmd += ['-j', j_value]
    if not show_ninja:
        cmd.append('-q')
    output = subprocess_utils.run_command(cmd)
    jar_dict: JarTargetDict = {}
    # pylint: disable=line-too-long
    # Resulting jar_dict after parsing: {
    #   '//gpu:gpu_benchmark': pathlib.Path('out/Debug/obj/gpu/gpu_benchmark__apk.processed.jar'),
    #   '//media/midi:midi_java': pathlib.Path('out/Debug/obj/media/midi/midi_java.javac.jar'),
    #   '//clank/third_party/google3:clock_java: pathlib.Path('out/Debug/../../clank/third_party/google3/libs/clock.jar'),
    # }
    # pylint: enable=line-too-long
    for line in output.splitlines():
        target_name, jar_path = line.split(': ', 1)
        if _should_ignore(jar_path):
            continue
        jar_dict[target_name] = build_output_dir / jar_path
    return jar_dict


def parse_original_targets_and_jars(gn_desc_output: str,
                                    build_output_dir: pathlib.Path,
                                    cr_position: int) -> JarTargetDict:
    """Parses gn desc output to list original java targets and output jar paths.

    Returns a dict mapping build_target: str to jar_path: str, where:
    - build_target is the original java dependency target in the form
      "//path/to:target".
    - jar_path is the path to the built jar in the build_output_dir,
      including the path to the output dir.
    """
    jar_dict: JarTargetDict = {}
    for build_target_line in gn_desc_output.split('\n'):
        if not build_target_line.endswith('__compile_java'):
            continue
        build_target = build_target_line.strip()
        original_build_target = build_target.replace('__compile_java', '')
        jar_path = _get_jar_path_for_target(build_output_dir, build_target,
                                            cr_position)
        jar_dict[original_build_target] = jar_path
    return jar_dict


def _get_jar_path_for_target(build_output_dir: pathlib.Path, build_target: str,
                             cr_position: int) -> pathlib.Path:
    """Calculates the output location of a jar for a java build target."""
    if cr_position == 0:  # Not running on main branch, use current convention.
        subdirectory = 'obj'
    elif cr_position < 761560:  # crrev.com/c/2161205
        subdirectory = 'gen'
    else:
        subdirectory = 'obj'
    target_path, target_name = build_target.split(':')
    assert target_path.startswith('//'), \
        f'Build target should start with "//" but is: "{build_target}"'
    jar_dir = target_path[len('//'):]
    jar_name = target_name.replace('__compile_java', '.javac.jar')
    return build_output_dir / subdirectory / jar_dir / jar_name


def main():
    """Runs jdeps on all JARs a build target depends on.

    Creates a JSON file from the jdeps output."""

    arg_parser = argparse.ArgumentParser(
        description='Runs jdeps (dependency analysis tool) on all JARs and '
        'writes the resulting dependency graph into a JSON file.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-o',
        '--output',
        required=True,
        help='Path to the file to write JSON output to. Will be created '
        'if it does not yet exist and overwrite existing content if it does.')
    arg_parser.add_argument(
        '-C',
        '--build_output_dir',
        help='Build output directory, will attempt to guess if not provided.')
    arg_parser.add_argument(
        '-p',
        '--prefix',
        default=[],
        dest='prefixes',
        action='append',
        help='If any package prefixes are passed, these will be used to filter '
        'classes so that only classes with a package matching one of the '
        'prefixes are kept in the graph. By default no filtering is performed.'
    )
    arg_parser.add_argument(
        '-t',
        '--target',
        help='If a specific target is specified, only transitive deps of that '
        'target are included in the graph. By default all known javac jars are '
        'included.')
    arg_parser.add_argument('-d',
                            '--checkout-dir',
                            default=_SRC_PATH,
                            help='Path to the chromium checkout directory. By '
                            'default the checkout containing this script is '
                            'used.')
    arg_parser.add_argument('-j',
                            help='-j value to pass to ninja instead of using '
                            'autoninja to autoset this value.')
    arg_parser.add_argument('--jdeps-path',
                            help='Path to the jdeps executable.')
    arg_parser.add_argument('-g',
                            '--gn-path',
                            default='gn',
                            help='Path to the gn executable.')
    arg_parser.add_argument('--skip-rebuild',
                            action='store_true',
                            help='Skip rebuilding, useful on bots where '
                            'compile is a separate step right before running '
                            'this script.')
    arg_parser.add_argument('--show-ninja',
                            action='store_true',
                            help='Used to show ninja output.')
    arg_parser.add_argument('-v',
                            '--verbose',
                            action='store_true',
                            help='Used to display detailed logging.')
    args = arg_parser.parse_args()

    if args.verbose:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    src_path = pathlib.Path(args.checkout_dir)

    if args.jdeps_path:
        jdeps_path = pathlib.Path(args.jdeps_path)
    else:
        jdeps_path = src_path / 'third_party/jdk/current/bin/jdeps'

    cr_position_str = git_metadata_utils.get_head_commit_cr_position(src_path)
    cr_position = int(cr_position_str) if cr_position_str else 0

    if args.build_output_dir:
        constants.SetOutputDirectory(args.build_output_dir)
    constants.CheckOutputDirectory()
    args.build_output_dir = pathlib.Path(constants.GetOutDirectory())
    logging.info(
        f'Using output dir: {_relsrc(args.build_output_dir, src_path)}')
    args_gn_path = args.build_output_dir / 'args.gn'
    logging.info(f'Contents of {_relsrc(args_gn_path, src_path)}:')
    with open(args_gn_path) as f:
        print(f.read())

    logging.info('Getting list of dependency jars...')
    if args.target:
        gn_desc_output = _run_gn_desc_list_dependencies(
            args.build_output_dir, args.target, args.gn_path, src_path)
        target_jars: JarTargetDict = parse_original_targets_and_jars(
            gn_desc_output, args.build_output_dir, cr_position)
    else:
        target_jars: JarTargetDict = run_and_parse_list_java_targets(
            args.build_output_dir, args.j, args.show_ninja, src_path)

    if args.skip_rebuild:
        logging.info(f'Skipping rebuilding jars.')
    else:
        # Always re-compile jars to have the most up-to-date jar files. This is
        # especially important when running this script locally and testing out
        # build changes that affect the dependency graph. When a specific target
        # is used via -t, however, we need to specify targets instead of jars
        # since some targets don't output their corresponding jars.
        if args.target:
            # Remove the // prefix and add the __compile_java suffix. This is
            # guaranteed to exist since the targets were derived by removing the
            # suffix earlier in parse_original_targets_and_jars.
            to_recompile = [
                t[2:] + '__compile_java' for t in target_jars.keys()
            ]
        else:
            to_recompile = [
                p.relative_to(args.build_output_dir)
                for p in target_jars.values()
            ]
        if not args.show_ninja:
            logging.info(
                f'Re-building {len(rel_jar_paths)} jars for up-to-date deps. '
                'This may take a while the first time through. Pass '
                '--show-ninja to see ninja progress.')
        if args.j:
            cmd = [subprocess_utils.resolve_ninja(), '-j', args.j]
        else:
            cmd = [subprocess_utils.resolve_autoninja()]
        subprocess.run(cmd + ['-C', args.build_output_dir] + to_recompile,
                       capture_output=not args.show_ninja,
                       check=True)

    logging.info('Running jdeps...')
    # jdeps already has some parallelism
    jdeps_process_number = math.ceil(multiprocessing.cpu_count() / 2)
    with multiprocessing.Pool(jdeps_process_number) as pool:
        jdeps_outputs = pool.map(
            functools.partial(_run_jdeps, jdeps_path, src_path,
                              args.build_output_dir), target_jars.values())

    logging.info('Parsing jdeps output...')
    jdeps_parser = JavaClassJdepsParser()
    for raw_jdeps_output, build_target in zip(jdeps_outputs,
                                              target_jars.keys()):
        if raw_jdeps_output is None:
            continue
        logging.debug(f'Parsing jdeps for {build_target}')
        jdeps_parser.parse_raw_jdeps_output(build_target,
                                            raw_jdeps_output,
                                            prefixes=tuple(args.prefixes))

    class_graph = jdeps_parser.graph
    logging.info(f'Parsed class-level dependency graph, '
                 f'got {class_graph.num_nodes} nodes '
                 f'and {class_graph.num_edges} edges.')

    package_graph = package_dependency.JavaPackageDependencyGraph(class_graph)
    logging.info(f'Created package-level dependency graph, '
                 f'got {package_graph.num_nodes} nodes '
                 f'and {package_graph.num_edges} edges.')

    target_graph = target_dependency.JavaTargetDependencyGraph(class_graph)
    logging.info(f'Created target-level dependency graph, '
                 f'got {target_graph.num_nodes} nodes '
                 f'and {target_graph.num_edges} edges.')

    logging.info(f'Dumping JSON representation to {args.output}.')
    serialization.dump_class_and_package_and_target_graphs_to_file(
        class_graph, package_graph, target_graph, args.output, src_path)
    logging.info('Done')


if __name__ == '__main__':
    main()
