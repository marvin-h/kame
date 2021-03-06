/*	$KAME: cnd.c,v 1.22 2007/02/17 12:11:42 t-momose Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <poll.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#ifdef __NetBSD__
#include <net/route.h>
#endif /* __NetBSD__ */
#include <netinet/in.h>
#ifndef __OpenBSD__
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#endif /* __OpenBSD__ */
#include <netinet/icmp6.h>
#include <netinet/ip6mh.h>
#include <netinet/ip6.h>
#ifndef __OpenBSD__
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#define TCPTIMERS
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#endif /* __OpenBSD__ */
#include <net/mipsock.h>
#include <netinet6/mip6.h>

#include "callout.h"
#include "stat.h"
#include "shisad.h"
#include "fdlist.h"
#include "command.h"
#include "config.h"

int main(int, char **);

static void cn_usage(char *);
static void cn_lists_init(void);

/*static void command_show_status(int, char *);*/
static void command_flush(int, char *);
static void terminate(int);

struct command_table show_command_table[] = {
	{"bc", command_show_bc, "binding cache"},
	{"kbc", command_show_kbc, "binding cache in kernel"},
	{"stat", command_show_stat, "statistics"},
	{"callout", show_callout_table, "show callout table "},
	{NULL}
};

struct command_table command_table[] = {
	{"show", NULL, "Show status", show_command_table},
	{"flush", command_flush, "Flush binding caches"},
};

/* Global Variables */
int mhsock, mipsock, icmp6sock;
struct mip6stat mip6stat;
int homeagent_mode = 0;

/* configuration parameters */
int debug = 0;
int foreground = 0;
int namelookup = 1;
int command_port = CND_COMMAND_PORT;
char *conffile = CND_CONFFILE;

static void
cn_usage(path)
	char *path;
{
	char *cmd;

	cmd = strrchr(path, '/');
	if (!cmd)
		cmd = path;
	else
		cmd++;
	fprintf(stderr, "%s [-fn] [-c configfile]\n", cmd);
} 


int
main(argc, argv)
	int argc;
	char **argv;
{
	int pfds;
	int ch = 0;
	FILE *pidfp;

	/* get options */
	while ((ch = getopt(argc, argv, "fnc:")) != -1) {
		switch (ch) {
		case 'f':
			foreground = 1;
			break;
		case 'n':
			namelookup = 0;
			break;
		case 'c':
			conffile = optarg;
			break;
		default:
			fprintf(stderr, "unknown option\n");
			cn_usage(argv[0]);
			exit(0);
			/* Not reach */
			break;
		}
	}

	/* parse configuration file and set default values. */
	if (parse_config(CFM_CND, conffile) == 0) {
		config_get_number(CFT_DEBUG, &debug, config_params);
		config_get_number(CFT_COMMANDPORT, &command_port,
		    config_params);
	}

	kernel_debug(debug);

	/* open syslog infomation. */
	openlog("shisad(cnd)", 0, LOG_DAEMON);
	syslog(LOG_INFO, "-- Start CN daemon at -- \n");

	/* open sockets */
	mhsock_open();
	mipsock_open();
	icmp6sock_open();

	/* start timer */
	shisad_callout_init();

	/* initialization */
	fdlist_init();
	if (command_init("cn> ", command_table, sizeof(command_table) / sizeof(struct command_table), command_port, NULL) < 0) {
		fprintf(stderr, "Unable to open user interface\n");
	}
	cn_lists_init();
	init_nonces();

	/* register signal handlers. */
	signal(SIGTERM, terminate);
	signal(SIGINT, terminate);

	if (foreground == 0)
		daemon(0, 0);

	/* dump current PID */
	if ((pidfp = fopen(CND_PIDFILE, "w")) != NULL) {
		fprintf(pidfp, "%d\n", getpid());
		fclose(pidfp);
	}

	new_fd_list(mhsock, POLLIN, mh_input_common);
        new_fd_list(mipsock, POLLIN, mipsock_input_common);
        new_fd_list(icmp6sock, POLLIN, icmp6_input_common);

	/* notify a kernel to behave as a correspondent node. */
	mipsock_nodetype_request(MIP6_NODETYPE_CORRESPONDENT_NODE, 1);

	while (1) {
		clear_revents();
	    
		if ((pfds = poll(fdl_fds, fdl_nfds, get_next_timeout())) < 0) {
			perror("poll");
			continue;
		}

		if (pfds != 0) {
			dispatch_fdfunctions(fdl_fds, fdl_nfds);
		}
		/* Timeout */
		callout_expire_check();
	}

	return (0);
}

static void
cn_lists_init(void)
{
	mip6_bc_init();
}

int
cn_receive_dst_unreach(icp)
	struct icmp6_hdr *icp;
{
	struct ip6_hdr *iip6;
	struct binding_cache *bc;
	struct ip6_rthdr2 *rth2;

	iip6 = (struct ip6_hdr *)(icp + 1);
	if ((rth2 = find_rthdr2(iip6)) == NULL)
		return (0);
	bc = mip6_bc_lookup((struct in6_addr *)(rth2 + 1), &iip6->ip6_src, 0);
	if (bc)  {
		mip6_bc_delete(bc);
		syslog(LOG_INFO, 
		       "binding for %s is deleted due to ICMP destunreach.",
		       ip6_sprintf(&iip6->ip6_dst));
	}

	return (0);
}

int
mipsock_input(miphdr)
	struct mip_msghdr *miphdr;
{
	int err = 0;
	struct mipm_nodetype_info *nodeinfo;

	switch (miphdr->miph_type) {
	case MIPM_NODETYPE_INFO:
		nodeinfo = (struct mipm_nodetype_info *)miphdr;
		homeagent_mode = nodeinfo->mipmni_enable & MIP6_NODETYPE_HOME_AGENT;
	case MIPM_BE_HINT:
		mipsock_behint_input(miphdr);
		break;
	default:
		break;
	}

	return (err);
}

static void
command_flush(s, arg)
	int s;
	char *arg;
{
	flush_bc();
}

static void
terminate(dummy)
	int dummy;
{
	mip6_flush_kernel_bc();
	mipsock_nodetype_request(MIP6_NODETYPE_CORRESPONDENT_NODE, 0);
	unlink(CND_PIDFILE);
	exit(1);
}

#undef EXP_SESSIONTEST

/*
 * Check whether sessions exist between specified node
 */
int
have_session(addr)
	struct in6_addr *addr;
{
#ifdef EXP_SESSIONTEST
#ifdef __NetBSD__
	struct kinfo_pcb *pcblist;
	int mib[8];
	size_t namelen = 0, size = 0, i;
	char *mibname = "net.inet6.tcp6.pcblist";

	memset(mib, 0, sizeof(mib));

	/* get dynamic pcblist node */
	if (sysctlnametomib(mibname, mib, &namelen) == -1) {
		if (errno == ENOENT)
			return (0);
	}

	if (sysctl(mib, sizeof(mib) / sizeof(*mib), NULL, &size,
		   NULL, 0) == -1)
		return (0);
		
	if ((pcblist = malloc(size)) == NULL)
		return (0);
	memset(pcblist, 0, size);

	mib[6] = sizeof(*pcblist);
	mib[7] = size / sizeof(*pcblist);

	if (sysctl(mib, sizeof(mib) / sizeof(*mib), pcblist,
		   &size, NULL, 0) == -1) {
		free(pcblist);
		return (0);
	}

	for (i = 0; i < size / sizeof(*pcblist); i++) {
		struct sockaddr_in6 src, dst;

		memcpy(&src, &pcblist[i].ki_s, sizeof(src));
		memcpy(&dst, &pcblist[i].ki_d, sizeof(dst));

		if (IN6_ARE_ADDR_EQUAL(addr, &dst.sin6_addr)) {
			/* XXX Is checking tcp state needed ?*/
			free(pcblist);
			return (1);
		}
	}
	
	free(pcblist);
	return (0);
#else /* __NetBSD__ */
	char *mibvar = "net.inet.tcp.pcblist";
	char *buf;
	struct xinpgen *xig, *oxig;
	size_t len = 0;
	struct inpcb *inp;

	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		return (0);
	}        
	if ((buf = malloc(len)) == 0) {
		return (0);
	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		free(buf);
		return (0);
	}
        
        if (len <= sizeof(struct xinpgen)) {
            free(buf);
            return (0);
        }
            
	oxig = xig = (struct xinpgen *)buf;
	for (xig = (struct xinpgen *)((char *)xig + xig->xig_len);
	     xig->xig_len > sizeof(struct xinpgen);
	     xig = (struct xinpgen *)((char *)xig + xig->xig_len)) {
		inp = &((struct xtcpcb *)xig)->xt_inp;
		if ((inp->inp_vflag & INP_IPV6) == 0 ||
		    IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
			continue;

		if (IN6_ARE_ADDR_EQUAL(addr, &inp->in6p_faddr)) {
			/* XXX Is checking tcp state needed ?*/
			free(buf);
			return (1);
		}
	}

	free(buf);
	return (0);
#endif /* __NetBSD__ */
#else /* EXP_SESSIONTEST */
	return (1);
#endif /* EXP_SESSIONTEST */
}
