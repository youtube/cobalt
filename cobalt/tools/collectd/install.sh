#!/bin/bash
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

if [[ ${EUID} -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
fi

function print_msg() {
    echo  '============= ' ${@} ' ============='
}

WEBROOT=/var/www/html
CGP_ROOT=${WEBROOT}/CGP

COLLECTD_CONF_DIR=/etc/collectd/collectd.conf.d
RRD_ENABLED=1
if [[ -d "/etc/collectd/google.conf.d" ]]; then
    COLLECTD_CONF_DIR=/etc/collectd/google.conf.d
    RRD_ENABLED=0
fi


print_msg "Installing apt deps"
PKGS="collectd-core lighttpd libphp-jpgraph python-websocket"
[[ ! -f /etc/collectd/collectd.conf ]] && PKGS="${PKGS} collectd"
apt install ${PKGS}
# grab the installed php version
CGI_PKG=$(dpkg -l "*php*" | egrep -i "ii.*php.*-cli" | sed -E 's/.*(php.*-)(cli).*/\1cgi/')
apt install ${CGI_PKG}


print_msg "Checking out CGP from Github to: " ${CGP_ROOT}
[[ ! -d "${CGP_ROOT}" ]] && git -C ${WEBROOT} clone https://github.com/pommi/CGP.git


print_msg "Copying config files"
sed 's@##PLUGIN_DIR##@'"${PWD}"'@' < conf/cobalt.conf.tmpl > /tmp/cobalt.conf
cp -v /tmp/cobalt.conf ${COLLECTD_CONF_DIR}/cobalt.conf

[[ "${RRD_ENABLED}" -eq "0" ]] && cp -v conf/rrd.conf ${COLLECTD_CONF_DIR}/
cp -v conf/csv.conf ${COLLECTD_CONF_DIR}/
# Change graph display defaults in CGP config
patch --forward -r - ${CGP_ROOT}/conf/config.php < conf/CGP_conf.patch
[[ "${RRD_ENABLED}" -eq "1" ]] && cp -v conf/standard_types.conf ${COLLECTD_CONF_DIR}/


print_msg "Restarting services"
lighty-enable-mod fastcgi fastcgi-php
service collectd restart
service lighttpd restart

print_msg "Cobalt DevTools do not currently listen on 'localhost' or 127.0.0.1"
print_msg "Please manually edit target IP address to your adapter IP address in " ${COLLECTD_CONF_DIR}/cobalt.conf
ifconfig | egrep "inet " | sed -E "s/.*inet (.*) netmask.*/\1/" | egrep -v "127.0.0.1"
