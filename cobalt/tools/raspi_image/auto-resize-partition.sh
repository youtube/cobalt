#!/bin/sh
if [ -e /boot/1stboot.txt ]; then
  export PATH=$PATH:/usr/sbin:/sbin
  raspi-config --expand-rootfs
  rm /boot/1stboot.txt
  reboot
fi
