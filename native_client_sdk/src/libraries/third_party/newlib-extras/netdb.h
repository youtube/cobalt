/*-
 * Copyright (c) 1980, 1983, 1988, 1993
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

/*
 *      @(#)netdb.h	8.1 (Berkeley) 6/2/93
 *      From: Id: netdb.h,v 8.9 1996/11/19 08:39:29 vixie Exp $
 * $FreeBSD: src/include/netdb.h,v 1.23 2002/03/23 17:24:53 imp Exp $
 */

#ifndef _NETDB_H_
#define _NETDB_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <machine/ansi.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __socklen_t_defined
typedef unsigned int socklen_t;
#define __socklen_t_defined 1
#endif

#ifdef	_BSD_SOCKLEN_T_
typedef	_BSD_SOCKLEN_T_	socklen_t;
#undef	_BSD_SOCKLEN_T_
#endif

#ifndef _PATH_HEQUIV
# define	_PATH_HEQUIV	"/etc/hosts.equiv"
#endif
#define	_PATH_HOSTS	"/etc/hosts"
#define	_PATH_NETWORKS	"/etc/networks"
#define	_PATH_PROTOCOLS	"/etc/protocols"
#define	_PATH_SERVICES	"/etc/services"
#define _PATH_NSSWITCH_CONF  "/etc/nsswitch.conf"

__BEGIN_DECLS
extern int *__h_errno_location(void);
__END_DECLS

#define h_errno (*(__h_errno_location()))

#define	MAXALIASES	35
  /* For now, only support one return address. */
#define MAXADDRS         2
/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct hostent {
  char	*h_name;	/* official name of host */
  char	**h_aliases;	/* alias list */
  int	h_addrtype;	/* host address type */
  int	h_length;	/* length of address */
  char	**h_addr_list;	/* list of addresses from name server */
  char *h_addr;         /* address, for backward compatibility */
  /* private data, for re-entrancy */
  char *__host_addrs[MAXADDRS];
  char *__host_aliases[MAXALIASES];
  unsigned int __host_addr[4];
};

/*
 * Assumption here is that a network number
 * fits in an unsigned long -- probably a poor one.
 */
struct netent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
	unsigned long	n_net;		/* network # */
};

struct servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

struct protoent {
	char	*p_name;	/* official protocol name */
	char	**p_aliases;	/* alias list */
	int	p_proto;	/* protocol # */
};

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct	sockaddr *ai_addr;	/* binary address */
	struct	addrinfo *ai_next;	/* next structure in linked list */
};

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritative Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */

/*
 * Error return codes from getaddrinfo()
 */
/* Error values for `getaddrinfo' function.  */
# define EAI_BADFLAGS     -1    /* Invalid value for `ai_flags' field.  */
# define EAI_NONAME       -2    /* NAME or SERVICE is unknown.  */
# define EAI_AGAIN        -3    /* Temporary failure in name resolution.  */
# define EAI_FAIL         -4    /* Non-recoverable failure in name res.  */
# define EAI_NODATA       -5    /* No address associated with NAME.  */
# define EAI_FAMILY       -6    /* `ai_family' not supported.  */
# define EAI_SOCKTYPE     -7    /* `ai_socktype' not supported.  */
# define EAI_SERVICE      -8    /* SERVICE not supported for `ai_socktype'.  */
# define EAI_ADDRFAMILY   -9    /* Address family for NAME not supported.  */
# define EAI_MEMORY       -10   /* Memory allocation failure.  */
# define EAI_SYSTEM       -11   /* System error returned in `errno'.  */
# define EAI_OVERFLOW     -12   /* Argument buffer overflow.  */
# ifdef __USE_GNU
#  define EAI_INPROGRESS  -100  /* Processing request in progress.  */
#  define EAI_CANCELED    -101  /* Request canceled.  */
#  define EAI_NOTCANCELED -102  /* Request not canceled.  */
#  define EAI_ALLDONE     -103  /* All requests done.  */
#  define EAI_INTR        -104  /* Interrupted by a signal.  */
#  define EAI_IDN_ENCODE  -105  /* IDN encoding failed.  */
# endif

/*
 * Flag values for getaddrinfo()
 */
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */
#define	AI_NUMERICHOST	0x00000004 /* prevent name resolution */
#define AI_NUMERICSERV  0x00000008 /* don't use name resolution. */
/* valid flags for addrinfo */
#define AI_MASK \
    (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_ADDRCONFIG)

#define	AI_ALL		0x00000100 /* IPv6 and IPv4-mapped (with AI_V4MAPPED) */
#define	AI_V4MAPPED_CFG	0x00000200 /* accept IPv4-mapped if kernel supports */
#define	AI_ADDRCONFIG	0x00000400 /* only if any address is assigned */
#define	AI_V4MAPPED	0x00000800 /* accept IPv4-mapped IPv6 address */
/* special recommended flags for getipnodebyname */
#define	AI_DEFAULT	(AI_V4MAPPED_CFG | AI_ADDRCONFIG)

/*
 * Constants for getnameinfo()
 */
#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32

/*
 * Flag values for getnameinfo()
 */
#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#define NI_WITHSCOPEID	0x00000020

/*
 * Scope delimit character
 */
#define	SCOPE_DELIMITER	'%'

__BEGIN_DECLS
void		endhostent(void);
void		endhostent_r(FILE **, int *);
void		endnetent(void);
void		endnetgrent(void);
void		endprotoent(void);
void		endservent(void);
void		freehostent(struct hostent *);
struct hostent	*gethostbyaddr(const void *, socklen_t, int);
struct hostent	*gethostbyname(const char *);
struct hostent	*gethostbyname2(const char *, int);
struct hostent	*gethostent(void);
int             gethostent_r(struct hostent *, char *, int, int *, FILE **);
struct hostent	*getipnodebyaddr(const void *, size_t, int, int *);
struct hostent	*getipnodebyname(const char *, int, int, int *);
struct netent	*getnetbyaddr(uint32_t, int);
struct netent	*getnetbyname(const char *);
struct netent	*getnetent(void);
int		getnetgrent(char **, char **, char **);
struct protoent	*getprotobyname(const char *);
struct protoent	*getprotobynumber(int);
struct protoent	*getprotoent(void);
struct servent	*getservbyname(const char *, const char *);
struct servent	*getservbyport(int, const char *);
struct servent	*getservent(void);
void		herror(const char *);
__const char	*hstrerror(int);
int		innetgr(const char *, const char *, const char *, const char *);
void		sethostent(int);
void		sethostent_r(int, FILE **, int *);
/* void		sethostfile(const char *); */
void		setnetent(int);
void		setprotoent(int);
int		getaddrinfo(const char *__restrict, const char *__restrict,
			    const struct addrinfo *__restrict,
			    struct addrinfo **__restrict);
int		getnameinfo(const struct sockaddr *__restrict, socklen_t,
			    char *__restrict, socklen_t, char *__restrict,
			    socklen_t, unsigned int);
void		freeaddrinfo(struct addrinfo *);
char		*gai_strerror(int);
int		setnetgrent(const char *);
void		setservent(int);

/*
 * PRIVATE functions specific to the FreeBSD implementation
 */

/* DO NOT USE THESE, THEY ARE SUBJECT TO CHANGE AND ARE NOT PORTABLE!!! */
void	_sethosthtent(int);
void	_sethosthtent_r(int, FILE **, int *);
void	_endhosthtent(void);
void	_endhosthtent_r(FILE **, int *);
void	_sethostdnsent(int);
void	_endhostdnsent(void);
void	_setnethtent(int);
void	_endnethtent(void);
void	_setnetdnsent(int);
void	_endnetdnsent(void);
struct hostent * _gethostbynisname(const char *, int);
struct hostent * _gethostbynisaddr(const char *, int, int);
void _map_v4v6_address(const char *, char *);
void _map_v4v6_hostent(struct hostent *, char **, int *);
__END_DECLS

#endif /* !_NETDB_H_ */
