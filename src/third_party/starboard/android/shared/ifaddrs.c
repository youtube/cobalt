/* external/dhcpcd/ifaddrs.c
**
** Copyright 2011, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");.
** you may not use this file except in compliance with the License..
** You may obtain a copy of the License at.
**
**     http://www.apache.org/licenses/LICENSE-2.0.
**
** Unless required by applicable law or agreed to in writing, software.
** distributed under the License is distributed on an "AS IS" BASIS,.
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied..
** See the License for the specific language governing permissions and.
** limitations under the License.
*/

#include <arpa/inet.h>
#include <sys/socket.h>
//#include "ifaddrs.h"
#include "third_party/starboard/android/shared/ifaddrs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/ether.h>
#include <netdb.h>
#include <linux/if_packet.h>
#include <netinet/if_ether.h>
#include <linux/if_arp.h>
//#include <netutils/ifc.h>
#include "third_party/starboard/android/shared/ifc.h"

struct ifaddrs *get_interface(const char *name, sa_family_t family)
{
    unsigned addr, flags;
    int masklen;
    struct ifaddrs *ifa;
    struct sockaddr_in *saddr = NULL;
    struct sockaddr_in *smask = NULL;
    struct sockaddr_ll *hwaddr = NULL;
    unsigned char hwbuf[ETH_ALEN];

    if (ifc_get_info(name, &addr, &masklen, &flags))
        return NULL;

    if ((family == AF_INET) && (addr == 0))
        return NULL;

    ifa = malloc(sizeof(struct ifaddrs));
    if (!ifa)
        return NULL;
    memset(ifa, 0, sizeof(struct ifaddrs));

    ifa->ifa_name = malloc(strlen(name)+1);
    if (!ifa->ifa_name) {
        free(ifa);
        return NULL;
    }
    strcpy(ifa->ifa_name, name);
    ifa->ifa_flags = flags;

    if (family == AF_INET) {
        saddr = malloc(sizeof(struct sockaddr_in));
        if (saddr) {
            saddr->sin_addr.s_addr = addr;
            saddr->sin_family = family;
        }
        ifa->ifa_addr = (struct sockaddr *)saddr;

        if (masklen != 0) {
            smask = malloc(sizeof(struct sockaddr_in));
            if (smask) {
                smask->sin_addr.s_addr = prefixLengthToIpv4Netmask(masklen);
                smask->sin_family = family;
            }
        }
        ifa->ifa_netmask = (struct sockaddr *)smask;
    } else if (family == AF_PACKET) {
        if (!ifc_get_hwaddr(name, hwbuf)) {
            hwaddr = malloc(sizeof(struct sockaddr_ll));
            if (hwaddr) {
                memset(hwaddr, 0, sizeof(struct sockaddr_ll));
                hwaddr->sll_family = family;
                /* hwaddr->sll_protocol = ETHERTYPE_IP; */
                hwaddr->sll_hatype = ARPHRD_ETHER;
                hwaddr->sll_halen = ETH_ALEN;
                memcpy(hwaddr->sll_addr, hwbuf, ETH_ALEN);
            }
        }
        ifa->ifa_addr = (struct sockaddr *)hwaddr;
        ifa->ifa_netmask = (struct sockaddr *)smask;
    }
    return ifa;
}

int getifaddrs(struct ifaddrs **ifap)
{
    DIR *d;
    struct dirent *de;
    struct ifaddrs *ifa;
    struct ifaddrs *ifah = NULL;

    if (!ifap)
        return -1;
    *ifap = NULL;

    if (ifc_init())
       return -1;

    d = opendir("/sys/class/net");
    if (d == 0)
        return -1;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.')
            continue;
        ifa = get_interface(de->d_name, AF_INET);
        if (ifa != NULL) {
            ifa->ifa_next = ifah;
            ifah = ifa;
        }
        ifa = get_interface(de->d_name, AF_PACKET);
        if (ifa != NULL) {
            ifa->ifa_next = ifah;
            ifah = ifa;
        }
    }
    *ifap = ifah;
    closedir(d);
    ifc_close();
    return 0;
}

void freeifaddrs(struct ifaddrs *ifa)
{
    struct ifaddrs *ifp;

    while (ifa) {
        ifp = ifa;
        free(ifp->ifa_name);
        if (ifp->ifa_addr)
            free(ifp->ifa_addr);
        if (ifp->ifa_netmask)
            free(ifp->ifa_netmask);
        ifa = ifa->ifa_next;
        free(ifp);
    }
}
