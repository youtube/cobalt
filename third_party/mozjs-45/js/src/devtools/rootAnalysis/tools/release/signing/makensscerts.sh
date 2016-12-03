#!/bin/sh
# Creates a test NSS key store in .nss with a test signing key
# (for mar signing)
rm -rf .nss
mkdir .nss
certutil -d .nss -N
certutil -d .nss -S -s "CN=test" -n test -x -t ",," -g 2048
