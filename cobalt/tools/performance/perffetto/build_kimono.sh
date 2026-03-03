#!/bin/bash

(cd ${HOME}/cobalt/src && cobalt/build/android/package.py \
    --platforms android-arm \
    --branch main)

cd $(p4 g4d "kimono-symbol")

rm -rf ${HOME}/google3_snapshot_to_copy/

blaze run video/youtube/tv/cobalt/piper_scripts:android_release_to_piper \
     -- \
     --logtostderr \
     --branch=main \
     --snapshot_from_local=${HOME}/cobalt/src/out/packages \
     --platforms=arm \
     --snapshot_to_local=${HOME}/google3_snapshot_to_copy/branch_main \
     --nosubmit

cp -r ${HOME}/google3_snapshot_to_copy/* ./third_party/cobalt/app/android/coat/
blaze build --config=FishfoodTrunk java/com/google/android/apps/youtube/tv:YouTubeCobaltTvFishfoodTrunk

rm -rf ${HOME}/cobalt/src/out/YouTubeCobaltTvFishfoodTrunk.apk
cp blaze-bin/java/com/google/android/apps/youtube/tv/YouTubeCobaltTvFishfoodTrunk.apk ${HOME}/cobalt/src/out/YouTubeCobaltTvFishfoodTrunk.apk
md5sum ${HOME}/cobalt/src/out/YouTubeCobaltTvFishfoodTrunk.apk
cd ${HOME}/cobalt/src
