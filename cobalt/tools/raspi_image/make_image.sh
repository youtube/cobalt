#!/bin/bash
#
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script downloads and customizes a Raspbian image for running
# Cobalt. Qemu is used for running on-target customization steps.
# Please see README for full description

# Image versions to use
BASEDIR=raspbian_lite-2020-02-14
BASENAME=2020-02-13-raspbian-buster-lite

# Image and script sources
IMAGE_URL=http://downloads.raspberrypi.org/raspbian_lite/images
SHRINK_SCRIPT="https://raw.githubusercontent.com/aoakley/cotswoldjam/\
master/raspbian-shrink/raspbian-shrink"

# Set up locations for temporary files
STAGING_DIR=/tmp/pi2_image
IMG_FILE=${STAGING_DIR}/${BASENAME}.img
SHRUNK_IMG_FILE=${STAGING_DIR}/${BASENAME}_shrunk_$(date +"%Y%m%d").img
BOOT_MOUNT=${STAGING_DIR}/boot
ROOT_MOUNT=${STAGING_DIR}/rootfs
SSH_KEY_FILE=${STAGING_DIR}/pi_key
QEMU_ADDRESS=127.0.0.1

# Reuse some command args
RUN_ON_PI="ssh -i ${SSH_KEY_FILE} -o StrictHostKeyChecking=no -p 8022 pi@${QEMU_ADDRESS} sudo"
WGET="wget --show-progress -nc"

# Check expected tools are available
function tools_check() {
 if ! command -v qemu-system-arm --version  &> /dev/null ; then
  echo Could not find QEMU, see README for instructions
  return
 fi
 if ! command -v dcfldd --version  &> /dev/null ; then
   echo Could not find dcfldd, see README for instructions
 fi
}

function fetch_image() {
 mkdir -p ${STAGING_DIR}
 ${WGET} ${IMAGE_URL}/${BASEDIR}/${BASENAME}.zip \
  -O ${STAGING_DIR}/${BASENAME}.zip
 if [[ ! -f ${IMG_FILE} ]]; then
  unzip ${STAGING_DIR}/${BASENAME}.zip -d ${STAGING_DIR}
 fi
}

# Mount the image file boot partition
function mount_boot() {
 BOOT_LOCATION=$(fdisk -l ${IMG_FILE}| egrep img1| tr -s ' '| cut -f 2 -d " ")
 MOUNT_OFFSET=$(bc <<< "$BOOT_LOCATION * 512")
 mkdir -p ${BOOT_MOUNT}
 sudo mount -v -o offset=${MOUNT_OFFSET} ${IMG_FILE} ${BOOT_MOUNT}
}

# Mount the image file root partition
function mount_root() {
 BOOT_LOCATION=$(fdisk -l ${IMG_FILE}| egrep img2| tr -s ' '| cut -f 2 -d " ")
 MOUNT_OFFSET=$(bc <<< "$BOOT_LOCATION * 512")
 mkdir -p ${ROOT_MOUNT}
 sudo mount -v -o offset=${MOUNT_OFFSET} -t ext4 ${IMG_FILE} ${ROOT_MOUNT}
}

function unmount_root() {
 sudo umount ${ROOT_MOUNT}
}
function unmount_boot() {
 sudo umount ${BOOT_MOUNT}
}

function modify_boot() {
 # Set GPU/main memory split
 sudo sh -c "echo gpu_mem=512 >> ${BOOT_MOUNT}/config.txt"
 # Ensure 1st boot image resize runs
 sudo touch ${BOOT_MOUNT}/1stboot.txt
 # Drop auto-resize script in place, crontab entry will be added later
 sudo cp -v auto-resize-partition.sh ${BOOT_MOUNT}/
 sudo cp -v crontab.txt ${BOOT_MOUNT}/
 sudo chmod +x ${BOOT_MOUNT}/auto-resize-partition.sh
}

function modify_root() {
 # Enable SSH by default
 sudo rm ${ROOT_MOUNT}/etc/{rc2.d,rc3.d,rc4.d,rc5.d}/K01ssh
 for i in 2 3 4 5 ; do
  sudo ln -s ../init.d/ssh ${ROOT_MOUNT}/etc/rc${i}.d/S02ssh
 done
 sudo ln -s /lib/systemd/system/ssh.service \
  ${ROOT_MOUNT}/etc/systemd/system/multi-user.target.wants/ssh.service
 sudo ln -s /lib/systemd/system/ssh.service \
  ${ROOT_MOUNT}/etc/systemd/system/sshd.service
}

function get_kernel() {
 # Get matching kernel and DTB files from boot partition
 cp ${BOOT_MOUNT}/kernel7.img ${STAGING_DIR}
 cp ${BOOT_MOUNT}/bcm2709-rpi-2-b.dtb ${STAGING_DIR}
}

function run_qemu() {
 tools_check
 # Ensure image is in usable size for Qemu
 qemu-img resize ${IMG_FILE} 4G
 # Boot qemu in background
 qemu-system-arm $@ \
    -machine raspi2 \
    -drive file=${IMG_FILE},format=raw,if=sd \
    -dtb ${STAGING_DIR}/bcm2709-rpi-2-b.dtb \
    -kernel ${STAGING_DIR}/kernel7.img \
    -append 'rw earlycon=pl011,0x3f201000 console=ttyAMA0 \
        loglevel=8 root=/dev/mmcblk0p2 fsck.repair=yes \
        net.ifnames=0 rootwait memtest=1 \
        dwc_otg.fiq_fsm_enable=0' \
    -m 1G -smp 4 \
    -no-reboot \
    -netdev user,id=net0,hostfwd=tcp::8022-:22 \
    -usb -device usb-kbd -device usb-tablet \
    -device usb-net,netdev=net0
}

function run_qemu_backgrounded() {
  run_qemu -daemonize
}

# Useful for local image testing
function run_qemu_console() {
 run_qemu -serial stdio
}

function kill_qemu() {
 pkill -9 qemu-system-arm
}

function make_ssh_key() {
  if [[ ! -f ${SSH_KEY_FILE} ]]; then
    ssh-keygen -q -N ""  -b 1024 -f ${SSH_KEY_FILE} \
      -C "cobalt-dev@googlegroups.com"
  fi
}

function deploy_key() {
 echo Copying SSH key to target, please use password \'raspberry\' at \
    the login prompt.
 # Remove previous host key
 ssh-keygen -R ${QEMU_ADDRESS}
 # Accept a new one
 ssh-keyscan -p 8022 ${QEMU_ADDRESS} >> ~/.ssh/known_hosts
 ssh-copy-id -i ${SSH_KEY_FILE} -p 8022 -o StrictHostKeyChecking=no pi@${QEMU_ADDRESS}
 ${RUN_ON_PI} echo ssh connection works
}

function wait_for_qemu() {
 # Loop until SSH connection attempt responds with permission denied
 while :; do
  echo "waiting for SSH to be up"
  cmd=$(ssh -o StrictHostKeyChecking=no -o ConnectTimeout=3 -o BatchMode=yes \
            localhost -p 8022 echo up 2>&1)
  OUTPUT=$?
  if [ $OUTPUT -eq 255 ]; then
   if [[ $cmd == *"Permission"* ]] ; then
    echo "SSH seems up"
    break
   fi
  fi
  echo $cmd_out
  sleep 2
 done
}

function get_shrink_script() {
 wget -nc ${SHRINK_SCRIPT} -O ${STAGING_DIR}/raspbian-shrink
 chmod +x ${STAGING_DIR}/raspbian-shrink
}

# Resize root partition to fill image size
function first_run_expand() {
 ${RUN_ON_PI} raspi-config --expand-rootfs
 ${RUN_ON_PI} reboot; sleep 1
 kill_qemu
}

# Turn off swapfile
function disable_swap() {
 ${RUN_ON_PI} dphys-swapfile swapoff
 ${RUN_ON_PI} dphys-swapfile uninstall
 ${RUN_ON_PI} systemctl disable dphys-swapfile.service
}

# Clean up large footprint packages
function clean_unnecessary_packages() {
 ${RUN_ON_PI} apt-get purge -qy \
  libraspberrypi-doc \
  avahi-daemon
}

# Update APT sources and install required libraries
function run_apt_updates() {
 ${RUN_ON_PI} apt-get -qy update
 ${RUN_ON_PI} apt-get install -y --auto-remove libpulse-dev libasound2-dev \
  libavformat-dev libavresample-dev
 # We are pulling in new deps from above, so lets fully upgrade.
 ${RUN_ON_PI} apt-get -qy upgrade
}

function install_cron_entry() {
  # Fully replaces crontab, which has no other entries anyway
  ${RUN_ON_PI} crontab /boot/crontab.txt
}

function pre_shrink_cleanup() {
 ${RUN_ON_PI} rm -rf /var/cache
 ${RUN_ON_PI} sync
}

# Run the shrink script
function shrink_image() {
 tools_check
 sudo ${STAGING_DIR}/raspbian-shrink ${IMG_FILE} \
  ${SHRUNK_IMG_FILE}
 echo =====
 echo Final shrunk image available at: ${SHRUNK_IMG_FILE}
}

function customize_on_qemu() {
 run_qemu_backgrounded
 wait_for_qemu
 make_ssh_key
 deploy_key
 first_run_expand
 run_qemu_backgrounded
 wait_for_qemu
 disable_swap
 clean_unnecessary_packages
 run_apt_updates
 install_cron_entry
 pre_shrink_cleanup
 kill_qemu
}

function prepare_raspi_image() {
 tools_check
 fetch_image
 mount_root
 modify_root
 unmount_root
 mount_boot
 modify_boot
 get_kernel
 unmount_boot
 customize_on_qemu
 get_shrink_script
 shrink_image
}

# If script is executed in the subshell, launch the full
# customization script.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]] ; then
  echo "Running full image preparation script."
  prepare_raspi_image
else
  echo "Script is sourced in current shell, ready to execute individual steps."
fi
