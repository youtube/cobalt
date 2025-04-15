import os
import sys

REPOSITORY_ROOT = os.path.abspath(os.path.abspath(__file__) + "/../../../")
print(REPOSITORY_ROOT)

sys.path.append(os.path.join(REPOSITORY_ROOT, 'build', 'android'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'devil'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'telemetry'))
#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Checks if an Android application is running, stops it if it is,
then launches it with arguments, waits for a specified duration,
and finally stops the application.

Uses devil library, adapted for structure potentially found in Chromium M114.
Uses generic Exception handling due to missing devil.core on target environment.
"""

import argparse
import logging
import sys
import time  # Keep time import for sleep
# Removed subprocess and re imports

# Assuming devil is accessible via PYTHONPATH or vpython's magic
try:
  from devil.android import device_utils
  # Corrected import for M114 structure based on find result:
  from devil.android.sdk.intent import Intent as DevilIntent
  from devil.android.sdk import adb_wrapper
  # NOTE: devil.core import is removed as requested.
  # from devil.core import exceptions as devil_exceptions

except ImportError as e:
  logging.error("Devil libraries not found or import error: %s", e)
  logging.error(
      "Ensure this script is run using vpython from within your checkout,")
  logging.error("or that the correct devil path is in your PYTHONPATH.")
  sys.exit(1)
except Exception as e:
  logging.error("An unexpected error occurred during initial imports: %s", e)
  sys.exit(1)

# --- Configuration ---
DEFAULT_URL = "about:blank"
DEFAULT_FLAGS_STRING_FORMAT = '--remote-allow-origins=*,--url="{url}"'
EXTRA_ARGS_KEY = "commandLineArgs"


# === Function to Stop Application ===
def stop_application(device, package_name):
  """Stops the specified application package."""
  logging.info("Attempting to stop application: %s", package_name)
  try:
    device.ForceStop(package_name)
    logging.info("ForceStop command sent successfully for package %s.",
                 package_name)
    return True
  # Generic catch for errors
  except Exception as e:
    logging.exception("An unexpected error occurred during ForceStop: %s", e)
  return False


# === Function to Start Application ===
def start_app_with_args(device, package_name, url, flags_format):
  """
    Starts the specified activity with command line args.
    Args are same as before. Returns True/False.
    """
  command_line_flags = flags_format.format(url=url)
  logging.info("Constructed launch flags string: %s", command_line_flags)

  try:
    intent_extras = {EXTRA_ARGS_KEY: command_line_flags}
    logging.debug("Intent extras to be passed: %s", intent_extras)
    logging.info("Attempting to start package '%s' with action MAIN",
                 package_name)

    app_intent = DevilIntent(
        action="android.intent.action.MAIN",
        component="dev.cobalt.coat/dev.cobalt.app.MainActivity",
        extras=intent_extras)

    device.StartActivity(app_intent, blocking=True)
    logging.info("StartActivity command sent successfully for package %s.",
                 package_name)
    return True

  except AttributeError as e:
    logging.error("AttributeError during intent creation/modification: %s", e)
  # Generic catch for errors
  except Exception as e:
    logging.exception("An unexpected error occurred during StartActivity: %s",
                      e)

  return False


# === Main Orchestration ===
def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument(
      '-d',
      '--device',
      dest='device_serial',
      help='Target device serial number. Uses first healthy device if omitted.')
  # Launch Args
  parser.add_argument(
      '-p',
      '--package',
      required=True,
      dest='package_name',
      help='The application package name to launch and stop.')
  parser.add_argument(
      '--url',
      default=DEFAULT_URL,
      help='URL to pass via command line arg (default: %(default)s)')
  parser.add_argument(
      '--flags-format',
      default=DEFAULT_FLAGS_STRING_FORMAT,
      dest='flags_format',
      help='Format string for command line args, use "{url}" as placeholder '
      '(default: %(default)s)')
  # Run Duration Arg (Replaces Monitor Args)
  parser.add_argument(
      '-r',
      '--run-duration',
      type=int,
      required=True,
      dest='run_duration',
      help='REQUIRED: Duration in seconds to let the application run before stopping.'
  )
  # General Args
  parser.add_argument(
      '-v',
      '--verbose',
      action='count',
      default=0,
      help='Increase logging verbosity (e.g., -v -> INFO, -vv -> DEBUG).')

  args = parser.parse_args()

  if args.run_duration <= 0:
    parser.error("--run-duration must be a positive integer.")

  # Setup Logging
  log_level = logging.WARNING
  if args.verbose == 1:
    log_level = logging.INFO
  elif args.verbose >= 2:
    log_level = logging.DEBUG
  logging.basicConfig(
      level=log_level,
      format='%(asctime)s %(levelname)s: %(message)s',
      datefmt='%H:%M:%S',
      force=True)

  target_device = None
  exit_code = 1  # Default to error

  try:
    # 1. Get Device
    if args.device_serial:
      logging.info("Attempting to use specified device: %s", args.device_serial)
      target_device = device_utils.DeviceUtils(args.device_serial)
      target_device.WaitUntilReady(timeout=10)
      logging.info("Using specified device: %s", target_device.serial)
    else:
      logging.info("Searching for healthy devices...")
      healthy_devices = device_utils.DeviceUtils.HealthyDevices()
      if not healthy_devices:
        logging.error("No healthy Android devices found.")
        return 1
      target_device = healthy_devices[0]
      logging.info("Found healthy device: %s (Using the first one)",
                   target_device.serial)

    # === Check if App is Running using pidof and Stop if Necessary ===
    logging.info("--- Checking initial application state (using pidof) ---")
    try:
      pid_output_list = target_device.RunShellCommand(
          ['pidof', args.package_name], check_return=False, timeout=5)
      pids = "".join(pid_output_list).strip()
      logging.debug("pidof output: '%s'", pids)
      if pids:
        logging.warning(
            "Application '%s' appears to be running (PID(s): %s). Stopping it before launch.",
            args.package_name, pids)
        stopped_ok = stop_application(target_device, args.package_name)
        if stopped_ok:
          logging.info("Pre-stop successful. Pausing before relaunch...")
          time.sleep(3)
        else:
          logging.warning(
              "Pre-stop command failed. Attempting launch anyway...")
      else:
        logging.info(
            "Application '%s' does not appear to be running (pidof returned empty).",
            args.package_name)
    except Exception as check_err:
      logging.warning(
          "Could not determine if app was running using pidof: %s. Proceeding with launch attempt.",
          check_err)
    # ====================================================================

    # 2. Launch App
    logging.info("=== Phase 1: Launching Application ===")
    launched_ok = start_app_with_args(target_device, args.package_name,
                                      args.url, args.flags_format)
    if not launched_ok:
      logging.error("Exiting due to launch failure.")
      stop_application(target_device, args.package_name)  # Attempt cleanup
      return 3

    # 3. Wait for Specified Duration with Progress Logging
    logging.info("=== Phase 2: Running for Specified Duration (%d seconds) ===",
                 args.run_duration)
    total_duration = args.run_duration
    # --- Configuration for progress logging ---
    log_interval = 10  # How often to print progress (in seconds)
    # Make interval slightly smaller than total if total is small, but no less than 1s
    if total_duration < log_interval and total_duration > 0:
      log_interval = max(1, total_duration // 2)
    elif total_duration == 0:
      log_interval = 1  # Avoid division by zero if duration is 0 (already checked by argparse though)
    # --- End Configuration ---

    elapsed_time = 0
    while elapsed_time < total_duration:
      # Determine how long to sleep in this iteration (up to log_interval)
      remaining_time = total_duration - elapsed_time
      sleep_time = min(log_interval, remaining_time)

      # Prevent sleeping for 0 or negative time if calculation is off
      if sleep_time <= 0:
        break

      time.sleep(sleep_time)
      elapsed_time += sleep_time

      # Log progress
      logging.info("   ...running... %d / %d seconds elapsed.",
                   int(elapsed_time), total_duration)

    logging.info("Run duration complete.")
    # --- The script continues to Phase 3: Stopping Application ---

    # 4. Stop App
    logging.info("=== Phase 3: Stopping Application ===")
    stopped_ok = stop_application(target_device, args.package_name)

    # Determine final exit code
    if stopped_ok:
      logging.info(
          "=== Overall Success: App launched, ran for duration, app stopped. ==="
      )
      exit_code = 0  # Success
    else:
      logging.error(
          "=== Finished with Error: App launched and ran, but app stop failed. ==="
      )
      exit_code = 4  # Stop fail

  # Generic catch for setup errors
  except Exception as e:
    logging.exception("An unexpected error occurred during setup: %s", e)
    # Attempt to stop the app if setup failed after launch attempt started implicitly
    if target_device and args.package_name:
      logging.warning("Attempting to stop application after setup error.")
      stop_application(target_device, args.package_name)
    exit_code = 1
  finally:
    logging.info("Script finished with exit code %d.", exit_code)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
