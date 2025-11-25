import subprocess
import sys


def run_command(cmd, check=True):
  """
  Executes a command, streams its output, and exits if it fails.
  """
  print(f"--- Running: {' '.join(cmd)} ---")
  try:
    # We stream the output instead of capturing it to provide real-time
    # feedback in a CI environment.
    process = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    for line in iter(process.stdout.readline, ''):
      sys.stdout.write(line)

    process.wait()
    if check and process.returncode != 0:
      print(
          f"\n--- Command failed with exit code {process.returncode} ---",
          file=sys.stderr)
      sys.exit(1)
  except FileNotFoundError as e:
    print(f"Error: Command not found - {e.filename}", file=sys.stderr)
    sys.exit(1)
  except Exception as e:
    print(f"An unexpected error occurred: {e}", file=sys.stderr)
    sys.exit(1)

