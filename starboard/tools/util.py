#
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#
"""Common helper functions."""

import logging
import multiprocessing
import os
import shutil
import subprocess
import time
import zipfile


def Which(filename):
  """Searches the environment's PATH for |filename|, returning the first."""
  for path in os.environ['PATH'].split(os.pathsep):
    full_name = os.path.join(path, filename)
    if os.path.exists(full_name) and os.path.isfile(full_name):
      return full_name
  return None

def MakeDirs(destination_dir):
  """Wrapper around os.makedirs that is a noop if the directory exists."""
  if os.path.isdir(destination_dir):
    return
  logging.debug('Creating %s', destination_dir)
  os.makedirs(destination_dir)


def MakeCleanDirs(destination_dir):
  """Ensure that the directory exists and is empty."""
  if os.path.exists(destination_dir):
    logging.debug('Removing %s', destination_dir)
    shutil.rmtree(destination_dir)
  MakeDirs(destination_dir)


def SpawnProcess(cmd_line, env=None, cwd=None, shell=None):
  """Spawn a process that executes the command line.

  Wrapper around subprocess.Popen that behaves consistently across platforms
  with respect to executable files found on the PATH variable.

  Args:
    cmd_line: A list containing the executable and arguments.
    env: dict of environment variables
    cwd: Working directory to change into before executing
    shell: Override whether or not to run the command through the shell.
  Raises:
    RuntimeError: if process returns an error.
  Returns:
    A subprocess.Popen object.
  """

  logging.debug('Executing %s', cmd_line)
  if cwd:
    logging.debug('cwd is %s', cwd)

  if shell is None and os.name == 'nt':
    # On Windows the PATH variable is ignored when looking for executables
    # unless shell is set to True.
    shell = True

  if shell is None:
    # By default if the user passes in a command as a string,
    # assume they want it run under a subshell.
    # For lists of commands, assume they don't.
    shell = not isinstance(cmd_line, list)

  logging.debug('shell is %r', shell)

  return subprocess.Popen(cmd_line,
                          shell=shell,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          env=env,
                          cwd=cwd,
                          universal_newlines=True)


def Execute(cmd_line, env=None, cwd=None, shell=None):
  """Run the specific command line and handle error.

  Args:
    cmd_line: Command line. Can either be a list of args or a string.
    env: Dict of environment variables.
    cwd: Working directory for the process.
    shell: Set to True to spawn a shell for the process.
  Raises:
    RuntimeError: if process returns an error.
  Returns:
    bytearray containing any command output.
  """

  proc = SpawnProcess(cmd_line, env, cwd, shell)
  output = bytearray()
  for line in proc.stdout:
    logging.info(line.rstrip())
    output += line

  if proc.wait() != 0:
    msg = '%s failed (%d):\n%s' % (cmd_line, proc.returncode, output)
    raise RuntimeError(msg)
  return output


def ExecuteMultiple(cmd_line_list,
                    env=None,
                    cwd=None,
                    shell=None,
                    timeout=None):
  """Spawn multiple processes for each argument in the list.

  Launch a process for each item in cmd_line_list. Return a list
  containing the return code and stdout for each one.

  Args:
    cmd_line_list: List of command lines to execute.
    env: Dictionary of any additional environment variable settings.
    cwd: Working directory to set for the processes.
    shell: Should we execute the commands in a subshell or not.
    timeout: Seconds to wait for all processes.  If None, wait forever.

  Returns:
    List of tuples containing returncode, stdout for each process.
  """

  # List of created command_line and Popen objects.
  process_info = {}
  results = []
  for i, cmd_line in enumerate(cmd_line_list):
    proc = SpawnProcess(cmd_line, env, cwd, shell)
    process_info[proc] = (i, cmd_line)
    results.append((0, ''))

  # Wait for all, and process the results.
  # Results are in the same order as the command line list.

  start_time = time.time()
  running_processes = process_info.keys()

  # Run until everything is done, or we timeout.
  while True:
    # Iterate over a copy, so we can remove elements from the list.
    for proc in running_processes[:]:
      if proc.poll() is not None:
        running_processes.remove(proc)
        proc_index, cmd_line = process_info[proc]
        output = '\n'.join(proc.stdout.readlines())
        results[proc_index] = proc.returncode, output

    # Handle timeouts, if set.
    if timeout is not None:
      if time.time() - start_time > timeout:
        # Abort any remaining processes if timeout deadline has been exceeded.
        for proc in running_processes:
          proc_index, cmd_line = process_info[proc]
          proc.kill()
          results[proc_index] = (-1, 'timed out')
        break

    # Continue to wait, or exit the loop.
    if running_processes:
      time.sleep(1)
    else:
      break

  return results


def Decompress(archive_file, destination_dir):
  """Extracts a zip file into a cleaned directory.

  Args:
    archive_file: The zip file to extract.
    destination_dir: The directory to clean and place the extracted files in.
  """
  MakeCleanDirs(destination_dir)
  logging.info('Decompressing %s -> %s', archive_file, destination_dir)
  zip_file = zipfile.ZipFile(archive_file, mode='r')
  zip_file.extractall(destination_dir)


def Compress(job_list):
  """Take a list of (src, dest) pairs and spawn processes to compress them.

  Makes a .zip file named |dest|.zip for each item in job_list containing the
  contents of the directory |src| using shutil.make_archive.

  Args:
    job_list: list of (src, dest) tuples which will be passed on to
        shutil.make_archive, where dest corresponds to |base_name| and src
        corresponds to |root_dir|.
  Returns:
    A list of paths to .zip archives.
  """
  compress_procs = []
  outputs = []
  for source, dest in job_list:
    logging.info('Compressing %s -> %s.zip', source, dest)
    outputs.append('%s.zip' % dest)
    compress_procs.append(multiprocessing.Process(target=shutil.make_archive,
                                                  args=(dest, 'zip', source,
                                                        None)))

  for proc in compress_procs:
    proc.start()

  logging.debug('Waiting for %d compression jobs...', len(compress_procs))
  for proc in compress_procs:
    proc.join()
  return outputs

def SetupDefaultLoggingConfig(logging_lvl=logging.INFO):
  fmt = '[%(filename)s:%(lineno)s:%(levelname)s] %(message)s'
  logging.basicConfig(format=fmt, level=logging_lvl)
