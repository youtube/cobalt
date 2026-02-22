
import unittest
import sys
import os
import subprocess
from unittest.mock import MagicMock, patch, call

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from docker_worker_pool import DockerWorkerPool

class TestDockerWorkerPool(unittest.TestCase):
    def setUp(self):
        self.cwd = "/path/to/cwd"
        self.image = "docker/image:tag"

    @patch("subprocess.run")
    def test_run(self, mock_run):
        pool = DockerWorkerPool(1, self.cwd, self.image)
        # Mock queue to have a worker
        worker_id = "worker_1"
        pool._queue.put(worker_id)

        args = ["/bin/app", "arg1"]

        # Test basic run (no timeout)
        mock_process = MagicMock()
        mock_process.returncode = 0
        mock_process.communicate.return_value = ("stdout", "stderr")
        mock_process.poll.return_value = 0
        # Selectors need distinct file descriptors
        mock_process.stdout.fileno.return_value = 1
        mock_process.stderr.fileno.return_value = 2

        with patch("subprocess.Popen", return_value=mock_process) as mock_popen:
            success, out, err, code = pool.run(args)

            self.assertTrue(success)
            self.assertEqual(out, "stdout")
            mock_popen.assert_called_once()
            cmd_arg = mock_popen.call_args[0][0]
            self.assertIn("docker", cmd_arg)
            self.assertIn("exec", cmd_arg)
            self.assertIn("/bin/app arg1", cmd_arg[cmd_arg.index("-c") + 1])

    @patch("subprocess.Popen")
    @patch("selectors.DefaultSelector")
    def test_run_with_timeout(self, mock_selector, mock_popen):
        pool = DockerWorkerPool(1, self.cwd, self.image)
        pool._queue.put("worker_1")

        args = ["/bin/app", "arg1"]

        # Mock process
        process_mock = MagicMock()
        process_mock.poll.side_effect = [None, 0, 0, 0] # Run once then exit
        process_mock.stdout.readline.return_value = "output"
        process_mock.stderr.readline.return_value = ""
        process_mock.returncode = 0
        process_mock.communicate.return_value = ("", "")
        mock_popen.return_value = process_mock

        # Mock selector
        selector_instance = mock_selector.return_value
        selector_instance.select.return_value = []

        success, stdout, stderr, code = pool.run(args, timeout=10)

        self.assertTrue(success)
        self.assertEqual(code, 0)
        mock_popen.assert_called_once()

    @patch("subprocess.run")
    def test_startup_shutdown(self, mock_run):
         # Test context manager
         with DockerWorkerPool(1, self.cwd, self.image) as pool:
             # Should submit startup task
             # We can't easily verify it ran without internal mocking or race condition checks
             pass

         # Shutdown should have been called
         # Cannot easily verify thread executor shutdown unless we mock it.
         pass

if __name__ == "__main__":
    unittest.main()
