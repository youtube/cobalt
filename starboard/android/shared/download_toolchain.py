import os
import subprocess

from pathlib import Path

current_dir = Path(__file__).resolve().parents[0]

subprocess.check_call(
    ['bash', os.path.join(current_dir, 'download_sdk.sh')],
    env={'ANDROID_HOME': os.path.join(current_dir, 'android_toolchain')})
