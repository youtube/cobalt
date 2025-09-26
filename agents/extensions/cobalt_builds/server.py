
import sys

try:
    #!/usr/bin/env vpython3
    # Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
    """A MCP server for building and running Cobalt binaries."""

    # [VPYTHON:BEGIN]
    # python_version: "3.11"
    # wheel: <
    #   name: "infra/python/wheels/mcp-py3"
    #   version: "version:1.9.4"
    # >
    # wheel: <
    #   name: "infra/python/wheels/pydantic-py3"
    #   version: "version:2.11.7"
    # >
    # wheel: <
    #   name: "infra/python/wheels/starlette-py3"
    #   version: "version:0.47.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/anyio-py3"
    #   version: "version:4.9.0"
    # >
    # wheel: <
    #   name: "infra/python/wheels/sniffio-py3"
    #   version: "version:1.3.0"
    # >
    # wheel: <
    #   name: "infra/python/wheels/idna-py3"
    #   version: "version:3.4"
    # >
    # wheel: <
    #   name: "infra/python/wheels/typing-extensions-py3"
    #   version: "version:4.13.2"
    # >
    # wheel: <
    #   name: "infra/python/wheels/httpx_sse-py3"
    #   version: "version:0.4.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/httpx-py3"
    #   version: "version:0.28.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/certifi-py3"
    #   version: "version:2025.4.26"
    # >
    # wheel: <
    #   name: "infra/python/wheels/httpcore-py3"
    #   version: "version:1.0.9"
    # >
    # wheel: <
    #   name: "infra/python/wheels/h11-py3"
    #   version: "version:0.16.0"
    # >
    # wheel: <
    #   name: "infra/python/wheels/pydantic-settings-py3"
    #   version: "version:2.10.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/python-multipart-py3"
    #   version: "version:0.0.20"
    # >
    # wheel: <
    #   name: "infra/python/wheels/sse-starlette-py3"
    #   version: "version:2.4.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/uvicorn-py3"
    #   version: "version:0.35.0"
    # >
    # wheel: <
    #   name: "infra/python/wheels/annotated-types-py3"
    #   version: "version:0.7.0"
    # >
    # wheel: <
    #   name: "infra/python/wheels/pydantic_core/${vpython_platform}"
    #   version: "version:2.33.2"
    # >
    # wheel: <
    #   name: "infra/python/wheels/typing-inspection-py3"
    #   version: "version:0.4.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/python-dotenv-py3"
    #   version: "version:1.1.1"
    # >
    # wheel: <
    #   name: "infra/python/wheels/click-py3"
    #   version: "version:8.0.3"
    # >
    # [VPYTHON:END]

    import argparse
    import asyncio
    import json
    import os
    import re
    import signal
    import subprocess
    import time
    import uuid
    from concurrent.futures import ThreadPoolExecutor
    from datetime import datetime
    from typing import Dict, List
    import logging
    from mcp.server.fastmcp import FastMCP
    from pydantic import BaseModel

    from analysis import analyze_log
    import config

    # Suppress informational logs from the MCP server
    logging.getLogger('mcp').setLevel(logging.WARNING)

    from build_helpers import (
        PLATFORM_ALIASES,
        TaskStatus,
        get_out_dir,
        get_platform
    )

    RUNNABLE_PLATFORMS = [
        'evergreen-x64',
        'linux-x64x11',
        'linux-x64x11-modular',
        'linux-x64x11-no-starboard',
    ]

    # --- MCP Server Setup ---
    mcp = FastMCP(
        'cobalt_builds',
        'A server for building and running Cobalt binaries.',
    )

    executor = ThreadPoolExecutor(max_workers=4)

    # All status is now derived from the filesystem, no in-memory storage needed.


    def _generate_task_id(task_type: str, name: str) -> str:
      """Generates a human-readable task ID."""
      timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
      return f'{task_type}_{name}_{timestamp}'


    class TaskRunner:
      """A class to encapsulate the logic for running build and test tasks."""

      def __init__(self, task_id: str, cmd: List[str], log_path: str):
        self.task_id = task_id
        self.cmd = cmd
        self.log_path = log_path

      def run_sync(self) -> None:
        """Runs the task synchronously."""
        try:
          with open(self.log_path, 'w') as log_file:
            log_file.write(' '.join(self.cmd) + '\n')
            log_file.flush()

            process = subprocess.Popen(
                self.cmd, stdout=log_file, stderr=subprocess.STDOUT)
            process.wait()

            log_file.write(f'\nRETURN_CODE: {process.returncode}\n')

        except Exception as e:
          with open(self.log_path, 'a') as log_file:
            log_file.write(f'\nTHREAD_EXCEPTION: {e}\n')
            log_file.write(f'\nRETURN_CODE: -1\n')

      async def run_async(self) -> None:
        """Runs the task asynchronously."""
        try:
          with open(self.log_path, 'w') as log_file:
            log_file.write(' '.join(self.cmd) + '\n')
            log_file.flush()
            process = await asyncio.create_subprocess_exec(
                *self.cmd, stdout=log_file, stderr=subprocess.STDOUT)
            await process.wait()

          with open(self.log_path, 'a') as f:
            f.write(f'\nRETURN_CODE: {process.returncode}\n')

        except FileNotFoundError:
          with open(self.log_path, 'a') as f:
            f.write(f'\nERROR: Binary not found: {self.cmd[0]}\n')
            f.write(f'\nRETURN_CODE: -1\n')


    # --- Tool Definitions ---
    @mcp.tool()
    def get_supported_platforms() -> List[str]:
      """Returns a list of supported platforms, their aliases, and whether they are runnable."""
      gn_py_path = './cobalt/build/gn.py'
      platforms = []
      try:
        with open(gn_py_path, 'r') as f:
          content = f.read()
          chromium_platforms = re.search(r'_CHROMIUM_PLATFORMS = \[(.*?)\]',
                                         content, re.DOTALL)
          if chromium_platforms:
            platforms.extend(
                [p.strip().strip("'\"") for p in chromium_platforms.group(1).split(',')
                 if p.strip()])
          cobalt_starboard_platforms = re.search(
              r'_COBALT_STARBOARD_PLATFORMS = \[(.*?)\]', content, re.DOTALL)
          if cobalt_starboard_platforms:
            platforms.extend([
                p.strip().strip("'\"")
                for p in cobalt_starboard_platforms.group(1).split(',')
                if p.strip()
            ])
          cobalt_android_platforms = re.search(
              r'_COBALT_ANDROID_PLATFORMS = \[(.*?)\]', content, re.DOTALL)
          if cobalt_android_platforms:
            platforms.extend([
                p.strip().strip("'\"")
                for p in cobalt_android_platforms.group(1).split(',') if p.strip()
            ])
      except IOError as e:
        return [f"Error reading platforms from {gn_py_path}: {e}"]

      reversed_aliases = {v: k for k, v in PLATFORM_ALIASES.items()}
      formatted_platforms = []
      for p in sorted(list(set(platforms))):
        info = []
        alias = reversed_aliases.get(p)
        if alias:
          info.append(f'alias: {alias}')
        if p in RUNNABLE_PLATFORMS:
          info.append('runnable')

        if info:
          formatted_platforms.append(f"{p} ({', '.join(info)})")
        else:
          formatted_platforms.append(p)
      formatted_platforms.append('--- END OF PLATFORMS ---')
      return formatted_platforms


    @mcp.tool()
    def configure_build(platform: str, variant: str) -> str:
      """Configures a build and returns the output directory."""
      out_dir = get_out_dir(platform, variant)
      cmd = [
          './cobalt/build/gn.py',
          f'-p={get_platform(platform)}',
          f'-c={variant}',
          out_dir
      ]
      subprocess.run(cmd, check=True)
      return out_dir


    def _get_final_task_result(task_id: str, cmd: List[str]) -> str:
        """Formats the final result of a synchronous build or run task."""
        task_status = status(task_id)
        status_dict = task_status.model_dump()

        task_type = 'build' if task_id.startswith('build_') else 'run'
        status_dict[f'{task_type}_id'] = task_id
        status_dict['command'] = ' '.join(cmd)

        if task_status.status == 'success':
            message = f"✅ {task_type.capitalize()} '{task_id}' completed successfully."
            log_snippet = ""
            try:
                with open(task_status.output_log, 'r') as f:
                    lines = f.readlines()
                # Get last 5 non-empty lines, stripping them
                log_snippet = '\n'.join([line.strip() for line in lines if line.strip()][-5:])
            except Exception as e:
                log_snippet = f"Could not read log snippet: {e}"

            status_dict['message'] = message
            status_dict['log_snippet'] = log_snippet
            return json.dumps(status_dict)
        else:  # Handles 'failure' and any other non-success status
            return analyze_log(task_id)


    @mcp.tool()
    async def build(platform: str,
                  variant: str,
                  target: str,
                  extra_args: List[str] | None = None,
                  dry_run: bool = False,
                  background: bool = False) -> str:
      """Starts a build. By default, it waits for completion.

      To run in the background, set the 'background' parameter to True.
      When running in the background, use the 'status' tool with the returned
      'build_id' to check for completion.
      If the build fails, the log will be automatically analyzed.
      """
      out_dir = get_out_dir(platform, variant)
      target_to_build = target
      if 'android' in get_platform(platform):
        if not target.endswith('__apk'):
          target_to_build = f'{target}__apk'
      cmd = ['autoninja', '-C', out_dir, target_to_build]
      if extra_args:
        cmd.extend(extra_args)
      if dry_run:
        return json.dumps({'command': ' '.join(cmd)})

      build_id = _generate_task_id('build', target)
      log_path = f'/tmp/{build_id}.log'

      build_info_path = f'/tmp/{build_id}.json'
      build_info = {
          'platform': platform,
          'variant': variant,
          'target': target,
          'extra_args': extra_args,
      }
      with open(build_info_path, 'w') as f:
        json.dump(build_info, f)

      task_runner = TaskRunner(build_id, cmd, log_path)
      loop = asyncio.get_running_loop()

      if background:
        loop.run_in_executor(executor, task_runner.run_sync)
        return json.dumps({
            'build_id': build_id,
            'command': ' '.join(cmd),
            'output_log': log_path,
            'status': 'in_progress'
        })
      else:
        # Run synchronously
        await loop.run_in_executor(executor, task_runner.run_sync)
        return _get_final_task_result(build_id, cmd)


    @mcp.tool()
    async def run(platform: str | None = None,
                variant: str | None = None,
                binary_name: str | None = None,
                build_id_or_log_file: str | None = None,
                args: List[str] | None = None,
                dry_run: bool = False,
                debug: bool = False,
                background: bool = False) -> str:
      """Runs a binary. By default, it waits for completion.

      To run in the background, set the 'background' parameter to True.
      When running in the background, use the 'status' tool with the returned
      'run_id' to check for completion.
      If the run fails, the log will be automatically analyzed.

      Can be called in two ways:
      1. By providing `platform`, `variant`, and `binary_name`.
      2. By providing a `build_id_or_log_file` from a previous build.

      Additional arguments can be passed to the binary via the `args` list.
      """
      if build_id_or_log_file:
        if platform or variant or binary_name:
          return json.dumps({
              'error':
                  'Cannot specify both build_id_or_log_file and platform/variant/binary_name.'
          })
        task_id = build_id_or_log_file
        if '/' in task_id:
          task_id = os.path.basename(task_id).replace('.log', '')

        build_info_path = f'/tmp/{task_id}.json'
        if not os.path.isfile(build_info_path):
          return json.dumps(
              {'error': f'Build info file not found: {build_info_path}'})
        try:
          with open(build_info_path, 'r') as f:
            build_info = json.load(f)
            platform = build_info.get('platform')
            variant = build_info.get('variant')
            binary_name = build_info.get('target')
        except (IOError, json.JSONDecodeError) as e:
          return json.dumps(
              {'error': f'Error reading build info from {build_info_path}: {e}'})

      if not all([platform, variant, binary_name]):
        return json.dumps({
            'error':
                'Must provide either build_id_or_log_file or platform, variant, and binary_name.'
        })

      out_dir = get_out_dir(platform, variant)
      cmd_list = []
      platform_full = get_platform(platform)

      run_id = _generate_task_id('run', binary_name)
      log_path = f'/tmp/{run_id}.log'

      if 'android' in platform_full:
        apk_name = binary_name.replace('_loader', '')
        apk_path = f'{out_dir}/{apk_name}_apk/{apk_name}-debug.apk'

        install_cmd_str = ' '.join(['adb', 'install', '-r', apk_path])
        run_cmd_list = [
            'build/android/test_runner.py', 'gtest', '-s', binary_name, '-v',
            '--isolated-script-test-launcher-retry-limit=0',
            '--output-directory', out_dir, '--', '--exitfirst'
        ]
        if args:
            run_cmd_list.extend(args)
        run_cmd_str = ' '.join(run_cmd_list)

        if dry_run:
            return json.dumps({'command': f'{install_cmd_str} && {run_cmd_str}'})

        script_content = f"""#!/bin/bash
    set -e
    echo "Installing APK from {apk_path}..."
    {install_cmd_str}
    echo "Running tests..."
    {run_cmd_str}
    """
        script_path = f'/tmp/{run_id}_script.sh'
        with open(script_path, 'w') as f:
            f.write(script_content)
        os.chmod(script_path, 0o755)
        cmd_list = [script_path]
      else:
        executable_name = binary_name
        if 'modular' in platform_full or 'evergreen' in platform_full:
            if not binary_name.endswith('_loader'):
                executable_name = f'{binary_name}_loader'
        
        executable_path = f'{out_dir}/{executable_name}'
        py_executable_path = f'{executable_path}.py'

        if 'evergreen' in platform_full and os.path.exists(py_executable_path):
            cmd_list = [py_executable_path]
        else:
            cmd_list = [executable_path]

        if args:
            cmd_list.extend(args)
        
        if dry_run:
            return json.dumps({'command': ' '.join(cmd_list)})

      if debug:
        cmd_list = ['gdb', '-ex="set confirm off"', '-ex=r', '-ex=bt', '-ex=q', '--args'] + cmd_list

      task_runner = TaskRunner(run_id, cmd_list, log_path)

      if background:
        asyncio.create_task(task_runner.run_async())
        return json.dumps({
            'run_id': run_id,
            'command': ' '.join(cmd_list),
            'output_log': log_path,
            'status': 'in_progress'
        })
      else:
        await task_runner.run_async()
        return _get_final_task_result(run_id, cmd_list)


    @mcp.tool()
    def stop(task_id: str) -> bool:
      """Stops a running task."""
      log_path = f'/tmp/{task_id}.log'
      if not os.path.exists(log_path):
        return False
      try:
        result = subprocess.run(
            ['fuser', log_path], capture_output=True, text=True, check=True)
        pid_to_kill = int(result.stdout.strip().split()[0])
        os.kill(pid_to_kill, signal.SIGTERM)
        # Best effort to mark the log as cancelled.
        with open(log_path, 'a') as f:
          f.write('\nRETURN_CODE: CANCELLED\n')
        return True
      except (FileNotFoundError, subprocess.CalledProcessError,
              ProcessLookupError, ValueError):
        return False




    @mcp.tool()
    def status(task_id: str) -> TaskStatus:
      """Checks the status of a task."""
      log_path = f'/tmp/{task_id}.log'

      if not os.path.exists(log_path):
        return TaskStatus(status='not_found', output_log='')

      # Use fuser as the source of truth for "in_progress"
      try:
        result = subprocess.run(
            ['fuser', log_path], capture_output=True, text=True)
        if result.stdout.strip():
          pid = int(result.stdout.strip().split()[0])
          return TaskStatus(status='in_progress', output_log=log_path, pid=pid)
      except (FileNotFoundError, ValueError):
        pass # Fall through to log check

      # If fuser finds no process, the task is finished.
      try:
        with open(log_path, 'r') as f:
          if 'RETURN_CODE: 0' in f.read():
            return TaskStatus(status='success', output_log=log_path)
      except IOError:
        pass # Fall through to failure

      # If the process is gone and there's no success marker, it's a failure.
      return TaskStatus(status='failure', output_log=log_path)


    @mcp.tool()
    def analyze(task_id_or_log_file: str, log_type: str | None = None) -> str:
      """Analyzes a log for errors and warnings.

      Accepts either a task ID from a build or run, or a direct path to a log file.
      The `log_type` can be 'build' or 'test'. If not provided, it's inferred
      from the log file name (e.g., 'build_...' or 'run_...').
      """
      return analyze_log(task_id_or_log_file, log_type)


    # --- Server Main ---
    def main() -> None:
      """Main entry point for the server."""
      try:
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '--serve', action='store_true', help='Run in stdio server mode.')
        parser.add_argument(
            '--log-dir', type=str, help='Directory to write logs to.')
        args = parser.parse_args()

        if args.serve:
          mcp.run(transport='stdio')
        else:
          print('This script is a server. Run with --serve to start.')
      except KeyboardInterrupt:
        # Silently exit on Ctrl+C, as the client will handle the user message.
        sys.exit(0)


    if __name__ == '__main__':
      main()
except KeyboardInterrupt:
    # Silently exit on Ctrl+C, as the client will handle the user message.
    sys.exit(0)
