#!/usr/bin/env python3
import sys
import subprocess
import os
import re
import json
import tempfile
import shutil
import hashlib
import time
import fcntl

def log(msg):
    timestamp = time.strftime("%H:%M:%S")
    print(f"[{timestamp}] {msg}", file=sys.stderr)
    sys.stderr.flush()

# This script acts as a host-side launcher for RDK tests.
# It handles syncing files, running via SSH, and downloading XML results.

TARGET_HOSTS = ["100.107.44.149", "100.107.44.165"]
TARGET_USER = "root"
BASE_TARGET_DIR = "/data/test/cobalt"

def get_file_hash(file_path):
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def acquire_host():
    """Acquires a lock on one of the available hosts."""
    while True:
        for host in TARGET_HOSTS:
            lock_file_path = os.path.join(tempfile.gettempdir(), f"rdk_host_{host}.lock")
            lock_file = open(lock_file_path, "w")
            try:
                # Non-blocking lock
                fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return host, lock_file
            except IOError:
                lock_file.close()
                continue
        # Wait a bit before retrying all hosts
        time.sleep(1)

def main():
    # parallel_test_runner.py calls this with various orders of arguments.
    # We look for the one that ends with '_loader.py'.

    gtest_args = []
    local_loader = None
    
    for arg in sys.argv[1:]:
        if arg.endswith("_loader.py"):
            local_loader = arg
        else:
            gtest_args.append(arg)

    if not local_loader:
        print("Error: No loader script specified.")
        sys.exit(1)

    # Extract test name from loader name (e.g. nplb_loader.py -> nplb)
    loader_basename = os.path.basename(local_loader)
    test_name = loader_basename.replace("_loader.py", "")
    lib_name = f"lib{test_name}.so"

    # Acquire an available host
    target_host, host_lock = acquire_host()
    log(f"[{test_name}] Acquired host {target_host}")
    
    # Use test-specific directory on device to avoid interference
    target_dir = f"{BASE_TARGET_DIR}/{test_name}"

    try:
        # 0. Ensure target dirs exist
        subprocess.run(["sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}", f"mkdir -p {target_dir} {target_dir}/home {target_dir}/cache"], capture_output=True)

        # 1. Determine GN target and archive runtime dependencies
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, "../../.."))
        targets_json_path = os.path.join(script_dir, "targets/evergreen-arm-hardfp-rdk/test_targets.json")
        
        with open(targets_json_path, "r") as f:
            targets_info = json.load(f)
        
        gn_target = None
        target_loader_name = f"{test_name}_loader"
        for target in targets_info["test_targets"]:
            if target.split(":")[-1] == target_loader_name:
                gn_target = target
                break
                
        if not gn_target:
            print(f"Error: Could not find GN target for {test_name} in {targets_json_path}")
            sys.exit(1)

        log(f"[{test_name}] Archiving runtime dependencies for {gn_target}...")
        
        local_dir = os.path.dirname(local_loader)
        archive_script = os.path.join(project_root, "cobalt/build/archive_test_artifacts.py")
        
        # Use a predictable temp dir based on test name to cache the archive locally
        cache_dir = os.path.join(tempfile.gettempdir(), f"rdk_test_cache_{test_name}")
        os.makedirs(cache_dir, exist_ok=True)
        
        archive_name = f"{target_loader_name}_deps.tar.gz"
        local_archive = os.path.join(cache_dir, archive_name)
        
        archive_cmd = [
            sys.executable, archive_script,
            "--source-dir", project_root,
            "--out-dir", local_dir,
            "--destination-dir", cache_dir,
            "--targets", gn_target,
            "--compression", "gz", "--archive-per-target", "--flatten-deps"
        ]
        subprocess.run(archive_cmd, check=True, capture_output=True)
        
        local_hash = get_file_hash(local_archive)

        # 2. Sync and extract archive on target
        # Check if we need to transfer
        hash_file = f"{target_dir}/{archive_name}.sha256"
        check_hash_cmd = ["sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}", f"cat {hash_file}"]
        result = subprocess.run(check_hash_cmd, capture_output=True, text=True)
        remote_hash = result.stdout.strip()
        
        if remote_hash != local_hash:
            log(f"[{test_name}] Syncing archive ({os.path.getsize(local_archive)//1024//1024} MB) to {target_host}...")
            # Use rsync for speed and robustness
            sync_cmd = [
                "sshpass", "-p", "", "rsync", "-az", "-e", "ssh -o StrictHostKeyChecking=no",
                local_archive,
                f"{TARGET_USER}@{target_host}:{target_dir}/"
            ]
            subprocess.run(sync_cmd, check=True, capture_output=True)
            
            # Update remote hash
            update_hash_cmd = ["sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}", f"echo {local_hash} > {hash_file}"]
            subprocess.run(update_hash_cmd, check=True, capture_output=True)
            
            log(f"[{test_name}] Extracting archive on device {target_host}...")
            extract_cmd = [
                "sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no",
                f"{TARGET_USER}@{target_host}",
                f"cd {target_dir} && tar -xzf {archive_name}"
            ]
            subprocess.run(extract_cmd, check=True, capture_output=True)
        else:
            log(f"[{test_name}] Archive already on device {target_host} and up to date.")

        # 3. Handle flagfile if present
        remote_gtest_args = []
        local_flagfile = None
        remote_flagfile = None
        
        for arg in gtest_args:
            if arg.startswith("--gtest_flagfile="):
                local_flagfile = arg.split("=", 1)[1]
                flagfile_name = os.path.basename(local_flagfile)
                remote_flagfile = f"{target_dir}/{flagfile_name}"
                # Upload flagfile
                scp_cmd = ["sshpass", "-p", "", "scp", "-q", "-o", "StrictHostKeyChecking=no", local_flagfile, f"{TARGET_USER}@{target_host}:{remote_flagfile}"]
                subprocess.run(scp_cmd, check=True, capture_output=True)
                remote_gtest_args.append(f"--gtest_flagfile={remote_flagfile}")
            elif arg.startswith("--gtest_output=xml:"):
                local_xml = arg.split(":", 2)[1]
                xml_name = os.path.basename(local_xml)
                remote_xml = f"{target_dir}/{xml_name}"
                remote_gtest_args.append(f"--gtest_output=xml:{remote_xml}")
            else:
                remote_gtest_args.append(arg)

        # 4. Construct remote command
        # Use the escape trick to bypass the hardcoded prefix /usr/share/content/data/
        remote_content = f"../../../../{target_dir.lstrip('/')}"
        remote_cmd = (
            f"cd {target_dir} && HOME={target_dir}/home ./elf_loader_sandbox "
            f"--evergreen_content={remote_content} "
            f"--evergreen_library={lib_name} "
            f"{' '.join(remote_gtest_args)}"
        )

        # 5. Run via SSH
        log(f"[{test_name}] Running on device {target_host}...")
        ssh_cmd = ["sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}", remote_cmd]
        result = subprocess.run(ssh_cmd, capture_output=True, text=True)
        
        # Forward stdout/stderr
        # Filter stdout if it's a test listing to avoid polluting with SSH noise
        if "--gtest_list_tests" in gtest_args:
            filtered_stdout = []
            for line in result.stdout.splitlines():
                # gtest_list_tests lines start with suite name (no space) or test name (2 spaces)
                # Suite names end with a dot. Test names start with 2 spaces.
                if re.match(r'^[A-Za-z0-9_/]+\.$|^  [A-Za-z0-9_/]+', line):
                    filtered_stdout.append(line)
            sys.stdout.write("\n".join(filtered_stdout) + "\n")
        else:
            sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        sys.stdout.flush()
        sys.stderr.flush()

        # 6. Download XML if requested
        for arg in gtest_args:
            if arg.startswith("--gtest_output=xml:"):
                local_xml = arg.split(":", 2)[1]
                xml_name = os.path.basename(local_xml)
                remote_xml = f"{target_dir}/{xml_name}"
                # Download
                scp_cmd = ["sshpass", "-p", "", "scp", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}:{remote_xml}", local_xml]
                subprocess.run(scp_cmd, capture_output=True)
                # Cleanup remote xml
                cleanup_cmd = ["sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}", f"rm {remote_xml}"]
                subprocess.run(cleanup_cmd, capture_output=True)

        # 7. Cleanup remote files (optional, but keep for now to avoid clutter)
        # Actually, don't cleanup lib/sandbox to keep it fast.
        if remote_flagfile:
            cleanup_cmd = ["sshpass", "-p", "", "ssh", "-q", "-o", "StrictHostKeyChecking=no", f"{TARGET_USER}@{target_host}", f"rm {remote_flagfile}"]
            subprocess.run(cleanup_cmd, capture_output=True)

        sys.exit(result.returncode)

    finally:
        # Release the lock
        fcntl.flock(host_lock, fcntl.LOCK_UN)
        host_lock.close()

if __name__ == "__main__":
    main()
