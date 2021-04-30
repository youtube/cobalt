# GYP to GN Migration Changes
This file tracks changes to configuration meta build configuration variables in
the GYP to GN migration.

## Removed:
sb_disable_cpp14_audit
sb_disable_microphone_idl
starboard_path
tizen_os

## Renamed:
sb_deploy_output_dir -> sb_install_output_dir
OS == starboard -> is_starboard

## Added:
