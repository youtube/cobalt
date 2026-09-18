#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Generates Cobalt's filtered ICU data bundle (icudtl.dat) at build time."""

import argparse
import concurrent.futures
import glob
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys

_ICU_TOOLS = (
    'gencnval',
    'gencfu',
    'makeconv',
    'genbrk',
    'gensprep',
    'gendict',
    'icupkg',
    'genrb',
    'gencmn',
)


def _get_host_toolchain(icu_root):
  """Returns (cc, cxx, ar, cflags, ldflags) for building host ICU CLI tools."""
  src_root = os.path.abspath(os.path.join(icu_root, '..', '..'))
  llvm_bin = os.path.join(src_root, 'third_party', 'llvm-build',
                          'Release+Asserts', 'bin')
  clang_bin = os.path.join(llvm_bin, 'clang')
  clangxx_bin = os.path.join(llvm_bin, 'clang++')
  llvm_ar_bin = os.path.join(llvm_bin, 'llvm-ar')

  if platform.system() == 'Darwin':
    cc = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--find', 'clang'], text=True).strip()
    cxx = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--find', 'clang++'], text=True).strip()
    ar = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--find', 'ar'], text=True).strip()
    sdk_path = subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
    cflags = ['-isysroot', sdk_path]
    ldflags = ['-isysroot', sdk_path]
    return cc, cxx, ar, cflags, ldflags

  sysroot = os.path.join(src_root, 'build', 'linux',
                         'debian_bullseye_amd64-sysroot')
  if os.path.isfile(clangxx_bin) and os.path.isdir(sysroot):
    cc = clang_bin
    cxx = clangxx_bin
    ar = llvm_ar_bin if os.path.isfile(llvm_ar_bin) else 'ar'
    cflags = [f'--sysroot={sysroot}']
    ldflags = [f'--sysroot={sysroot}', '-fuse-ld=lld']
    return cc, cxx, ar, cflags, ldflags

  cc = shutil.which('clang') or shutil.which('gcc') or 'cc'
  cxx = shutil.which('clang++') or shutil.which('g++') or 'c++'
  ar = shutil.which('llvm-ar') or shutil.which('ar') or 'ar'
  return cc, cxx, ar, [], []


def _build_host_tools(icu_src, build_dir, num_cores):
  """Compiles and links the 9 ICU host CLI tools into build_dir/bin."""
  bin_dir = os.path.join(build_dir, 'bin')
  if all(
      os.path.isfile(os.path.join(bin_dir, tool)) for tool in _ICU_TOOLS):
    return bin_dir

  obj_dir = os.path.join(build_dir, 'obj')
  os.makedirs(obj_dir, exist_ok=True)
  os.makedirs(bin_dir, exist_ok=True)

  icu_root = os.path.abspath(os.path.join(icu_src, '..'))
  cc, cxx, ar, host_cflags, host_ldflags = _get_host_toolchain(icu_root)

  common_flags = host_cflags + [
      '-O2',
      '-w',
      '-DU_STATIC_IMPLEMENTATION',
      '-DU_ATTRIBUTE_DEPRECATED=',
      '-DU_COMMON_IMPLEMENTATION',
      '-DU_I18N_IMPLEMENTATION',
      '-DU_TOOLUTIL_IMPLEMENTATION',
      '-I' + os.path.join(icu_src, 'common'),
      '-I' + os.path.join(icu_src, 'i18n'),
      '-I' + os.path.join(icu_src, 'tools', 'toolutil'),
  ]

  lib_sources = []
  for sub in ('stubdata', 'common', 'i18n', 'tools/toolutil'):
    lib_sources.extend(sorted(glob.glob(os.path.join(icu_src, sub, '*.c'))))
    lib_sources.extend(sorted(glob.glob(os.path.join(icu_src, sub, '*.cpp'))))

  def compile_source(src_path, extra_includes=()):
    rel = os.path.relpath(src_path, icu_src).replace(os.sep, '_')
    obj_path = os.path.join(obj_dir, rel + '.o')
    if os.path.isfile(obj_path):
      return obj_path
    is_cpp = src_path.endswith('.cpp')
    lang_flags = (['-std=c++17'] if is_cpp else
                  ['-std=gnu11', '-Dchar16_t=uint16_t'])
    cmd = ([cxx if is_cpp else cc] + common_flags + list(extra_includes) +
           lang_flags + ['-c', src_path, '-o', obj_path])
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
      raise RuntimeError(f'Failed to compile {src_path}:\n{res.stderr}')
    return obj_path

  with concurrent.futures.ThreadPoolExecutor(max_workers=num_cores) as executor:
    lib_objs = list(executor.map(compile_source, lib_sources))

  lib_a = os.path.join(build_dir, 'libicu_host.a')
  if os.path.exists(lib_a):
    os.remove(lib_a)
  subprocess.run([ar, 'rcs', lib_a] + lib_objs, check=True)

  def build_tool(tool):
    tool_dir = os.path.join(icu_src, 'tools', tool)
    tool_srcs = [
        s for s in sorted(
            glob.glob(os.path.join(tool_dir, '*.c')) +
            glob.glob(os.path.join(tool_dir, '*.cpp')))
        if os.path.basename(s) != 'derb.cpp'
    ]
    tool_objs = [
        compile_source(s, extra_includes=('-I' + tool_dir,)) for s in tool_srcs
    ]
    out_bin = os.path.join(bin_dir, tool)
    link_cmd = ([cxx] + host_ldflags + ['-o', out_bin] + tool_objs +
                [lib_a, '-ldl', '-lpthread', '-lm'])
    res = subprocess.run(link_cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
      raise RuntimeError(f'Failed to link {tool}:\n{res.stderr}')
    return out_bin

  with concurrent.futures.ThreadPoolExecutor(
      max_workers=len(_ICU_TOOLS)) as executor:
    list(executor.map(build_tool, _ICU_TOOLS))

  return bin_dir


def _build_filtered_icudata(icu_src, filter_file, bin_dir, build_dir,
                            num_cores):
  """Runs icutools.databuilder and gencmn to generate filtered icudt74l.dat."""
  sys.path.insert(0, os.path.join(icu_src, 'python'))
  sys.path.insert(0, os.path.join(icu_src, 'data'))
  # pylint: disable=import-outside-toplevel,g-import-not-at-top
  from icutools.databuilder import __main__ as db_main
  from icutools.databuilder import filtration
  from icutools.databuilder import utils
  from icutools.databuilder.request_types import CopyRequest
  from icutools.databuilder.request_types import PrintFileRequest
  from icutools.databuilder.request_types import RepeatedExecutionRequest
  from icutools.databuilder.request_types import SingleExecutionRequest
  from icutools.databuilder.request_types import VariableRequest
  import BUILDRULES
  # pylint: enable=import-outside-toplevel,g-import-not-at-top

  out_build_dir = os.path.join(build_dir, 'data_out', 'icudt74l')
  tmp_dir = os.path.join(build_dir, 'data_tmp')
  shutil.rmtree(out_build_dir, ignore_errors=True)
  shutil.rmtree(tmp_dir, ignore_errors=True)
  os.makedirs(out_build_dir, exist_ok=True)
  os.makedirs(tmp_dir, exist_ok=True)

  db_args = db_main.flag_parser.parse_args([
      '--mode=unix-exec',
      f"--src_dir={os.path.join(icu_src, 'data')}",
      f'--filter_file={filter_file}',
      f'--out_dir={out_build_dir}',
      f'--tmp_dir={tmp_dir}',
      f'--tool_dir={bin_dir}',
      '--seqmode=parallel',
  ])
  config = db_main.Config(db_args)
  common_vars = {
      'SRC_DIR': db_args.src_dir,
      'IN_DIR': db_args.src_dir,
      'OUT_DIR': db_args.out_dir,
      'TMP_DIR': db_args.tmp_dir,
      'FILTERS_DIR': config.filter_dir,
      'CWD_DIR': os.getcwd(),
      'INDEX_NAME': 'res_index',
      'ICUDATA_CHAR': 'l',
      'LIBRARY_DATA_DIR': os.path.join(db_args.out_dir, 'build'),
  }
  io = db_main.IO(db_args.src_dir)
  requests = BUILDRULES.generate(config, io, common_vars)
  if 'fileReplacements' in config.filters_json_data:
    common_vars['IN_DIR'] = '{TMP_DIR}/in'.format(**common_vars)
    requests = db_main.add_copy_input_requests(requests, config, common_vars)
  requests = filtration.apply_filters(requests, config, io)
  requests = utils.flatten_requests(requests, config, common_vars)
  build_dirs = utils.compute_directories(requests)
  for bd in build_dirs:
    os.makedirs(bd.format(**common_vars), exist_ok=True)

  def exec_cmd(cmd_str):
    res = subprocess.run(
        cmd_str, shell=True, capture_output=True, text=True, check=False)
    if res.returncode != 0:
      raise RuntimeError(f'ICU data command failed: {cmd_str}\n{res.stderr}')

  # Run icupkg and gencnval requests first because gencfu depends on core
  # .icu/.nrm files being present in out_build_dir.
  early_requests = [
      r for r in requests
      if getattr(getattr(r, 'tool', None), 'name', None) in ('icupkg',
                                                             'gencnval')
  ]
  later_requests = [
      r for r in requests
      if getattr(getattr(r, 'tool', None), 'name', None) not in ('icupkg',
                                                                 'gencnval')
  ]

  with concurrent.futures.ThreadPoolExecutor(max_workers=num_cores) as executor:
    for req in early_requests + later_requests:
      if isinstance(req, PrintFileRequest):
        out_path = '{DIRNAME}/{FILENAME}'.format(
            DIRNAME=utils.dir_for(req.output_file).format(**common_vars),
            FILENAME=req.output_file.filename,
        )
        with open(out_path, 'w', encoding='utf-8') as f:
          f.write(req.content)
      elif isinstance(req, CopyRequest):
        in_path = '{DIRNAME}/{FILENAME}'.format(
            DIRNAME=utils.dir_for(req.input_file).format(**common_vars),
            FILENAME=req.input_file.filename,
        )
        out_path = '{DIRNAME}/{FILENAME}'.format(
            DIRNAME=utils.dir_for(req.output_file).format(**common_vars),
            FILENAME=req.output_file.filename,
        )
        shutil.copyfile(in_path, out_path)
      elif isinstance(req, VariableRequest):
        pass
      elif isinstance(req, SingleExecutionRequest):
        tmpl = f'{bin_dir}/{req.tool.name} {{ARGS}}'
        cmd = utils.format_single_request_command(req, tmpl, common_vars)
        exec_cmd(cmd)
      elif isinstance(req, RepeatedExecutionRequest):
        tmpl = f'{bin_dir}/{req.tool.name} {{ARGS}}'
        cmds = [
            utils.format_repeated_request_command(req, tmpl, loop_vars,
                                                  common_vars)
            for loop_vars in utils.repeated_execution_request_looper(req)
        ]
        list(executor.map(exec_cmd, cmds))

  subprocess.run(
      [
          os.path.join(bin_dir, 'gencmn'),
          '-c',
          '-e',
          'icudt74',
          '-n',
          'icudt74l',
          '-s',
          out_build_dir,
          '-t',
          'dat',
          '-d',
          tmp_dir,
          '0',
          os.path.join(tmp_dir, 'icudata.lst'),
      ],
      check=True,
  )
  return os.path.join(tmp_dir, 'icudt74l.dat')


def main():
  parser = argparse.ArgumentParser(
      description='Generate Cobalt icudtl.dat from filters/cobalt.json.')
  parser.add_argument(
      '--filter-file',
      required=True,
      help='Path to the ICU JSON filter file (e.g. filters/cobalt.json).')
  parser.add_argument(
      '--out-file',
      required=True,
      help='Output path for generated icudtl.dat.')
  parser.add_argument(
      '--build-dir',
      required=True,
      help='Intermediate directory for caching ICU host tools and data build.')
  args = parser.parse_args()

  script_dir = os.path.dirname(os.path.abspath(__file__))
  icu_root = os.path.abspath(os.path.join(script_dir, '..'))
  icu_src = os.path.join(icu_root, 'source')
  filter_file = os.path.abspath(args.filter_file)
  out_file = os.path.abspath(args.out_file)
  build_dir = os.path.abspath(args.build_dir)

  os.makedirs(build_dir, exist_ok=True)
  os.makedirs(os.path.dirname(out_file), exist_ok=True)

  num_cores = multiprocessing.cpu_count() or 4
  bin_dir = _build_host_tools(icu_src, build_dir, num_cores)
  generated_dat = _build_filtered_icudata(icu_src, filter_file, bin_dir,
                                          build_dir, num_cores)

  tmp_out_file = f'{out_file}.tmp.{os.getpid()}'
  shutil.copyfile(generated_dat, tmp_out_file)
  os.replace(tmp_out_file, out_file)
  return 0


if __name__ == '__main__':
  sys.exit(main())
