/*	$KAME: mld6.c,v 1.38 2002/01/31 14:14:53 jinmei Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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
 * Copyright (c) 1988 Stephen Deering.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	@(#)igmp.c	8.1 (Berkeley) 7/19/93
 */

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include "opt_inet.h"
#include "opt_inet6.h"
#endif
#ifdef __NetBSD__
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/syslog.h>
#ifdef __OpenBSD__
#include <dev/rndvar.h>
#endif

#include <net/if.h>
#ifdef NEW_STRUCT_ROUTE
#include <net/route.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/mld6_var.h>

#include <net/net_osdep.h>

/*
 * Protocol constants
 */

/* denotes that the MLD max response delay field specifies time in milliseconds */
#define MLD6_TIMER_SCALE	1000
/*
 * time between repetitions of a node's initial report of interest in a
 * multicast address(in seconds)
 */
#define MLD6_UNSOLICITED_REPORT_INTERVAL	10

static struct ip6_pktopts ip6_opts;
static int mld6_timers_are_running;
/* XXX: These are necessary for KAME's link-local hack */
static struct in6_addr all_nodes_linklocal = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
static struct sockaddr_in6 all_routers_linklocal;

static void mld6_sendpkt __P((struct in6_multi *, int,
			      const struct sockaddr_in6 *));

void
mld6_init()
{
	static u_int8_t hbh_buf[8];
	struct ip6_hbh *hbh = (struct ip6_hbh *)hbh_buf;
	u_int16_t rtalert_code = htons((u_int16_t)IP6OPT_RTALERT_MLD);

	mld6_timers_are_running = 0;

	/* ip6h_nxt will be fill in later */
	hbh->ip6h_len = 0;	/* (8 >> 3) - 1 */

	/* XXX: grotty hard coding... */
	hbh_buf[2] = IP6OPT_PADN;	/* 2 byte padding */
	hbh_buf[3] = 0;
	hbh_buf[4] = IP6OPT_RTALERT;
	hbh_buf[5] = IP6OPT_RTALERT_LEN - 2;
	bcopy((caddr_t)&rtalert_code, &hbh_buf[6], sizeof(u_int16_t));

	all_routers_linklocal.sin6_family = AF_INET6;
	all_routers_linklocal.sin6_len = sizeof(struct sockaddr_in6);
	all_routers_linklocal.sin6_addr = in6addr_linklocal_allrouters;

	init_ip6pktopts(&ip6_opts);
	ip6_opts.ip6po_hbh = hbh;
}

void
mld6_start_listening(in6m)
	struct in6_multi *in6m;
{
#ifdef __NetBSD__
	int s = splsoftnet();
#else
	int s = splnet();
#endif

	/*
	 * RFC2710 page 10:
	 * The node never sends a Report or Done for the link-scope all-nodes
	 * address.
	 * MLD messages are never sent for multicast addresses whose scope is 0
	 * (reserved) or 1 (node-local).
	 */
	/* XXX */
	all_nodes_linklocal.s6_addr16[1] = htons(in6m->in6m_ifp->if_index);
	if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr, &all_nodes_linklocal) ||
	    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) < IPV6_ADDR_SCOPE_LINKLOCAL) {
		in6m->in6m_timer = 0;
		in6m->in6m_state = MLD6_OTHERLISTENER;
	} else {
		mld6_sendpkt(in6m, MLD_LISTENER_REPORT, NULL);
		in6m->in6m_timer = MLD6_RANDOM_DELAY(
			MLD6_UNSOLICITED_REPORT_INTERVAL * PR_FASTHZ);
		in6m->in6m_state = MLD6_IREPORTEDLAST;
		mld6_timers_are_running = 1;
	}
	splx(s);
}

void
mld6_stop_listening(in6m)
	struct in6_multi *in6m;
{
	/* XXX */
	all_nodes_linklocal.s6_addr16[1] = htons(in6m->in6m_ifp->if_index);
		
	/* XXX: necessary when mrouting */
	if (in6_addr2zoneid(in6m->in6m_ifp,
			    &all_routers_linklocal.sin6_addr,
			    &all_routers_linklocal.sin6_scope_id)) {
		/* XXX impossible */
		return;
	}
	if (in6_embedscope(&all_routers_linklocal.sin6_addr,
			   &all_routers_linklocal)) {
		/* XXX impossible */
		return;
	}

	if (in6m->in6m_state == MLD6_IREPORTEDLAST &&
	    (!IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr,
				 &all_nodes_linklocal)) &&
	    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) >
	    IPV6_ADDR_SCOPE_INTFACELOCAL) {
		mld6_sendpkt(in6m, MLD_LISTENER_DONE, &all_routers_linklocal);
	}
}

void
mld6_input(m, off)
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct mld_hdr *mldh;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct in6_multi *in6m;
	struct in6_ifaddr *ia;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct ifmultiaddr *ifma;
#endif
	int timer;		/* timer value in the MLD query header */

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, sizeof(*mldh),);
	mldh = (struct mld_hdr *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(mldh, struct mld_hdr *, m, off, sizeof(*mldh));
	if (mldh == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}
#endif

	/* source address validation */
	ip6 = mtod(m, struct ip6_hdr *); /* in case mpullup */
	if (!(IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_src) ||
	      IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src))) {
#if 0				/* do not log in an input path */
		log(LOG_INFO,
		    "mld6_input: src %s is not link-local (grp=%s)\n",
		    ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&mldh->mld_addr));
#endif
		/*
		 * spec (RFC2710) does not explicitly
		 * specify to discard the packet from a non link-local
		 * source address. But we believe it's expected to do so.
		 */
		m_freem(m);
		return;
	}

	/*
	 * In the MLD6 specification, there are 3 states and a flag.
	 *
	 * In Non-Listener state, we simply don't have a membership record.
	 * In Delaying Listener state, our timer is running (in6m->in6m_timer)
	 * In Idle Listener state, our timer is not running (in6m->in6m_timer==0)
	 *
	 * The flag is in6m->in6m_state, it is set to MLD6_OTHERLISTENER if
	 * we have heard a report from another member, or MLD6_IREPORTEDLAST
	 * if we sent the last report.
	 */
	switch (mldh->mld_type) {
	case MLD_LISTENER_QUERY:
		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN6_IS_ADDR_UNSPECIFIED(&mldh->mld_addr) &&
		    !IN6_IS_ADDR_MULTICAST(&mldh->mld_addr))
			break;	/* print error or log stat? */
		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] =
				htons(ifp->if_index); /* XXX */

		/*
		 * - Start the timers in all of our membership records
		 *   that the query applies to for the interface on
		 *   which the query arrived excl. those that belong
		 *   to the "all-nodes" group (ff02::1).
		 * - Restart any timer that is already running but has
		 *   A value longer than the requested timeout.
		 * - Use the value specified in the query message as
		 *   the maximum timeout.
		 */
		IFP_TO_IA6(ifp, ia);
		if (ia == NULL)
			break;

		/*
		 * XXX: System timer resolution is too low to handle Max
		 * Response Delay, so set 1 to the internal timer even if
		 * the calculated value equals to zero when Max Response
		 * Delay is positive.
		 */
		timer = ntohs(mldh->mld_maxdelay)*PR_FASTHZ/MLD6_TIMER_SCALE;
		if (timer == 0 && mldh->mld_maxdelay)
			timer = 1;
		all_nodes_linklocal.s6_addr16[1] =
			htons(ifp->if_index); /* XXX */
		
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
#else
		for (in6m = ia->ia6_multiaddrs.lh_first;
		     in6m;
		     in6m = in6m->in6m_entry.le_next)
#endif
		{
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
			if (ifma->ifma_addr->sa_family != AF_INET6)
				continue;
			in6m = (struct in6_multi *)ifma->ifma_protospec;
			if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr,
					       &all_nodes_linklocal) ||
			    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) <
			    IPV6_ADDR_SCOPE_LINKLOCAL)
				continue;
#else
			if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr,
						&all_nodes_linklocal) ||
			    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) <
			    IPV6_ADDR_SCOPE_LINKLOCAL)
				continue;
#endif

			if (IN6_IS_ADDR_UNSPECIFIED(&mldh->mld_addr) ||
			    IN6_ARE_ADDR_EQUAL(&mldh->mld_addr,
						&in6m->in6m_addr))
			{
				if (timer == 0) {
					/* send a report immediately */
					mld6_sendpkt(in6m, MLD_LISTENER_REPORT,
						NULL);
					in6m->in6m_timer = 0; /* reset timer */
					in6m->in6m_state = MLD6_IREPORTEDLAST;
				}
				else if (in6m->in6m_timer == 0 || /* idle */
					in6m->in6m_timer > timer) {
					in6m->in6m_timer =
						MLD6_RANDOM_DELAY(timer);
					mld6_timers_are_running = 1;
				}
			}
		}

		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] = 0; /* XXX */
		break;
	case MLD_LISTENER_REPORT:
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * can potentially get looped back if we are a multicast
		 * router, so discard reports sourced by me.
		 * Note that it is impossible to check IFF_LOOPBACK flag of
		 * ifp for this purpose, since ip6_mloopback pass the physical
		 * interface to looutput.
		 */
		if (m->m_flags & M_LOOP) /* XXX: grotty flag, but efficient */
			break;

		if (!IN6_IS_ADDR_MULTICAST(&mldh->mld_addr))
			break;

		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] =
				htons(ifp->if_index); /* XXX */
		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN6_LOOKUP_MULTI(mldh->mld_addr, ifp, in6m);
		if (in6m) {
			in6m->in6m_timer = 0; /* transit to idle state */
			in6m->in6m_state = MLD6_OTHERLISTENER; /* clear flag */
		}

		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] = 0; /* XXX */
		break;
	default:
#if 0
		/*
		 * this case should be impossible because of filtering in
		 * icmp6_input().  But we explicitly disabled this part
		 * just in case.
		 */
		log(LOG_ERR, "mld6_input: illegal type(%d)", mldh->mld_type);
#endif
		break;
	}

	m_freem(m);
}

void
mld6_fasttimeo()
{
	struct in6_multi *in6m;
	struct in6_multistep step;
	int s;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!mld6_timers_are_running)
		return;

#ifdef __NetBSD__
	s = splsoftnet();
#else
	s = splnet();
#endif
	mld6_timers_are_running = 0;
	IN6_FIRST_MULTI(step, in6m);
	while (in6m != NULL) {
		if (in6m->in6m_timer == 0) {
			/* do nothing */
		} else if (--in6m->in6m_timer == 0) {
			mld6_sendpkt(in6m, MLD_LISTENER_REPORT, NULL);
			in6m->in6m_state = MLD6_IREPORTEDLAST;
		} else {
			mld6_timers_are_running = 1;
		}
		IN6_NEXT_MULTI(step, in6m);
	}
	splx(s);
}

static void
mld6_sendpkt(in6m, type, dst)
	struct in6_multi *in6m;
	int type;
	const struct sockaddr_in6 *dst;
{
	struct mbuf *mh, *md;
	struct mld_hdr *mldh;
	struct ip6_hdr *ip6;
	struct ip6_moptions im6o;
	struct in6_ifaddr *ia;
	struct ifnet *ifp = in6m->in6m_ifp;
	struct sockaddr_in6 src_sa, dst_sa;
	int ignflags;

	/*
	 * At first, find a link local address on the outgoing interface
	 * to use as the source address of the MLD packet.
	 * We do not reject tentative addresses for MLD report to deal with
	 * the case where we first join a link-local address.
	 */
	ignflags = (IN6_IFF_NOTREADY|IN6_IFF_ANYCAST) & ~IN6_IFF_TENTATIVE;
	if ((ia = in6ifa_ifpforlinklocal(ifp, ignflags)) == NULL)
		return;
	if ((ia->ia6_flags & IN6_IFF_TENTATIVE))
		ia = NULL;

	/*
	 * Allocate mbufs to store ip6 header and MLD header.
	 * We allocate 2 mbufs and make chain in advance because
	 * it is more convenient when inserting the hop-by-hop option later.
	 */
	MGETHDR(mh, M_DONTWAIT, MT_HEADER);
	if (mh == NULL)
		return;
	MGET(md, M_DONTWAIT, MT_DATA);
	if (md == NULL) {
		m_free(mh);
		return;
	}
	mh->m_next = md;

	mh->m_pkthdr.rcvif = NULL;
	mh->m_pkthdr.len = sizeof(struct ip6_hdr) + sizeof(struct mld_hdr);
	mh->m_len = sizeof(struct ip6_hdr);
	MH_ALIGN(mh, sizeof(struct ip6_hdr));

	/* fill in the ip6 header */
	ip6 = mtod(mh, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	/* ip6_plen will be set later */
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	/* ip6_hlim will be set by im6o.im6o_multicast_hlim */
	ip6->ip6_src = ia ? ia->ia_addr.sin6_addr : in6addr_any;
	ip6->ip6_dst = dst ? dst->sin6_addr : in6m->in6m_addr;
	/* set packet addresses in a full sockaddr_in6 form */
	bzero(&src_sa, sizeof(src_sa));
	bzero(&dst_sa, sizeof(dst_sa));
	src_sa.sin6_family = dst_sa.sin6_family = AF_INET6;
	src_sa.sin6_len = dst_sa.sin6_len = sizeof(struct sockaddr_in6);
	src_sa.sin6_addr = ip6->ip6_src;
	dst_sa.sin6_addr = ip6->ip6_dst;
	if (in6_addr2zoneid(ifp, &src_sa.sin6_addr, &src_sa.sin6_scope_id) ||
	    in6_addr2zoneid(ifp, &dst_sa.sin6_addr, &dst_sa.sin6_scope_id)) {
		/* XXX: impossible */
		m_free(mh);
		return;
	}
	if (!ip6_setpktaddrs(mh, &src_sa, &dst_sa)) {
		m_free(mh);
		return;
	}

	/* fill in the MLD header */
	md->m_len = sizeof(struct mld_hdr);
	mldh = mtod(md, struct mld_hdr *);
	mldh->mld_type = type;
	mldh->mld_code = 0;
	mldh->mld_cksum = 0;
	/* XXX: we assume the function will not be called for query messages */
	mldh->mld_maxdelay = 0;
	mldh->mld_reserved = 0;
	mldh->mld_addr = in6m->in6m_addr;
	in6_clearscope(&mldh->mld_addr); /* XXX */
	mldh->mld_cksum = in6_cksum(mh, IPPROTO_ICMPV6, sizeof(struct ip6_hdr),
				    sizeof(struct mld_hdr));

	/* construct multicast option */
	bzero(&im6o, sizeof(im6o));
	im6o.im6o_multicast_ifp = ifp;
	im6o.im6o_multicast_hlim = 1;

	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing daemon can hear it.
	 */
	im6o.im6o_multicast_loop = (ip6_mrouter != NULL);

	/* increment output statictics */
	icmp6stat.icp6s_outhist[type]++;
	icmp6_ifstat_inc(ifp, ifs6_out_msg);
	switch (type) {
	case MLD_LISTENER_QUERY:
		icmp6_ifstat_inc(ifp, ifs6_out_mldquery);
		break;
	case MLD_LISTENER_REPORT:
		icmp6_ifstat_inc(ifp, ifs6_out_mldreport);
		break;
	case MLD_LISTENER_DONE:
		icmp6_ifstat_inc(ifp, ifs6_out_mlddone);
		break;
	}

	ip6_output(mh, &ip6_opts, NULL, ia ? 0 : IPV6_UNSPECSRC, &im6o, NULL);
}
