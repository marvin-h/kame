/*	$OpenBSD: in.c,v 1.25 2002/04/01 02:44:08 itojun Exp $	*/
/*	$NetBSD: in.c,v 1.26 1996/02/13 23:41:39 christos Exp $	*/

/*
 * Copyright (c) 2002 INRIA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by INRIA and its
 *	contributors.
 * 4. Neither the name of INRIA nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Implementation of Internet Group Management Protocol, Version 3.
 *
 * Developed by Hitoshi Asaeda, INRIA, February 2002.
 */

/*
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
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
 * Copyright (c) 1982, 1986, 1991, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/igmp_var.h>

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#include "ether.h"
#include <net/net_osdep.h>

#ifdef INET

static int in_mask2len(struct in_addr *);
static void in_len2mask(struct in_addr *, int);
static int in_lifaddr_ioctl(struct socket *, u_long, caddr_t,
	struct ifnet *);

static int in_addprefix(struct in_ifaddr *, int);
static int in_scrubprefix(struct in_ifaddr *);

#ifndef SUBNETSARELOCAL
#define	SUBNETSARELOCAL	0
#endif

#ifndef HOSTZEROBROADCAST
#define HOSTZEROBROADCAST 1
#endif

int subnetsarelocal = SUBNETSARELOCAL;
int hostzeroisbroadcast = HOSTZEROBROADCAST;

/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(in)
	struct in_addr in;
{
	register struct in_ifaddr *ia;

	if (subnetsarelocal) {
		for (ia = in_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next)
			if ((in.s_addr & ia->ia_netmask) == ia->ia_net)
				return (1);
	} else {
		for (ia = in_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next)
			if ((in.s_addr & ia->ia_subnetmask) == ia->ia_subnet)
				return (1);
	}
	return (0);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(in)
	struct in_addr in;
{
	register u_int32_t net;

	if (IN_EXPERIMENTAL(in.s_addr) || IN_MULTICAST(in.s_addr))
		return (0);
	if (IN_CLASSA(in.s_addr)) {
		net = in.s_addr & IN_CLASSA_NET;
		if (net == 0 || net == htonl(IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Trim a mask in a sockaddr
 */
void
in_socktrim(ap)
	struct sockaddr_in *ap;
{
	register char *cplim = (char *) &ap->sin_addr;
	register char *cp = (char *) (&ap->sin_addr + 1);

	ap->sin_len = 0;
	while (--cp >= cplim)
		if (*cp) {
			(ap)->sin_len = cp - (char *) (ap) + 1;
			break;
		}
}

static int
in_mask2len(mask)
	struct in_addr *mask;
{
	int x, y;
	u_char *p;

	p = (u_char *)mask;
	for (x = 0; x < sizeof(*mask); x++) {
		if (p[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < 8; y++) {
			if ((p[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * 8 + y;
}

static void
in_len2mask(mask, len)
	struct in_addr *mask;
	int len;
{
	int i;
	u_char *p;

	p = (u_char *)mask;
	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		p[i] = 0xff;
	if (len % 8)
		p[i] = (0xff00 >> (len % 8)) & 0xff;
}

int	in_interfaces;		/* number of external internet interfaces */

/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(so, cmd, data, ifp)
	struct socket *so;
	u_long cmd;
	caddr_t data;
	register struct ifnet *ifp;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct in_ifaddr *ia = 0;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error, hostIsNew, maskIsNew;
	int newifaddr;

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return(EPERM);
		/*fall through*/
	case SIOCGLIFADDR:
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp);
	}

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp)
		for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next)
			if (ia->ia_ifp == ifp)
				break;

	switch (cmd) {

	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifra->ifra_addr.sin_family == AF_INET)
		    for (; ia != 0; ia = ia->ia_list.tqe_next) {
			if (ia->ia_ifp == ifp &&
			    ia->ia_addr.sin_addr.s_addr ==
				ifra->ifra_addr.sin_addr.s_addr)
			    break;
		}
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);

		if (ifp == 0)
			panic("in_control");
		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK);
			bzero((caddr_t)ia, sizeof *ia);
			TAILQ_INSERT_TAIL(&in_ifaddr, ia, ia_list);
			TAILQ_INSERT_TAIL(&ifp->if_addrlist, (struct ifaddr *)ia,
			    ifa_list);
			ia->ia_ifa.ifa_addr = sintosa(&ia->ia_addr);
			ia->ia_ifa.ifa_dstaddr = sintosa(&ia->ia_dstaddr);
			ia->ia_ifa.ifa_netmask = sintosa(&ia->ia_sockmask);
			ia->ia_sockmask.sin_len = 8;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
			LIST_INIT(&ia->ia_multiaddrs);
			if ((ifp->if_flags & IFF_LOOPBACK) == 0)
				in_interfaces++;

			newifaddr = 1;
		} else
			newifaddr = 0;
		break;

	case SIOCSIFBRDADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);
		/* FALLTHROUGH */

	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia && satosin(&ifr->ifr_addr)->sin_addr.s_addr) {
			struct in_ifaddr *ia2;

			for (ia2 = ia; ia2; ia2 = ia2->ia_list.tqe_next) {
				if (ia2->ia_ifp == ifp &&
				    ia2->ia_addr.sin_addr.s_addr ==
				    satosin(&ifr->ifr_addr)->sin_addr.s_addr)
					break;
			}
			if (ia2 && ia2->ia_ifp == ifp)
				ia = ia2;
		}
		if (ia == (struct in_ifaddr *)0)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR:
		*satosin(&ifr->ifr_addr) = ia->ia_addr;
		break;

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*satosin(&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		break;

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*satosin(&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK:
		*satosin(&ifr->ifr_addr) = ia->ia_sockmask;
		break;

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *satosin(&ifr->ifr_dstaddr);
		if (ifp->if_ioctl && (error = (*ifp->if_ioctl)
					(ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			return (error);
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = sintosa(&oldaddr);
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr = sintosa(&ia->ia_dstaddr);
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		break;

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ia->ia_broadaddr = *satosin(&ifr->ifr_broadaddr);
		break;

	case SIOCSIFADDR:
		error = in_ifinit(ifp, ia, satosin(&ifr->ifr_addr), 1);
		return error;

	case SIOCSIFNETMASK:
		ia->ia_subnetmask = ia->ia_sockmask.sin_addr.s_addr =
		    ifra->ifra_addr.sin_addr.s_addr;
		break;

	case SIOCAIFADDR:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
					       ia->ia_addr.sin_addr.s_addr)
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_subnetmask = ia->ia_sockmask.sin_addr.s_addr;
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
		    (hostIsNew || maskIsNew)) {
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		}
		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		return (error);

	case SIOCDIFADDR:
		in_ifscrub(ifp, ia);
		TAILQ_REMOVE(&ifp->if_addrlist, (struct ifaddr *)ia, ifa_list);
		TAILQ_REMOVE(&in_ifaddr, ia, ia_list);
		IFAFREE((&ia->ia_ifa));
		break;

#ifdef MROUTING
	case SIOCGETVIFCNT:
	case SIOCGETSGCNT:
		return (mrt_ioctl(cmd, data));
#endif /* MROUTING */

#ifdef IGMPV3
	case SIOCSIPMSFILTER:
		/* Set IPv4 Multicast Source Filter */
		return (ip_setmopt_srcfilter(so, (struct ip_msfilter **)data));
	case SIOCGIPMSFILTER:
		/* Get IPv4 Multicast Source Filter */
		return (ip_getmopt_srcfilter(so, (struct ip_msfilter **)data));
	case SIOCSMSFILTER:
		/* Set Protocol-Independent Multicast Source Filter */
		return (sock_setmopt_srcfilter
				(so, (struct group_filter **)data));
	case SIOCGMSFILTER:
		/* Get Protocol-Independent Multicast Source Filter */
		return (sock_getmopt_srcfilter
				(so, (struct group_filter **)data));
#endif /* IGMPV3 */

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
	return (0);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (???)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		EINVAL since we can't deduce hostid part of the address.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in_ioctl()
 */
static int
in_lifaddr_ioctl(so, cmd, data, ifp)
	struct socket *so;
	u_long cmd;
	caddr_t	data;
	struct ifnet *ifp;
{
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;
	struct sockaddr *sa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in_lifaddr_ioctl");
		/*NOTRECHED*/
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/*FALLTHROUGH*/
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		sa = (struct sockaddr *)&iflr->addr;
		if (sa->sa_family != AF_INET)
			return EINVAL;
		if (sa->sa_len != sizeof(struct sockaddr_in))
			return EINVAL;
		/* XXX need improvement */
		sa = (struct sockaddr *)&iflr->dstaddr;
		if (sa->sa_family
		 && sa->sa_family != AF_INET)
			return EINVAL;
		if (sa->sa_len && sa->sa_len != sizeof(struct sockaddr_in))
			return EINVAL;
		break;
	default: /*shouldn't happen*/
#if 0
		panic("invalid cmd to in_lifaddr_ioctl");
		/*NOTREACHED*/
#else
		return EOPNOTSUPP;
#endif
	}
	if (sizeof(struct in_addr) * 8 < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in_aliasreq ifra;

		if (iflr->flags & IFLR_PREFIX)
			return EINVAL;

		/* copy args to in_aliasreq, perform ioctl(SIOCAIFADDR_IN6). */
		bzero(&ifra, sizeof(ifra));
		bcopy(iflr->iflr_name, ifra.ifra_name,
			sizeof(ifra.ifra_name));

		bcopy(&iflr->addr, &ifra.ifra_addr,
			((struct sockaddr *)&iflr->addr)->sa_len);

		if (((struct sockaddr *)&iflr->dstaddr)->sa_family) {	/*XXX*/
			bcopy(&iflr->dstaddr, &ifra.ifra_dstaddr,
				((struct sockaddr *)&iflr->dstaddr)->sa_len);
		}

		ifra.ifra_mask.sin_family = AF_INET;
		ifra.ifra_mask.sin_len = sizeof(struct sockaddr_in);
		in_len2mask(&ifra.ifra_mask.sin_addr, iflr->prefixlen);

		return in_control(so, SIOCAIFADDR, (caddr_t)&ifra, ifp);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in_ifaddr *ia;
		struct in_addr mask, candidate, match;
		struct sockaddr_in *sin;
		int cmp;

		bzero(&mask, sizeof(mask));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in_len2mask(&mask, iflr->prefixlen);

			sin = (struct sockaddr_in *)&iflr->addr;
			match.s_addr = sin->sin_addr.s_addr;
			match.s_addr &= mask.s_addr;

			/* if you set extra bits, that's wrong */
			if (match.s_addr != sin->sin_addr.s_addr)
				return EINVAL;

			cmp = 1;
		} else {
			if (cmd == SIOCGLIFADDR) {
				/* on getting an address, take the 1st match */
				cmp = 0;	/*XXX*/
			} else {
				/* on deleting an address, do exact match */
				in_len2mask(&mask, 32);
				sin = (struct sockaddr_in *)&iflr->addr;
				match.s_addr = sin->sin_addr.s_addr;

				cmp = 1;
			}
		}

		for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			if (!cmp)
				break;
			candidate.s_addr = ((struct sockaddr_in *)&ifa->ifa_addr)->sin_addr.s_addr;
			candidate.s_addr &= mask.s_addr;
			if (candidate.s_addr == match.s_addr)
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia = (struct in_ifaddr *)ifa;

		if (cmd == SIOCGLIFADDR) {
			/* fill in the if_laddrreq structure */
			bcopy(&ia->ia_addr, &iflr->addr, ia->ia_addr.sin_len);

			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &iflr->dstaddr,
					ia->ia_dstaddr.sin_len);
			} else
				bzero(&iflr->dstaddr, sizeof(iflr->dstaddr));

			iflr->prefixlen =
				in_mask2len(&ia->ia_sockmask.sin_addr);

			iflr->flags = 0;	/*XXX*/

			return 0;
		} else {
			struct in_aliasreq ifra;

			/* fill in_aliasreq and do ioctl(SIOCDIFADDR_IN6) */
			bzero(&ifra, sizeof(ifra));
			bcopy(iflr->iflr_name, ifra.ifra_name,
				sizeof(ifra.ifra_name));

			bcopy(&ia->ia_addr, &ifra.ifra_addr,
				ia->ia_addr.sin_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				bcopy(&ia->ia_dstaddr, &ifra.ifra_dstaddr,
					ia->ia_dstaddr.sin_len);
			}
			bcopy(&ia->ia_sockmask, &ifra.ifra_dstaddr,
				ia->ia_sockmask.sin_len);

			return in_control(so, SIOCDIFADDR, (caddr_t)&ifra, ifp);
		}
	    }
	}

	return EOPNOTSUPP;	/*just for safety*/
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(ifp, ia)
	register struct ifnet *ifp;
	register struct in_ifaddr *ia;
{

	in_scrubprefix(ia);
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
in_ifinit(ifp, ia, sin, scrub)
	register struct ifnet *ifp;
	register struct in_ifaddr *ia;
	struct sockaddr_in *sin;
	int scrub;
{
	register u_int32_t i = sin->sin_addr.s_addr;
	struct sockaddr_in oldaddr;
	int s = splimp(), flags = RTF_UP, error;

	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin;
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		ia->ia_addr = oldaddr;
		splx(s);
		return (error);
	}
	splx(s);
	if (scrub) {
		ia->ia_ifa.ifa_addr = sintosa(&oldaddr);
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = sintosa(&ia->ia_addr);
	}
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = ia->ia_subnetmask;
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
			ia->ia_subnet | ~ia->ia_subnetmask;
		ia->ia_netbroadcast.s_addr =
			ia->ia_net | ~ia->ia_netmask;
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			return (0);
		flags |= RTF_HOST;
	}
	error = in_addprefix(ia, flags);
	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in_addr addr;

		addr.s_addr = INADDR_ALLHOSTS_GROUP;
#ifdef IGMPV3
		in_addmulti(&addr, ifp, 0, NULL, MCAST_EXCLUDE, 0, &error);
#else
		in_addmulti(&addr, ifp);
#endif
	}
	return (error);
}

#define rtinitflags(x) \
	((((x)->ia_ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) != 0) \
	    ? RTF_HOST : 0)

/*
 * add a route to prefix ("connected route" in cisco terminology).
 * does nothing if there's some interface address with the same prefix already.
 */
static int
in_addprefix(target, flags)
	struct in_ifaddr *target;
	int flags;
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p;
	int error;

	if ((flags & RTF_HOST) != 0)
		prefix = target->ia_dstaddr.sin_addr;
	else
		prefix = target->ia_addr.sin_addr;
	mask = target->ia_sockmask.sin_addr;
	prefix.s_addr &= mask.s_addr;

	for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next) {
		/* easy one first */
		if (mask.s_addr != ia->ia_sockmask.sin_addr.s_addr)
			continue;

		if (rtinitflags(ia))
			p = ia->ia_dstaddr.sin_addr;
		else
			p = ia->ia_addr.sin_addr;
		p.s_addr &= ia->ia_sockmask.sin_addr.s_addr;
		if (prefix.s_addr != p.s_addr)
			continue;

		/*
		 * if we got a matching prefix route inserted by other
		 * interface adderss, we don't need to bother
		 */
		if (ia->ia_flags & IFA_ROUTE)
			return 0;
	}

	/*
	 * noone seem to have prefix route.  insert it.
	 */
	error = rtinit(&target->ia_ifa, (int)RTM_ADD, flags);
	if (!error)
		target->ia_flags |= IFA_ROUTE;
	return error;
}

/*
 * remove a route to prefix ("connected route" in cisco terminology).
 * re-installs the route by using another interface address, if there's one
 * with the same prefix (otherwise we lose the route mistakenly).
 */
static int
in_scrubprefix(target)
	struct in_ifaddr *target;
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p;
	int error;

	if ((target->ia_flags & IFA_ROUTE) == 0)
		return 0;

	if (rtinitflags(target))
		prefix = target->ia_dstaddr.sin_addr;
	else
		prefix = target->ia_addr.sin_addr;
	mask = target->ia_sockmask.sin_addr;
	prefix.s_addr &= mask.s_addr;

	for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next) {
		/* easy one first */
		if (mask.s_addr != ia->ia_sockmask.sin_addr.s_addr)
			continue;

		if (rtinitflags(ia))
			p = ia->ia_dstaddr.sin_addr;
		else
			p = ia->ia_addr.sin_addr;
		p.s_addr &= ia->ia_sockmask.sin_addr.s_addr;
		if (prefix.s_addr != p.s_addr)
			continue;

		/*
		 * if we got a matching prefix route, move IFA_ROUTE to him
		 */
		if ((ia->ia_flags & IFA_ROUTE) == 0) {
			rtinit(&(target->ia_ifa), (int)RTM_DELETE,
			    rtinitflags(target));
			target->ia_flags &= ~IFA_ROUTE;

			error = rtinit(&ia->ia_ifa, (int)RTM_ADD,
			    rtinitflags(ia) | RTF_UP);
			if (error == 0)
				ia->ia_flags |= IFA_ROUTE;
			return error;
		}
	}

	/*
	 * noone seem to have prefix route.  remove it.
	 */
	rtinit(&(target->ia_ifa), (int)RTM_DELETE, rtinitflags(target));
	target->ia_flags &= ~IFA_ROUTE;
	return 0;
}

#undef rtinitflags

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(in, ifp)
	struct in_addr in;
	struct ifnet *ifp;
{
	struct ifnet *ifn, *if_first, *if_target;
	register struct ifaddr *ifa;

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;

	if (ifp == NULL) {
	  	if_first = ifnet.tqh_first;
		if_target = 0;
	} else {
		if_first = ifp;
		if_target = ifp->if_list.tqe_next;
	}

#define ia (ifatoia(ifa))
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 * If ifp is NULL, check against all the interfaces.
	 */
        for (ifn = if_first; ifn != if_target; ifn = ifn->if_list.tqe_next) {
		if ((ifn->if_flags & IFF_BROADCAST) == 0)
			continue;
		for (ifa = ifn->if_addrlist.tqh_first; ifa;
		    ifa = ifa->ifa_list.tqe_next)
			if (ifa->ifa_addr->sa_family == AF_INET &&
			    in.s_addr != ia->ia_addr.sin_addr.s_addr &&
			    (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
			     in.s_addr == ia->ia_netbroadcast.s_addr ||
			     (hostzeroisbroadcast &&
			      /*
			       * Check for old-style (host 0) broadcast.
			       */
			      (in.s_addr == ia->ia_subnet ||
			       in.s_addr == ia->ia_net))))
				return 1;
	}
	return (0);
#undef ia
}

/*
 * Add an address to the list of IP multicast addresses for a given interface.
 */
struct in_multi *
#ifdef IGMPV3
in_addmulti(ap, ifp, numsrc, ss, mode, init, error)
#else
in_addmulti(ap, ifp)
#endif
	register struct in_addr *ap;
	register struct ifnet *ifp;
#ifdef IGMPV3
	u_int16_t numsrc;
	struct sockaddr_storage *ss;
	u_int mode;			/* requested filter mode by socket */
	int init;			/* indicate initial join by socket */
	int *error;			/* return code of each sub routine */
#endif
{
	register struct in_multi *inm;
	struct ifreq ifr;
	struct in_ifaddr *ia;
#ifdef IGMPV3
	struct mbuf *m = NULL;
	struct ias_head *newhead = NULL;/* this may become new ims_cur->head */
	u_int curmode;			/* current filter mode */
	u_int newmode;			/* newly calculated filter mode */
	u_int16_t curnumsrc;		/* current ims_cur->numsrc */
	u_int16_t newnumsrc;		/* new ims_cur->numsrc */
	int timer_init = 1;		/* indicate timer initialization */
	int buflen = 0;
	u_int8_t type = 0;		/* State-Change report type */
	struct router_info *rti;
#else
	int dummy;			/* dummy */
	int *error = &dummy;		/* dummy */
#endif
	int s = splsoftnet();

	/*
	 * See if address already in list.
	 */
	IN_LOOKUP_MULTI(*ap, ifp, inm);

#ifdef IGMPV3
	/*
	 * MCAST_INCLUDE with empty source list means (*,G) leave.
	 */
	if ((mode == MCAST_INCLUDE) && (numsrc == 0)) {
	    *error = EINVAL;
	    splx(s);
	    return NULL;
	}
#endif

	if (inm != NULL) {
#ifdef IGMPV3
	    /*
	     * Found it; merge source addresses in inm_source and send
	     * State-Change Report if needed, and increment the reference
	     * count. just increment the refrence count if group address
	     * is local.
	     */
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		++inm->inm_refcount;
		splx(s);
		return inm;
	    }

	    /* inm_source is already allocated. */
	    curmode = inm->inm_source->ims_mode;
	    curnumsrc= inm->inm_source->ims_cur->numsrc;
	    /*
	     * Add each source address to inm_source and get new source
	     * filter mode and its calculated source list.
	     */
	    if ((*error = in_addmultisrc(inm, numsrc, ss, mode, init,
				&newhead, &newmode, &newnumsrc)) != 0) {
		splx(s);
		return NULL;
	    }
	    if (newhead != NULL) {
 		/*
		 * Merge new source list to current pending report's source
		 * list.
		 */
		if ((*error = in_merge_msf_state
				(inm, newhead, newmode, newnumsrc)) > 0) {
		    /* State-Change Report will not be sent. Just return
		     * immediately. */
		    /* Each ias linked from newhead is used by new curhead,
		     * so only newhead is freed. */
		    FREE(newhead, M_MSFILTER);
		    *error = 0; /* to make caller behave as normal */
		    splx(s);
		    return inm;
		}
	    } else {
		/* Only newhead was merged in a former function. */
		inm->inm_source->ims_mode = newmode;
		inm->inm_source->ims_cur->numsrc = newnumsrc;
	    }

	    /*
	     * Let IGMP know that we have joined an IP multicast group with
	     * source list if upstream router is IGMPv3 capable.
	     * If there was no pending source list change, an ALLOW or a
	     * BLOCK State-Change Report will not be sent, but a TO_IN or a
	     * TO_EX State-Change Report will be sent in any case.
	     */
	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode || curnumsrc != newnumsrc) {
			if (curmode != newmode) {
			    if (newmode == MCAST_INCLUDE)
				type = CHANGE_TO_INCLUDE_MODE;
			    else
				type = CHANGE_TO_EXCLUDE_MODE;
			    }
			    igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
		}
	    } else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 * Otherwise, ALW or BLK record will be blocked or pending
		 * list will never be cleaned when upstream router switches
		 * to IGMPv3. XXX
		 */
		in_clear_all_pending_report(inm);
	    }
	    *error = 0;
	    /*
	     * If this is an initial join request by the socket, increase
	     * refcount.
	     */
	    if (init)
#endif
		++inm->inm_refcount;
	} else {
	    /*
	     * New address; allocate a new multicast record and link it into
	     * the interface's multicast list.
	     */
	    inm = (struct in_multi *)malloc(sizeof(*inm), M_IPMADDR, M_NOWAIT);
	    if (inm == NULL) {
		*error = ENOBUFS;
		splx(s);
		return (NULL);
	    }
	    inm->inm_addr = *ap;
	    inm->inm_ifp = ifp;
	    inm->inm_refcount = 1;
	    inm->inm_timer = 0;
	    IFP_TO_IA(ifp, ia);
	    if (ia == NULL) {
		free(inm, M_IPMADDR);
		*error = ENOBUFS /*???*/;
		splx(s);
		return (NULL);
	    }
	    inm->inm_ia = ia;
	    IFAREF(&inm->inm_ia->ia_ifa);
	    LIST_INSERT_HEAD(&ia->ia_multiaddrs, inm, inm_list);
	    /*
	     * Ask the network driver to update its multicast reception filter
	     * appropriately for the new address.
	     */
	    satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
	    satosin(&ifr.ifr_addr)->sin_family = AF_INET;
	    satosin(&ifr.ifr_addr)->sin_addr = *ap;
	    if ((ifp->if_ioctl == NULL) ||
		(*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr) != 0) {
		LIST_REMOVE(inm, inm_list);
		free(inm, M_IPMADDR);
		*error = EINVAL /*???*/;
		splx(s);
		return (NULL);
	    }

#ifdef IGMPV3
	    for (rti = rti_head; rti != 0; rti = rti->rti_next) {
		if (rti->rti_ifp == inm->inm_ifp) {
		    inm->inm_rti = rti;
		    break;
 		}
	    }
	    if (rti == 0) {
		if ((rti = rti_init(inm->inm_ifp)) == NULL) {
		    LIST_REMOVE(inm, inm_list);
		    free(inm, M_IPMADDR);
		    *error = ENOBUFS;
		    splx(s);
		    return NULL;
	    	} else
		    inm->inm_rti = rti;
	    }

	    inm->inm_source = NULL;
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		splx(s);
		return inm;
	    }

	    if ((*error = in_addmultisrc(inm, numsrc, ss, mode, init,
					&newhead, &newmode, &newnumsrc)) != 0) {
		in_free_all_msf_source_list(inm);
		LIST_REMOVE(inm, inm_list);
		free(inm, M_IPMADDR);
		splx(s);
		return NULL;
	    }
	    /* Only newhead was merged in a former function. */
	    curmode = inm->inm_source->ims_mode;
	    inm->inm_source->ims_mode = newmode;
	    inm->inm_source->ims_cur->numsrc = newnumsrc;

	    /*
	     * Let IGMP know that we have joined a new IP multicast group
	     * with source list if upstream router is IGMPv3 capable.
	     * If the router doesn't speak IGMPv3, then send Report message
	     * with no source address since it is a first join request.
	     */
	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode) {
		    if (newmode == MCAST_INCLUDE)
			type = CHANGE_TO_INCLUDE_MODE; /* never happen? */
		    else
			type = CHANGE_TO_EXCLUDE_MODE;
		}
		igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
	    } else {
		/*
		 * Let IGMP know that we have joined a new IP multicast group.
		 * If MSF's pending records exist, they must be deleted.
		 */
		in_clear_all_pending_report(inm);
		igmp_joingroup(inm);
	    }
	    *error = 0;
#else
	    igmp_joingroup(inm);
#endif /* IGMPV3 */
		igmp_joingroup(inm);
	}
#ifdef IGMPV3
	if (newhead != NULL)
		/* Each ias is linked from new curhead, so only newhead (not
		 * ias_list) is freed */
		FREE(newhead, M_MSFILTER);
#endif

	splx(s);
	return (inm);
}

/*
 * Delete a multicast address record.
 */
void
#ifdef IGMPV3
in_delmulti(inm, numsrc, ss, mode, final, error)
#else
in_delmulti(inm)
#endif
	register struct in_multi *inm;
#ifdef IGMPV3
	u_int16_t numsrc;
	struct sockaddr_storage *ss;
	u_int mode;			/* requested filter mode by socket */
	int final;			/* indicate complete leave by socket */
	int *error;			/* return code of each sub routine */
#endif
{
	struct ifreq ifr;
#ifdef IGMPV3
	struct mbuf *m = NULL;
	struct ias_head *newhead = NULL;/* this may become new ims_cur->head */
	u_int curmode;			/* current filter mode */
	u_int newmode;			/* newly calculated filter mode */
	u_int16_t curnumsrc;		/* current ims_cur->numsrc */
	u_int16_t newnumsrc;		/* new ims_cur->numsrc */
	int timer_init = 1;		/* indicate timer initialization */
	int buflen = 0;
	u_int8_t type = 0;		/* State-Change report type */
#endif

	int s = splsoftnet();

#ifdef IGMPV3
	if ((mode == MCAST_INCLUDE) && (numsrc == 0)) {
		*error = EINVAL;
		splx(s);
		return;
	}
	if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		if (--inm->inm_refcount == 0) {
			/*
			 * Unlink from list.
			 */
			LIST_REMOVE(inm, inm_list);
			IFAFREE(&inm->inm_ia->ia_ifa);
			/*
			 * Notify the network driver to update its multicast
			 * reception filter.
			 */
			satosin(&ifr.ifr_addr)->sin_family = AF_INET;
			satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
			(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
							     (caddr_t)&ifr);
			free(inm, M_IPMADDR);
		}
		splx(s);
		return;
	}

	curmode = inm->inm_source->ims_mode;
	curnumsrc = inm->inm_source->ims_cur->numsrc;
	/*
	 * Delete each source address from inm_source and get new source
	 * filter mode and its calculated source list, and send State-Change
	 * Report if needed.
	 */
	if ((*error = in_delmultisrc(inm, numsrc, ss, mode, final,
				&newhead, &newmode, &newnumsrc)) != 0) {
		splx(s);
		return;
	}
	if (newhead != NULL) {
		if ((*error = in_merge_msf_state
				(inm, newhead, newmode, newnumsrc)) > 0) {
			/* State-Change Report will not be sent. Just return 
			 * immediately. */
			FREE(newhead, M_MSFILTER);
			splx(s);
			return;
		}
	} else {
		/* Only newhead was merged in a former function. */
		inm->inm_source->ims_mode = newmode;
		inm->inm_source->ims_cur->numsrc = newnumsrc;
	}

	/*
	 * If this is a final leave request by the socket, decrease
	 * refcount.
	 */
	if (final)
		--inm->inm_refcount;

	if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode || curnumsrc != newnumsrc) {
			if (curmode != newmode) {
				if (newmode == MCAST_INCLUDE)
					type = CHANGE_TO_INCLUDE_MODE;
				else
					type = CHANGE_TO_EXCLUDE_MODE;
			}
			igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
		}
	} else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 * Otherwise, ALW or BLK record will be blocked or pending
		 * list will never be cleaned when upstream router switches
		 * to IGMPv3. XXX
		 */
		in_clear_all_pending_report(inm);
		if (inm->inm_refcount == 0) {
			inm->inm_source->ims_robvar = 0;
			igmp_leavegroup(inm);
		}
	}

	if (inm->inm_refcount == 0) {
		/*
		 * We cannot use timer for robstness times report
		 * transmission when inm_refcount becomes 0, since inm
		 * itself will be removed here. So, in this case, report
		 * retransmission will be done quickly. XXX my spec.
		 */
		timer_init = 0;
		while (inm->inm_source->ims_robvar > 0) {
			m = NULL;
			buflen = 0;
			igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
			if (m != NULL)
				igmp_sendbuf(m, inm->inm_ifp);
		}
		in_free_all_msf_source_list(inm);
		LIST_REMOVE(inm, inm_list);
		IFAFREE(&inm->inm_ia->ia_ifa);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
		(*inm->inm_ifp->if_ioctl)
				(inm->inm_ifp, SIOCDELMULTI, (caddr_t)&ifr);
		free(inm, M_IPMADDR);
	}
	*error = 0;
	if (newhead != NULL)
		FREE(newhead, M_MSFILTER);
#else
	if (--inm->inm_refcount == 0) {
		/*
		 * No remaining claims to this record; let IGMP know that
		 * we are leaving the multicast group.
		 */
		igmp_leavegroup(inm);
		/*
		 * Unlink from list.
		 */
		LIST_REMOVE(inm, inm_list);
		/*
		 * Notify the network driver to update its multicast reception
		 * filter.
		 */
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
		(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
							     (caddr_t)&ifr);
		free(inm, M_IPMADDR);
	}
#endif

	splx(s);
}

#ifdef IGMPV3
/*
 * Add an address to the list of IP multicast addresses for a given interface.
 * Add source addresses to the list also, if upstream router is IGMPv3 capable
 * and the number of source is not 0.
 */
struct in_multi *
in_modmulti(ap, ifp, numsrc, ss, mode,
		old_num, old_ss, old_mode, init, grpjoin, error)
	struct in_addr *ap;
	struct ifnet *ifp;
	u_int16_t numsrc, old_num;
	struct sockaddr_storage *ss, *old_ss;
	u_int mode, old_mode;		/* requested/current filter mode */
	int init;			/* indicate initial join by socket */
	u_int grpjoin;			/* on/off of (*,G) join by socket */
	int *error;			/* return code of each sub routine */
{
	struct mbuf *m = NULL;
	struct in_multi *inm;
	struct ifreq ifr;
	struct in_ifaddr *ia;
	struct ias_head *newhead = NULL;/* this becomes new ims_cur->head */
	u_int curmode;			/* current filter mode */
	u_int newmode;			/* newly calculated filter mode */
	u_int16_t curnumsrc;		/* current ims_cur->numsrc */
	u_int16_t newnumsrc;		/* new ims_cur->numsrc */
	int timer_init = 1;		/* indicate timer initialization */
	int buflen = 0;
	u_int8_t type = 0;		/* State-Change report type */
	struct router_info *rti;
	int s;

	*error = 0; /* initialize */

	if ((mode != MCAST_INCLUDE && mode != MCAST_EXCLUDE) ||
		(old_mode != MCAST_INCLUDE && old_mode != MCAST_EXCLUDE)) {
	    *error = EINVAL;
	    return NULL;
	}

	s = splsoftnet();

	/*
	 * See if address already in list.
	 */
	IN_LOOKUP_MULTI(*ap, ifp, inm);

	if (inm != NULL) {
	    /*
	     * If requested multicast address is local address, update
	     * the condition, join or leave, based on a requested filter.
	     */
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		if (numsrc != 0) {
		    splx(s);
		    *error = EINVAL;
		    return NULL; /* source filter is not supported for
				    local group address. */
		}
		if (mode == MCAST_INCLUDE) {
		    if (--inm->inm_refcount == 0) {
			/*
			 * Unlink from list.
			 */
			LIST_REMOVE(inm, inm_list);
			IFAFREE(&inm->inm_ia->ia_ifa);
			/*
			 * Notify the network driver to update its multicast
			 * reception filter.
			 */
			satosin(&ifr.ifr_addr)->sin_family = AF_INET;
			satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
			(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
								(caddr_t)&ifr);
			free(inm, M_IPMADDR);
		    }
		    splx(s);
		    return NULL; /* not an error! */
		} else if (mode == MCAST_EXCLUDE) {
		    ++inm->inm_refcount;
		    splx(s);
		    return inm;
		}
	    }

	    curmode = inm->inm_source->ims_mode;
	    curnumsrc = inm->inm_source->ims_cur->numsrc;
	    if ((*error = in_modmultisrc(inm, numsrc, ss, mode,
					old_num, old_ss, old_mode, grpjoin,
					&newhead, &newmode, &newnumsrc)) != 0) {
		splx(s);
		return NULL;
	    }
	    if (newhead != NULL) {
		/*
		 * Merge new source list to current pending report's source
		 * list.
		 */
		if ((*error = in_merge_msf_state
				(inm, newhead, newmode, newnumsrc)) > 0) {
		    /* State-Change Report will not be sent. Just return
		     * immediately. */
		    FREE(newhead, M_MSFILTER);
		    splx(s);
		    return inm;
		}
	    } else {
		/* Only newhead was merged. */
		inm->inm_source->ims_mode = newmode;
		inm->inm_source->ims_cur->numsrc = newnumsrc;
	    }

	    /*
	     * Let IGMP know that we have joined an IP multicast group with
	     * source list if upstream router is IGMPv3 capable.
	     * If there was no pending source list change, an ALLOW or a
	     * BLOCK State-Change Report will not be sent, but a TO_IN or a
	     * TO_EX State-Change Report will be sent in any case.
	     */
	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode || curnumsrc != newnumsrc || old_num) {
			if (curmode != newmode) {
			    if (newmode == MCAST_INCLUDE)
				type = CHANGE_TO_INCLUDE_MODE;
			    else
				type = CHANGE_TO_EXCLUDE_MODE;
			}
			igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
		}
	    } else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 */
		in_clear_all_pending_report(inm);
	    }
	    *error = 0;
	    /* for this group address, initial join request by the socket. */
	    if (init)
		++inm->inm_refcount;

	} else {
	    /*
	     * If there is some sources to be deleted, or if the request is
	     * join a local group address with some filtered address, return.
	     */
	    if ((old_num != 0) ||
	    		(IN_LOCAL_GROUP(ap->s_addr) && numsrc != 0)) {
		*error = EINVAL;
		splx(s);
		return NULL;
	    }

	    /*
	     * New address; allocate a new multicast record and link it into
	     * the interface's multicast list.
	     */
	    inm = (struct in_multi *)malloc(sizeof(*inm), M_IPMADDR, M_NOWAIT);
	    if (inm == NULL) {
		*error = ENOBUFS;
		splx(s);
		return NULL;
	    }
	    inm->inm_addr = *ap;
	    inm->inm_ifp = ifp;
	    inm->inm_refcount = 1;
	    inm->inm_timer = 0;
	    IFP_TO_IA(ifp, ia);
	    if (ia == NULL) {
		free(inm, M_IPMADDR);
		*error = ENOBUFS /*???*/;
		splx(s);
		return NULL;
	    }
	    inm->inm_ia = ia;
	    IFAREF(&inm->inm_ia->ia_ifa);
	    LIST_INSERT_HEAD(&ia->ia_multiaddrs, inm, inm_list);
	    /*
	     * Ask the network driver to update its multicast reception filter
	     * appropriately for the new address.
	     */
	    satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
	    satosin(&ifr.ifr_addr)->sin_family = AF_INET;
	    satosin(&ifr.ifr_addr)->sin_addr = *ap;
	    if ((ifp->if_ioctl == NULL) ||
		(*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)&ifr) != 0) {
		LIST_REMOVE(inm, inm_list);
		free(inm, M_IPMADDR);
		*error = EINVAL /*???*/;
		splx(s);
		return NULL;
	    }

	    for (rti = rti_head; rti != 0; rti = rti->rti_next) {
		if (rti->rti_ifp == inm->inm_ifp) {
		    inm->inm_rti = rti;
		    break;
		}
	    }
	    if (rti == 0) {
		if ((rti = rti_init(inm->inm_ifp)) == NULL) {
		    LIST_REMOVE(inm, inm_list);
		    free(inm, M_IPMADDR);
		    *error = ENOBUFS;
		    splx(s);
		    return NULL;
	    	} else
		    inm->inm_rti = rti;
	    }

	    inm->inm_source = NULL;
	    if (IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
		splx(s);
		return inm;
	    }

	    if ((*error = in_modmultisrc(inm, numsrc, ss, mode, 0, NULL,
					MCAST_INCLUDE, grpjoin, &newhead,
					&newmode, &newnumsrc)) != 0) {
		in_free_all_msf_source_list(inm);
		LIST_REMOVE(inm, inm_list);
		free(inm, M_IPMADDR);
		splx(s);
		return NULL;
	    }
	    /* Only newhead was merged in a former function. */
	    curmode = inm->inm_source->ims_mode;
	    inm->inm_source->ims_mode = newmode;
	    inm->inm_source->ims_cur->numsrc = newnumsrc;

	    if (inm->inm_rti->rti_type == IGMP_v3_ROUTER) {
		if (curmode != newmode) {
		    if (newmode == MCAST_INCLUDE)
			type = CHANGE_TO_INCLUDE_MODE;/* never happen??? */
		    else
			type = CHANGE_TO_EXCLUDE_MODE;
		}
		igmp_send_state_change_report
				(&m, &buflen, inm, type, timer_init);
	    } else {
		/*
		 * If MSF's pending records exist, they must be deleted.
		 */
		in_clear_all_pending_report(inm);
		igmp_joingroup(inm);
	    }
	    *error = 0;
	}
	if (newhead != NULL)
	    FREE(newhead, M_MSFILTER);

	splx(s);
	return inm;
}
#endif /* IGMPV3 */
#endif /* INET */
