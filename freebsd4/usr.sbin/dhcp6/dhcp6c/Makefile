# Copyright (c) 2003 WIDE Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

SRCDIR=	${.CURDIR}/../../../kame/dhcp6

PROG=	dhcp6c
SRCS=	dhcp6c.c common.c config.c if.c prefixconf.c dhcp6c_ia.c timer.c \
	dhcp6c_script.c base64.c auth.c dhcp6_ctl.c addrconf.c \
	cfparse.y cftoken.l
CLEANFILES+= lex.yy.c y.tab.h y.tab.c
CFLAGS+= -g -Wall -Werror
CFLAGS+= -I${.OBJDIR} -I${SRCDIR}
CFLAGS+= -DINET6=1 -DHAVE_GETIFADDRS=1 -DTIME_WITH_SYS_TIME=1 \
	-DHAVE_IFADDRS_H=1 -DHAVE_STDARG_H=1

LDADD+=	-L${.OBJDIR}/../../../lib/libinet6 \
	-L/usr/local/v6/lib -linet6
DPADD+=	${.OBJDIR}/../../../lib/libinet6/libinet6.a \
	/usr/local/v6/lib/libinet6.a
LDADD+=	-ll -ly
DPADD+=	${LIBL} ${LIBY}

MAN5=	dhcp6c.conf.5
MAN8=	dhcp6c.8

# FreeBSD special
SRCS+=y.tab.h

.PATH:	${SRCDIR}

.include <bsd.prog.mk>
