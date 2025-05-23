#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Download the 4 files below from the ICU trunk and put them in
# source/data/misc to update the IANA timezone database.
#
#   metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
#
# For IANA Time zone database, see https://www.iana.org/time-zones

# See
# https://stackoverflow.com/questions/160608/do-a-git-export-like-svn-export/19689284#19689284
# about 'svn export' and github.

branch="trunk"

# ICU tz file is sometimes updated in the maintenance branch long before
# being updated in trunk.
if [ $# -ge 1 ];
then
  branch="branches/maint/maint-$1"
  echo "Downloading tz files from ${branch}"
fi

datapath="source/data/misc"
sourcedirurl="https://github.com/unicode-org/icu/${branch}/icu4c/${datapath}"
cd "$(dirname "$0")/../${datapath}"

for f in metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
do
  echo "${sourcedirurl}/${f}"
  svn --force export "${sourcedirurl}/${f}"
done
