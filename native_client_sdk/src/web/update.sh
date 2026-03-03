#!/bin/sh
for file in index.html manifest.html; do
  gcloud storage cp --predefined-acl=public-read $file gs://nativeclient-mirror/nacl/nacl_sdk
done
