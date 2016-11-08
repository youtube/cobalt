/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a glibc header of the same name.
 ***   It contains only constants, structures, and macros generated from
 ***   the original header, and thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _IFADDRS_H
#define _IFADDRS_H

#include <sys/socket.h>

struct ifaddrs {
  struct ifaddrs  *ifa_next;
  char            *ifa_name;
  unsigned int     ifa_flags;
  struct sockaddr *ifa_addr;
  struct sockaddr *ifa_netmask;
  union {
    struct sockaddr *ifu_broadaddr;
    struct sockaddr *ifu_dstaddr;
  } ifa_ifu;
#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#define ifa_dstaddr   ifa_ifu.ifu_dstaddr
  void            *ifa_data;
};

__BEGIN_DECLS
extern int getifaddrs(struct ifaddrs **ifap);
extern void freeifaddrs(struct ifaddrs *ifa);
__END_DECLS

#endif
