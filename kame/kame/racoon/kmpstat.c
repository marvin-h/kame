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
/* YIPS @(#)$Id: kmpstat.c,v 1.12 2000/06/08 06:43:52 sakane Exp $ */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <net/pfkeyv2.h>
#include <netkey/keydb.h>
#include <netkey/key_var.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "schedule.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "handler.h"
#include "pfkey.h"
#include "admin_var.h"
#include "admin.h"

#if 0 /*quickhack */
struct myaddrs {
	struct myaddrs *next;
	struct sockaddr *addr;
	int sock;
};
struct myaddrs *myaddrs = NULL;
#endif

struct command_tag {
	char *str;
	u_int16_t cmd;
} command[] = {
{ "reload-config",	ADMIN_RELOAD_CONF },
{ "rc",			ADMIN_RELOAD_CONF },
{ "show-schedule",	ADMIN_SHOW_SCHED },
{ "sc",			ADMIN_SHOW_SCHED },
{ "show-sa",		ADMIN_SHOW_SA },
{ "ss",			ADMIN_SHOW_SA },
{ "flush-sa",		ADMIN_FLUSH_SA },
{ "fs",			ADMIN_FLUSH_SA },
{ "delete-sa",		ADMIN_DELETE_SA },
{ "ds",			ADMIN_DELETE_SA },
{ "establish-sa",	ADMIN_ESTABLISH_SA },
{ "es",			ADMIN_ESTABLISH_SA },
};

struct proto_tag {
	char *str;
	u_int32_t proto;
} proto[] = {
{ "isakmp",	ADMIN_PROTO_ISAKMP },
{ "ipsec",	ADMIN_PROTO_IPSEC },
{ "ah",		ADMIN_PROTO_AH },
{ "esp",	ADMIN_PROTO_ESP },
{ "internal",	ADMIN_PROTO_INTERNAL },
};

struct ul_proto_tag {
	char *str;
	u_short ul_proto;
} ul_proto[] = {
{ "any",	0 },
{ "icmp",	IPPROTO_ICMP },
{ "tcp",	IPPROTO_TCP },
{ "udp",	IPPROTO_UDP },
};

int port = PORT_ADMIN;
int so;

char combuf[512];

char *comarg;
static char _addr1_[NI_MAXHOST], _addr2_[NI_MAXHOST];

char *pname;
int long_format = 0;
u_int32_t debug = 0;

void Usage __P((void));
int com_init __P((void));
int com_send __P((void));
int com_recv __P((void));

int get_combuf __P((int ac, char **));
u_int set_combuf_cmd __P((char *));
u_int set_combuf_proto __P((char *));
int set_combuf_index __P((caddr_t, int, char **));
int set_combuf_indexes __P((caddr_t, int, char **));
u_int set_combuf_family __P((char *));
int set_combuf_comb_address __P((void *, u_int, char *, u_int *));
int set_combuf_sockaddr __P((void *, u_int, char *, char *));
u_int set_combuf_ul_proto __P((char *));

void dump_isakmp_sa __P((char *, int));
void dump_internal __P((char *, int));
char *pindex_isakmp __P((isakmp_index *));
void print_schedule __P((caddr_t, int));
char * fixed_addr __P((char *, char *, int));

void
Usage()
{
	printf(
"Usage:\n"
"  %s [-p (admin port)] reload-config\n"
"  %s [-p (admin port)] [-l] show-sa <protocol>\n"
"  %s [-p (admin port)] flush-sa <protocol>\n"
"  %s [-p (admin port)] delete-sa <saopts>\n"
"  %s [-p (admin port)] establish-sa <saopts>\n"
"\n"
"    <protocol>: \"isakmp\", \"esp\" or \"ah\".\n"
"        In the case of \"show-sa\" or \"flush-sa\", you can use \"ipsec\".\n"
"\n"
"    <saopts>: \"isakmp\" <family> <src> <dst>\n"
"            : {\"esp\",\"ah\"} <family> <src/prefixlen/port> <dst/prefixlen/port>\n"
"                              <ul_proto>\n"
"    <family>: \"inet\" or \"inet6\"\n"
"    <ul_proto>: \"icmp\", \"tcp\", \"udp\" or \"any\"\n",
	pname, pname, pname, pname, pname);
}

int
main(ac, av)
	int ac;
	char **av;
{
	extern char *optarg;
	extern int optind;
	int c;

	pname = *av;

	while ((c = getopt(ac, av, "p:lhd")) != EOF) {
		switch(c) {
		case 'p':
			port = atoi(optarg);
			break;

		case 'l':
			long_format++;
			break;

		case 'd':
			debug = 0xffffffff;
			break;

		case 'h':
			Usage();
			exit(0);
		default:
			printf("Unsupported option: %c\n", c);
			goto bad;
		}
	}

	ac -= optind;
	av += optind;

	if (get_combuf(ac, av) < 0) {
		Usage();
		goto bad;
	}

	if (debug) {
		hexdump(combuf, ((struct admin_com *)combuf)->ac_len);
		exit(0);
	}

	if (com_init() < 0)
		goto bad;

	if (com_send() < 0)
		goto bad;

	if (com_recv() < 0)
		goto bad;

	exit(0);

    bad:
	exit(-1);
}

int
com_init()
{
	struct sockaddr_in name;

	if ((so = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	memset((char *)&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons((u_short)0);
	name.sin_addr.s_addr = htonl(0x7f000001);
	name.sin_len = sizeof(name);

	if (bind(so, (struct sockaddr *)&name, sizeof(name)) < 0) {
		perror("bind");
		(void)close(so);
		return -1;
	}

	memset((char *)&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons((u_short)port);
	name.sin_addr.s_addr = htonl(0x7f000001);
	name.sin_len = sizeof(name);

	if (connect(so, (struct sockaddr *)&name, sizeof(name)) < 0) {
		perror("connect");
		(void)close(so);
		return -1;
	}

	return so;
}

int
com_send()
{
	int len = ((struct admin_com *)combuf)->ac_len;

	if ((len = send(so, combuf, len, 0)) < 0){
		perror("send");
		(void)close(so);
		return -1;
	}

	return len;
}

int
com_recv()
{
	struct admin_com *com;
	caddr_t buf0, buf;
	int len;

	/* receive by PEEK */
	if ((len = recv(so, combuf, sizeof(combuf), MSG_PEEK)) < 0) {
		perror("recv");
		goto bad;
	}
	com = (struct admin_com *)combuf;

	/* sanity check */
	if (len < sizeof(*com))
		goto bad;
	if (len == 0)
		goto bad;	/* ignore */

	/* error ? */
	switch (com->ac_errno) {
	case 0:
		break;
	case ENOENT:
		printf("no entry\n");
		goto end;
	default:
		printf("Error occured with %d\n", com->ac_errno);
		goto end;
	}

	/* allocate buffer */
	buf0 = malloc(com->ac_len);
	if (buf0 == NULL) {
		fprintf(stderr, "no buffer available.\n");
		goto bad;
	}

	/* read real message */
    {
	int l = 0;
	caddr_t p = buf0;
	while (l < com->ac_len) {
		if ((len = recv(so, p, com->ac_len, 0)) < 0) {
			perror("recv");
			goto bad;
		}
		l += len;
		p += len;
	}
    }

	com = (struct admin_com *)buf0;
	len = com->ac_len - sizeof(*com);
	buf = buf0 + sizeof(*com);

	switch (com->ac_cmd) {
	case ADMIN_SHOW_SCHED:
		print_schedule(buf, len);
		break;

	case ADMIN_SHOW_SA:
	   {
		switch (com->ac_proto) {
		case ADMIN_PROTO_ISAKMP:
			dump_isakmp_sa(buf, len);
			break;
		case ADMIN_PROTO_IPSEC:
		case ADMIN_PROTO_AH:
		case ADMIN_PROTO_ESP:
		    {
			struct sadb_msg *msg = (struct sadb_msg *)buf;

			switch (msg->sadb_msg_errno) {
			case ENOENT:
				switch (msg->sadb_msg_type) {
				case SADB_DELETE:
				case SADB_GET:
					printf("No entry.\n");
					break;
				case SADB_DUMP:
					printf("No SAD entries.\n");
					break;
				}
				break;
			case 0:
				while (1) {
					pfkey_sadump(msg);
					if (msg->sadb_msg_seq == 0)
						break;
					msg = (struct sadb_msg *)((caddr_t)msg +
						     PFKEY_UNUNIT64(msg->sadb_msg_len));
				}
				break;
			default:
				printf("%s.\n", strerror(msg->sadb_msg_errno));
			}
		    }
			break;
		case ADMIN_PROTO_INTERNAL:
			dump_internal(buf, len);
			break;
		default:
			printf("Invalid proto [%d]\n", com->ac_proto);
		}

	    }
		break;

	default:
		/* IGNORE */
	}

    end:
	(void)close(so);
	return 0;

    bad:
	return -1;
}

/* %%% */
int
get_combuf(ac, av)
	int ac;
	char **av;
{
	struct admin_com *com = (struct admin_com *)combuf;

	/* checking the string of command. */
	if ((com->ac_cmd = set_combuf_cmd(*av)) == (u_int16_t)~0)
		goto bad;
	av++;
	ac--;

	/* initialization */
	com->ac_len = sizeof(struct admin_com);
	com->ac_errno = 0;
	com->ac_proto = 0;

	switch (com->ac_cmd) {
	case ADMIN_RELOAD_CONF:
	case ADMIN_SHOW_SCHED:
		break;

	case ADMIN_SHOW_SA:
	case ADMIN_FLUSH_SA:
		/* validity check */
		if (ac != 1)
			goto bad;

		if ((com->ac_proto = set_combuf_proto(*av)) == (u_int16_t)~0)
			goto bad;
		break;
		
	case ADMIN_DELETE_SA:
	case ADMIN_ESTABLISH_SA:
		/* validity check */
		if (ac < 1)
			goto bad;

		if ((com->ac_proto = set_combuf_proto(*av)) == (u_int16_t)~0)
			goto bad;
		av++;
		ac--;
	    {
		caddr_t p = combuf + sizeof(*com);

		/* get index(es) */
		switch (com->ac_proto) {
		case ADMIN_PROTO_ISAKMP:
			if (set_combuf_index(p, ac, av) < 0)
				goto bad;
			break;
		case ADMIN_PROTO_AH:
		case ADMIN_PROTO_ESP:
			if (set_combuf_indexes(p, ac, av) < 0)
				goto bad;
			break;
		default:
			printf("Illegal protocol.\n");
			goto bad;
		}
	    }

		com->ac_len += sizeof(struct admin_com_indexes);
		break;

	default:
		goto bad;
	}

	return 0;

    bad:
	return -1;
}

u_int
set_combuf_cmd(str)
	char *str;
{
	int i;

	if (str == NULL)
		return ~0;

	for (i = 0; i < sizeof(command)/sizeof(command[0]); i++) {
		if (strcmp(command[i].str, str) == 0)
			return command[i].cmd;
	}

	printf("Invalid command [%s]\n", str);

	return ~0;
}

u_int
set_combuf_proto(str)
	char *str;
{
	int i;

	if (str == NULL)
		return ~0;

	/* checking the string of protocol */
	for (i = 0; i < sizeof(proto)/sizeof(proto[0]); i++) {
		if (strcmp(proto[i].str, str) == 0)
			return proto[i].proto;
	}

	printf("Invalid proto [%s]\n", str);

	return ~0;
}

int
set_combuf_index(buf, ac, av)
	caddr_t buf;
	int ac;
	char **av;
{
	struct admin_com_indexes *index_buf = (struct admin_com_indexes *)buf;
	u_int family;

	memset((caddr_t)index_buf, 0, sizeof(struct admin_com_indexes));

	if (*av == NULL)
		return -1;

	if (ac != 3) {
		printf("Too few arguments.\n");
		return -1;
	}

	/* checking the string of family */
	if ((family = set_combuf_family(*av)) == ~0)
		return -1;
	av++;

	/* set soruce address */
	if (set_combuf_sockaddr(&index_buf->src, family, *av, NULL) < 0)
		return -1;
	av++;

	/* set destination address */
	if (set_combuf_sockaddr(&index_buf->dst, family, *av, NULL) < 0)
		return -1;

	return sizeof(struct admin_com_indexes);
}

int
set_combuf_indexes(buf, ac, av)
	caddr_t buf;
	int ac;
	char **av;
{
	struct admin_com_indexes *index_buf = (struct admin_com_indexes *)buf;
	u_int family;

	memset((caddr_t)index_buf, 0, sizeof(struct admin_com_indexes));

	if (ac != 4 && ac != 6)
		return -1;

	if (*av == NULL)
		return -1;

	/* checking the string of family */
	if ((family = set_combuf_family(*av)) == ~0)
		return -1;
	av++;

	/* set soruce address */
	if (set_combuf_comb_address(&index_buf->src,
			family, *av, (u_int *)&index_buf->prefs) < 0)
		return -1;
	av++;

	/* set destination address */
	if (set_combuf_comb_address(&index_buf->dst,
			family, *av, (u_int *)&index_buf->prefd) < 0)
		return -1;
	av++;

	/* checking the string of upper layer protocol */
	if ((index_buf->ul_proto = set_combuf_ul_proto(*av)) == (u_int8_t)~0)
		return -1;

	return sizeof(struct admin_com_indexes);
}

u_int
set_combuf_family(str)
	char *str;
{
	if (strcmp("inet", str) == 0)
		return AF_INET;
#ifdef INET6
	else if (strcmp("inet6", str) == 0)
		return AF_INET6;
#endif

	printf("Invalid family [%s].\n", str);

	return ~0;
}

int
set_combuf_comb_address(buf, family, str, pref)
	void *buf;
	u_int family;
	char *str;
	u_int *pref;
{
	int i;
	char p_name[124], *p_pref, *p;

	for (i = 0, p = str; *p != NULL && *p != '/'; p++, i++) ;
	if (i == strlen(str) || *p == NULL || *++p == NULL) {
		printf("Illegal format [%s].\n", str);
		return -1;
	}
	memcpy(p_name, str, i);
	p_name[i] = NULL;
	p_pref = p;
	for (i = 0; *p != NULL && *p != '/'; p++, i++) ;
	if (*p == NULL || *++p == NULL) {
		printf("Illegal format [%s].\n", str);
		return -1;
	}
	p_pref[i] = NULL;
	*pref = (u_int8_t)atoi(p_pref);
		/* XXX should be handled the error to atoi(). */

	if (set_combuf_sockaddr(buf, family, p_name, p) < 0)
		return -1;

	return 0;
}

int
set_combuf_sockaddr(buf, family, name, port)
	void *buf;
	u_int family;
	char *name, *port;
{
	struct addrinfo hint, *ai;
	int error;

	memset(&hint, 0, sizeof(hint));
	hint.ai_family = PF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;

	if ((error = getaddrinfo(name, port, &hint, &ai)) != 0) {
		printf("%s, %s/%s\n", gai_strerror(error), name, port);
		return -1;
	}

	memcpy(buf, ai->ai_addr, ai->ai_addr->sa_len);

	return ai->ai_addr->sa_len;
}

u_int
set_combuf_ul_proto(str)
	char *str;
{
	int i;

	for (i = 0; i < sizeof(ul_proto)/sizeof(ul_proto[0]); i++) {
		if (strcmp(ul_proto[i].str, str) == 0)
			return ul_proto[i].ul_proto;
	}

	printf("Invalud ulp [%s]\n", str);
	return ~0;
}

/* %%% */
void
dump_isakmp_sa(buf, len)
	char *buf;
	int len;
{
	struct ph1dump *pd;
	struct tm *tm;
	char tbuf[56];

/* isakmp status header */
/* short header;
 1234567890123456789012 0000000000000000:0000000000000000 000000000000
*/
char *header1 = 
"Destination            Cookies                           Created";

/* semi long header;
 1234567890123456789012 0000000000000000:0000000000000000 00 X 00 X X 0000-00-00 00:00:00 1234567890123456789012 
*/
char *header2 = 
"Destination            Cookies                           ST S V  E U Created             Source";

/* long header;
 0000:0000:0000:0000:0000:0000:0000:0000.00000 0000000000000000:0000000000000000 00 X 00 X X 0000-00-00 00:00:00 0000:0000:0000:0000:0000:0000:0000:0000.00000 
*/
char *header3 =
"Destination                                   Cookies                           ST S V  E U Created             Source";

/* phase status header */
/* short format;
   side stats source address         destination address   
   xxx  xxxxx 1234567890123456789012 1234567890123456789012
*/

	static char *estr[] = { "", "B", "M", "U", "A", "I", };

	switch (long_format) {
	case 0:
		printf("%s\n", header1);
		break;
	case 1:
		printf("%s\n", header2);
		break;
	case 2:
	default:
		printf("%s\n", header3);
		break;
	}

	if (len % sizeof(*pd))
		printf("invalid length %d\n", len);
	len /= sizeof(*pd);

	pd = (struct ph1dump *)buf;

	while (len-- > 0) {
	    {
		char *p = NULL;

		GETNAMEINFO((struct sockaddr *)&pd->remote, _addr1_, _addr2_);
		switch (long_format) {
		case 0:
		case 1:
			p = fixed_addr(_addr1_, _addr2_, 22);
			break;
		case 2:
		default:
			p = fixed_addr(_addr1_, _addr2_, 45);
			break;
		}
		printf("%s ", p);
	    }

		printf("%s ", pindex_isakmp(&pd->index));

		if (long_format >= 1) {
			printf("%2d %c %x ",
				pd->status,
				pd->side == INITIATOR ? 'I' : 'R',
				pd->version);
			if (ARRAYLEN(estr) > pd->etype)
				printf("%s ", estr[pd->etype]);
		}

		tm = localtime(&pd->created);
		strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %T", tm);
		printf("%s ", tbuf);

		if (long_format >= 1) {
			char *p = NULL;

			GETNAMEINFO((struct sockaddr *)&pd->local, _addr1_, _addr2_);
			switch (long_format) {
			case 0:
				break;
			case 1:
				p = fixed_addr(_addr1_, _addr2_, 22);
				break;
			case 2:
			default:
				p = fixed_addr(_addr1_, _addr2_, 45);
				break;
			}
			printf("%s ", p);
		}

		printf("\n");

		pd++;
	}

	return;
}

/* %%% */
void
dump_internal(buf, tlen)
	char *buf;
	int tlen;
{
	struct ph2handle *iph2;
	struct sockaddr *addr;

/*
short header;
 source address         destination address    
 1234567890123456789012 1234567890123456789012 
*/
char *short_h1 = 
"Source                 Destination            ";

/*
long header;
 source address                                destination address                           
 123456789012345678901234567890123456789012345 123456789012345678901234567890123456789012345 
 0000:0000:0000:0000:0000:0000:0000:0000.00000 0000:0000:0000:0000:0000:0000:0000:0000.00000 0000:0000:0000:0000:0000:0000:0000:0000.00000
*/
char *long_h1 = 
"Source                                        Destination                                  ";

	printf("%s\n", long_format ? long_h1 : short_h1);

	while (tlen > 0) {
		iph2 = (struct ph2handle *)buf;
		addr = (struct sockaddr *)(++iph2);

		GETNAMEINFO(addr, _addr1_, _addr2_);
		printf("%s ", long_format ?
			  fixed_addr(_addr1_, _addr2_, 45)
			: fixed_addr(_addr1_, _addr2_, 22));
		addr++;
		tlen -= addr->sa_len;

		GETNAMEINFO(addr, _addr1_, _addr2_);
		printf("%s ", long_format ?
			  fixed_addr(_addr1_, _addr2_, 45)
			: fixed_addr(_addr1_, _addr2_, 22));
		addr++;
		tlen -= addr->sa_len;

		printf("\n");
	}

	return;
}

/* %%% */
char *
pindex_isakmp(index)
	isakmp_index *index;
{
	static char buf[64];
	u_char *p;
	int i, j;

	memset(buf, 0, sizeof(buf));

	/* copy index */
	p = (u_char *)index;
	for (j = 0, i = 0; i < sizeof(isakmp_index); i++) {
		snprintf((char *)&buf[j], sizeof(buf) - j, "%02x", p[i]);
		j += 2;
		switch (i) {
		case 7:
#if 0
		case 15:
#endif
			buf[j++] = ':';
		}
	}

	return buf;
}

/* print schedule */
char *str_sched_stat[] = {
"off",
"on",
"dead",
};

char *str_sched_id[] = {
"PH1resend",
"PH1lifetime",
"PH2resend",
"PSTacquire",
"PSTlifetime",
};

void
print_schedule(buf, len)
	caddr_t buf;
	int len;
{
	struct scheddump *sc = (struct scheddump *)buf;
	struct tm *tm;
	char tbuf[56];

	if (len % sizeof(*sc))
		printf("invalid length %d\n", len);
	len /= sizeof(*sc);

	/*      00000000 00000000 00000000 xxx........*/
	printf("index    tick     xtime    created\n");

	while (len-- > 0) {
		tm = localtime(&sc->created);
		strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %T", tm);

		printf("%-8ld %-8ld %-8ld %s\n",
			sc->id,
			(long)sc->tick,
			(long)sc->xtime,
			tbuf);
		sc++;
	}

	return;
}

char *
fixed_addr(addr, port, len)
	char *addr, *port;
	int len;
{
	static char _addr_buf_[BUFSIZ];
	char *p;
	int plen, i;

	/* initialize */
	memset(_addr_buf_, ' ', sizeof(_addr_buf_));

	plen = strlen(port);
	if (len < plen + 1)
		return NULL;

	p = _addr_buf_;
	for (i = 0; i < len - plen - 1 && addr[i] != '\0'; /*noting*/)
		*p++ = addr[i++];
	*p++ = '.';

	for (i = 0; i < plen && port[i] != '\0'; /*noting*/)
		*p++ = port[i++];

	_addr_buf_[len] = '\0';

	return _addr_buf_;
}
