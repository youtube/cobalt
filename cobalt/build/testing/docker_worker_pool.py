
import os
import shlex
import subprocess
import sys
import time
import uuid
import queue
import selectors
import threading
from concurrent.futures import ThreadPoolExecutor
from typing import List, Tuple, Optional

class DockerWorkerPool:
    def __init__(self, num_workers: int, cwd: str, docker_image: str):
        self.num_workers = num_workers
        self.cwd = cwd
        self.docker_image = docker_image
        self.workers = []
        self._lock = threading.Lock()
        self._queue = queue.Queue()
        self._startup_executor = ThreadPoolExecutor(max_workers=num_workers)
        self._executor = ThreadPoolExecutor(max_workers=num_workers)

    def __enter__(self):
        print(f"Starting {self.num_workers} Docker containers...")
        # Start workers asynchronously
        for _ in range(self.num_workers):
            self._startup_executor.submit(self._start_worker)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.shutdown()

    def _start_worker(self):
        worker_id = f"cobalt_run_{uuid.uuid4().hex[:8]}"

        cmd = [
            "docker", "run", "-d", "--rm",
            "--name", worker_id,
            "-v", f"{self.cwd}:{self.cwd}",
            "-w", self.cwd,
            "--entrypoint", "tail", # Keep alive
            self.docker_image,
            "-f", "/dev/null"
        ]

        try:
            subprocess.run(cmd, check=True, capture_output=True)
            print(f"Starting worker: {worker_id}")
            with self._lock:
                self.workers.append(worker_id)
            self._queue.put(worker_id)
        except subprocess.CalledProcessError as e:
            print(f"Failed to start worker {worker_id}: {e}")
            pass

    def shutdown(self):
        # TODO: Replace with docker kill $(docker ps <matching_prefix>)

        print("Shutting down workers...")
        # Ensure all startup tasks finish to avoid leaking docker images.
        self._startup_executor.shutdown(wait=True)
        # Kill any running tasks on shutdown.
        self._executor.shutdown(wait=False)

        with self._lock:
            workers_to_kill = list(self.workers)
            self.workers = []

        if not workers_to_kill:
            return

        def kill_worker(name):
            subprocess.run(["docker", "rm", "-f", name],
                         capture_output=True, check=False, timeout=10)

        with ThreadPoolExecutor(max_workers=len(workers_to_kill)) as killer:
            killer.map(kill_worker, workers_to_kill)

    def _get_exec_command(self, worker_id: str, cmd: List[str]) -> List[str]:
        # Handle mounts for the binary if needed?
        # We can't mount AFTER run.
        # So we assume the worker was started with sufficient mounts (CWD).

        # Construct command
        # cmd is [binary, arg1, arg2...]
        flat_cmd = " ".join([shlex.quote(c) for c in cmd])

        sh_cmd = f"export DISPLAY=:99 && {flat_cmd}"

        return [
            "docker", "exec",
            "-w", self.cwd,
            worker_id,
            "sh", "-c", sh_cmd
        ]

    def _execute_cmd_internal(self, cmd: List[str], result_parser, timeout: Optional[int]):
        worker_id = self._queue.get()
        try:
            full_cmd = self._get_exec_command(worker_id, cmd)

            try:
                process = subprocess.Popen(
                    full_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    cwd=self.cwd,
                    bufsize=1 # Line buffered
                )
                os.set_blocking(process.stdout.fileno(), False)
                os.set_blocking(process.stderr.fileno(), False)
            except Exception as e:
                # If execution fails, return generic error result
                return result_parser(-1, "", str(e)) if result_parser else (False, "", str(e), -1)

            selector = selectors.DefaultSelector()
            selector.register(process.stdout, selectors.EVENT_READ)
            selector.register(process.stderr, selectors.EVENT_READ)

            stdout_acc = []
            stderr_acc = []
            last_output_time = time.time()
            timed_out = False

            try:
                while True:
                    if process.poll() is not None:
                        # Ensure we read remaining output
                        break

                    elapsed = time.time() - last_output_time
                    if timeout and elapsed > timeout:
                        timed_out = True
                        process.kill()
                        break

                    # Select with short timeout to check poll()
                    events = selector.select(timeout=1.0)

                    if events:
                        last_output_time = time.time()
                        for key, mask in events:
                            if key.fileobj == process.stdout:
                                chunk = process.stdout.read(8192)
                                if chunk:
                                    stdout_acc.append(chunk)
                            elif key.fileobj == process.stderr:
                                chunk = process.stderr.read(8192)
                                if chunk:
                                    stderr_acc.append(chunk)
            except Exception as e:
                process.kill()
                stderr_acc.append(f"\nRunner Exception: {e}")
                return result_parser(-1, "".join(stdout_acc), "".join(stderr_acc)) if result_parser else (False, "".join(stdout_acc), "".join(stderr_acc), -1)
            finally:
                selector.close()
                if process.poll() is None:
                    process.kill()
                    process.wait()

            stdout_str = "".join(stdout_acc)
            stderr_str = "".join(stderr_acc)

            rest_out, rest_err = process.communicate()
            stdout_str += rest_out
            stderr_str += rest_err

            if timed_out:
                return result_parser(-2, stdout_str, stderr_str + "\n(IDLE TIMEOUT)") if result_parser else (False, stdout_str, stderr_str + "\n(IDLE TIMEOUT)", -2)

            if result_parser:
                return result_parser(process.returncode, stdout_str, stderr_str)
            else:
                return process.returncode == 0, stdout_str, stderr_str, process.returncode

        finally:
            self._queue.put(worker_id)

    def run_cmd_async(self, cmd: List[str], result_parser=None, timeout: Optional[int] = None):
        """
        Submits a command to be run asynchronously on a worker.
        Returns a Future.
        """
        return self._executor.submit(self._execute_cmd_internal, cmd, result_parser, timeout)

    def run(self, cmd: List[str], timeout: Optional[int] = None) -> Tuple[bool, str, str, int]:
        """
        Runs a command on an available worker (Blocking).
        """
        return self.run_cmd_async(cmd, result_parser=None, timeout=timeout).result()

