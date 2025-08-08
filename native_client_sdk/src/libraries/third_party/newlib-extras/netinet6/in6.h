/*	$FreeBSD: src/sys/netinet6/in6.h,v 1.19 2002/04/19 04:46:22 suz Exp $	*/
/*	$KAME: in6.h,v 1.89 2001/05/27 13:28:35 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in.h	8.3 (Berkeley) 1/3/94
 */

#ifndef __KAME_NETINET_IN_H_INCLUDED_
#error "do not include netinet6/in6.h directly, include netinet/in.h.  see RFC2553"
#endif

#ifndef _NETINET6_IN6_H_
#define _NETINET6_IN6_H_

#include <sys/types.h>

/*
 * Identification of the network protocol stack
 * for *BSD-current/release: http://www.kame.net/dev/cvsweb.cgi/kame/COVERAGE
 * has the table of implementation/integration differences.
 */
#define __KAME__
#define __KAME_VERSION		"20010528/FreeBSD"

/*
 * Local port number conventions:
 *
 * Ports < IPPORT_RESERVED are reserved for privileged processes (e.g. root),
 * unless a kernel is compiled with IPNOPRIVPORTS defined.
 *
 * When a user does a bind(2) or connect(2) with a port number of zero,
 * a non-conflicting local port address is chosen.
 *
 * The default range is IPPORT_ANONMIN to IPPORT_ANONMAX, although
 * that is settable by sysctl(3); net.inet.ip.anonportmin and
 * net.inet.ip.anonportmax respectively.
 *
 * A user may set the IPPROTO_IP option IP_PORTRANGE to change this
 * default assignment range.
 *
 * The value IP_PORTRANGE_DEFAULT causes the default behavior.
 *
 * The value IP_PORTRANGE_HIGH is the same as IP_PORTRANGE_DEFAULT,
 * and exists only for FreeBSD compatibility purposes.
 *
 * The value IP_PORTRANGE_LOW changes the range to the "low" are
 * that is (by convention) restricted to privileged processes.
 * This convention is based on "vouchsafe" principles only.
 * It is only secure if you trust the remote host to restrict these ports.
 * The range is IPPORT_RESERVEDMIN to IPPORT_RESERVEDMAX.
 */

#define	IPV6PORT_RESERVED	1024
#define	IPV6PORT_ANONMIN	49152
#define	IPV6PORT_ANONMAX	65535
#define	IPV6PORT_RESERVEDMIN	600
#define	IPV6PORT_RESERVEDMAX	(IPV6PORT_RESERVED-1)

/*
 * IPv6 address
 */
struct in6_addr {
	union {
		uint8_t   __u6_addr8[16];
		uint16_t  __u6_addr16[8];
		uint32_t  __u6_addr32[4];
	} __u6_addr;			/* 128-bit IP6 address */
};

#define s6_addr   __u6_addr.__u6_addr8
#ifdef _KERNEL	/* XXX nonstandard */
#define s6_addr8  __u6_addr.__u6_addr8
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32
#endif

#define INET6_ADDRSTRLEN	46

/*
 * Socket address for IPv6
 */
#ifndef _XOPEN_SOURCE
#define SIN6_LEN
#endif
struct sockaddr_in6 {
	uint8_t	sin6_family;	/* AF_INET6 (sa_family_t) */
	uint16_t	sin6_port;	/* Transport layer port # (in_port_t)*/
	uint32_t	sin6_flowinfo;	/* IP6 flow information */
	struct in6_addr	sin6_addr;	/* IP6 address */
	uint32_t	sin6_scope_id;	/* scope zone index */
};

/*
 * Local definition for masks
 */
#ifdef _KERNEL	/* XXX nonstandard */
#define IN6MASK0	{{{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }}}
#define IN6MASK32	{{{ 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, \
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}
#define IN6MASK64	{{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}
#define IN6MASK96	{{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
			    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 }}}
#define IN6MASK128	{{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
			    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }}}
#endif

#ifdef _KERNEL
extern const struct sockaddr_in6 sa6_any;

extern const struct in6_addr in6mask0;
extern const struct in6_addr in6mask32;
extern const struct in6_addr in6mask64;
extern const struct in6_addr in6mask96;
extern const struct in6_addr in6mask128;
#endif /* _KERNEL */

/*
 * Macros started with IPV6_ADDR is KAME local
 */
#ifdef _KERNEL	/* XXX nonstandard */
#if BYTE_ORDER == BIG_ENDIAN
#define IPV6_ADDR_INT32_ONE	1
#define IPV6_ADDR_INT32_TWO	2
#define IPV6_ADDR_INT32_MNL	0xff010000
#define IPV6_ADDR_INT32_MLL	0xff020000
#define IPV6_ADDR_INT32_SMP	0x0000ffff
#define IPV6_ADDR_INT16_ULL	0xfe80
#define IPV6_ADDR_INT16_USL	0xfec0
#define IPV6_ADDR_INT16_MLL	0xff02
#elif BYTE_ORDER == LITTLE_ENDIAN
#define IPV6_ADDR_INT32_ONE	0x01000000
#define IPV6_ADDR_INT32_TWO	0x02000000
#define IPV6_ADDR_INT32_MNL	0x000001ff
#define IPV6_ADDR_INT32_MLL	0x000002ff
#define IPV6_ADDR_INT32_SMP	0xffff0000
#define IPV6_ADDR_INT16_ULL	0x80fe
#define IPV6_ADDR_INT16_USL	0xc0fe
#define IPV6_ADDR_INT16_MLL	0x02ff
#endif
#endif

/*
 * Definition of some useful macros to handle IP6 addresses
 */
#define IN6ADDR_ANY_INIT \
	{{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}
#define IN6ADDR_LOOPBACK_INIT \
	{{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}}
#define IN6ADDR_NODELOCAL_ALLNODES_INIT \
	{{{ 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}}
#define IN6ADDR_LINKLOCAL_ALLNODES_INIT \
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}}
#define IN6ADDR_LINKLOCAL_ALLROUTERS_INIT \
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }}}


__BEGIN_DECLS

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
extern const struct in6_addr in6addr_nodelocal_allnodes;
extern const struct in6_addr in6addr_linklocal_allnodes;
extern const struct in6_addr in6addr_linklocal_allrouters;

__END_DECLS

/*
 * Equality
 * NOTE: Some of kernel programming environment (for example, openbsd/sparc)
 * does not supply memcmp().  For userland memcmp() is preferred as it is
 * in ANSI standard.
 */
#ifdef _KERNEL
#define IN6_ARE_ADDR_EQUAL(a, b)			\
    (bcmp(&(a)->s6_addr[0], &(b)->s6_addr[0], sizeof(struct in6_addr)) == 0)
#else
#define IN6_ARE_ADDR_EQUAL(a, b)			\
    (memcmp(&(a)->s6_addr[0], &(b)->s6_addr[0], sizeof(struct in6_addr)) == 0)
#endif

#ifdef _KERNEL			/* non standard */
/* see if two addresses are equal in a scope-conscious manner. */
#define SA6_ARE_ADDR_EQUAL(a, b) \
	(((a)->sin6_scope_id == 0 || (b)->sin6_scope_id == 0 || \
	  ((a)->sin6_scope_id == (b)->sin6_scope_id)) && \
	 (bcmp(&(a)->sin6_addr, &(b)->sin6_addr, sizeof(struct in6_addr)) == 0))
#endif

/*
 * Unspecified
 */
#define IN6_IS_ADDR_UNSPECIFIED(a)	\
	((*(const uint32_t *)(const void *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[8]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[12]) == 0))

/*
 * Loopback
 */
#define IN6_IS_ADDR_LOOPBACK(a)		\
	((*(const uint32_t *)(const void *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[8]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[12]) == ntohl(1)))

/*
 * IPv4 compatible
 */
#define IN6_IS_ADDR_V4COMPAT(a)		\
	((*(const uint32_t *)(const void *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[8]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[12]) != 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[12]) != ntohl(1)))

/*
 * Mapped
 */
#define IN6_IS_ADDR_V4MAPPED(a)		      \
	((*(const uint32_t *)(const void *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(const uint32_t *)(const void *)(&(a)->s6_addr[8]) == ntohl(0x0000ffff)))

/*
 * KAME Scope Values
 */

#ifdef _KERNEL	/* XXX nonstandard */
#define IPV6_ADDR_SCOPE_NODELOCAL	0x01
#define IPV6_ADDR_SCOPE_LINKLOCAL	0x02
#define IPV6_ADDR_SCOPE_SITELOCAL	0x05
#define IPV6_ADDR_SCOPE_ORGLOCAL	0x08	/* just used in this file */
#define IPV6_ADDR_SCOPE_GLOBAL		0x0e
#else
#define __IPV6_ADDR_SCOPE_NODELOCAL	0x01
#define __IPV6_ADDR_SCOPE_LINKLOCAL	0x02
#define __IPV6_ADDR_SCOPE_SITELOCAL	0x05
#define __IPV6_ADDR_SCOPE_ORGLOCAL	0x08	/* just used in this file */
#define __IPV6_ADDR_SCOPE_GLOBAL	0x0e
#endif

/*
 * Unicast Scope
 * Note that we must check topmost 10 bits only, not 16 bits (see RFC2373).
 */
#define IN6_IS_ADDR_LINKLOCAL(a)	\
	(((a)->s6_addr[0] == 0xfe) && (((a)->s6_addr[1] & 0xc0) == 0x80))
#define IN6_IS_ADDR_SITELOCAL(a)	\
	(((a)->s6_addr[0] == 0xfe) && (((a)->s6_addr[1] & 0xc0) == 0xc0))

/*
 * Multicast
 */
#define IN6_IS_ADDR_MULTICAST(a)	((a)->s6_addr[0] == 0xff)

#ifdef _KERNEL	/* XXX nonstandard */
#define IPV6_ADDR_MC_SCOPE(a)		((a)->s6_addr[1] & 0x0f)
#else
#define __IPV6_ADDR_MC_SCOPE(a)		((a)->s6_addr[1] & 0x0f)
#endif

/*
 * Multicast Scope
 */
#ifdef _KERNEL	/* refers nonstandard items */
#define IN6_IS_ADDR_MC_NODELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (IPV6_ADDR_MC_SCOPE(a) == IPV6_ADDR_SCOPE_NODELOCAL))
#define IN6_IS_ADDR_MC_LINKLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (IPV6_ADDR_MC_SCOPE(a) == IPV6_ADDR_SCOPE_LINKLOCAL))
#define IN6_IS_ADDR_MC_SITELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) && 	\
	 (IPV6_ADDR_MC_SCOPE(a) == IPV6_ADDR_SCOPE_SITELOCAL))
#define IN6_IS_ADDR_MC_ORGLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (IPV6_ADDR_MC_SCOPE(a) == IPV6_ADDR_SCOPE_ORGLOCAL))
#define IN6_IS_ADDR_MC_GLOBAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (IPV6_ADDR_MC_SCOPE(a) == IPV6_ADDR_SCOPE_GLOBAL))
#else
#define IN6_IS_ADDR_MC_NODELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_NODELOCAL))
#define IN6_IS_ADDR_MC_LINKLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_LINKLOCAL))
#define IN6_IS_ADDR_MC_SITELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) && 	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_SITELOCAL))
#define IN6_IS_ADDR_MC_ORGLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_ORGLOCAL))
#define IN6_IS_ADDR_MC_GLOBAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_GLOBAL))
#endif

#ifdef _KERNEL	/* nonstandard */
/*
 * KAME Scope
 */
#define IN6_IS_SCOPE_LINKLOCAL(a)	\
	((IN6_IS_ADDR_LINKLOCAL(a)) ||	\
	 (IN6_IS_ADDR_MC_LINKLOCAL(a)))

#define IFA6_IS_DEPRECATED(a) \
	((a)->ia6_lifetime.ia6t_preferred != 0 && \
	 (a)->ia6_lifetime.ia6t_preferred < time_second)
#define IFA6_IS_INVALID(a) \
	((a)->ia6_lifetime.ia6t_expire != 0 && \
	 (a)->ia6_lifetime.ia6t_expire < time_second)
#endif /* _KERNEL */

/*
 * IP6 route structure
 */
#ifndef _XOPEN_SOURCE
struct route_in6 {
	struct	rtentry *ro_rt;
	struct	sockaddr_in6 ro_dst;
};
#endif

/*
 * Options for use with [gs]etsockopt at the IPV6 level.
 * First word of comment is data type; bool is stored in int.
 */
/* no hdrincl */
#if 0 /* the followings are relic in IPv4 and hence are disabled */
#define IPV6_OPTIONS		1  /* buf/ip6_opts; set/get IP6 options */
#define IPV6_RECVOPTS		5  /* bool; receive all IP6 opts w/dgram */
#define IPV6_RECVRETOPTS	6  /* bool; receive IP6 opts for response */
#define IPV6_RECVDSTADDR	7  /* bool; receive IP6 dst addr w/dgram */
#define IPV6_RETOPTS		8  /* ip6_opts; set/get IP6 options */
#endif
#define IPV6_SOCKOPT_RESERVED1	3  /* reserved for future use */
#define IPV6_UNICAST_HOPS	4  /* int; IP6 hops */
#define IPV6_MULTICAST_IF	9  /* u_char; set/get IP6 multicast i/f  */
#define IPV6_MULTICAST_HOPS	10 /* u_char; set/get IP6 multicast hops */
#define IPV6_MULTICAST_LOOP	11 /* u_char; set/get IP6 multicast loopback */
#define IPV6_JOIN_GROUP		12 /* ip6_mreq; join a group membership */
#define IPV6_LEAVE_GROUP	13 /* ip6_mreq; leave a group membership */
#define IPV6_PORTRANGE		14 /* int; range to choose for unspec port */
#define ICMP6_FILTER		18 /* icmp6_filter; icmp6 filter */
/* RFC2292 options */
#define IPV6_PKTINFO		19 /* bool; send/recv if, src/dst addr */
#define IPV6_HOPLIMIT		20 /* bool; hop limit */
#define IPV6_NEXTHOP		21 /* bool; next hop addr */
#define IPV6_HOPOPTS		22 /* bool; hop-by-hop option */
#define IPV6_DSTOPTS		23 /* bool; destination option */
#define IPV6_RTHDR		24 /* bool; routing header */
#define IPV6_PKTOPTIONS		25 /* buf/cmsghdr; set/get IPv6 options */

#define IPV6_CHECKSUM		26 /* int; checksum offset for raw socket */
#define IPV6_V6ONLY		27 /* bool; only bind INET6 at wildcard bind */
#ifndef _KERNEL
#define IPV6_BINDV6ONLY		IPV6_V6ONLY
#endif

#if 1 /* IPSEC */
#define IPV6_IPSEC_POLICY	28 /* struct; get/set security policy */
#endif
#define IPV6_FAITH		29 /* bool; accept FAITH'ed connections */

#if 1 /* IPV6FIREWALL */
#define IPV6_FW_ADD		30 /* add a firewall rule to chain */
#define IPV6_FW_DEL		31 /* delete a firewall rule from chain */
#define IPV6_FW_FLUSH		32 /* flush firewall rule chain */
#define IPV6_FW_ZERO		33 /* clear single/all firewall counter(s) */
#define IPV6_FW_GET		34 /* get entire firewall rule chain */
#endif

/* to define items, should talk with KAME guys first, for *BSD compatibility */

#define IPV6_RTHDR_LOOSE     0 /* this hop need not be a neighbor. XXX old spec */
#define IPV6_RTHDR_STRICT    1 /* this hop must be a neighbor. XXX old spec */
#define IPV6_RTHDR_TYPE_0    0 /* IPv6 routing header type 0 */

/*
 * Defaults and limits for options
 */
#define IPV6_DEFAULT_MULTICAST_HOPS 1	/* normally limit m'casts to 1 hop */
#define IPV6_DEFAULT_MULTICAST_LOOP 1	/* normally hear sends if a member */

/*
 * Argument structure for IPV6_JOIN_GROUP and IPV6_LEAVE_GROUP.
 */
struct ipv6_mreq {
	struct in6_addr	ipv6mr_multiaddr;
	unsigned int	ipv6mr_interface;
};

/*
 * IPV6_PKTINFO: Packet information(RFC2292 sec 5)
 */
struct in6_pktinfo {
	struct in6_addr	ipi6_addr;	/* src/dst IPv6 address */
	unsigned int	ipi6_ifindex;	/* send/recv interface index */
};

/*
 * Argument for IPV6_PORTRANGE:
 * - which range to search when port is unspecified at bind() or connect()
 */
#define	IPV6_PORTRANGE_DEFAULT	0	/* default range */
#define	IPV6_PORTRANGE_HIGH	1	/* "high" - request firewall bypass */
#define	IPV6_PORTRANGE_LOW	2	/* "low" - vouchsafe security */

#ifndef _XOPEN_SOURCE
/*
 * Definitions for inet6 sysctl operations.
 *
 * Third level is protocol number.
 * Fourth level is desired variable within that protocol.
 */
#define IPV6PROTO_MAXID	(IPPROTO_PIM + 1)	/* don't list to IPV6PROTO_MAX */

#define CTL_IPV6PROTO_NAMES { \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, \
	{ "tcp6", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "udp6", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, \
	{ "ip6", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, \
	{ "ipsec6", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "icmp6", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "pim6", CTLTYPE_NODE }, \
}

/*
 * Names for IP sysctl objects
 */
#define IPV6CTL_FORWARDING	1	/* act as router */
#define IPV6CTL_SENDREDIRECTS	2	/* may send redirects when forwarding*/
#define IPV6CTL_DEFHLIM		3	/* default Hop-Limit */
#ifdef notyet
#define IPV6CTL_DEFMTU		4	/* default MTU */
#endif
#define IPV6CTL_FORWSRCRT	5	/* forward source-routed dgrams */
#define IPV6CTL_STATS		6	/* stats */
#define IPV6CTL_MRTSTATS	7	/* multicast forwarding stats */
#define IPV6CTL_MRTPROTO	8	/* multicast routing protocol */
#define IPV6CTL_MAXFRAGPACKETS	9	/* max packets reassembly queue */
#define IPV6CTL_SOURCECHECK	10	/* verify source route and intf */
#define IPV6CTL_SOURCECHECK_LOGINT 11	/* minimume logging interval */
#define IPV6CTL_ACCEPT_RTADV	12
#define IPV6CTL_KEEPFAITH	13
#define IPV6CTL_LOG_INTERVAL	14
#define IPV6CTL_HDRNESTLIMIT	15
#define IPV6CTL_DAD_COUNT	16
#define IPV6CTL_AUTO_FLOWLABEL	17
#define IPV6CTL_DEFMCASTHLIM	18
#define IPV6CTL_GIF_HLIM	19	/* default HLIM for gif encap packet */
#define IPV6CTL_KAME_VERSION	20
#define IPV6CTL_USE_DEPRECATED	21	/* use deprecated addr (RFC2462 5.5.4) */
#define IPV6CTL_RR_PRUNE	22	/* walk timer for router renumbering */
#if 0	/* obsolete */
#define IPV6CTL_MAPPED_ADDR	23
#endif
#define IPV6CTL_V6ONLY		24
#define IPV6CTL_RTEXPIRE	25	/* cloned route expiration time */
#define IPV6CTL_RTMINEXPIRE	26	/* min value for expiration time */
#define IPV6CTL_RTMAXCACHE	27	/* trigger level for dynamic expire */

#define IPV6CTL_USETEMPADDR	32	/* use temporary addresses (RFC3041) */
#define IPV6CTL_TEMPPLTIME	33	/* preferred lifetime for tmpaddrs */
#define IPV6CTL_TEMPVLTIME	34	/* valid lifetime for tmpaddrs */
#define IPV6CTL_AUTO_LINKLOCAL	35	/* automatic link-local addr assign */
#define IPV6CTL_RIP6STATS	36	/* raw_ip6 stats */

/* New entries should be added here from current IPV6CTL_MAXID value. */
/* to define items, should talk with KAME guys first, for *BSD compatibility */
#define IPV6CTL_MAXID		37

#endif /* !_XOPEN_SOURCE */

/*
 * Redefinition of mbuf flags
 */
#define	M_AUTHIPHDR	M_PROTO2
#define	M_DECRYPTED	M_PROTO3
#define	M_LOOP		M_PROTO4
#define	M_AUTHIPDGM	M_PROTO5

#ifdef _KERNEL
struct cmsghdr;

int	in6_cksum __P((struct mbuf *, uint8_t, uint32_t, uint32_t));
int	in6_localaddr __P((struct in6_addr *));
int	in6_addrscope __P((struct in6_addr *));
struct	in6_ifaddr *in6_ifawithscope __P((struct ifnet *, struct in6_addr *));
struct	in6_ifaddr *in6_ifawithifp __P((struct ifnet *, struct in6_addr *));
extern void in6_if_up __P((struct ifnet *));
struct sockaddr;
extern	u_char	ip6_protox[];

void	in6_sin6_2_sin __P((struct sockaddr_in *sin,
			    struct sockaddr_in6 *sin6));
void	in6_sin_2_v4mapsin6 __P((struct sockaddr_in *sin,
				 struct sockaddr_in6 *sin6));
void	in6_sin6_2_sin_in_sock __P((struct sockaddr *nam));
void	in6_sin_2_v4mapsin6_in_sock __P((struct sockaddr **nam));

#define	satosin6(sa)	((struct sockaddr_in6 *)(sa))
#define	sin6tosa(sin6)	((struct sockaddr *)(sin6))
#define	ifatoia6(ifa)	((struct in6_ifaddr *)(ifa))

extern int	(*faithprefix_p)(struct in6_addr *);
#endif /* _KERNEL */

__BEGIN_DECLS
struct cmsghdr;

extern int inet6_option_space __P((int));
extern int inet6_option_init __P((void *, struct cmsghdr **, int));
extern int inet6_option_append __P((struct cmsghdr *, const uint8_t *,
	int, int));
extern uint8_t *inet6_option_alloc __P((struct cmsghdr *, int, int, int));
extern int inet6_option_next __P((const struct cmsghdr *, uint8_t **));
extern int inet6_option_find __P((const struct cmsghdr *, uint8_t **, int));

extern size_t inet6_rthdr_space __P((int, int));
extern struct cmsghdr *inet6_rthdr_init __P((void *, int));
extern int inet6_rthdr_add __P((struct cmsghdr *, const struct in6_addr *,
		unsigned int));
extern int inet6_rthdr_lasthop __P((struct cmsghdr *, unsigned int));
#if 0 /* not implemented yet */
extern int inet6_rthdr_reverse __P((const struct cmsghdr *, struct cmsghdr *));
#endif
extern int inet6_rthdr_segments __P((const struct cmsghdr *));
extern struct in6_addr *inet6_rthdr_getaddr __P((struct cmsghdr *, int));
extern int inet6_rthdr_getflags __P((const struct cmsghdr *, int));

extern int inet6_opt_init __P((void *, size_t));
extern int inet6_opt_append __P((void *, size_t, int, uint8_t,
				 size_t, uint8_t, void **));
extern int inet6_opt_finish __P((void *, size_t, int));
extern int inet6_opt_set_val __P((void *, size_t, void *, int));

extern int inet6_opt_next __P((void *, size_t, int, uint8_t *,
			       size_t *, void **));
extern int inet6_opt_find __P((void *, size_t, int, uint8_t,
			  size_t *, void **));
extern int inet6_opt_get_val __P((void *, size_t, void *, int));
extern size_t inet6_rth_space __P((int, int));
extern void *inet6_rth_init __P((void *, int, int, int));
extern int inet6_rth_add __P((void *, const struct in6_addr *));
extern int inet6_rth_reverse __P((const void *, void *));
extern int inet6_rth_segments __P((const void *));
extern struct in6_addr *inet6_rth_getaddr __P((const void *, int));
__END_DECLS

#endif /* !_NETINET6_IN6_H_ */
